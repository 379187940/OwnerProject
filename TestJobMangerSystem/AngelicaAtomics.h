// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once
#include <type_traits>
//////////////////////////////////////////////////////////////////////////
// Interlocked API
//////////////////////////////////////////////////////////////////////////

// Returns the resulting incremented value
LONG CryInterlockedIncrement(int volatile* pDst);

// Returns the resulting decremented value
LONG CryInterlockedDecrement(int volatile* pDst);

// Returns the resulting added value
LONG CryInterlockedAdd(volatile LONG* pVal, LONG add);

// Returns the resulting added value
size_t CryInterlockedAdd(volatile size_t* pVal, size_t add);

//////////////////////////////////////////////////////////////////////////
// Returns initial value prior exchange
LONG CryInterlockedExchange(volatile LONG* pDst, LONG exchange);

// Returns initial value prior exchange
long long CryInterlockedExchange64(volatile long long* addr, long long exchange);

// Returns initial value prior exchange
LONG CryInterlockedExchangeAdd(volatile LONG* pDst, LONG value);

// Returns initial value prior exchange
size_t CryInterlockedExchangeAdd(volatile size_t* pDst, size_t value);

// Returns initial value prior exchange
LONG CryInterlockedExchangeAnd(volatile LONG* pDst, LONG value);

// Returns initial value prior exchange
LONG CryInterlockedExchangeOr(volatile LONG* pDst, LONG value);

// Returns initial value prior exchange
void* CryInterlockedExchangePointer(void* volatile* pDst, void* pExchange);

//////////////////////////////////////////////////////////////////////////
// Returns initial value prior exchange
LONG CryInterlockedCompareExchange(volatile LONG* pDst, LONG exchange, LONG comperand);

// Returns initial value prior exchange
long long CryInterlockedCompareExchange64(volatile long long* pDst, long long exchange, long long comperand);

#if ANGELICA_PLATFORM_64BIT
// Returns initial value prior exchange
unsigned char CryInterlockedCompareExchange128(volatile long long* pDst, long long exchangeHigh, long long exchangeLow, long long* comparandResult);
#endif

// Returns initial address prior exchange
void* CryInterlockedCompareExchangePointer(void* volatile* pDst, void* pExchange, void* pComperand);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// CryInterlocked*SList Function, these are specialized C-A-S
// functions for single-linked lists which prevent the A-B-A problem there
// there are implemented in the platform specific CryThread_*.h files
//NOTE: The sizes are verified at compile-time in the implementation functions, but this is still ugly

#if ANGELICA_PLATFORM_64BIT
	#define LOCK_FREE_LINKED_LIST_DOUBLE_SIZE_PTR_ALIGN 16
#elif ANGELICA_PLATFORM_32BIT
	#define LOCK_FREE_LINKED_LIST_DOUBLE_SIZE_PTR_ALIGN 8
#else
	#error "Unsupported plaform"
#endif

struct SLockFreeSingleLinkedListEntry
{
	_declspec(align(LOCK_FREE_LINKED_LIST_DOUBLE_SIZE_PTR_ALIGN)) SLockFreeSingleLinkedListEntry * volatile pNext;
};
static_assert(std::alignment_of<SLockFreeSingleLinkedListEntry>::value == sizeof(uintptr_t) * 2, "Alignment failure for SLockFreeSingleLinkedListEntry");
template<typename DestinationType, typename SourceType> inline DestinationType alias_cast(SourceType pPtr)
{
	union
	{
		SourceType      pSrc;
		DestinationType pDst;
	} conv_union;
	conv_union.pSrc = pPtr;
	return conv_union.pDst;

}
struct SLockFreeSingleLinkedListHeader
{
	//! Initializes the single-linked list.
	friend void CryInitializeSListHead(SLockFreeSingleLinkedListHeader& list);

	//! Push one element atomically onto the front of a single-linked list.
	friend void CryInterlockedPushEntrySList(SLockFreeSingleLinkedListHeader& list, SLockFreeSingleLinkedListEntry& element);

