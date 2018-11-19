// Copyright 2001-2017 Angelicatek GmbH / Angelicatek Group. All rights reserved. 

#pragma once
#include <type_traits>
//////////////////////////////////////////////////////////////////////////
// Interlocked API
//////////////////////////////////////////////////////////////////////////

// Returns the resulting incremented value
LONG AngelicaInterlockedIncrement(int volatile* pDst);

// Returns the resulting decremented value
LONG AngelicaInterlockedDecrement(int volatile* pDst);

// Returns the resulting added value
LONG AngelicaInterlockedAdd(volatile LONG* pVal, LONG add);

// Returns the resulting added value
size_t AngelicaInterlockedAdd(volatile size_t* pVal, size_t add);

//////////////////////////////////////////////////////////////////////////
// Returns initial value prior exchange
LONG AngelicaInterlockedExchange(volatile LONG* pDst, LONG exchange);

// Returns initial value prior exchange
long long AngelicaInterlockedExchange64(volatile long long* addr, long long exchange);

// Returns initial value prior exchange
LONG AngelicaInterlockedExchangeAdd(volatile LONG* pDst, LONG value);

// Returns initial value prior exchange
size_t AngelicaInterlockedExchangeAdd(volatile size_t* pDst, size_t value);

// Returns initial value prior exchange
LONG AngelicaInterlockedExchangeAnd(volatile LONG* pDst, LONG value);

// Returns initial value prior exchange
LONG AngelicaInterlockedExchangeOr(volatile LONG* pDst, LONG value);

// Returns initial value prior exchange
void* AngelicaInterlockedExchangePointer(void* volatile* pDst, void* pExchange);

//////////////////////////////////////////////////////////////////////////
// Returns initial value prior exchange
LONG AngelicaInterlockedCompareExchange(volatile LONG* pDst, LONG exchange, LONG comperand);

// Returns initial value prior exchange
long long AngelicaInterlockedCompareExchange64(volatile long long* pDst, long long exchange, long long comperand);

#if ANGELICA_PLATFORM_64BIT
// Returns initial value prior exchange
unsigned char AngelicaInterlockedCompareExchange128(volatile long long* pDst, long long exchangeHigh, long long exchangeLow, long long* comparandResult);
#endif

// Returns initial address prior exchange
void* AngelicaInterlockedCompareExchangePointer(void* volatile* pDst, void* pExchange, void* pComperand);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// AngelicaInterlocked*SList Function, these are specialized C-A-S
// functions for single-linked lists which prevent the A-B-A problem there
// there are implemented in the platform specific AngelicaThread_*.h files
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
	friend void AngelicaInitializeSListHead(SLockFreeSingleLinkedListHeader& list);

	//! Push one element atomically onto the front of a single-linked list.
	friend void AngelicaInterlockedPushEntrySList(SLockFreeSingleLinkedListHeader& list, SLockFreeSingleLinkedListEntry& element);

	//! Push a list of elements atomically onto the front of a single-linked list.
	//! \note The entries must already be linked (ie, last must be reachable by moving through pNext starting at first).
	friend void AngelicaInterlockedPushListSList(SLockFreeSingleLinkedListHeader& list, SLockFreeSingleLinkedListEntry& first, SLockFreeSingleLinkedListEntry& last, unsigned int count);

	//! Retrieves a pointer to the first item on a single-linked list.
	//! \note This does not remove the item from the list, and it's unsafe to inspect anything but the returned address.
	//friend void* AngelicaRtlFirstEntrySList(SLockFreeSingleLinkedListHeader& list);

	//! Pops one element atomically from the front of a single-linked list, and returns a pointer to the item.
	//! \note If the list was empty, nullptr is returned instead.
	friend void* AngelicaInterlockedPopEntrySList(SLockFreeSingleLinkedListHeader& list);

	//! Flushes the entire single-linked list, and returns a pointer to the first item that was on the list.
	//! \note If the list was empty, nullptr is returned instead.
	friend void* AngelicaInterlockedFlushSList(SLockFreeSingleLinkedListHeader& list);

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

