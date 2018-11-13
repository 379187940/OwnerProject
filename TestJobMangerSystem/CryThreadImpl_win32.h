// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

//#include <CryCore/Platform/platform.h>
#include "CryThread_win32.h"
//#include <CryCore/Platform/CryWindows.h>
#include "Win32specific.h"
namespace CryMT
{
namespace detail
{

static_assert(sizeof(CRY_CRITICAL_SECTION) == sizeof(CRITICAL_SECTION), "Win32 CRITICAL_SECTION size does not match CRY_CRITICAL_SECTION");
static_assert(sizeof(CRY_SRWLOCK) == sizeof(SRWLOCK), "Win32 SRWLOCK size does not match CRY_SRWLOCK");
static_assert(sizeof(CRY_CONDITION_VARIABLE) == sizeof(CONDITION_VARIABLE), "Win32 CONDITION_VARIABLE size does not match CRY_CONDITION_VARIABLE");

//////////////////////////////////////////////////////////////////////////
//CryLock_SRWLOCK
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CRY_SRWLOCK::CRY_SRWLOCK()
	: SRWLock_(0)
{
	static_assert(sizeof(SRWLock_) == sizeof(PSRWLOCK), "RWLock-pointer has invalid size");
	InitializeSRWLock(reinterpret_cast<PSRWLOCK>(&SRWLock_));
}

//////////////////////////////////////////////////////////////////////////
//CRY_CONDITION_VARIABLE
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CRY_CONDITION_VARIABLE::CRY_CONDITION_VARIABLE()
	: condVar_(0)
{
	static_assert(sizeof(condVar_) == sizeof(PCONDITION_VARIABLE), "ConditionVariable-pointer has invalid size");
	InitializeConditionVariable(reinterpret_cast<PCONDITION_VARIABLE>(&condVar_));
}

//////////////////////////////////////////////////////////////////////////
// CryLock_SRWLOCK
//////////////////////////////////////////////////////////////////////////






//////////////////////////////////////////////////////////////////////////
void CryLock_SRWLOCK::Lock()
{
	AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&m_win32_lock_type.SRWLock_));
	//EnterCriticalSection(&m_cs);
}

//////////////////////////////////////////////////////////////////////////
void CryLock_SRWLOCK::Unlock()
{
	ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&m_win32_lock_type.SRWLock_));
	//LeaveCriticalSection(&m_cs);
}


//////////////////////////////////////////////////////////////////////////
// CryLock_SRWLOCK_Recursive
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
void CryLock_SRWLOCK_Recursive::Lock()
{
	const unsigned long threadId = GetCurrentThreadId();

	if (threadId == m_exclusiveOwningThreadId)
	{
		++m_recurseCounter;
	}
	else
	{
		m_win32_lock_type.Lock();
		assert(m_recurseCounter == 0);
		assert(m_exclusiveOwningThreadId == THREADID_NULL);
		m_exclusiveOwningThreadId = threadId;
	}
}

//////////////////////////////////////////////////////////////////////////
void CryLock_SRWLOCK_Recursive::Unlock()
{
	const unsigned long threadId = GetCurrentThreadId();
	assert(m_exclusiveOwningThreadId == threadId);

	if (m_recurseCounter)
	{
		--m_recurseCounter;
	}
	else
	{
		m_exclusiveOwningThreadId = THREADID_NULL;
		m_win32_lock_type.Unlock();
	}
}


//////////////////////////////////////////////////////////////////////////
// CryLock_CritSection
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CryLock_CriticalSection::CryLock_CriticalSection()
{
	InitializeCriticalSection((CRITICAL_SECTION*)&m_win32_lock_type);
}

//////////////////////////////////////////////////////////////////////////
CryLock_CriticalSection::~CryLock_CriticalSection()
{
	DeleteCriticalSection((CRITICAL_SECTION*)&m_win32_lock_type);
}

//////////////////////////////////////////////////////////////////////////
void CryLock_CriticalSection::Lock()
{
	EnterCriticalSection((CRITICAL_SECTION*)&m_win32_lock_type);
}

//////////////////////////////////////////////////////////////////////////
void CryLock_CriticalSection::Unlock()
{
	LeaveCriticalSection((CRITICAL_SECTION*)&m_win32_lock_type);
}


}
}

//////////////////////////////////////////////////////////////////////////
CryEvent::CryEvent()
{
	m_handle = (void*)CreateEvent(NULL, FALSE, FALSE, NULL);
}

//////////////////////////////////////////////////////////////////////////
CryEvent::~CryEvent()
{
	CloseHandle(m_handle);
}

//////////////////////////////////////////////////////////////////////////
void CryEvent::Reset()
{
	ResetEvent(m_handle);
}

//////////////////////////////////////////////////////////////////////////
void CryEvent::Set()
{
	SetEvent(m_handle);
}

//////////////////////////////////////////////////////////////////////////
void CryEvent::Wait() const
{
	WaitForSingleObject(m_handle, INFINITE);
}

