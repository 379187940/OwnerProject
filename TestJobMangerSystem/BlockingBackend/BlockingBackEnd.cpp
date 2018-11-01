// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   BlockingBackEnd.h
//  Version:     v1.00
//  Created:     07/05/2011 by Christopher Bolte
//  Compilers:   Visual Studio.NET
// -------------------------------------------------------------------------
//  History:
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "../MSVCspecific.h"
#include "../Win32specific.h"
#include <assert.h>
#include "..\BitFiddling.h"
#include "BlockingBackEnd.h"
#include "../JobManager.h"
//#include "../../System.h"
//#include "../../CPUDetect.h"

///////////////////////////////////////////////////////////////////////////////
JobManager::BlockingBackEnd::CBlockingBackEnd::CBlockingBackEnd(JobManager::SInfoBlock** pRegularWorkerFallbacks, unsigned int nRegularWorkerThreads) :
	m_Semaphore(SJobQueue_BlockingBackEnd::eMaxWorkQueueJobsRegularPriority),
	m_pRegularWorkerFallbacks(pRegularWorkerFallbacks),
	m_nRegularWorkerThreads(nRegularWorkerThreads),
	m_pWorkerThreads(NULL),
	m_nNumWorker(0)
{
	m_JobQueue.Init();

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	m_pBackEndWorkerProfiler = 0;
#endif
}

///////////////////////////////////////////////////////////////////////////////
JobManager::BlockingBackEnd::CBlockingBackEnd::~CBlockingBackEnd()
{
}

///////////////////////////////////////////////////////////////////////////////
bool JobManager::BlockingBackEnd::CBlockingBackEnd::Init(unsigned int nSysMaxWorker)
{
	m_pWorkerThreads = new CBlockingBackEndWorkerThread*[nSysMaxWorker];

	// create single worker thread for blocking backend
	for (unsigned int i = 0; i < nSysMaxWorker; ++i)
	{
		m_pWorkerThreads[i] = new CBlockingBackEndWorkerThread(this, m_Semaphore, m_JobQueue, m_pRegularWorkerFallbacks, m_nRegularWorkerThreads, i);

		if (!GetGlobalThreadManager()->SpawnThread(m_pWorkerThreads[i], "JobSystem_Worker_%u (Blocking)", i))
		{
			//CryFatalError("Error spawning \"JobSystem_Worker_%u (Blocking)\" thread.", i);
		}
	}

	m_nNumWorker = nSysMaxWorker;

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	m_pBackEndWorkerProfiler = new JobManager::CWorkerBackEndProfiler;
	m_pBackEndWorkerProfiler->Init(m_nNumWorker);
#endif

	return true;
}

