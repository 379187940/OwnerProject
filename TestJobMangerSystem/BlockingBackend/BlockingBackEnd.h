// Copyright 2001-2017 Angelicatek GmbH / Angelicatek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   ThreadBackEnd.h
//  Version:     v1.00
//  Created:     07/05/2011 by Christopher Bolte
//  Compilers:   Visual Studio.NET
// -------------------------------------------------------------------------
//  History:
////////////////////////////////////////////////////////////////////////////

#ifndef BLOCKING_BACKEND_H_
#define BLOCKING_BACKEND_H_

#include "../IJobManager.h"
#include "../JobStructs.h"

#include "../IThreadManager.h"

namespace JobManager
{
class CJobManager;
class CWorkerBackEndProfiler;
}

namespace JobManager {
namespace BlockingBackEnd {
namespace detail {
// stack size for each worker thread of the blocking backend
enum {eStackSize = 32 * 1024 };

}   // namespace detail

// forward declarations
class CBlockingBackEnd;

// class to represent a worker thread for the PC backend
class CBlockingBackEndWorkerThread : public IThread
{
public:
	CBlockingBackEndWorkerThread(CBlockingBackEnd* pBlockingBackend, AngelicaFastSemaphore& rSemaphore, JobManager::SJobQueue_BlockingBackEnd& rJobQueue, JobManager::SInfoBlock** pRegularWorkerFallbacks, unsigned int nRegularWorkerThreads, unsigned int nID);
	~CBlockingBackEndWorkerThread();

	// Start accepting work on thread
	virtual void ThreadEntry();

	// Signals to the worker thread that is should not accept anymore work and exit
	void SignalStopWork();

private:
	void DoWork();
	void DoWorkProducerConsumerQueue(SInfoBlock& rInfoBlock);

	unsigned int                                 m_nId;                   // id of the worker thread
	volatile bool                          m_bStop;
	AngelicaFastSemaphore&                      m_rSemaphore;
	JobManager::SJobQueue_BlockingBackEnd& m_rJobQueue;
	CBlockingBackEnd*                      m_pBlockingBackend;

	// members used for special blocking backend fallback handling
	JobManager::SInfoBlock** m_pRegularWorkerFallbacks;
	unsigned int                   m_nRegularWorkerThreads;
};

// the implementation of the PC backend
// has n-worker threads which use atomic operations to pull from the job queue
// and uses a semaphore to signal the workers if there is work requiered
class CBlockingBackEnd : public IBackend
{
public:
	CBlockingBackEnd(JobManager::SInfoBlock** pRegularWorkerFallbacks, unsigned int nRegularWorkerThreads);
	virtual ~CBlockingBackEnd();

	bool           Init(unsigned int nSysMaxWorker);
	bool           ShutDown();
	void           Update() {}

	virtual void   AddJob(JobManager::CJobDelegator& crJob, const JobManager::TJobHandle cJobHandle, JobManager::SInfoBlock& rInfoBlock);

	virtual unsigned int GetNumWorkerThreads() const { return m_nNumWorker; }

	void           AddBlockingFallbackJob(JobManager::SInfoBlock* pInfoBlock, unsigned int nWorkerThreadID);

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	JobManager::IWorkerBackEndProfiler* GetBackEndWorkerProfiler() const { return m_pBackEndWorkerProfiler; }
#endif

private:
	friend class JobManager::CJobManager;

	JobManager::SJobQueue_BlockingBackEnd m_JobQueue;                   // job queue node where jobs are pushed into and from
	AngelicaFastSemaphore                      m_Semaphore;                  // semaphore to count available jobs, to allow the workers to go sleeping instead of spinning when no work is requiered
	CBlockingBackEndWorkerThread**        m_pWorkerThreads;             // worker threads for blocking backend
	unsigned char m_nNumWorker;                                                 // number of allocated worker threads

	// members used for special blocking backend fallback handling
	JobManager::SInfoBlock** m_pRegularWorkerFallbacks;
	unsigned int                   m_nRegularWorkerThreads;

	// members required for profiling jobs in the frame profiler
#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	JobManager::IWorkerBackEndProfiler* m_pBackEndWorkerProfiler;
#endif
};

} // namespace BlockingBackEnd
} // namespace JobManager

#endif // BLOCKING_BACKEND_H_