//////////////////////////////////////////////////////////////////////////
bool CryEvent::Wait(const unsigned int timeoutMillis) const
{
	if (WaitForSingleObject(m_handle, timeoutMillis) == WAIT_TIMEOUT)
		return false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
void CryConditionVariable::Wait(CryMutex& lock)
{
	TimedWait(lock, INFINITE);
}

void CryConditionVariable::Wait(CryMutexFast& lock)
{
	TimedWait(lock, INFINITE);
}

//////////////////////////////////////////////////////////////////////////
bool CryConditionVariable::TimedWait(CryMutex& lock, unsigned int millis)
{
	if (lock.s_value == CryMT::detail::eLockType_SRW)
	{
		assert(lock.m_recurseCounter == 0);
		lock.m_exclusiveOwningThreadId = THREADID_NULL;
		bool ret = SleepConditionVariableSRW(reinterpret_cast<PCONDITION_VARIABLE>(&m_condVar), reinterpret_cast<PSRWLOCK>(&lock.m_win32_lock_type), millis, ULONG(0)) == TRUE;
		lock.m_exclusiveOwningThreadId = GetCurrentThreadId();
		return ret;

	}
	else if (lock.s_value == CryMT::detail::eLockType_CRITICAL_SECTION)
	{
		return SleepConditionVariableCS(reinterpret_cast<PCONDITION_VARIABLE>(&m_condVar), reinterpret_cast<PCRITICAL_SECTION>(&lock.m_win32_lock_type), millis) == TRUE;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CryConditionVariable::TimedWait(CryMutexFast& lock, unsigned int millis)
{
	if (lock.s_value == CryMT::detail::eLockType_SRW)
	{
		return SleepConditionVariableSRW(reinterpret_cast<PCONDITION_VARIABLE>(&m_condVar), reinterpret_cast<PSRWLOCK>(&lock.m_win32_lock_type), millis, ULONG(0)) == TRUE;
	}
	else if (lock.s_value == CryMT::detail::eLockType_CRITICAL_SECTION)
	{
		return SleepConditionVariableCS(reinterpret_cast<PCONDITION_VARIABLE>(&m_condVar), reinterpret_cast<PCRITICAL_SECTION>(&lock.m_win32_lock_type), millis) == TRUE;
	}
}

//////////////////////////////////////////////////////////////////////////
void CryConditionVariable::NotifySingle()
{
	WakeConditionVariable(reinterpret_cast<PCONDITION_VARIABLE>(&m_condVar));
}

//////////////////////////////////////////////////////////////////////////
void CryConditionVariable::Notify()
{
	WakeAllConditionVariable(reinterpret_cast<PCONDITION_VARIABLE>(&m_condVar));
}

//////////////////////////////////////////////////////////////////////////
CrySemaphore::CrySemaphore(int nMaximumCount, int nInitialCount)
{
	m_Semaphore = (void*)CreateSemaphore(NULL, nInitialCount, nMaximumCount, NULL);
}

//////////////////////////////////////////////////////////////////////////
CrySemaphore::~CrySemaphore()
{
	CloseHandle((HANDLE)m_Semaphore);
}

//////////////////////////////////////////////////////////////////////////
void CrySemaphore::Acquire()
{
	WaitForSingleObject((HANDLE)m_Semaphore, INFINITE);
}

//////////////////////////////////////////////////////////////////////////
void CrySemaphore::Release()
{
	ReleaseSemaphore((HANDLE)m_Semaphore, 1, NULL);
}

//////////////////////////////////////////////////////////////////////////
CryFastSemaphore::CryFastSemaphore(int nMaximumCount, int nInitialCount) :
	m_Semaphore(nMaximumCount),
	m_nCounter(nInitialCount)
{
}

//////////////////////////////////////////////////////////////////////////
CryFastSemaphore::~CryFastSemaphore()
{
}

//////////////////////////////////////////////////////////////////////////
void CryFastSemaphore::Acquire()
{
	int nCount = ~0;
	do
	{
		nCount = *const_cast<volatile int*>(&m_nCounter);
	}
	while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&m_nCounter), nCount - 1, nCount) != nCount);

	// if the count would have been 0 or below, go to kernel semaphore
	if ((nCount - 1) < 0)
		m_Semaphore.Acquire();
}

//////////////////////////////////////////////////////////////////////////
void CryFastSemaphore::Release()
{
	int nCount = ~0;
	do
	{
		nCount = *const_cast<volatile int*>(&m_nCounter);
	}
	while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&m_nCounter), nCount + 1, nCount) != nCount);

	// wake up kernel semaphore if we have waiter
	if (nCount < 0)
		m_Semaphore.Release();
}

///////////////////////////////////////////////////////////////////////////////
namespace CryMT {

//////////////////////////////////////////////////////////////////////////
void CryMemoryBarrier()
{
	MemoryBarrier();
}

//////////////////////////////////////////////////////////////////////////
void CryYieldThread()
{
	SwitchToThread();
}
} // namespace CryMT
