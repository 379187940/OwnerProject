// Copyright 2001-2017 Angelicatek GmbH / Angelicatek Group. All rights reserved. 

#pragma once

// Include basic multithread primitives.
#include "AngelicaAtomics.h"
typedef const char*            cstr;
//class AngelicaConditionVariable;
class AngelicaSemaphore;
class AngelicaFastSemaphore;

#define THREAD_NAME_LENGTH_MAX 64

enum AngelicaLockType
{
	ANGELICALOCK_FAST      = 1,  //!< A fast potentially (non-recursive) mutex.
	ANGELICALOCK_RECURSIVE = 2,  //!< A recursive mutex.
};

//! Primitive locks and conditions.
//! Primitive locks are represented by instance of class AngelicaLockT<Type>.
template<AngelicaLockType Type> class AngelicaLockT
{
	/* Unsupported lock type. */
};

//////////////////////////////////////////////////////////////////////////
// Typedefs.
//////////////////////////////////////////////////////////////////////////
typedef AngelicaLockT<ANGELICALOCK_RECURSIVE> AngelicaCriticalSection;
typedef AngelicaLockT<ANGELICALOCK_FAST>      AngelicaCriticalSectionNonRecursive;

//////////////////////////////////////////////////////////////////////////
//! AngelicaAutoCriticalSection implements a helper class to automatically.
//! lock critical section in constructor and release on destructor.
template<class LockClass> class AngelicaAutoLock
{
public:
	AngelicaAutoLock() = delete;
	AngelicaAutoLock(const AngelicaAutoLock<LockClass>&) = delete;
	AngelicaAutoLock<LockClass>& operator=(const AngelicaAutoLock<LockClass>&) = delete;

	AngelicaAutoLock(LockClass& Lock) : m_pLock(&Lock) { m_pLock->Lock(); }
	AngelicaAutoLock(const LockClass& Lock) : m_pLock(const_cast<LockClass*>(&Lock)) { m_pLock->Lock(); }
	~AngelicaAutoLock() { m_pLock->Unlock(); }
private:
	LockClass* m_pLock;
};

//! AngelicaOptionalAutoLock implements a helper class to automatically.
//! Lock critical section (if needed) in constructor and release on destructor.
template<class LockClass> class AngelicaOptionalAutoLock
{
private:
	LockClass* m_Lock;
	bool       m_bLockAcquired;

	AngelicaOptionalAutoLock();
	AngelicaOptionalAutoLock(const AngelicaOptionalAutoLock<LockClass>&);
	AngelicaOptionalAutoLock<LockClass>& operator=(const AngelicaOptionalAutoLock<LockClass>&);

public:
	AngelicaOptionalAutoLock(LockClass& Lock, bool acquireLock) : m_Lock(&Lock), m_bLockAcquired(false)
	{
		if (acquireLock)
		{
			Acquire();
		}
	}
	~AngelicaOptionalAutoLock()
	{
		Release();
	}
	void Release()
	{
		if (m_bLockAcquired)
		{
			m_Lock->Unlock();
			m_bLockAcquired = false;
		}
	}
	void Acquire()
	{
		if (!m_bLockAcquired)
		{
			m_Lock->Lock();
			m_bLockAcquired = true;
		}
	}
};

//! AngelicaAutoSet implements a helper class to automatically.
//! set and reset value in constructor and release on destructor.
template<class ValueClass> class AngelicaAutoSet
{
private:
	ValueClass* m_pValue;

	AngelicaAutoSet();
	AngelicaAutoSet(const AngelicaAutoSet<ValueClass>&);
	AngelicaAutoSet<ValueClass>& operator=(const AngelicaAutoSet<ValueClass>&);

public:
	AngelicaAutoSet(ValueClass& value) : m_pValue(&value) { *m_pValue = (ValueClass)1; }
	~AngelicaAutoSet() { *m_pValue = (ValueClass)0; }
};

//! Auto critical section is the most commonly used type of auto lock.
typedef AngelicaAutoLock<AngelicaCriticalSection>             AngelicaAutoCriticalSection;
typedef AngelicaAutoLock<AngelicaCriticalSectionNonRecursive> AngelicaAutoCriticalSectionNoRecursive;

#define AUTO_LOCK_T(Type, lock) PREFAST_SUPPRESS_WARNING(6246); AngelicaAutoLock<Type> __AutoLock(lock)
#define AUTO_LOCK(lock)         AUTO_LOCK_T(AngelicaCriticalSection, lock)
#define AUTO_LOCK_CS(csLock)    AngelicaAutoCriticalSection __AL__ ## csLock(csLock)