	//! Push a list of elements atomically onto the front of a single-linked list.
	//! \note The entries must already be linked (ie, last must be reachable by moving through pNext starting at first).
	friend void CryInterlockedPushListSList(SLockFreeSingleLinkedListHeader& list, SLockFreeSingleLinkedListEntry& first, SLockFreeSingleLinkedListEntry& last, unsigned int count);

	//! Retrieves a pointer to the first item on a single-linked list.
	//! \note This does not remove the item from the list, and it's unsafe to inspect anything but the returned address.
	//friend void* CryRtlFirstEntrySList(SLockFreeSingleLinkedListHeader& list);

	//! Pops one element atomically from the front of a single-linked list, and returns a pointer to the item.
	//! \note If the list was empty, nullptr is returned instead.
	friend void* CryInterlockedPopEntrySList(SLockFreeSingleLinkedListHeader& list);

	//! Flushes the entire single-linked list, and returns a pointer to the first item that was on the list.
	//! \note If the list was empty, nullptr is returned instead.
	friend void* CryInterlockedFlushSList(SLockFreeSingleLinkedListHeader& list);

private:
	_declspec(align(LOCK_FREE_LINKED_LIST_DOUBLE_SIZE_PTR_ALIGN)) SLockFreeSingleLinkedListEntry * volatile pNext;

#if ANGELICA_PLATFORM_ORBIS
	// Only need "salt" on platforms using CAS (ORBIS uses embedded salt)
#elif ANGELICA_PLATFORM_POSIX
	// If pointers 32bit, salt should be as well. Otherwise we get 4 bytes of padding between pNext and salt and CAS operations fail
	#if ANGELICA_PLATFORM_64BIT
	volatile unsigned long long salt;
	#else
	volatile unsigned int salt;
	#endif
#endif
};
static_assert(std::alignment_of<SLockFreeSingleLinkedListHeader>::value == sizeof(uintptr_t) * 2, "Alignment failure for SLockFreeSingleLinkedListHeader");
#undef LOCK_FREE_LINKED_LIST_DOUBLE_SIZE_PTR_ALIGN

#if _WIN32
	#include "AngelicaAtomics_win32.h"
#endif

#define WRITE_LOCK_VAL (1 << 16)

void*      CryCreateCriticalSection();
void       CryCreateCriticalSectionInplace(void*);
void       CryDeleteCriticalSection(void* cs);
void       CryDeleteCriticalSectionInplace(void* cs);
void       CryEnterCriticalSection(void* cs);
bool       CryTryCriticalSection(void* cs);
void       CryLeaveCriticalSection(void* cs);

inline void CrySpinLock(volatile int* pLock, int checkVal, int setVal)
{
	static_assert(sizeof(int) == sizeof(LONG), "Unsecured cast. Int is not same size as LONG.");
	CSimpleThreadBackOff threadBackoff;
	while (CryInterlockedCompareExchange((volatile LONG*)pLock, setVal, checkVal) != checkVal)
	{
		threadBackoff.backoff();
	}
}

inline void CryReleaseSpinLock(volatile int* pLock, int setVal)
{
	*pLock = setVal;
}

inline void CryReadLock(volatile int* rw)
{
	CryInterlockedAdd(rw, 1);
#ifdef NEED_ENDIAN_SWAP
	volatile char* pw = (volatile char*)rw + 1;
#else
	volatile char* pw = (volatile char*)rw + 2;
#endif

	CSimpleThreadBackOff backoff;
	for (; *pw; )
	{
		backoff.backoff();
	}
}

inline void CryReleaseReadLock(volatile int* rw)
{
	CryInterlockedAdd(rw, -1);
}

inline void CryWriteLock(volatile int* rw)
{
	CrySpinLock(rw, 0, WRITE_LOCK_VAL);
}

inline void CryReleaseWriteLock(volatile int* rw)
{
	CryInterlockedAdd(rw, -WRITE_LOCK_VAL);
}

