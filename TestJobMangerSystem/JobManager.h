// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/*
   definitions for job manager
   singleton implementation
 */

#ifndef __JOB_MANAGER_H__
#define __JOB_MANAGER_H__
#pragma once

#define JOBSYSTEM_INVOKER_COUNT (128)


#include <map>
#include<windows.h>
#include "IJobManager.h"
#include "JobStructs.h"
///////////////////////////////////////////////////////////////////////////////
namespace JobManager
{
/////////////////////////////////////////////////////////////////////////////
// Util Functions to get access Thread local data (needs to be unique per worker thread)
namespace detail {
// function to manipulate the per thread fallback job freelist
void                    PushToFallbackJobList(JobManager::SInfoBlock* pInfoBlock);
JobManager::SInfoBlock* PopFromFallbackJobList();

// functions to access the per thread worker thread id
void   SetWorkerThreadId(unsigned int nWorkerThreadId);
unsigned int GetWorkerThreadId();

} // namespace detail

// Tracks CPU/PPU worker thread(s) utilization and job execution time per frame
class CWorkerBackEndProfiler : public IWorkerBackEndProfiler
{
public:
	CWorkerBackEndProfiler();
	virtual ~CWorkerBackEndProfiler();

	virtual void Init(const unsigned short numWorkers);

	// Update the profiler at the beginning of the sample period
	virtual void Update();
	virtual void Update(const unsigned int curTimeSample);

	// Register a job with the profiler
	virtual void RegisterJob(const unsigned int jobId, const char* jobName);

	// Record execution information for a registered job
	virtual void RecordJob(const unsigned short profileIndex, const unsigned char workerId, const unsigned int jobId, const unsigned int runTimeMicroSec);

	// Get worker frame stats for the JobManager::detail::eJOB_FRAME_STATS - 1 frame
	virtual void GetFrameStats(JobManager::CWorkerFrameStats& rStats) const;
	virtual void GetFrameStats(TJobFrameStatsContainer& rJobStats, IWorkerBackEndProfiler::EJobSortOrder jobSortOrder) const;
	virtual void GetFrameStats(JobManager::CWorkerFrameStats& rStats, TJobFrameStatsContainer& rJobStats, IWorkerBackEndProfiler::EJobSortOrder jobSortOrder) const;

	// Get worker frame stats summary
	virtual void GetFrameStatsSummary(SWorkerFrameStatsSummary& rStats) const;
	virtual void GetFrameStatsSummary(SJobFrameStatsSummary& rStats) const;

	// Returns the index of the active multi-buffered profile data
	virtual unsigned short GetProfileIndex() const;

	// Get the number of workers tracked
	virtual unsigned int GetNumWorkers() const;

protected:
	void GetWorkerStats(const unsigned char nBufferIndex, JobManager::CWorkerFrameStats& rWorkerStats) const;
	void GetJobStats(const unsigned char nBufferIndex, TJobFrameStatsContainer& rJobStatsContainer, IWorkerBackEndProfiler::EJobSortOrder jobSortOrder) const;
	void ResetWorkerStats(const unsigned char nBufferIndex, const unsigned int curTimeSample);
	void ResetJobStats(const unsigned char nBufferIndex);

protected:
	struct SJobStatsInfo
	{
		JobManager::SJobFrameStats m_pJobStats[JobManager::detail::eJOB_FRAME_STATS * JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS];    // Array of job stats (multi buffered)
	};

	struct SWorkerStatsInfo
	{
		unsigned int                    m_nStartTime[JobManager::detail::eJOB_FRAME_STATS]; // Start Time of sample period (multi buffered)
		unsigned int                    m_nEndTime[JobManager::detail::eJOB_FRAME_STATS];   // End Time of sample period (multi buffered)
		unsigned short                    m_nNumWorkers;                                      // Number of workers tracked
		JobManager::SWorkerStats* m_pWorkerStats;                                     // Array of worker stats for each worker (multi buffered)
	};

protected:
	unsigned char            m_nCurBufIndex;      // Current buffer index [0,(JobManager::detail::eJOB_FRAME_STATS-1)]
	SJobStatsInfo    m_JobStatsInfo;      // Information about all job activities
	SWorkerStatsInfo m_WorkerStatsInfo;   // Information about each worker's utilization
};

class CJobLambda : public CJobBase
{
public:
	CJobLambda(const char* jobName, const std::function<void()>& lambda)
	{
		m_jobHandle = GetJobManagerInterface()->GetJobHandle(jobName, &Invoke);
		m_JobDelegator.SetJobParamData(0);
		m_JobDelegator.SetParamDataSize(0);
		m_JobDelegator.SetDelegator(Invoke);
		m_JobDelegator.SetLambda(lambda);
		SetJobProgramData(m_jobHandle);
	}
	void SetPriorityLevel(unsigned int nPriorityLevel)
	{
		m_JobDelegator.SetPriorityLevel(nPriorityLevel);
	}
	void SetBlocking()
	{
		m_JobDelegator.SetBlocking();
	}

private:
	static void Invoke(void* p)
	{
	}

public:
	TJobHandle m_jobHandle;
};

// singleton managing the job queues
class ANGELICA_ALIGN(128) CJobManager: public IJobManager
{
public:
	// singleton stuff
	static CJobManager* Instance();

