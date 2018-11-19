// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
//#include "System.h"
#include "ThreadConfigManager.h"
#include "AngelicaAtomics.h"
#include "Win32specific.h"
#include <atlstr.h>
#include "IThreadManager.h"
#include "IThreadConfigManager.h"
#include "MSVCspecific.h"
#include "AngelicaThread.h"
#include "AngelicaThread_win32.h"
#include <map>
#include "smartptr.h"
#include "AngelicaThreadImpl.h"

#define INCLUDED_FROM_SYSTEM_THREADING_CPP

#include "AngelicaThreadUtil_win32.h"
#include "FairMonitor.h"
#undef INCLUDED_FROM_SYSTEM_THREADING_CPP

//////////////////////////////////////////////////////////////////////////
static void ApplyThreadConfig(unsigned long threadId, CryThreadUtil::TThreadHandle pThreadHandle, const SThreadConfig& rThreadDesc)
{
	// Apply config
	if (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_ThreadName)
	{
		CryThreadUtil::CrySetThreadName(threadId, rThreadDesc.szThreadName);
	}
	if (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_Affinity)
	{
		CryThreadUtil::CrySetThreadAffinityMask(pThreadHandle, rThreadDesc.affinityFlag);
	}
	if (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_Priority)
	{
		CryThreadUtil::CrySetThreadPriority(pThreadHandle, rThreadDesc.priority);
	}
	if (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_PriorityBoost)
	{
		CryThreadUtil::CrySetThreadPriorityBoost(pThreadHandle, !rThreadDesc.bDisablePriorityBoost);
	}

	/*CryComment("<ThreadInfo> Configured thread \"%s\" %s | AffinityMask: %u %s | Priority: %i %s | PriorityBoost: %s %s",
	           rThreadDesc.szThreadName, (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_ThreadName) ? "" : "(ignored)",
	           rThreadDesc.affinityFlag, (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_Affinity) ? "" : "(ignored)",
	           rThreadDesc.priority, (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_Priority) ? "" : "(ignored)",
	           !rThreadDesc.bDisablePriorityBoost ? "enabled" : "disabled", (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_PriorityBoost) ? "" : "(ignored)");*/
}
class CThreadManager;
//////////////////////////////////////////////////////////////////////////
struct SThreadMetaData : public CMultiThreadRefCount
{
	SThreadMetaData()
		: m_pThreadTask(0)
		, m_pThreadMngr(nullptr)
		, m_threadHandle(0)
		, m_threadId(0)
		, m_threadName("Cry_UnnamedThread")
		, m_isRunning(false)
	{
	}

	IThread*                                m_pThreadTask; // Pointer to thread task to be executed
	CThreadManager*                         m_pThreadMngr; // Pointer to thread manager

	CryThreadUtil::TThreadHandle            m_threadHandle; // Thread handle
	unsigned long                                m_threadId;     // The active threadId, 0 = Invalid Id
	FairMonitor								m_threadExitMonitor;
	//CryMutex                                m_threadExitMutex;     // Mutex used to safeguard thread exit condition signaling
	//CryConditionVariable                    m_threadExitCondition; // Signaled when the thread is about to exit

	CStringA m_threadName; // Thread name
	volatile bool                           m_isRunning;  // Indicates the thread is not ready to exit yet
};

//////////////////////////////////////////////////////////////////////////
class CThreadManager : public IThreadManager
{
public:
	// <interfuscator:shuffle>
	virtual ~CThreadManager()
	{
	}

	virtual bool          SpawnThread(IThread* pThread, const char* sThreadName, ...) override;
	virtual bool          JoinThread(IThread* pThreadTask, EJoinMode eJoinMode) override;

	//virtual bool          RegisterThirdPartyThread(void* pThreadHandle, const char* sThreadName, ...) override;
	virtual bool          UnRegisterThirdPartyThread(const char* sThreadName, ...) override;

	virtual const char*   GetThreadName(unsigned long nThreadId) override;
	virtual unsigned long      GetThreadId(const char* sThreadName, ...) override;

	virtual void          ForEachOtherThread(IThreadManager::ThreadModifFunction fpThreadModiFunction, void* pFuncData = 0) override;