//////////////////////////////////////////////////////////////////////////
struct ReadLock
{
	ReadLock(volatile int& rw) : prw(&rw)
	{
		CryReadLock(prw);
	}

	~ReadLock()
	{
		CryReleaseReadLock(prw);
	}

private:
	volatile int* const prw;
};

struct ReadLockCond
{
	ReadLockCond(volatile int& rw, int bActive) : iActive(0),prw(&rw)
	{
		if (bActive)
		{
			iActive = 1;
			CryReadLock(prw);
		}
	}

	void SetActive(int bActive = 1)
	{
		iActive = bActive;
	}

	void Release()
	{
		CryInterlockedAdd(prw, -iActive);
	}

	~ReadLockCond()
	{
		CryInterlockedAdd(prw, -iActive);
	}

private:
	int                 iActive;
	volatile int* const prw;
};

//////////////////////////////////////////////////////////////////////////
struct WriteLock
{
	WriteLock(volatile int& rw) : prw(&rw)
	{
		CryWriteLock(&rw);
	}

	~WriteLock()
	{
		CryReleaseWriteLock(prw);
	}

private:
	volatile int* const prw;
};

//////////////////////////////////////////////////////////////////////////
struct WriteAfterReadLock
{
	WriteAfterReadLock(volatile int& rw) : prw(&rw)
	{
		CrySpinLock(&rw, 1, WRITE_LOCK_VAL + 1);
	}

	~WriteAfterReadLock()
	{
		CryInterlockedAdd(prw, -WRITE_LOCK_VAL);
	}

private:
	volatile int* const prw;
};

//////////////////////////////////////////////////////////////////////////
struct WriteLockCond
{
	WriteLockCond(volatile int& rw, int bActive = 1) : prw(&rw), iActive(0)
	{
		if (bActive)
		{
			iActive = WRITE_LOCK_VAL;
			CrySpinLock(prw, 0, iActive);
		}
	}

	WriteLockCond() : iActive(0), prw(&iActive) {}

	~WriteLockCond()
	{
		CryInterlockedAdd(prw, -iActive);
	}

	void SetActive(int bActive = 1)
	{
		iActive = -bActive & WRITE_LOCK_VAL;
	}

	void Release()
	{
		CryInterlockedAdd(prw, -iActive);
	}

	int           iActive; //!< Not private because used directly in Physics RWI.
	volatile int* prw;     //!< Not private because used directly in Physics RWI.
};

//////////////////////////////////////////////////////////////////////////
#if defined(EXCLUDE_PHYSICS_THREAD)
inline void SpinLock(volatile int* pLock, int checkVal, int setVal)
{
	*(int*)pLock = setVal;
}
inline void AtomicAdd(volatile int* pVal, int iAdd)                    { *(int*)pVal += iAdd; }
inline void AtomicAdd(volatile unsigned int* pVal, int iAdd)           { *(unsigned int*)pVal += iAdd; }

inline void JobSpinLock(volatile int* pLock, int checkVal, int setVal) { CrySpinLock(pLock, checkVal, setVal); }
#else
inline void SpinLock(volatile int* pLock, int checkVal, int setVal)
{
	CrySpinLock(pLock, checkVal, setVal);
}
inline void AtomicAdd(volatile int* pVal, int iAdd)                    { CryInterlockedAdd(pVal, iAdd); }
inline void AtomicAdd(volatile unsigned int* pVal, int iAdd)           { CryInterlockedAdd((volatile int*)pVal, iAdd); }

inline void JobSpinLock(volatile int* pLock, int checkVal, int setVal) { SpinLock(pLock, checkVal, setVal); }
#endif

inline void JobAtomicAdd(volatile int* pVal, int iAdd)
{
	CryInterlockedAdd(pVal, iAdd);
}
inline void JobAtomicAdd(volatile unsigned int* pVal, int iAdd) { CryInterlockedAdd((volatile int*)pVal, iAdd); }