	//destructor
	virtual ~CJobManager()
	{
		delete m_pThreadBackEnd;
		delete m_pFallBackBackEnd;
		_aligned_free(m_pBlockingBackEnd);
	}

	virtual void Init(unsigned int nSysMaxWorker) override;

	// wait for a job, preempt the calling thread if the job is not done yet
	virtual const bool WaitForJob(JobManager::SJobState & rJobState) const override;

	//adds a job
	virtual void AddJob(JobManager::CJobDelegator & crJob, const JobManager::TJobHandle cJobHandle) override;

	virtual void AddLambdaJob(const char* jobName, const std::function<void()> &lambdaCallback, TPriorityLevel priority = JobManager::eRegularPriority, SJobState * pJobState = nullptr) override;

	//obtain job handle from name
	virtual const JobManager::TJobHandle GetJobHandle(const char* cpJobName, const unsigned int cStrLen, JobManager::Invoker pInvoker) override;
	virtual const JobManager::TJobHandle GetJobHandle(const char* cpJobName, JobManager::Invoker pInvoker) override
	{
		return GetJobHandle(cpJobName, (int)strlen(cpJobName), pInvoker);
	}

	virtual JobManager::IBackend* GetBackEnd(JobManager::EBackEndType backEndType) override
	{
		switch (backEndType)
		{
		case eBET_Thread:
			return m_pThreadBackEnd;
		case eBET_Blocking:
			return m_pBlockingBackEnd;
		case eBET_Fallback:
			return m_pFallBackBackEnd;
		default:
			//ANGELICA_ASSERT_MESSAGE(0, "Unsupported EBackEndType encountered.");
			__debugbreak();
			return 0;
		}
		;

		return 0;
	}

	//shuts down job manager
	virtual void ShutDown() override;

	virtual bool InvokeAsJob(const char* cpJobName) const override;
	virtual bool InvokeAsJob(const JobManager::TJobHandle cJobHandle) const override;

	virtual void SetJobFilter(const char* pFilter) override
	{
		m_pJobFilter = pFilter;
	}

	virtual void SetJobSystemEnabled(int nEnable) override
	{
		m_nJobSystemEnabled = nEnable;
	}

	virtual void PushProfilingMarker(const char* pName) override;
	virtual void PopProfilingMarker() override;

	// move to right place
	enum { nMarkerEntries = 1024 };
	struct SMarker
	{
		//panzhijie
		//typedef CryFixedStringT<64> TMarkerString;
		enum MarkerType { PUSH_MARKER, POP_MARKER };

		SMarker() {}
		SMarker(MarkerType _type, CTimeValue _time, bool _bIsMainThread) : type(_type), time(_time), bIsMainThread(_bIsMainThread) {}
		SMarker(MarkerType _type, const char* pName, CTimeValue _time, bool _bIsMainThread) : type(_type), time(_time), bIsMainThread(_bIsMainThread) {
			memcpy(marker, pName, strlen(pName)+1);
		}
		
		//TMarkerString marker;
		char marker[60];
		MarkerType    type;
		CTimeValue    time;
		bool          bIsMainThread;
	};
	unsigned int m_nMainThreadMarkerIndex[SJobProfilingDataContainer::nCapturedFrames];
	unsigned int m_nRenderThreadMarkerIndex[SJobProfilingDataContainer::nCapturedFrames];
	SMarker m_arrMainThreadMarker[SJobProfilingDataContainer::nCapturedFrames][nMarkerEntries];
	SMarker m_arrRenderThreadMarker[SJobProfilingDataContainer::nCapturedFrames][nMarkerEntries];
	// move to right place

	//copy the job parameter into the jobinfo  structure
	static void CopyJobParameter(const unsigned int cJobParamSize, void* pDest, const void* pSrc);

	unsigned int GetWorkerThreadId() const override;

	virtual JobManager::SJobProfilingData* GetProfilingData(unsigned short nProfilerIndex) override;
	virtual unsigned short ReserveProfilingData() override;

	void Update(int nJobSystemProfiler) override;

	virtual void SetFrameStartTime(const CTimeValue &rFrameStartTime) override;

	//ColorB GetRegionColor(SMarker::TMarkerString marker);
	JobManager::Invoker GetJobInvoker(unsigned int nIdx)
	{
		assert(nIdx < m_nJobInvokerIdx);
		assert(nIdx < JOBSYSTEM_INVOKER_COUNT);
		return m_arrJobInvokers[nIdx];
	}
	virtual unsigned int GetNumWorkerThreads() const override
	{
		return m_pThreadBackEnd ? m_pThreadBackEnd->GetNumWorkerThreads() : 0;
	}