///////////////////////////////////////////////////////////////////////////////
//! Base class for lockless Producer/Consumer queue, due platforms specific they are implemented in AngelicaThead_platform.h.
namespace AngelicaMT {
namespace detail {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class SingleProducerSingleConsumerQueueBase
{
public:
	SingleProducerSingleConsumerQueueBase()
	{}

	void Push(void* pObj, volatile unsigned int& rProducerIndex, volatile unsigned int& rConsumerIndex, unsigned int nBufferSize, void* arrBuffer, unsigned int nObjectSize);
	void Pop(void* pObj, volatile unsigned int& rProducerIndex, volatile unsigned int& rConsumerIndex, unsigned int nBufferSize, void* arrBuffer, unsigned int nObjectSize);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class N_ProducerSingleConsumerQueueBase
{
public:
	N_ProducerSingleConsumerQueueBase()
	{
		AngelicaInitializeSListHead(fallbackList);
	}

	void Push(void* pObj, volatile unsigned int& rProducerIndex, volatile unsigned int& rConsumerIndex, volatile unsigned int& rRunning, void* arrBuffer, unsigned int nBufferSize, unsigned int nObjectSize, volatile unsigned int* arrStates);
	bool Pop(void* pObj, volatile unsigned int& rProducerIndex, volatile unsigned int& rConsumerIndex, volatile unsigned int& rRunning, void* arrBuffer, unsigned int nBufferSize, unsigned int nObjectSize, volatile unsigned int* arrStates);

private:
	SLockFreeSingleLinkedListHeader fallbackList;
	struct SFallbackList
	{
		SLockFreeSingleLinkedListEntry nextEntry;
		char                           alignment_padding[128 - sizeof(SLockFreeSingleLinkedListEntry)];
		char                           object[1];         //!< Struct will be overallocated with enough memory for the object
	};
};

} // namespace detail
} // namespace AngelicaMT

//////////////////////////////////////////////////////////////////////////
namespace AngelicaMT {
	void AngelicaMemoryBarrier();
	void AngelicaYieldThread();
} // namespace AngelicaMT

// Include architecture specific code.
#if _WIN32
	#include "AngelicaThread_win32.h"
#endif




//! Auto-locking classes.
template<class T, bool bDEBUG = false>
class AutoLockRead
{
protected:
	const T& m_lock;
public:
	AutoLockRead(const T& lock, cstr strDebug = 0)
		: m_lock(lock) { m_lock.LockRead(bDEBUG, strDebug, bDEBUG); }
	~AutoLockRead()
	{ m_lock.UnlockRead(); }
};

template<class T, bool bDEBUG = false>
class AutoLockModify
{
protected:
	const T& m_lock;
public:
	AutoLockModify(const T& lock, cstr strDebug = 0)
		: m_lock(lock) { m_lock.LockModify(bDEBUG, strDebug, bDEBUG); }
	~AutoLockModify()
	{ m_lock.UnlockModify(); }
};


///////////////////////////////////////////////////////////////////////////////
//! Base class for lockless Producer/Consumer queue, due platforms specific they are implemented in AngelicaThead_platform.h.
namespace AngelicaMT {
namespace detail {

///////////////////////////////////////////////////////////////////////////////
inline void SingleProducerSingleConsumerQueueBase::Push(void* pObj, volatile unsigned int& rProducerIndex, volatile unsigned int& rConsumerIndex, unsigned int nBufferSize, void* arrBuffer, unsigned int nObjectSize)
{
	// spin if queue is full
	CSimpleThreadBackOff backoff;
	while (rProducerIndex - rConsumerIndex == nBufferSize)
	{
		backoff.backoff();
	}

	AngelicaMT::AngelicaMemoryBarrier();
	char* pBuffer = alias_cast<char*>(arrBuffer);
	unsigned int nIndex = rProducerIndex % nBufferSize;

	memcpy(pBuffer + (nIndex * nObjectSize), pObj, nObjectSize);
	AngelicaMT::AngelicaMemoryBarrier();
	rProducerIndex += 1;
	AngelicaMT::AngelicaMemoryBarrier();
}

///////////////////////////////////////////////////////////////////////////////
inline  void SingleProducerSingleConsumerQueueBase::Pop(void* pObj, volatile unsigned int& rProducerIndex, volatile unsigned int& rConsumerIndex, unsigned int nBufferSize, void* arrBuffer, unsigned int nObjectSize)
{
	AngelicaMT::AngelicaMemoryBarrier();
	// busy-loop if queue is empty
	CSimpleThreadBackOff backoff;
	while (rProducerIndex - rConsumerIndex == 0)
	{
		backoff.backoff();
	}

	char* pBuffer = alias_cast<char*>(arrBuffer);
	unsigned int nIndex = rConsumerIndex % nBufferSize;

	memcpy(pObj, pBuffer + (nIndex * nObjectSize), nObjectSize);
	AngelicaMT::AngelicaMemoryBarrier();
	rConsumerIndex += 1;
	AngelicaMT::AngelicaMemoryBarrier();
}

///////////////////////////////////////////////////////////////////////////////
inline  void N_ProducerSingleConsumerQueueBase::Push(void* pObj, volatile unsigned int& rProducerIndex, volatile unsigned int& rConsumerIndex, volatile unsigned int& rRunning, void* arrBuffer, unsigned int nBufferSize, unsigned int nObjectSize, volatile unsigned int* arrStates)
{
	AngelicaMT::AngelicaMemoryBarrier();
	unsigned int nProducerIndex;
	unsigned int nConsumerIndex;

	int iter = 0;
	CSimpleThreadBackOff backoff;
	do
	{
		nProducerIndex = rProducerIndex;
		nConsumerIndex = rConsumerIndex;

		if (nProducerIndex - nConsumerIndex == nBufferSize)
		{
			if (iter++ > CSimpleThreadBackOff::kHardYieldInterval)
			{
				unsigned int nSizeToAlloc = sizeof(SFallbackList) + nObjectSize - 1;
				SFallbackList* pFallbackEntry = (SFallbackList*)_aligned_malloc(nSizeToAlloc, 128);
				memcpy(pFallbackEntry->object, pObj, nObjectSize);
				AngelicaMT::AngelicaMemoryBarrier();
				AngelicaInterlockedPushEntrySList(fallbackList, pFallbackEntry->nextEntry);
				return;
			}
			backoff.backoff();
			continue;
		}

		if (AngelicaInterlockedCompareExchange(alias_cast<volatile LONG*>(&rProducerIndex), nProducerIndex + 1, nProducerIndex) == nProducerIndex)
			break;
	}
	while (true);

	AngelicaMT::AngelicaMemoryBarrier();
	char* pBuffer = alias_cast<char*>(arrBuffer);
	unsigned int nIndex = nProducerIndex % nBufferSize;

	memcpy(pBuffer + (nIndex * nObjectSize), pObj, nObjectSize);
	AngelicaMT::AngelicaMemoryBarrier();
	arrStates[nIndex] = 1;
	AngelicaMT::AngelicaMemoryBarrier();
}

///////////////////////////////////////////////////////////////////////////////
inline  bool N_ProducerSingleConsumerQueueBase::Pop(void* pObj, volatile unsigned int& rProducerIndex, volatile unsigned int& rConsumerIndex, volatile unsigned int& rRunning, void* arrBuffer, unsigned int nBufferSize, unsigned int nObjectSize, volatile unsigned int* arrStates)
{
	AngelicaMT::AngelicaMemoryBarrier();

	// busy-loop if queue is empty
	CSimpleThreadBackOff backoff;
	if (rRunning && rProducerIndex - rConsumerIndex == 0)
	{
		while (rRunning && rProducerIndex - rConsumerIndex == 0)
		{
			backoff.backoff();
		}
	}

	if (rRunning == 0 && rProducerIndex - rConsumerIndex == 0)
	{
		SFallbackList* pFallback = (SFallbackList*)AngelicaInterlockedPopEntrySList(fallbackList);
		IF (pFallback, 0)
		{
			memcpy(pObj, pFallback->object, nObjectSize);
			_aligned_free(pFallback);
			return true;
		}
		// if the queue was empty, make sure we really are empty
		return false;
	}

	backoff.reset();
	while (arrStates[rConsumerIndex % nBufferSize] == 0)
	{
		backoff.backoff();
	}

	char* pBuffer = alias_cast<char*>(arrBuffer);
	unsigned int nIndex = rConsumerIndex % nBufferSize;

	memcpy(pObj, pBuffer + (nIndex * nObjectSize), nObjectSize);
	AngelicaMT::AngelicaMemoryBarrier();
	arrStates[nIndex] = 0;
	AngelicaMT::AngelicaMemoryBarrier();
	rConsumerIndex += 1;
	AngelicaMT::AngelicaMemoryBarrier();

	return true;
}

} // namespace detail
} // namespace AngelicaMT

// Include all multithreading containers.
#include "MultiThread_Containers.h"
