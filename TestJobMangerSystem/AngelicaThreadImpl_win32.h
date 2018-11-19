// Copyright 2001-2017 Angelicatek GmbH / Angelicatek Group. All rights reserved. 

#pragma once

#include "AngelicaThread_win32.h"
#include "Win32specific.h"
namespace AngelicaMT
{
namespace detail
{

static_assert(sizeof(ANGELICA_CRITICAL_SECTION) == sizeof(CRITICAL_SECTION), "Win32 CRITICAL_SECTION size does not match ANGELICA_CRITICAL_SECTION");
//static_assert(sizeof(ANGELICA_SRWLOCK) == sizeof(SRWLOCK), "Win32 SRWLOCK size does not match ANGELICA_SRWLOCK");
static_assert(sizeof(ANGELICA_CONDITION_VARIABLE) == sizeof(CONDITION_VARIABLE), "Win32 CONDITION_VARIABLE size does not match ANGELICA_CONDITION_VARIABLE");

//////////////////////////////////////////////////////////////////////////
//AngelicaLock_SRWLOCK
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//ANGELICA_SRWLOCK::ANGELICA_SRWLOCK()
//	: SRWLock_(0)
//{
//	static_assert(sizeof(SRWLock_) == sizeof(PSRWLOCK), "RWLock-pointer has invalid size");
//	InitializeSRWLock(reinterpret_cast<PSRWLOCK>(&SRWLock_));
//}

//////////////////////////////////////////////////////////////////////////
//ANGELICA_CONDITION_VARIABLE
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
ANGELICA_CONDITION_VARIABLE::ANGELICA_CONDITION_VARIABLE()
	: condVar_(0)
{
	static_assert(sizeof(condVar_) == sizeof(PCONDITION_VARIABLE), "ConditionVariable-pointer has invalid size");
	InitializeConditionVariable(reinterpret_cast<PCONDITION_VARIABLE>(&condVar_));
}

//////////////////////////////////////////////////////////////////////////
// AngelicaLock_SRWLOCK
//////////////////////////////////////////////////////////////////////////






//////////////////////////////////////////////////////////////////////////
void AngelicaLock_SRWLOCK::Lock()
{
	//AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&m_win32_lock_type.SRWLock_));
	EnterCriticalSection(&m_cs);
}

//////////////////////////////////////////////////////////////////////////
void AngelicaLock_SRWLOCK::Unlock()
{
	//ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&m_win32_lock_type.SRWLock_));
	LeaveCriticalSection(&m_cs);
}


//////////////////////////////////////////////////////////////////////////
// AngelicaLock_SRWLOCK_Recursive
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
void AngelicaLock_SRWLOCK_Recursive::Lock()
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
void AngelicaLock_SRWLOCK_Recursive::Unlock()
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
// AngelicaLock_CritSection
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
AngelicaLock_CriticalSection::AngelicaLock_CriticalSection()
{
	InitializeCriticalSection((CRITICAL_SECTION*)&m_win32_lock_type);
}

//////////////////////////////////////////////////////////////////////////
AngelicaLock_CriticalSection::~AngelicaLock_CriticalSection()
{
	DeleteCriticalSection((CRITICAL_SECTION*)&m_win32_lock_type);
}

//////////////////////////////////////////////////////////////////////////
void AngelicaLock_CriticalSection::Lock()
{
	EnterCriticalSection((CRITICAL_SECTION*)&m_win32_lock_type);
}

//////////////////////////////////////////////////////////////////////////
void AngelicaLock_CriticalSection::Unlock()
{
	LeaveCriticalSection((CRITICAL_SECTION*)&m_win32_lock_type);
}


}
}

//////////////////////////////////////////////////////////////////////////
AngelicaEvent::AngelicaEvent()
{
	m_handle = (void*)CreateEvent(NULL, FALSE, FALSE, NULL);
}

//////////////////////////////////////////////////////////////////////////
AngelicaEvent::~AngelicaEvent()
{
	CloseHandle(m_handle);
}

//////////////////////////////////////////////////////////////////////////
void AngelicaEvent::Reset()
{
	ResetEvent(m_handle);
}

//////////////////////////////////////////////////////////////////////////
void AngelicaEvent::Set()
{
	SetEvent(m_handle);
}

//////////////////////////////////////////////////////////////////////////
void AngelicaEvent::Wait() const
{
	WaitForSingleObject(m_handle, INFINITE);
}