	// get a free semaphore from the jobmanager pool
	virtual JobManager::TSemaphoreHandle AllocateSemaphore(volatile const void* pOwner) override;

	// return a semaphore to the jobmanager pool
	virtual void DeallocateSemaphore(JobManager::TSemaphoreHandle nSemaphoreHandle, volatile const void* pOwner) override;

	// increase the refcounter of a semaphore, but only if it is > 0, else returns false
	virtual bool AddRefSemaphore(JobManager::TSemaphoreHandle nSemaphoreHandle, volatile const void* pOwner) override;

	// 'allocate' a semaphore in the jobmanager, and return the index of it
	virtual SJobFinishedConditionVariable* GetSemaphore(JobManager::TSemaphoreHandle nSemaphoreHandle, volatile const void* pOwner) override;

	virtual void DumpJobList() override;

	//virtual bool OnInputEvent(const SInputEvent &event) override;

	void IncreaseRunJobs();
	void IncreaseRunFallbackJobs();

	void AddBlockingFallbackJob(JobManager::SInfoBlock * pInfoBlock, unsigned int nWorkerThreadID);

	const char* GetJobName(JobManager::Invoker invoker);

private:
	//static ColorB GenerateColorBasedOnName(const char* name);

	CryCriticalSection m_JobManagerLock;                             // lock to protect non-performance critical parts of the jobmanager
	JobManager::Invoker m_arrJobInvokers[JOBSYSTEM_INVOKER_COUNT];   // support 128 jobs for now
	unsigned int m_nJobInvokerIdx;

	const char* m_pJobFilter;
	int m_nJobSystemEnabled;                                // should the job system be used
	int m_bJobSystemProfilerEnabled;                        // should the job system profiler be enabled
	bool m_bJobSystemProfilerPaused;                        // should the job system profiler be paused

	bool m_Initialized;                                     //true if JobManager have been initialized

	IBackend* m_pFallBackBackEnd;               // Backend for development, jobs are executed in their calling thread
	IBackend* m_pThreadBackEnd;                 // Backend for regular jobs, available on PC/XBOX. on Xbox threads are polling with a low priority
	IBackend* m_pBlockingBackEnd;               // Backend for tasks which can block to prevent stalling regular jobs in this case

	unsigned short m_nJobIdCounter;                     // JobId counter for jobs dynamically allocated at runtime

	std::set<JobManager::SJobStringHandle> m_registeredJobs;

	enum { nSemaphorePoolSize = 16 };
	SJobFinishedConditionVariable m_JobSemaphorePool[nSemaphorePoolSize];
	unsigned int m_nCurrentSemaphoreIndex;

	// per frame counter for jobs run/fallback jobs
	unsigned int m_nJobsRunCounter;
	unsigned int m_nFallbackJobsRunCounter;

	JobManager::SInfoBlock** m_pRegularWorkerFallbacks;
	unsigned int m_nRegularWorkerThreads;

	bool m_bSuspendWorkerForMP;
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	SJobProfilingDataContainer m_profilingData;
	std::map<JobManager::SJobStringHandle, ColorB> m_JobColors;
	std::map<SMarker::TMarkerString, ColorB> m_RegionColors;
	CTimeValue m_FrameStartTime[SJobProfilingDataContainer::nCapturedFrames];

#endif

	// singleton stuff
	CJobManager();
	// disable copy and assignment
	CJobManager(const CJobManager &);
	CJobManager& operator=(const CJobManager&);

};
}//JobManager
template<typename T>
inline bool IsAligned(T nData, size_t nAlign)
{
	assert((nAlign & (nAlign - 1)) == 0);
	return (size_t(nData) & (nAlign - 1)) == 0;
}
///////////////////////////////////////////////////////////////////////////////
inline void JobManager::CJobManager::CopyJobParameter(const unsigned int cJobParamSize, void* pDestParam, const void* pSrcParam)
{
	assert(IsAligned(cJobParamSize, 16) );//&& "JobParameter Size needs to be a multiple of 16"
	assert(cJobParamSize <= JobManager::SInfoBlock::scAvailParamSize );//&& "JobParameter Size larger than available storage"
	memcpy(pDestParam, pSrcParam, cJobParamSize);
}

///////////////////////////////////////////////////////////////////////////////
inline void JobManager::CJobManager::IncreaseRunJobs()
{
	CryInterlockedIncrement((int volatile*)&m_nJobsRunCounter);
}

///////////////////////////////////////////////////////////////////////////////
inline void JobManager::CJobManager::IncreaseRunFallbackJobs()
{
	CryInterlockedIncrement((int volatile*)&m_nFallbackJobsRunCounter);
}

#endif //__JOB_MANAGER_H__
