// Copyright 2001-2017 Angelicatek GmbH / Angelicatek Group. All rights reserved. 

#pragma once

#include <process.h>
namespace AngelicaMT {
namespace detail {
enum eLOCK_TYPE
{
	eLockType_CRITICAL_SECTION,
	eLockType_SRW,
	eLockType_MUTEX
};
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//! From winnt.h.
// Since we are not allowed to include windows.h while being included from platform.h and there seems to be no good way to include the
// required windows headers directly; without including a lot of other header, define a 1:1 copy of the required primitives defined in winnt.h.
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
struct ANGELICA_CRITICAL_SECTION // From winnt.h
{
	void*          DebugInfo;
	long           LockCount;
	long           RecursionCount;
	void*          OwningThread;
	void*          LockSemaphore;
	unsigned long* SpinCount;  //!< Force size on 64-bit systems when packed.
};

//////////////////////////////////////////////////////////////////////////
//struct ANGELICA_SRWLOCK // From winnt.h
//{
//	ANGELICA_SRWLOCK();
//	void* SRWLock_;
//};

//////////////////////////////////////////////////////////////////////////
struct ANGELICA_CONDITION_VARIABLE // From winnt.h
{
	ANGELICA_CONDITION_VARIABLE();
	void* condVar_;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class AngelicaLock_SRWLOCK
{
public:
	static const eLOCK_TYPE s_value = eLockType_SRW;
	friend class AngelicaConditionVariable;
public:
	//AngelicaLock_SRWLOCK() = default;
	AngelicaLock_SRWLOCK()
	{
		InitializeCriticalSection(&m_cs);
	}
	~AngelicaLock_SRWLOCK()
	{
		DeleteCriticalSection(&m_cs);
	}
	void Lock();
	void Unlock();


private:
	AngelicaLock_SRWLOCK(const AngelicaLock_SRWLOCK&) = delete;
	AngelicaLock_SRWLOCK& operator=(const AngelicaLock_SRWLOCK&) = delete;

private:
	CRITICAL_SECTION m_cs;
	//ANGELICA_SRWLOCK m_win32_lock_type;
};

//////////////////////////////////////////////////////////////////////////
// SRW Lock (Slim Reader/Writer Lock)
// Faster + lighter than CriticalSection. Also only enters into kernel mode if contended.
// Cannot be shared between processes.
class AngelicaLock_SRWLOCK_Recursive
{
public:
	static const eLOCK_TYPE s_value = eLockType_SRW;
	friend class AngelicaConditionVariable;

public:
	AngelicaLock_SRWLOCK_Recursive() : m_recurseCounter(0), m_exclusiveOwningThreadId(0) {}

	void Lock();
	void Unlock();

	// Deprecated
#ifndef _RELEASE
	bool IsLocked()
	{
		return m_exclusiveOwningThreadId == GetCurrentThreadId();
	}
#endif

private:
	AngelicaLock_SRWLOCK_Recursive(const AngelicaLock_SRWLOCK_Recursive&) = delete;
	AngelicaLock_SRWLOCK_Recursive& operator=(const AngelicaLock_SRWLOCK_Recursive&) = delete;

private:
	AngelicaLock_SRWLOCK m_win32_lock_type;
	unsigned int          m_recurseCounter;
	
	// Due to its semantics, this member can be accessed in an unprotected manner,
	// but only for comparison with the current tid.
	unsigned long m_exclusiveOwningThreadId;
};

//////////////////////////////////////////////////////////////////////////
// Critical section
// Faster then WinMutex as it only enters into kernel mode if contended.
// Cannot be shared between processes.
class AngelicaLock_CriticalSection
{
public:
	static const eLOCK_TYPE s_value = eLockType_CRITICAL_SECTION;
	friend class AngelicaConditionVariable;

public:
	AngelicaLock_CriticalSection();
	~AngelicaLock_CriticalSection();

	void Lock();
	void Unlock();

	//! Deprecated: do not use this function - its return value might already be wrong the moment it is returned.
#ifndef _RELEASE
	bool IsLocked()
	{
		return m_win32_lock_type.RecursionCount > 0 && (UINT_PTR)m_win32_lock_type.OwningThread == GetCurrentThreadId();
	}
#endif

private:
	AngelicaLock_CriticalSection(const AngelicaLock_CriticalSection&) = delete;
	AngelicaLock_CriticalSection& operator=(const AngelicaLock_CriticalSection&) = delete;

private:
	ANGELICA_CRITICAL_SECTION m_win32_lock_type;
};

} // detail
} // AngelicaMT

  //////////////////////////////////////////////////////////////////////////
  /////////////////////////    DEFINE LOCKS    /////////////////////////////
  //////////////////////////////////////////////////////////////////////////

template<> class AngelicaLockT<ANGELICALOCK_RECURSIVE> : public AngelicaMT::detail::AngelicaLock_SRWLOCK_Recursive
{
};
template<> class AngelicaLockT<ANGELICALOCK_FAST> : public AngelicaMT::detail::AngelicaLock_SRWLOCK
{
};

typedef AngelicaMT::detail::AngelicaLock_SRWLOCK_Recursive AngelicaMutex;
typedef AngelicaMT::detail::AngelicaLock_SRWLOCK           AngelicaMutexFast; // Not recursive

//////////////////////////////////////////////////////////////////////////
//! AngelicaEvent represent a synchronization event.
class AngelicaEvent
{
public:
	AngelicaEvent();
	~AngelicaEvent();

	//! Reset the event to the unsignalled state.
	void Reset();

	//! Set the event to the signalled state.
	void Set();

	//! Access a HANDLE to wait on.
	void* GetHandle() const { return m_handle; };

	//! Wait indefinitely for the object to become signalled.
	void Wait() const;

	//! Wait, with a time limit, for the object to become signalled.
	bool Wait(const unsigned int timeoutMillis) const;

private:
	AngelicaEvent(const AngelicaEvent&);
	AngelicaEvent& operator=(const AngelicaEvent&);

private:
	void* m_handle;
};
typedef AngelicaEvent AngelicaEventTimed;

//////////////////////////////////////////////////////////////////////////
class AngelicaConditionVariable
{
public:
	AngelicaConditionVariable() = default;
	/*void Wait(AngelicaMutex& lock);
	void Wait(AngelicaMutexFast& lock);
	bool TimedWait(AngelicaMutex& lock, unsigned int millis);
	bool TimedWait(AngelicaMutexFast& lock, unsigned int millis);*/
	void NotifySingle();
	void Notify();

private:
	AngelicaConditionVariable(const AngelicaConditionVariable&);
	AngelicaConditionVariable& operator=(const AngelicaConditionVariable&);

private:
	AngelicaMT::detail::ANGELICA_CONDITION_VARIABLE m_condVar;
};

//////////////////////////////////////////////////////////////////////////
//! Platform independent wrapper for a counting semaphore.
class AngelicaSemaphore
{
public:
	AngelicaSemaphore(int nMaximumCount, int nInitialCount = 0);
	~AngelicaSemaphore();
	void Acquire();
	void Release();

private:
	void* m_Semaphore;
};

//////////////////////////////////////////////////////////////////////////
//! Platform independent wrapper for a counting semaphore
//! except that this version uses C-A-S only until a blocking call is needed.
//! -> No kernel call if there are object in the semaphore.
class AngelicaFastSemaphore
{
public:
	AngelicaFastSemaphore(int nMaximumCount, int nInitialCount = 0);
	~AngelicaFastSemaphore();
	void Acquire();
	void Release();

private:
	AngelicaSemaphore   m_Semaphore;
	volatile int m_nCounter;
};