	virtual void          EnableFloatExceptions(EFPE_Severity eFPESeverity, unsigned long nThreadId = 0) override;
	virtual void          EnableFloatExceptionsForEachOtherThread(EFPE_Severity eFPESeverity) override;

	virtual unsigned int          GetFloatingPointExceptionMask() override;
	virtual void          SetFloatingPointExceptionMask(unsigned int nMask) override;

	IThreadConfigManager* GetThreadConfigManager() override
	{
		return &m_threadConfigManager;
	}
	// </interfuscator:shuffle>
private:
	static unsigned __stdcall RunThread(void* thisPtr);


private:
	bool     UnregisterThread(IThread* pThreadTask);

	bool     SpawnThreadImpl(IThread* pThread, const char* sThreadName);

	//bool     RegisterThirdPartyThreadImpl(CryThreadUtil::TThreadHandle pThreadHandle, const char* sThreadName);
	bool     UnRegisterThirdPartyThreadImpl(const char* sThreadName);

	unsigned long GetThreadIdImpl(const char* sThreadName);

private:
	// Note: Guard SThreadMetaData with a _smart_ptr and lock to ensure that a thread waiting to be signaled by another still
	// has access to valid SThreadMetaData even though the other thread terminated and as a result unregistered itself from the CThreadManager.
	// An example would be the join method. Where one thread waits on a signal from an other thread to terminate and release its SThreadMetaData,
	// sharing the same SThreadMetaData condition variable.
	typedef std::map<IThread*, _smart_ptr<SThreadMetaData>>                                                SpawnedThreadMap;
	typedef std::map<IThread*, _smart_ptr<SThreadMetaData>>::iterator                                      SpawnedThreadMapIter;
	typedef std::map<IThread*, _smart_ptr<SThreadMetaData>>::const_iterator                                SpawnedThreadMapConstIter;
	typedef std::pair<IThread*, _smart_ptr<SThreadMetaData>>                                               ThreadMapPair;

	typedef std::map<CString, _smart_ptr<SThreadMetaData>>                 SpawnedThirdPartyThreadMap;
	typedef std::map<CString, _smart_ptr<SThreadMetaData>>::iterator       SpawnedThirdPartyThreadMapIter;
	typedef std::map<CString, _smart_ptr<SThreadMetaData>>::const_iterator SpawnedThirdPartyThreadMapConstIter;
	typedef std::pair<CString, _smart_ptr<SThreadMetaData>>                ThirdPartyThreadMapPair;

	CryCriticalSection         m_spawnedThreadsLock; // Use lock for the rare occasion a thread is created/destroyed
	SpawnedThreadMap           m_spawnedThreads;     // Holds information of all spawned threads (through this system)

	CryCriticalSection         m_spawnedThirdPartyThreadsLock; // Use lock for the rare occasion a thread is created/destroyed
	SpawnedThirdPartyThreadMap m_spawnedThirdPartyThread;      // Holds information of all registered 3rd party threads (through this system)

