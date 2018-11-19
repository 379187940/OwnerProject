// Copyright 2001-2017 Angelicatek GmbH / Angelicatek Group. All rights reserved. 

/*************************************************************************
   -------------------------------------------------------------------------
   Description:
   Specialized Container for Renderer data with the following properties:
   - Created during the 3DEngine Update, consumed in the renderer in the following frame
   - This Container is very restricted and likely not optimal for other situations
   -------------------------------------------------------------------------
   History:
   - 13:06:2013: Created by Nico Moss
*************************************************************************/

#ifndef _ANGELICA_THREAD_SAFE_WORKER_CONTAINER_H_
#define _ANGELICA_THREAD_SAFE_WORKER_CONTAINER_H_

#include <AngelicaCore/Platform/platform.h>
#include <vector>

//! BE CAREFULL WHEN USING THIS CONTAINER!
//!
//! --- Properties: ---.
//! - Stores data local to worker thread to avoid thread-safety semantics.
//! - Allows for a single non-worker thread to be tracked which is stored in m_workers[0].
//!     Hence: As m_workers[0] is shared between all none-worker threads, ensure that only one additional none-worker thread may access this container e.g. MainThread.
//! - Coalesce memory to obtain a continues memory block.
//! - Coalesce memory to for faster element access to a continues memory block.
//!
//! --- Restrictions:---.
//! - The workers own the memory structure.
//! - The coalesced memory stores a copy of the workers used memory.
//!     Hence: Be careful when altering data within the coalesced memory.
//!     If the templated element is a pointer type than altering the memory pointed to, is not be an issue.
//!     If the templated element is of type class or struct than ensure that data changes are done on the worker local data and not on the coalesced memory. Use worker encoded indices to do so.
template<class T>
class CThreadSafeWorkerContainer
{
public:
	struct SDefaultNoOpFunctor
	{
		inline void operator()(T* pData) const {}
	};

public:
	CThreadSafeWorkerContainer();
	~CThreadSafeWorkerContainer();

	void Init();
	void SetNoneWorkerThreadID(unsigned long nThreadId) { m_foreignWorkerId = nThreadId; }

	//! Safe access of elements for calling thread via operator[].
	unsigned int ConvertToEncodedWorkerId_threadlocal(unsigned int nIndex) const;

	unsigned int GetNumWorkers() const;
	unsigned int GetWorkerId_threadlocal() const;

	//! \note Be aware that these values can potentially change if some objects are added in parallel.
	size_t size() const;
	bool   empty() const;
	size_t capacity() const;

	size_t size_threadlocal() const;
	bool   empty_threadlocal() const;
	size_t capacity_threadlocal() const;

	//! \note Be aware that this operator is more expensive if the memory was not coalesced before.
	T&       operator[](size_t n);
	const T& operator[](size_t n) const;

	T*       push_back_new();
	T*       push_back_new(size_t& nIndex);

	void     push_back(const T& rObj);
	void     push_back(const T& rObj, size_t& nIndex);

	//! \note These functions are changing the size of the continuous memory block and thus are *not* thread-safe.
	void                                        clear();
	template<class OnElementDeleteFunctor> void clear(const OnElementDeleteFunctor& rFunctor = CThreadSafeWorkerContainer<T>::SDefaultNoOpFunctor());
	void                                        erase(const T& rObj);
	void                                        resize(size_t n);
	void                                        reserve(size_t n);

	//  *not* thread-safe functions
	void PrefillContainer(T* pElement, size_t numElements);
	void CoalesceMemory();

	void GetMemoryUsage(IAngelicaSizer* pSizer) const;

private:

	class ANGELICA_ALIGN(128) SWorker
	{
	public:
		SWorker() : m_dataSize(0) {}

		unsigned int m_dataSize;
		std::vector<T> m_data;
	};

	T*   push_back_impl(size_t& nIndex);
	void ReserverCoalescedMemory(size_t n);

