// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

class IThreadConfigManager;

enum EJoinMode
{
	eJM_TryJoin,
	eJM_Join,
};

class IThread
{
public:
	// <interfuscator:shuffle>
	virtual ~IThread()
	{
	}

	//! Entry functions for code executed on thread.
	virtual void ThreadEntry() = 0;
	// </interfuscator:shuffle>
};

enum EFPE_Severity
{
	eFPE_None,  //!< No Floating Point Exceptions.
	eFPE_Basic, //!< Invalid operation, Div by 0.
	eFPE_All,   //!< Invalid operation, Div by 0, Denormalized operand, Overflow, Underflow, Inexact.
	eFPE_LastEntry
};

//#define SCOPED_ENABLE_FLOAT_EXCEPTIONS(eFPESeverity) CScopedFloatingPointException scopedSetFloatExceptionMask(eFPESeverity)
//#define SCOPED_DISABLE_FLOAT_EXCEPTIONS()            CScopedFloatingPointException scopedSetFloatExceptionMask(eFPE_None)

struct IThreadManager
{
public:
	// <interfuscator:shuffle>
	virtual ~IThreadManager()
	{
	}

	//! Get thread config manager.
	virtual IThreadConfigManager* GetThreadConfigManager() = 0;

	//! Spawn a new thread and apply thread config settings at thread beginning.
	virtual bool SpawnThread(IThread* pThread, const char* sThreadName, ...) = 0;

	//! Wait on another thread to exit (Blocking).
	//! Use eJM_TryJoin if you cannot be sure that the target thread is awake.
	//! \retval true if target thread has not been started yet or has already exited.
	//! \retval false if target thread is still running and therefore not in a state to exit.
	virtual bool JoinThread(IThread* pThreadTask, EJoinMode joinStatus) = 0;

	//! Register 3rd party thread with the thread manager.
	//! Applies thread config for thread if found.
	//! \param pThreadHandle If NULL, the current thread handle will be used.
	virtual bool RegisterThirdPartyThread(void* pThreadHandle, const char* sThreadName, ...) = 0;

	//! Unregister 3rd party thread with the thread manager.
	virtual bool UnRegisterThirdPartyThread(const char* sThreadName, ...) = 0;

	//! Get Thread Name.
	//! Returns "" if thread not found.
	virtual const char* GetThreadName(unsigned long nThreadId) = 0;

	//! Get ThreadID.
	virtual unsigned long GetThreadId(const char* sThreadName, ...) = 0;

	//! Execute function for each other thread but this one.
	typedef void (* ThreadModifFunction)(unsigned long nThreadId, void* pData);
	virtual void ForEachOtherThread(IThreadManager::ThreadModifFunction fpThreadModiFunction, void* pFuncData = 0) = 0;

	virtual void EnableFloatExceptions(EFPE_Severity eFPESeverity, unsigned long nThreadId = 0) = 0;
	virtual void EnableFloatExceptionsForEachOtherThread(EFPE_Severity eFPESeverity) = 0;

	virtual unsigned int GetFloatingPointExceptionMask() = 0;
	virtual void SetFloatingPointExceptionMask(unsigned int nMask) = 0;
	// </interfuscator:shuffle>
};
IThreadManager* GetGlobalThreadManager();
//class CScopedFloatingPointException
//{
//public:
//	CScopedFloatingPointException(EFPE_Severity eFPESeverity)
//	{
//		oldMask = g_ThreadManager.GetFloatingPointExceptionMask();
//		g_ThreadManager.EnableFloatExceptions(eFPESeverity);
//	}
//	~CScopedFloatingPointException()
//	{
//		g_ThreadManager.SetFloatingPointExceptionMask(oldMask);
//	}
//private:
//	unsigned int oldMask;
//};