///////////////////////////////////////////////////////////////////////////////
bool JobManager::BlockingBackEnd::CBlockingBackEnd::ShutDown()
{
	// On Exit: Full loop over all threads until each thread has stopped running.
	// Reason: It is unknown if all threads or just some might be woken up on a post
	// to the semaphore on all platforms.
	// ToDo for the ambitious: CHECK!
	do
	{
		bool allFinished = true;

		for (unsigned int i = 0; i < m_nNumWorker; ++i)
		{
			if (m_pWorkerThreads[i] == NULL)
				continue;

			m_pWorkerThreads[i]->SignalStopWork();
			allFinished = false;
		}

		if (allFinished)
			break;

		m_Semaphore.Release();

		for (unsigned int i = 0; i < m_nNumWorker; ++i)
		{
			if (m_pWorkerThreads[i] == NULL)
				continue;

			if (GetGlobalThreadManager()->JoinThread(m_pWorkerThreads[i], eJM_TryJoin))
			{
				delete m_pWorkerThreads[i];
				m_pWorkerThreads[i] = NULL;
			}
		}

	}
	while (true);

	m_pWorkerThreads = NULL;
	m_nNumWorker = 0;

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	SAFE_DELETE(m_pBackEndWorkerProfiler);
#endif

	return true;
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::BlockingBackEnd::CBlockingBackEnd::AddJob(JobManager::CJobDelegator& crJob, const JobManager::TJobHandle cJobHandle, JobManager::SInfoBlock& rInfoBlock)
{
	unsigned int nJobPriority = crJob.GetPriorityLevel();
	CJobManager* __restrict pJobManager = CJobManager::Instance();

	/////////////////////////////////////////////////////////////////////////////
	// Acquire Infoblock to use
	unsigned int jobSlot;

	JobManager::SInfoBlock* pFallbackInfoBlock = NULL;
	// only wait for a jobslot if we are submitting from a regular thread, or if we are submitting
	// a blocking job from a regular worker thread
	bool bWaitForFreeJobSlot = (JobManager::IsWorkerThread() == false);

	const JobManager::detail::EAddJobRes cEnqRes = m_JobQueue.GetJobSlot(jobSlot, nJobPriority, bWaitForFreeJobSlot);

#if !defined(_RELEASE)
	pJobManager->IncreaseRunJobs();
	if (cEnqRes == JobManager::detail::eAJR_NeedFallbackJobInfoBlock)
		pJobManager->IncreaseRunFallbackJobs();
#endif
	// allocate fallback infoblock if needed
	IF (cEnqRes == JobManager::detail::eAJR_NeedFallbackJobInfoBlock, 0)
		pFallbackInfoBlock = new JobManager::SInfoBlock();

	// copy info block into job queue
	PREFAST_ASSUME(pFallbackInfoBlock);
	JobManager::SInfoBlock& RESTRICT_REFERENCE rJobInfoBlock = (cEnqRes == JobManager::detail::eAJR_NeedFallbackJobInfoBlock ?
	                                                            *pFallbackInfoBlock : m_JobQueue.jobInfoBlocks[nJobPriority][jobSlot]);

	// since we will use the whole InfoBlock, and it is aligned to 128 bytes, clear the cacheline, this is faster than a cachemiss on write
#if CRY_PLATFORM_64BIT
	//STATIC_CHECK( sizeof(JobManager::SInfoBlock) == 512, ERROR_SIZE_OF_SINFOBLOCK_NOT_EQUALS_512 );
#else
	//STATIC_CHECK( sizeof(JobManager::SInfoBlock) == 384, ERROR_SIZE_OF_SINFOBLOCK_NOT_EQUALS_384 );
#endif

	// first cache line needs to be persistent
	//ResetLine128(&rJobInfoBlock, 128);
	//ResetLine128(&rJobInfoBlock, 256);
#if CRY_PLATFORM_64BIT
	//ResetLine128(&rJobInfoBlock, 384);
#endif

	/////////////////////////////////////////////////////////////////////////////
	// Initialize the InfoBlock
	rInfoBlock.AssignMembersTo(&rJobInfoBlock);

	// copy job parameter if it is a non-queue job
	if (crJob.GetQueue() == NULL)
	{
		JobManager::CJobManager::CopyJobParameter(crJob.GetParamDataSize(), rJobInfoBlock.GetParamAddress(), crJob.GetJobParamData());
	}

	assert(rInfoBlock.jobInvoker);

	const unsigned int cJobId = cJobHandle->jobId;
	rJobInfoBlock.jobId = (unsigned char)cJobId;

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	assert(cJobId < JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS);
	m_pBackEndWorkerProfiler->RegisterJob(cJobId, pJobManager->GetJobName(rInfoBlock.jobInvoker));
	rJobInfoBlock.frameProfIndex = (unsigned char)m_pBackEndWorkerProfiler->GetProfileIndex();
#endif

	/////////////////////////////////////////////////////////////////////////////
	// initialization finished, make all visible for worker threads
	// the producing thread won't need the info block anymore, flush it from the cache
	//FlushLine128(&rJobInfoBlock, 0);
	//FlushLine128(&rJobInfoBlock, 128);
#if CRY_PLATFORM_64BIT
	//FlushLine128(&rJobInfoBlock, 256);
	//FlushLine128(&rJobInfoBlock, 384);
#endif

	IF (cEnqRes == JobManager::detail::eAJR_NeedFallbackJobInfoBlock, 0)
	{
		// in case of a fallback job, add self to per thread list (thus no synchronization needed)
		JobManager::detail::PushToFallbackJobList(&rJobInfoBlock);
	}
	else
	{
		////CryLogAlways("Add Job to Slot 0x%x, priority 0x%x", jobSlot, nJobPriority );
		MemoryBarrier();
		m_JobQueue.jobInfoBlockStates[nJobPriority][jobSlot].SetReady();

		// Release semaphore count to signal the workers that work is available
		m_Semaphore.Release();
	}
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::BlockingBackEnd::CBlockingBackEndWorkerThread::SignalStopWork()
{
	m_bStop = true;
}

//////////////////////////////////////////////////////////////////////////
void JobManager::BlockingBackEnd::CBlockingBackEndWorkerThread::ThreadEntry()
{
	// set up thread id
	JobManager::detail::SetWorkerThreadId(m_nId | 0x40000000);
	do
	{
		SInfoBlock infoBlock;
		CJobManager* __restrict pJobManager = CJobManager::Instance();
		JobManager::SInfoBlock* pFallbackInfoBlock = JobManager::detail::PopFromFallbackJobList();

		IF (pFallbackInfoBlock, 0)
		{
			//CRY_PROFILE_REGION(PROFILE_SYSTEM, "JobWorkerThread: Fallback");

			// in case of a fallback job, just get it from the global per thread list
			pFallbackInfoBlock->AssignMembersTo(&infoBlock);
			if (!infoBlock.HasQueue())  // copy parameters for non producer/consumer jobs
			{
				JobManager::CJobManager::CopyJobParameter(infoBlock.paramSize << 4, infoBlock.GetParamAddress(), pFallbackInfoBlock->GetParamAddress());
			}

			// free temp info block again
			delete pFallbackInfoBlock;
		}
		else
		{
			///////////////////////////////////////////////////////////////////////////
			// wait for new work

			//CRY_PROFILE_REGION_WAITING(PROFILE_SYSTEM, "Wait - JobWorkerThread");

			m_rSemaphore.Acquire();

			IF (m_bStop == true, 0)
				break;

			bool bFoundBlockingFallbackJob = false;

			// handle fallbacks added by other worker threads
			for (unsigned int i = 0; i < m_nRegularWorkerThreads; ++i)
			{
				if (m_pRegularWorkerFallbacks[i])
				{
					JobManager::SInfoBlock* pRegularWorkerFallback = NULL;
					do
					{
						pRegularWorkerFallback = const_cast<JobManager::SInfoBlock*>(*(const_cast<volatile JobManager::SInfoBlock**>(&m_pRegularWorkerFallbacks[i])));
					}
					while (CryInterlockedCompareExchangePointer(alias_cast<void* volatile*>(&m_pRegularWorkerFallbacks[i]), pRegularWorkerFallback->pNext, alias_cast<void*>(pRegularWorkerFallback)) != pRegularWorkerFallback);

					// in case of a fallback job, just get it from the global per thread list
					pRegularWorkerFallback->AssignMembersTo(&infoBlock);
					if (!infoBlock.HasQueue())  // copy parameters for non producer/consumer jobs
					{
						JobManager::CJobManager::CopyJobParameter(infoBlock.paramSize << 4, infoBlock.GetParamAddress(), pRegularWorkerFallback->GetParamAddress());
					}

					// free temp info block again
					delete pRegularWorkerFallback;

					bFoundBlockingFallbackJob = true;
					break;
				}
			}

			// in case we didn't find a fallback, try the regular queue
			if (bFoundBlockingFallbackJob == false)
			{
				///////////////////////////////////////////////////////////////////////////
				// multiple steps to get a job of the queue
				// 1. get our job slot index
				unsigned long long currentPushIndex = ~0;
				unsigned long long currentPullIndex = ~0;
				unsigned long long newPullIndex = ~0;
				unsigned int nPriorityLevel = ~0;
				do
				{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID // emulate a 64bit atomic read on PC platfom
					currentPullIndex = CryInterlockedCompareExchange64(alias_cast<volatile signed long long*>(&m_rJobQueue.pull.index), 0, 0);
					currentPushIndex = CryInterlockedCompareExchange64(alias_cast<volatile signed long long*>(&m_rJobQueue.push.index), 0, 0);
#else
					currentPullIndex = *const_cast<volatile unsigned long long*>(&m_rJobQueue.pull.index);
					currentPushIndex = *const_cast<volatile unsigned long long*>(&m_rJobQueue.push.index);
#endif
					// spin if the updated push ptr didn't reach us yet
					if (currentPushIndex == currentPullIndex)
						continue;

					// compute priority level from difference between push/pull
					if (!JobManager::SJobQueuePos::IncreasePullIndex(currentPullIndex, currentPushIndex, newPullIndex, nPriorityLevel,
					                                                 m_rJobQueue.GetMaxWorkerQueueJobs(eHighPriority), m_rJobQueue.GetMaxWorkerQueueJobs(eRegularPriority), m_rJobQueue.GetMaxWorkerQueueJobs(eLowPriority), m_rJobQueue.GetMaxWorkerQueueJobs(eStreamPriority)))
						continue;

					// stop spinning when we succesfull got the index
					if (CryInterlockedCompareExchange64(alias_cast<volatile signed long long*>(&m_rJobQueue.pull.index), newPullIndex, currentPullIndex) == currentPullIndex)
						break;

				}
				while (true);

				// compute our jobslot index from the only increasing publish index
				unsigned int nExtractedCurIndex = static_cast<unsigned int>(JobManager::SJobQueuePos::ExtractIndex(currentPullIndex, nPriorityLevel));
				unsigned int nNumWorkerQUeueJobs = m_rJobQueue.GetMaxWorkerQueueJobs(nPriorityLevel);
				unsigned int nJobSlot = nExtractedCurIndex & (nNumWorkerQUeueJobs - 1);

				////CryLogAlways("Got Job From Slot 0x%x nPriorityLevel 0x%x", nJobSlot, nPriorityLevel );
				// 2. Wait still the produces has finished writing all data to the SInfoBlock
				JobManager::detail::SJobQueueSlotState* pJobInfoBlockState = &m_rJobQueue.jobInfoBlockStates[nPriorityLevel][nJobSlot];
				int iter = 0;
				while (!pJobInfoBlockState->IsReady())
				{
					Sleep(iter++ > 10 ? 1 : 0);
				}
				;

				// 3. Get a local copy of the info block as asson as it is ready to be used
				JobManager::SInfoBlock* pCurrentJobSlot = &m_rJobQueue.jobInfoBlocks[nPriorityLevel][nJobSlot];
				pCurrentJobSlot->AssignMembersTo(&infoBlock);
				if (!infoBlock.HasQueue())  // copy parameters for non producer/consumer jobs
				{
					JobManager::CJobManager::CopyJobParameter(infoBlock.paramSize << 4, infoBlock.GetParamAddress(), pCurrentJobSlot->GetParamAddress());
				}

				// 4. Remark the job state as suspended
				MemoryBarrier();
				pJobInfoBlockState->SetNotReady();

				// 5. Mark the jobslot as free again
				MemoryBarrier();
				pCurrentJobSlot->Release((1 << JobManager::SJobQueuePos::eBitsPerPriorityLevel) / m_rJobQueue.GetMaxWorkerQueueJobs(nPriorityLevel));
			}
		}

		///////////////////////////////////////////////////////////////////////////
		// now we have a valid SInfoBlock to start work on it
		// check if it is a producer/consumer queue job
		IF (infoBlock.HasQueue(), 0)
		{
			DoWorkProducerConsumerQueue(infoBlock);
		}
		else
		{
			// Now we are safe to use the info block
			assert(infoBlock.jobInvoker);
			assert(infoBlock.GetParamAddress());

			// store job start time
#if defined(JOBMANAGER_SUPPORT_PROFILING) && 0
			IF (infoBlock.GetJobState(), 1)
			{
				SJobState* pJobState = infoBlock.GetJobState();
				pJobState->LockProfilingData();
				if (pJobState->pJobProfilingData)
				{
					pJobState->pJobProfilingData->nStartTime = gEnv->pTimer->GetAsyncTime();
					pJobState->pJobProfilingData->nWorkerThread = GetWorkerThreadId();
				}
				pJobState->UnLockProfilingData();
			}
#endif

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
			const unsigned long long nStartTime = JobManager::IWorkerBackEndProfiler::GetTimeSample();
#endif

			// call delegator function to invoke job entry
#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
			//CRY_PROFILE_REGION(PROFILE_SYSTEM, "Job");
			//CRYPROFILE_SCOPE_PROFILE_MARKER(pJobManager->GetJobName(infoBlock.jobInvoker));
			//CRYPROFILE_SCOPE_PLATFORM_MARKER(pJobManager->GetJobName(infoBlock.jobInvoker));
#endif
			if (infoBlock.jobLambdaInvoker)
			{
				infoBlock.jobLambdaInvoker();
			}
			else
			{
				(*infoBlock.jobInvoker)(infoBlock.GetParamAddress());
			}

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
			JobManager::IWorkerBackEndProfiler* workerProfiler = m_pBlockingBackend->GetBackEndWorkerProfiler();
			const unsigned long long nEndTime = JobManager::IWorkerBackEndProfiler::GetTimeSample();
			workerProfiler->RecordJob(infoBlock.frameProfIndex, m_nId, static_cast<const unsigned int>(infoBlock.jobId), static_cast<const unsigned int>(nEndTime - nStartTime));
#endif

			IF (infoBlock.GetJobState(), 1)
			{
				SJobState* pJobState = infoBlock.GetJobState();
				pJobState->SetStopped();
			}
		}

	}
	while (m_bStop == false);

}
///////////////////////////////////////////////////////////////////////////////
inline void IncrQueuePullPointer_Blocking(INT_PTR& rCurPullAddr, const INT_PTR cIncr, const INT_PTR cQueueStart, const INT_PTR cQueueEnd)
{
	const INT_PTR cNextPull = rCurPullAddr + cIncr;
	rCurPullAddr = (cNextPull >= cQueueEnd) ? cQueueStart : cNextPull;
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::BlockingBackEnd::CBlockingBackEndWorkerThread::DoWorkProducerConsumerQueue(SInfoBlock& rInfoBlock)
{
	CJobManager* __restrict pJobManager = CJobManager::Instance();

	JobManager::SProdConsQueueBase* pQueue = (JobManager::SProdConsQueueBase*)rInfoBlock.GetQueue();

	volatile INT_PTR* pQueuePull = alias_cast<volatile INT_PTR*>(&pQueue->m_pPull);
	volatile INT_PTR* pQueuePush = alias_cast<volatile INT_PTR*>(&pQueue->m_pPush);

	unsigned int queueIncr = pQueue->m_PullIncrement;
	INT_PTR queueStart = pQueue->m_RingBufferStart;
	INT_PTR queueEnd = pQueue->m_RingBufferEnd;
	INT_PTR curPullPtr = *pQueuePull;

	bool bNewJobFound = true;

	// union used to construct 64 bit value for atomic updates
	union T64BitValue
	{
		signed long long doubleWord;
		struct
		{
			unsigned int word0;
			unsigned int word1;
		};
	};

	const unsigned int cParamSize = (rInfoBlock.paramSize << 4);

	Invoker pInvoker = NULL;
	unsigned char nJobInvokerIdx = (unsigned char) ~0;
	do
	{

		// == process job packet == //
		void* pParamMem = (void*)curPullPtr;
		SAddPacketData* const __restrict pAddPacketData = (SAddPacketData*)((unsigned char*)curPullPtr + cParamSize);

		// do we need another job invoker (multi-type job prod/con queue)
		IF (pAddPacketData->nInvokerIndex != nJobInvokerIdx, 0)
		{
			nJobInvokerIdx = pAddPacketData->nInvokerIndex;
			pInvoker = pJobManager->GetJobInvoker(nJobInvokerIdx);
		}
		if (!pInvoker && pAddPacketData->nInvokerIndex == (unsigned char) ~0)
		{
			pInvoker = rInfoBlock.jobInvoker;
		}

		// make sure we don't try to execute an already stopped job
		assert(pAddPacketData->pJobState && pAddPacketData->pJobState->IsRunning() == true);

		// call delegator function to invoke job entry
#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
		//CRY_PROFILE_REGION(PROFILE_SYSTEM, "Job");
		//CRYPROFILE_SCOPE_PROFILE_MARKER(pJobManager->GetJobName(rInfoBlock.jobInvoker));
		//CRYPROFILE_SCOPE_PLATFORM_MARKER(pJobManager->GetJobName(rInfoBlock.jobInvoker));
#endif
		PREFAST_ASSUME(pInvoker);
		(*pInvoker)(pParamMem);

		// mark job as finished
		IF (pAddPacketData->pJobState, 1)
		{
			pAddPacketData->pJobState->SetStopped();
		}

		// == update queue state == //
		IncrQueuePullPointer_Blocking(curPullPtr, queueIncr, queueStart, queueEnd);

		// load cur push once
		INT_PTR curPushPtr = *pQueuePush;

		// update pull ptr (safe since only change by a single job worker)
		*pQueuePull = curPullPtr;

		// check if we need to wake up the producer from a queue full state
		IF ((curPushPtr & 1) == 1, 0)
		{
			(*pQueuePush) = curPushPtr & ~1;
			pQueue->m_pQueueFullSemaphore->Release();
		}

		// clear queue full marker
		curPushPtr = curPushPtr & ~1;

		// work on next packet if still there
		IF (curPushPtr != curPullPtr, 1)
		{
			continue;
		}

		// temp end of queue, try to update the state while checking the push ptr
		bNewJobFound = false;

		JobManager::SJobSyncVariable runningSyncVar;
		runningSyncVar.SetRunning();
		JobManager::SJobSyncVariable stoppedSyncVar;

#if CRY_PLATFORM_64BIT // for 64 bit, we need to atomically swap 128 bit
		bool bStopLoop = false;
		bool bUnlockQueueFullstate = false;
		SJobSyncVariable queueStoppedSemaphore;
		signed long long resultValue[2] = { *alias_cast<signed long long*>(&runningSyncVar), curPushPtr };
		signed long long compareValue[2] = { *alias_cast<signed long long*>(&runningSyncVar), curPushPtr };  // use for result comparsion, since the original compareValue is overwritten
		signed long long exchangeValue[2] = { *alias_cast<signed long long*>(&stoppedSyncVar), curPushPtr };
		do
		{
			resultValue[0] = compareValue[0];
			resultValue[1] = compareValue[1];
			unsigned char ret = CryInterlockedCompareExchange128((volatile signed long long*)pQueue, exchangeValue[1], exchangeValue[0], resultValue);

			bNewJobFound = ((resultValue[1] & ~1) != curPushPtr);
			bStopLoop = bNewJobFound || (resultValue[0] == compareValue[0] && resultValue[1] == compareValue[1]);

			if (bNewJobFound == false && resultValue[0] > 1)
			{
				// get a copy of the syncvar for unlock (since we will overwrite it)
				queueStoppedSemaphore = *alias_cast<SJobSyncVariable*>(&resultValue[0]);

				// update the exchange value to ensure the CAS succeeds
				compareValue[0] = resultValue[0];
			}

			if (bNewJobFound == false && ((resultValue[1] & 1) == 1))
			{
				// update the exchange value to ensure the CAS succeeds
				compareValue[1] = resultValue[1];
				exchangeValue[1] = (resultValue[1] & ~1);
				bUnlockQueueFullstate = true; // needs to be unset after the loop to ensure a correct state
			}

		}
		while (!bStopLoop);

		if (bUnlockQueueFullstate)
		{
			assert(bNewJobFound == false);
			pQueue->m_pQueueFullSemaphore->Release();
		}
		if (queueStoppedSemaphore.IsRunning())
		{
			assert(bNewJobFound == false);
			queueStoppedSemaphore.SetStopped();
		}
#else
		bool bStopLoop = false;
		bool bUnlockQueueFullstate = false;
		SJobSyncVariable queueStoppedSemaphore;
		T64BitValue resultValue;

		T64BitValue compareValue;
		compareValue.word0 = *alias_cast<unsigned int*>(&runningSyncVar);
		compareValue.word1 = curPushPtr;

		T64BitValue exchangeValue;
		exchangeValue.word0 = *alias_cast<unsigned int*>(&stoppedSyncVar);
		exchangeValue.word1 = curPushPtr;

		do
		{

			resultValue.doubleWord = CryInterlockedCompareExchange64((volatile signed long long*)pQueue, exchangeValue.doubleWord, compareValue.doubleWord);

			bNewJobFound = ((resultValue.word1 & ~1) != curPushPtr);
			bStopLoop = bNewJobFound || resultValue.doubleWord == compareValue.doubleWord;

			if (bNewJobFound == false && resultValue.word0 > 1)
			{
				// get a copy of the syncvar for unlock (since we will overwrite it)
				queueStoppedSemaphore = *alias_cast<SJobSyncVariable*>(&resultValue.word0);

				// update the exchange value to ensure the CAS succeeds
				compareValue.word0 = resultValue.word0;
			}

			if (bNewJobFound == false && ((resultValue.word1 & 1) == 1))
			{
				// update the exchange value to ensure the CAS succeeds
				compareValue.word1 = resultValue.word1;
				exchangeValue.word1 = (resultValue.word1 & ~1);
				bUnlockQueueFullstate = true;
			}

		}
		while (!bStopLoop);

		if (bUnlockQueueFullstate)
		{
			assert(bNewJobFound == false);
			pQueue->m_pQueueFullSemaphore->Release();
		}
		if (queueStoppedSemaphore.IsRunning())
		{
			assert(bNewJobFound == false);
			queueStoppedSemaphore.SetStopped();
		}
#endif

	}
	while (bNewJobFound);

}

///////////////////////////////////////////////////////////////////////////////
JobManager::BlockingBackEnd::CBlockingBackEndWorkerThread::CBlockingBackEndWorkerThread(CBlockingBackEnd* pBlockingBackend, CryFastSemaphore& rSemaphore, JobManager::SJobQueue_BlockingBackEnd& rJobQueue, JobManager::SInfoBlock** pRegularWorkerFallbacks, unsigned int nRegularWorkerThreads, unsigned int nID) :
	m_rSemaphore(rSemaphore),
	m_rJobQueue(rJobQueue),
	m_bStop(false),
	m_pBlockingBackend(pBlockingBackend),
	m_nId(nID),
	m_pRegularWorkerFallbacks(pRegularWorkerFallbacks),
	m_nRegularWorkerThreads(nRegularWorkerThreads)
{
}

///////////////////////////////////////////////////////////////////////////////
JobManager::BlockingBackEnd::CBlockingBackEndWorkerThread::~CBlockingBackEndWorkerThread()
{

}

///////////////////////////////////////////////////////////////////////////////
void JobManager::BlockingBackEnd::CBlockingBackEnd::AddBlockingFallbackJob(JobManager::SInfoBlock* pInfoBlock, unsigned int nWorkerThreadID)
{
	volatile JobManager::SInfoBlock* pCurrentWorkerFallback = NULL;
	do
	{
		pCurrentWorkerFallback = *(const_cast<volatile JobManager::SInfoBlock**>(&m_pRegularWorkerFallbacks[nWorkerThreadID]));
		pInfoBlock->pNext = const_cast<JobManager::SInfoBlock*>(pCurrentWorkerFallback);
	}
	while (CryInterlockedCompareExchangePointer(alias_cast<void* volatile*>(&m_pRegularWorkerFallbacks[nWorkerThreadID]), pInfoBlock, alias_cast<void*>(pCurrentWorkerFallback)) != pCurrentWorkerFallback);
}