	unsigned long m_foreignWorkerId; //!< Id of the non-job-manager-worker thread that's also allowed to use this container.

	SWorker* m_workers;
	unsigned int   m_nNumWorkers;

	unsigned int   m_coalescedArrCapacity;
	T*       m_coalescedArr;
	bool     m_isCoalesced;
};

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline CThreadSafeWorkerContainer<T>::CThreadSafeWorkerContainer()
	: m_nNumWorkers(0)
	, m_workers(0)
	, m_coalescedArrCapacity(0)
	, m_coalescedArr(0)
	, m_isCoalesced(false)
{
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline CThreadSafeWorkerContainer<T>::~CThreadSafeWorkerContainer()
{
	clear();
	AngelicaAlignedDeleteArray(m_workers, m_nNumWorkers);
	m_workers = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::Init()
{
	m_nNumWorkers = GetJobManagerInterface()->GetNumWorkerThreads() + 1; 
	m_workers = AngelicaAlignedNewArray<CThreadSafeWorkerContainer<T>::SWorker>(m_nNumWorkers);

	m_foreignWorkerId = THREADID_NULL;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline size_t CThreadSafeWorkerContainer<T >::size() const
{
	unsigned int totalSize = 0;
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		totalSize += m_workers[i].m_dataSize;
	}
	return totalSize;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline bool CThreadSafeWorkerContainer<T >::empty() const
{
	return size() == 0;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline size_t CThreadSafeWorkerContainer<T >::capacity() const
{
	unsigned int totalCapacity = 0;
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		totalCapacity += m_workers[i].m_data.capacity();
	}
	return totalCapacity;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline size_t CThreadSafeWorkerContainer<T >::size_threadlocal() const
{
	const unsigned int nWorkerThreadId = GetWorkerId_threadlocal();
	return m_workers[nWorkerThreadId].m_dataSize;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline bool CThreadSafeWorkerContainer<T >::empty_threadlocal() const
{
	const unsigned int nWorkerThreadId = GetWorkerId_threadlocal();
	return m_workers[nWorkerThreadId].m_data.empty();
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline size_t CThreadSafeWorkerContainer<T >::capacity_threadlocal() const
{
	const unsigned int nWorkerThreadId = GetWorkerId_threadlocal();
	return m_workers[nWorkerThreadId].m_data.capacity();
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline T& CThreadSafeWorkerContainer<T >::operator[](size_t n)
{
	const unsigned int nHasWorkerEncodedIndex = (n & 0x80000000) >> 31;

	IF ((m_isCoalesced && !nHasWorkerEncodedIndex))
	{
		return m_coalescedArr[n];
	}
	else
	{
		const unsigned int nWorkerThreadId = (n & 0x7F00007F) >> 24; // Mask bit 24-30  (0 is starting bit)
		const unsigned int nOffset = (n & ~0xFF000000);              // Mask out top 8 bits

		// Encoded offset into worker local array
		if (nHasWorkerEncodedIndex)
		{
			return m_workers[nWorkerThreadId].m_data[nOffset];
		}
		else // None-coalesced and none worker encoded offset
		{
			unsigned int nTotalOffset = nOffset;
			for (int i = 0; i < m_nNumWorkers; ++i)
			{
				SWorker& worker = m_workers[i];

				if (nTotalOffset < worker.m_dataSize)
					return worker.m_data[nTotalOffset];
				else
					nTotalOffset -= worker.m_dataSize;
			}

			// Out of bound access detected!
			//ANGELICA_ASSERT_MESSAGE(false, "CThreadSafeWorkerContainer::operator[] - Out of bounds access");
			__debugbreak();
			return m_coalescedArr[0];
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline const T& CThreadSafeWorkerContainer<T >::operator[](size_t n) const
{
	return const_cast<const T&>(const_cast<CThreadSafeWorkerContainer<T>*>(this)->operator[](n));
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline T* CThreadSafeWorkerContainer<T >::push_back_new()
{
	size_t unused = ~0;
	return push_back_impl(unused);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline T* CThreadSafeWorkerContainer<T >::push_back_new(size_t& nIndex)
{
	return push_back_impl(nIndex);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::push_back(const T& rObj)
{
	size_t nUnused = ~0;
	T* pObj = push_back_impl(nUnused);
	*pObj = rObj;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::push_back(const T& rObj, size_t& nIndex)
{
	T* pObj = push_back_impl(nIndex);
	*pObj = rObj;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::clear()
{
	clear(SDefaultNoOpFunctor());
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
template<class OnElementDeleteFunctor>
inline void CThreadSafeWorkerContainer<T >::clear(const OnElementDeleteFunctor& rFunctor)
{
	// Reset worker data
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		// Delete elements
		unsigned int nSize = m_workers[i].m_data.size();
		for (int j = 0; j < nSize; ++j)
		{
			// Call on element delete functor
			// Note: Default functor will do nothing with the element
			rFunctor(&m_workers[i].m_data[j]);
		}

		stl::free_container(m_workers[i].m_data);
		m_workers[i].m_dataSize = 0;
	}

	// Reset container data
	if (m_coalescedArr)
		_aligned_free(m_coalescedArr);

	m_coalescedArr = 0;
	m_coalescedArrCapacity = 0;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::erase(const T& rObj)
{
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		typename std::vector<T>::iterator iter = m_workers[i].m_data.begin();
		typename std::vector<T>::iterator iterEnd = m_workers[i].m_data.end();

		for (; iter != iterEnd; ++iter)
		{
			if (rObj == *iter)
			{
				m_workers[i].m_data.erase(iter);
				--m_workers[i].m_dataSize;
				m_isCoalesced = false;
				return;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::resize(size_t n)
{
	CoalesceMemory();

	unsigned int nSizePerWorker = n / m_nNumWorkers;
	unsigned int nExcessSize = n % m_nNumWorkers;

	// Resize workers evenly
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		unsigned int nWorkerSize = nSizePerWorker + nExcessSize;

		if (nWorkerSize > m_workers[i].m_data.size())
			m_workers[i].m_data.resize(nWorkerSize);

		m_workers[i].m_dataSize = nWorkerSize;
		nExcessSize = 0; // First worker creates excess items
	}

	ReserverCoalescedMemory(n);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::reserve(size_t n)
{
	CoalesceMemory();

	unsigned int nSizePerWorker = n / m_nNumWorkers;
	unsigned int nExcessSize = n % m_nNumWorkers;

	// Resize workers evenly
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		unsigned int nWorkerSize = nSizePerWorker + nExcessSize;

		if (nWorkerSize > m_workers[i].m_data.size())
			m_workers[i].m_data.resize(nWorkerSize);

		nExcessSize = 0; // First worker creates excess items
	}

	ReserverCoalescedMemory(n);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::PrefillContainer(T* pElement, size_t numElements)
{
	reserve(numElements);

	unsigned int nOffset = 0;
	unsigned int nNumItemPerWorker = numElements / m_nNumWorkers;
	unsigned int nNumExcessItems = numElements % m_nNumWorkers;

	// Store items evenly in workers
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		unsigned int nNumItems = nNumItemPerWorker + nNumExcessItems;
		for (int j = 0; j < nNumItems; ++j)
		{
			m_workers[i].m_data[j] = pElement[nOffset + j];
		}

		m_workers[i].m_dataSize = nNumItems;
		nOffset += nNumItems;
		nNumExcessItems = 0; // First worker stores excess items
	}

	m_isCoalesced = false;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::CoalesceMemory()
{
	if (m_isCoalesced)
		return;

	static AngelicaCriticalSectionNonRecursive s_accessLock;
	AngelicaAutoLock<AngelicaCriticalSectionNonRecursive> lock(s_accessLock);

	// Ensure enough memory exists
	unsigned int minSizeNeeded = 0;
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		minSizeNeeded += m_workers[i].m_dataSize;
	}

	IF (minSizeNeeded >= m_coalescedArrCapacity, 0)
		ReserverCoalescedMemory(minSizeNeeded + (minSizeNeeded / 4));

	// Copy data to coalesced array
	unsigned int nOffest = 0;
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		SWorker& rWorker = m_workers[i];
		if (rWorker.m_dataSize == 0)
			continue;
		memcpy(m_coalescedArr + nOffest, &rWorker.m_data[0], sizeof(T) * rWorker.m_dataSize);
		nOffest += rWorker.m_dataSize;
	}

	m_isCoalesced = true;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
unsigned int CThreadSafeWorkerContainer<T >::ConvertToEncodedWorkerId_threadlocal(unsigned int nIndex) const
{
	const unsigned int nThreadId = GetWorkerId_threadlocal();
	assert(nIndex < m_workers[nThreadId].m_dataSize);
	return (unsigned int)((1 << 31) | (nThreadId << 24) | nIndex);
}

//////////////////////////////////////////////////////////////////////////
template<typename T>
unsigned int CThreadSafeWorkerContainer<T >::GetNumWorkers() const
{
	return m_nNumWorkers;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::GetMemoryUsage(IAngelicaSizer* pSizer) const
{
	pSizer->AddObject(m_coalescedArr, m_coalescedArrCapacity * sizeof(T));

	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		pSizer->AddContainer(m_workers[i].m_data);
	}
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::ReserverCoalescedMemory(size_t n)
{
	if (n <= m_coalescedArrCapacity)
		return;

	T* arrOldData = m_coalescedArr;
	m_coalescedArr = reinterpret_cast<T*>(_aligned_malloc(n * sizeof(T), alignof(T)));
	memcpy(m_coalescedArr, arrOldData, m_coalescedArrCapacity * sizeof(T));
	if (arrOldData)
		_aligned_free(arrOldData);
	m_coalescedArrCapacity = n;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline T* CThreadSafeWorkerContainer<T >::push_back_impl(size_t& nIndex)
{
	// Avoid writing to thread share resource and take hit of 'if statement to avoid false-sharing between threads
	IF (m_isCoalesced, 0)
		m_isCoalesced = false;

	// Get worker id
	const unsigned int nWorkerThreadId = GetWorkerId_threadlocal();

	SWorker& activeWorker = m_workers[nWorkerThreadId];

	// Ensure enough space
	if (activeWorker.m_dataSize >= activeWorker.m_data.size())
		activeWorker.m_data.resize(activeWorker.m_data.size() + (activeWorker.m_data.size() / 2) + 1);

	// Encode worker local offset into index and return
	T* retItem = &activeWorker.m_data[activeWorker.m_dataSize];
	nIndex = (size_t)((1 << 31) | (nWorkerThreadId << 24) | activeWorker.m_dataSize);
	++activeWorker.m_dataSize;
	return retItem;
}

template<typename T>
unsigned int CThreadSafeWorkerContainer<T >::GetWorkerId_threadlocal() const
{
	const unsigned int nWorkerId = JobManager::GetWorkerThreadId();

	// Check if none-worker is valid
	if (nWorkerId == ~0)
	{
		if (m_foreignWorkerId != GetCurrentThreadId())
			//AngelicaFatalError("Trying to access CThreadSafeWorkerContainer from an unspecified none-worker thread. Current specified none-worker threadId with access rights: %" PRI_THREADID, m_foreignWorkerId);
	}

	// None-worker has id of ~0 ... add +1 to shift to 0. Worker0 will use slot 1 etc.
	return nWorkerId + 1;
}

#endif // _ANGELICA_THREAD_SAFE_WORKER_CONTAINER_H_