//////////////////////////////////////////////////////////////////////////
bool AngelicaEvent::Wait(const unsigned int timeoutMillis) const
{
	if (WaitForSingleObject(m_handle, timeoutMillis) == WAIT_TIMEOUT)
		return false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//void AngelicaConditionVariable::Wait(AngelicaMutex& lock)
//{
//	TimedWait(lock, INFINITE);
//}
//
//void AngelicaConditionVariable::Wait(AngelicaMutexFast& lock)
//{
//	TimedWait(lock, INFINITE);
//}
//
////////////////////////////////////////////////////////////////////////////
//bool AngelicaConditionVariable::TimedWait(AngelicaMutex& lock, unsigned int millis)
//{
//	if (lock.s_value == AngelicaMT::detail::eLockType_SRW)
//	{
//		assert(lock.m_recurseCounter == 0);
//		lock.m_exclusiveOwningThreadId = THREADID_NULL;
//		bool ret = SleepConditionVariableSRW(reinterpret_cast<PCONDITION_VARIABLE>(&m_condVar), reinterpret_cast<PSRWLOCK>(&lock.m_win32_lock_type), millis, ULONG(0)) == TRUE;
//		lock.m_exclusiveOwningThreadId = GetCurrentThreadId();
//		return ret;
//
//	}
//	else if (lock.s_value == AngelicaMT::detail::eLockType_CRITICAL_SECTION)
//	{
//		return SleepConditionVariableCS(reinterpret_cast<PCONDITION_VARIABLE>(&m_condVar), reinterpret_cast<PCRITICAL_SECTION>(&lock.m_win32_lock_type), millis) == TRUE;
//	}
//}
//
////////////////////////////////////////////////////////////////////////////
//bool AngelicaConditionVariable::TimedWait(AngelicaMutexFast& lock, unsigned int millis)
//{
//	if (lock.s_value == AngelicaMT::detail::eLockType_SRW)
//	{
//		return SleepConditionVariableSRW(reinterpret_cast<PCONDITION_VARIABLE>(&m_condVar), reinterpret_cast<PSRWLOCK>(&lock.m_win32_lock_type), millis, ULONG(0)) == TRUE;
//	}
//	else if (lock.s_value == AngelicaMT::detail::eLockType_CRITICAL_SECTION)
//	{
//		return SleepConditionVariableCS(reinterpret_cast<PCONDITION_VARIABLE>(&m_condVar), reinterpret_cast<PCRITICAL_SECTION>(&lock.m_win32_lock_type), millis) == TRUE;
//	}
//}

//////////////////////////////////////////////////////////////////////////
void AngelicaConditionVariable::NotifySingle()
{
	WakeConditionVariable(reinterpret_cast<PCONDITION_VARIABLE>(&m_condVar));
}

//////////////////////////////////////////////////////////////////////////
void AngelicaConditionVariable::Notify()
{
	WakeAllConditionVariable(reinterpret_cast<PCONDITION_VARIABLE>(&m_condVar));
}

//////////////////////////////////////////////////////////////////////////
AngelicaSemaphore::AngelicaSemaphore(int nMaximumCount, int nInitialCount)
{
	m_Semaphore = (void*)CreateSemaphore(NULL, nInitialCount, nMaximumCount, NULL);
}

//////////////////////////////////////////////////////////////////////////
AngelicaSemaphore::~AngelicaSemaphore()
{
	CloseHandle((HANDLE)m_Semaphore);
}

//////////////////////////////////////////////////////////////////////////
void AngelicaSemaphore::Acquire()
{
	WaitForSingleObject((HANDLE)m_Semaphore, INFINITE);
}

//////////////////////////////////////////////////////////////////////////
void AngelicaSemaphore::Release()
{
	ReleaseSemaphore((HANDLE)m_Semaphore, 1, NULL);
}

//////////////////////////////////////////////////////////////////////////
AngelicaFastSemaphore::AngelicaFastSemaphore(int nMaximumCount, int nInitialCount) :
	m_Semaphore(nMaximumCount),
	m_nCounter(nInitialCount)
{
}

//////////////////////////////////////////////////////////////////////////
AngelicaFastSemaphore::~AngelicaFastSemaphore()
{
}

//////////////////////////////////////////////////////////////////////////
void AngelicaFastSemaphore::Acquire()
{
	int nCount = ~0;
	do
	{
		nCount = *const_cast<volatile int*>(&m_nCounter);
	}
	while (AngelicaInterlockedCompareExchange(alias_cast<volatile LONG*>(&m_nCounter), nCount - 1, nCount) != nCount);

	// if the count would have been 0 or below, go to kernel semaphore
	if ((nCount - 1) < 0)
		m_Semaphore.Acquire();
}

//////////////////////////////////////////////////////////////////////////
void AngelicaFastSemaphore::Release()
{
	int nCount = ~0;
	do
	{
		nCount = *const_cast<volatile int*>(&m_nCounter);
	}
	while (AngelicaInterlockedCompareExchange(alias_cast<volatile LONG*>(&m_nCounter), nCount + 1, nCount) != nCount);

	// wake up kernel semaphore if we have waiter
	if (nCount < 0)
		m_Semaphore.Release();
}

///////////////////////////////////////////////////////////////////////////////
namespace AngelicaMT {

//////////////////////////////////////////////////////////////////////////
void AngelicaMemoryBarrier()
{
	MemoryBarrier();
}

//////////////////////////////////////////////////////////////////////////
void AngelicaYieldThread()
{
	SwitchToThread();
}
} // namespace AngelicaMT