	CThreadConfigManager       m_threadConfigManager;
};
CThreadManager g_ThreadManager;
IThreadManager* GetGlobalThreadManager()
{
	return &g_ThreadManager;
}
//////////////////////////////////////////////////////////////////////////
unsigned __stdcall CThreadManager::RunThread(void* thisPtr)
{
	// Check that we are not spawning a thread before gEnv->pSystem has been set
	// Otherwise we cannot enable floating point exceptions
	//if (!gEnv || !gEnv->pSystem)
	//{
	//	//CryFatalError("[Error]: CThreadManager::RunThread requires gEnv->pSystem to be initialized.");
	//}

	//PLATFORM_PROFILER_MARKER("Thread_Run");

	SThreadMetaData* pThreadData = reinterpret_cast<SThreadMetaData*>(thisPtr);
	pThreadData->m_threadId = CryThreadUtil::CryGetCurrentThreadId();

	// Apply config
	const SThreadConfig* pThreadConfig = g_ThreadManager.GetThreadConfigManager()->GetDefaultThreadConfig();
	ApplyThreadConfig(pThreadData->m_threadId , pThreadData->m_threadHandle, *pThreadConfig);

	//ANGELICA_PROFILE_THREADNAME(pThreadData->m_threadName.GetBuffer(0));

	// Config not found, append thread name with no config tag
	
	CStringA  tmpString(pThreadData->m_threadName);
	const char* cNoConfigAppendix = "(NoCfgFound)";
	int nNumCharsToReplace = (int)strlen(cNoConfigAppendix);
	tmpString+=(cNoConfigAppendix);


	// Rename Thread
	CryThreadUtil::CrySetThreadName(pThreadData->m_threadId, tmpString.GetBuffer(0));
	//ANGELICA_PROFILE_THREADNAME(tmpString.GetBuffer(0));
	

	// Enable FPEs
	g_ThreadManager.EnableFloatExceptions((EFPE_Severity)0);

	// Execute thread code
	pThreadData->m_pThreadTask->ThreadEntry();

	// Disable FPEs
	g_ThreadManager.EnableFloatExceptions(eFPE_None);

	// Signal imminent thread end
	pThreadData->m_threadExitMonitor.BeginSynchronized();
	pThreadData->m_isRunning = false;
	pThreadData->m_threadExitMonitor.NotifyAll();
	pThreadData->m_threadExitMonitor.EndSynchronized();

	// Unregister thread
	// Note: Unregister after m_threadExitCondition.Notify() to ensure pThreadData is still valid
	pThreadData->m_pThreadMngr->UnregisterThread(pThreadData->m_pThreadTask);

//	PLATFORM_PROFILER_MARKER("Thread_Stop");
	CryThreadUtil::CryThreadExitCall();

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
bool CThreadManager::JoinThread(IThread* pThreadTask, EJoinMode eJoinMode)
{
	// Get thread object
	_smart_ptr<SThreadMetaData> pThreadImpl = 0;
	{
		AUTO_LOCK(m_spawnedThreadsLock);

		SpawnedThreadMapIter res = m_spawnedThreads.find(pThreadTask);
		if (res == m_spawnedThreads.end())
		{
			// Thread has already finished and unregistered itself.
			// As it is complete we cannot wait for it.
			// Hence return true.
			return true;
		}

		pThreadImpl = res->second; // Keep object alive
	}

	// On try join, exit if the thread is not in a state to exit
	if (eJoinMode == eJM_TryJoin && pThreadImpl->m_isRunning)
	{
		return false;
	}

	// Wait for completion of the target thread exit condition
	pThreadImpl->m_threadExitMonitor.BeginSynchronized();
	while (pThreadImpl->m_isRunning)
	{
		pThreadImpl->m_threadExitMonitor.Wait(INFINITE);
	}
	pThreadImpl->m_threadExitMonitor.EndSynchronized();

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CThreadManager::UnregisterThread(IThread* pThreadTask)
{
	AUTO_LOCK(m_spawnedThreadsLock);

	SpawnedThreadMapIter res = m_spawnedThreads.find(pThreadTask);
	if (res == m_spawnedThreads.end())
	{
		// Duplicate thread deletion
		//CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: UnregisterThread: Unable to unregister thread. Thread name could not be found. Double deletion? IThread pointer: %p", pThreadTask);
		return false;
	}

	m_spawnedThreads.erase(res);
	return true;
}

//////////////////////////////////////////////////////////////////////////
const char* CThreadManager::GetThreadName(unsigned long nThreadId)
{
	// Loop over internally spawned threads
	{
		AUTO_LOCK(m_spawnedThreadsLock);

		SpawnedThreadMapConstIter iter = m_spawnedThreads.begin();
		SpawnedThreadMapConstIter iterEnd = m_spawnedThreads.end();

		for (; iter != iterEnd; ++iter)
		{
			if (iter->second->m_threadId == nThreadId)
			{
				return iter->second->m_threadName.GetBuffer(0);
			}
		}
	}

	// Loop over third party threads
	{
		AUTO_LOCK(m_spawnedThirdPartyThreadsLock);

		SpawnedThirdPartyThreadMapConstIter iter = m_spawnedThirdPartyThread.begin();
		SpawnedThirdPartyThreadMapConstIter iterEnd = m_spawnedThirdPartyThread.end();

		for (; iter != iterEnd; ++iter)
		{
			if (iter->second->m_threadId == nThreadId)
			{
				return iter->second->m_threadName.GetBuffer(0);
			}
		}
	}

	return "";
}

//////////////////////////////////////////////////////////////////////////
void CThreadManager::ForEachOtherThread(IThreadManager::ThreadModifFunction fpThreadModiFunction, void* pFuncData)
{
	unsigned long nCurThreadId = CryThreadUtil::CryGetCurrentThreadId();

	// Loop over internally spawned threads
	{
		AUTO_LOCK(m_spawnedThreadsLock);

		SpawnedThreadMapConstIter iter = m_spawnedThreads.begin();
		SpawnedThreadMapConstIter iterEnd = m_spawnedThreads.end();

		for (; iter != iterEnd; ++iter)
		{
			if (iter->second->m_threadId != nCurThreadId)
			{
				fpThreadModiFunction(iter->second->m_threadId, pFuncData);
			}
		}
	}

	// Loop over third party threads
	{
		AUTO_LOCK(m_spawnedThirdPartyThreadsLock);

		SpawnedThirdPartyThreadMapConstIter iter = m_spawnedThirdPartyThread.begin();
		SpawnedThirdPartyThreadMapConstIter iterEnd = m_spawnedThirdPartyThread.end();

		for (; iter != iterEnd; ++iter)
		{
			if (iter->second->m_threadId != nCurThreadId)
			{
				fpThreadModiFunction(iter->second->m_threadId, pFuncData);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CThreadManager::SpawnThread(IThread* pThreadTask, const char* sThreadName, ...)
{
	va_list args;
	va_start(args, sThreadName);

	// Format thread name
	char strThreadName[THREAD_NAME_LENGTH_MAX];
	if (!sprintf_s(strThreadName, sThreadName, args))
	{
		//CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: ThreadName \"%s\" has been truncated to \"%s\". Max characters allowed: %i.", sThreadName, strThreadName, (int)sizeof(strThreadName) - 1);
	}

	// Spawn thread
	bool ret = SpawnThreadImpl(pThreadTask, strThreadName);

	if (!ret)
	{
		//CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: CSystem::SpawnThread error spawning thread: \"%s\"", strThreadName);
	}

	va_end(args);
	return ret;
}

//////////////////////////////////////////////////////////////////////////
bool CThreadManager::SpawnThreadImpl(IThread* pThreadTask, const char* sThreadName)
{
	if (pThreadTask == NULL)
	{
		//CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "<ThreadInfo>: SpawnThread '%s' ThreadTask is NULL : ignoring", sThreadName);
		return false;
	}

	// Init thread meta data
	SThreadMetaData* pThreadMetaData = new SThreadMetaData();
	pThreadMetaData->m_pThreadTask = pThreadTask;
	pThreadMetaData->m_pThreadMngr = this;
	pThreadMetaData->m_threadName = sThreadName;

	// Add thread to map
	{
		AUTO_LOCK(m_spawnedThreadsLock);
		SpawnedThreadMapIter res = m_spawnedThreads.find(pThreadTask);
		if (res != m_spawnedThreads.end())
		{
			// Thread with same name already spawned
			//CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: SpawnThread: Thread \"%s\" already exists.", sThreadName);
			delete pThreadMetaData;
			return false;
		}

		// Insert thread data
		m_spawnedThreads.insert(ThreadMapPair(pThreadTask, pThreadMetaData));
	}

	// Load config if we can and if no config has been defined to be loaded
	const SThreadConfig* pThreadConfig = g_ThreadManager.GetThreadConfigManager()->GetThreadConfig(sThreadName);

	// Create thread description
	CryThreadUtil::SThreadCreationDesc desc = { sThreadName, RunThread, pThreadMetaData, pThreadConfig->paramActivityFlag & SThreadConfig::eThreadParamFlag_StackSize ? pThreadConfig->stackSizeBytes : 0 };

	// Spawn new thread
	pThreadMetaData->m_isRunning = CryThreadUtil::CryCreateThread(&(pThreadMetaData->m_threadHandle), desc);

	// Validate thread creation
	if (!pThreadMetaData->m_isRunning)
	{
		//CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: SpawnThread: Could not spawn thread \"%s\".", sThreadName);

		// Remove thread from map (also releases SThreadMetaData _smart_ptr)
		m_spawnedThreads.erase(m_spawnedThreads.find(pThreadTask));
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//bool CThreadManager::RegisterThirdPartyThread(void* pThreadHandle, const char* sThreadName, ...)
//{
//	if (!pThreadHandle)
//	{
//		pThreadHandle = reinterpret_cast<void*>(CryThreadUtil::CryGetCurrentThreadHandle());
//	}
//
//	va_list args;
//	va_start(args, sThreadName);
//
//	// Format thread name
//	char strThreadName[THREAD_NAME_LENGTH_MAX];
//	if (!sprintf_s(strThreadName, sThreadName, args))
//	{
//		//CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: ThreadName \"%s\" has been truncated to \"%s\". Max characters allowed: %i.", sThreadName, strThreadName, (int)sizeof(strThreadName) - 1);
//	}
//
//	// Register 3rd party thread
//	bool ret = RegisterThirdPartyThreadImpl(reinterpret_cast<CryThreadUtil::TThreadHandle>(pThreadHandle), strThreadName);
//
//	va_end(args);
//	return ret;
//}

//////////////////////////////////////////////////////////////////////////
//bool CThreadManager::RegisterThirdPartyThreadImpl(CryThreadUtil::TThreadHandle threadHandle, const char* sThreadName)
//{
//	if (strcmp(sThreadName, "") == 0)
//	{
//		//CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: CThreadManager::RegisterThirdPartyThread error registering third party thread. No name provided.");
//		return false;
//	}
//	// Init thread meta data
//	SThreadMetaData* pThreadMetaData = new SThreadMetaData();
//	pThreadMetaData->m_pThreadTask = 0;
//	pThreadMetaData->m_pThreadMngr = this;
//	pThreadMetaData->m_threadName = sThreadName;
//	pThreadMetaData->m_threadHandle = CryThreadUtil::CryDuplicateThreadHandle(threadHandle); // Ensure that we are not storing a pseudo handle
//	pThreadMetaData->m_threadId = CryThreadUtil::CryGetThreadId(pThreadMetaData->m_threadHandle);
//
//	{
//		AUTO_LOCK(m_spawnedThirdPartyThreadsLock);
//
//		// Check for duplicate
//		SpawnedThirdPartyThreadMapConstIter res = m_spawnedThirdPartyThread.find(sThreadName);
//		if (res != m_spawnedThirdPartyThread.end())
//		{
//			/*CryFatalError("CThreadManager::RegisterThirdPartyThread - Unable to register thread \"%s\""
//			              "because another third party thread with the same name \"%s\" has already been registered with ThreadHandle: %p",
//			              sThreadName, res->second->m_threadName.GetBuffer(0), reinterpret_cast<void*>(threadHandle));*/
//
//			delete pThreadMetaData;
//			return false;
//		}
//
//		// Insert thread data
//		m_spawnedThirdPartyThread.insert(ThirdPartyThreadMapPair(pThreadMetaData->m_threadName.GetBuffer(0), pThreadMetaData));
//	}
//
//	// Get thread config
//	const SThreadConfig* pThreadConfig = g_ThreadManager.GetThreadConfigManager()->GetThreadConfig(sThreadName);
//
//	// Apply config (if not default config)
//	if (strcmp(pThreadConfig->szThreadName, sThreadName) == 0)
//	{
//		ApplyThreadConfig(threadHandle, *pThreadConfig);
//	}
//
//	// Update FP exception mask for 3rd party thread
//	if (pThreadMetaData->m_threadId)
//	{
//		CryThreadUtil::EnableFloatExceptions(pThreadMetaData->m_threadId, (EFPE_Severity)0);
//	}
//
//	return true;
//}

//////////////////////////////////////////////////////////////////////////
bool CThreadManager::UnRegisterThirdPartyThread(const char* sThreadName, ...)
{
	va_list args;
	va_start(args, sThreadName);

	// Format thread name
	char strThreadName[THREAD_NAME_LENGTH_MAX];
	if (!sprintf_s(strThreadName, sThreadName, args))
	{
		//CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: ThreadName \"%s\" has been truncated to \"%s\". Max characters allowed: %i.", sThreadName, strThreadName, (int)sizeof(strThreadName) - 1);
	}

	// Unregister 3rd party thread
	bool ret = UnRegisterThirdPartyThreadImpl(strThreadName);

	va_end(args);
	return ret;
}

//////////////////////////////////////////////////////////////////////////
bool CThreadManager::UnRegisterThirdPartyThreadImpl(const char* sThreadName)
{
	AUTO_LOCK(m_spawnedThirdPartyThreadsLock);

	SpawnedThirdPartyThreadMapIter res = m_spawnedThirdPartyThread.find(sThreadName);
	if (res == m_spawnedThirdPartyThread.end())
	{
		// Duplicate thread deletion
		//CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: UnRegisterThirdPartyThread: Unable to unregister thread. Thread name \"%s\" could not be found. Double deletion?", sThreadName);
		return false;
	}

	// Close thread handle
	CryThreadUtil::CryCloseThreadHandle(res->second->m_threadHandle);

	// Delete reference from container
	m_spawnedThirdPartyThread.erase(res);
	return true;
}

//////////////////////////////////////////////////////////////////////////
unsigned long CThreadManager::GetThreadId(const char* sThreadName, ...)
{
	va_list args;
	va_start(args, sThreadName);

	// Format thread name
	char strThreadName[THREAD_NAME_LENGTH_MAX];
	if (!sprintf_s(strThreadName, sThreadName, args))
	{
		//CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: ThreadName \"%s\" has been truncated to \"%s\". Max characters allowed: %i. ", sThreadName, strThreadName, (int)sizeof(strThreadName) - 1);
	}

	// Get thread name
	unsigned long ret = GetThreadIdImpl(strThreadName);

	va_end(args);
	return ret;
}

//////////////////////////////////////////////////////////////////////////
unsigned long CThreadManager::GetThreadIdImpl(const char* sThreadName)
{
	// Loop over internally spawned threads
	{
		AUTO_LOCK(m_spawnedThreadsLock);

		SpawnedThreadMapConstIter iter = m_spawnedThreads.begin();
		SpawnedThreadMapConstIter iterEnd = m_spawnedThreads.end();

		for (; iter != iterEnd; ++iter)
		{
			if (iter->second->m_threadName == sThreadName )
			{
				return iter->second->m_threadId;
			}
		}
	}

	// Loop over third party threads
	{
		AUTO_LOCK(m_spawnedThirdPartyThreadsLock);

		SpawnedThirdPartyThreadMapConstIter iter = m_spawnedThirdPartyThread.begin();
		SpawnedThirdPartyThreadMapConstIter iterEnd = m_spawnedThirdPartyThread.end();

		for (; iter != iterEnd; ++iter)
		{
			if (iter->second->m_threadName == sThreadName)
			{
				return iter->second->m_threadId;
			}
		}
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
static void EnableFPExceptionsForThread(unsigned long nThreadId, void* pData)
{
	EFPE_Severity eFPESeverity = *(EFPE_Severity*)pData;
	CryThreadUtil::EnableFloatExceptions(nThreadId, eFPESeverity);
}

//////////////////////////////////////////////////////////////////////////
void CThreadManager::EnableFloatExceptions(EFPE_Severity eFPESeverity, unsigned long nThreadId /*=0*/)
{
	CryThreadUtil::EnableFloatExceptions(nThreadId, eFPESeverity);
}

//////////////////////////////////////////////////////////////////////////
void CThreadManager::EnableFloatExceptionsForEachOtherThread(EFPE_Severity eFPESeverity)
{
	ForEachOtherThread(EnableFPExceptionsForThread, &eFPESeverity);
}

//////////////////////////////////////////////////////////////////////////
unsigned int CThreadManager::GetFloatingPointExceptionMask()
{
	return CryThreadUtil::GetFloatingPointExceptionMask();
}

//////////////////////////////////////////////////////////////////////////
void CThreadManager::SetFloatingPointExceptionMask(unsigned int nMask)
{
	CryThreadUtil::SetFloatingPointExceptionMask(nMask);
}

//////////////////////////////////////////////////////////////////////////
//void CSystem::InitThreadSystem()
//{
//	m_pThreadManager = new CThreadManager();
//	m_env.pThreadManager = m_pThreadManager;
//}
//
////////////////////////////////////////////////////////////////////////////
//void CSystem::ShutDownThreadSystem()
//{
//	SAFE_DELETE(m_pThreadManager);
//}