void*      AngelicaCreateCriticalSection();
void       AngelicaCreateCriticalSectionInplace(void*);
void       AngelicaDeleteCriticalSection(void* cs);
void       AngelicaDeleteCriticalSectionInplace(void* cs);
void       AngelicaEnterCriticalSection(void* cs);
bool       AngelicaTryCriticalSection(void* cs);
void       AngelicaLeaveCriticalSection(void* cs);

inline void AngelicaSpinLock(volatile int* pLock, int checkVal, int setVal)
{
	static_assert(sizeof(int) == sizeof(LONG), "Unsecured cast. Int is not same size as LONG.");
	CSimpleThreadBackOff threadBackoff;
	while (AngelicaInterlockedCompareExchange((volatile LONG*)pLock, setVal, checkVal) != checkVal)
	{
		threadBackoff.backoff();
	}
}

inline void AngelicaReleaseSpinLock(volatile int* pLock, int setVal)
{
	*pLock = setVal;
}

inline void AngelicaReadLock(volatile int* rw)
{
	AngelicaInterlockedAdd(rw, 1);
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

inline void AngelicaReleaseReadLock(volatile int* rw)
{
	AngelicaInterlockedAdd(rw, -1);
}

inline void AngelicaWriteLock(volatile int* rw)
{
	AngelicaSpinLock(rw, 0, WRITE_LOCK_VAL);
}

inline void AngelicaReleaseWriteLock(volatile int* rw)
{
	AngelicaInterlockedAdd(rw, -WRITE_LOCK_VAL);
}

//////////////////////////////////////////////////////////////////////////
struct ReadLock
{
	ReadLock(volatile int& rw) : prw(&rw)
	{
		AngelicaReadLock(prw);
	}

	~ReadLock()
	{
		AngelicaReleaseReadLock(prw);
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
			AngelicaReadLock(prw);
		}
	}

	void SetActive(int bActive = 1)
	{
		iActive = bActive;
	}

	void Release()
	{
		AngelicaInterlockedAdd(prw, -iActive);
	}

	~ReadLockCond()
	{
		AngelicaInterlockedAdd(prw, -iActive);
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
		AngelicaWriteLock(&rw);
	}

	~WriteLock()
	{
		AngelicaReleaseWriteLock(prw);
	}

private:
	volatile int* const prw;
};

//////////////////////////////////////////////////////////////////////////
struct WriteAfterReadLock
{
	WriteAfterReadLock(volatile int& rw) : prw(&rw)
	{
		AngelicaSpinLock(&rw, 1, WRITE_LOCK_VAL + 1);
	}

	~WriteAfterReadLock()
	{
		AngelicaInterlockedAdd(prw, -WRITE_LOCK_VAL);
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
			AngelicaSpinLock(prw, 0, iActive);
		}
	}

	WriteLockCond() : iActive(0), prw(&iActive) {}

	~WriteLockCond()
	{
		AngelicaInterlockedAdd(prw, -iActive);
	}

	void SetActive(int bActive = 1)
	{
		iActive = -bActive & WRITE_LOCK_VAL;
	}

	void Release()
	{
		AngelicaInterlockedAdd(prw, -iActive);
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

inline void JobSpinLock(volatile int* pLock, int checkVal, int setVal) { AngelicaSpinLock(pLock, checkVal, setVal); }
#else
inline void SpinLock(volatile int* pLock, int checkVal, int setVal)
{
	AngelicaSpinLock(pLock, checkVal, setVal);
}
inline void AtomicAdd(volatile int* pVal, int iAdd)                    { AngelicaInterlockedAdd(pVal, iAdd); }
inline void AtomicAdd(volatile unsigned int* pVal, int iAdd)           { AngelicaInterlockedAdd((volatile int*)pVal, iAdd); }

inline void JobSpinLock(volatile int* pLock, int checkVal, int setVal) { SpinLock(pLock, checkVal, setVal); }
#endif

inline void JobAtomicAdd(volatile int* pVal, int iAdd)
{
	AngelicaInterlockedAdd(pVal, iAdd);
}
inline void JobAtomicAdd(volatile unsigned int* pVal, int iAdd) { AngelicaInterlockedAdd((volatile int*)pVal, iAdd); }
