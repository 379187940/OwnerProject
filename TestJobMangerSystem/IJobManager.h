// Copyright 2001-2017 Angelicatek GmbH / Angelicatek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   IJobManager.h
//  Version:     v1.00
//  Created:     1/8/2011 by Christopher Bolte
//  Description: JobManager interface
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#include<windows.h>
#include "AngelicaThread.h"
#include "timevalue.h"
#include <functional>
#include "FairMonitor.h"

// Job manager settings

//! Enable to obtain stats of spu usage each frame.
#define JOBMANAGER_SUPPORT_FRAMEPROFILER

#if !defined(DEDICATED_SERVER)
//! Collect per-job informations about dispatch, start, stop and sync times.
	#define JOBMANAGER_SUPPORT_PROFILING
#endif

// Disable features which cost performance.
#if !defined(USE_FRAME_PROFILER) || ANGELICA_PLATFORM_MOBILE
	#undef JOBMANAGER_SUPPORT_FRAMEPROFILER
	#undef JOBMANAGER_SUPPORT_PROFILING
#endif

struct ILog;

//! Implementation of mutex/condition variable.
//! Used in the job manager for yield waiting in corner cases like waiting for a job to finish/jobqueue full and so on.
class SJobFinishedConditionVariable
{
public:
	SJobFinishedConditionVariable() :
		m_nRefCounter(0),
		m_pOwner(NULL)
	{
		m_nFinished = 1;
	}

	void Acquire()
	{
		//wait for completion of the condition
		m_CondNotify.BeginSynchronized();
		while (m_nFinished == 0)
			m_CondNotify.Wait(INFINITE);
		m_CondNotify.EndSynchronized();
	}

	void Release()
	{
		m_CondNotify.BeginSynchronized();
		m_nFinished = 1;
		m_CondNotify.NotifyAll();
		m_CondNotify.EndSynchronized();
		
	}

	void SetRunning()
	{
		m_nFinished = 0;
	}

	bool IsRunning()
	{
		return m_nFinished == 0;
	}

	void SetStopped()
	{
		m_nFinished = 1;
	}

	void SetOwner(volatile const void* pOwner)
	{
		if (m_pOwner != NULL)
			__debugbreak();
		m_pOwner = pOwner;
	}

	void ClearOwner(volatile const void* pOwner)
	{
		if (m_pOwner != pOwner)
			__debugbreak();

		m_pOwner = NULL;
	}

	bool AddRef(volatile const void* pOwner)
	{
		if (m_pOwner != pOwner)
			return false;

		++m_nRefCounter;
		return true;
	}
	unsigned int DecRef(volatile const void* pOwner)
	{
		if (m_pOwner != pOwner)
			__debugbreak();

		return --m_nRefCounter;
	}

	bool HasOwner() const { return m_pOwner != NULL; }

private:
	/*AngelicaMutex             m_Notify;
	AngelicaConditionVariable m_CondNotify;*/
	FairMonitor m_CondNotify;
	volatile unsigned int      m_nFinished;
	volatile unsigned int      m_nRefCounter;
	volatile const void* m_pOwner;
};

namespace JobManager {
class CJobBase;
}
namespace JobManager {
class CJobDelegator;
}
namespace JobManager {
struct SProdConsQueueBase;
}
namespace JobManager {
namespace detail {
struct SJobQueueSlotState;
}
}

//! Wrapper for Vector4 unsigned int.
struct _declspec(align(16)) SVEC4_UINT
{
#if !ANGELICA_PLATFORM_SSE2
	unsigned int v[4];
#else
	__m128 v;
#endif
};

// Global values.

//! Front end values.
namespace JobManager
{
//! Priority settings used for jobs.
enum TPriorityLevel
{
	eHighPriority     = 0,
	eRegularPriority  = 1,
	eLowPriority      = 2,
	eStreamPriority   = 3,
	eNumPriorityLevel = 4
};

//! Stores worker utilization. One instance per Worker.
//! One instance per cache line to avoid thread synchronization issues.
struct _declspec(align(128)) SWorkerStats
{
	unsigned int nExecutionPeriod;    //!< Accumulated job execution period in micro seconds.
	unsigned int nNumJobsExecuted;    //!< Number of jobs executed.
};

//! Type to reprensent a semaphore handle of the jobmanager.
typedef unsigned short TSemaphoreHandle;

//! Magic value to reprensent an invalid job handle.
enum  : unsigned int { INVALID_JOB_HANDLE = ((unsigned int)-1) };

//! BackEnd Type.
enum EBackEndType
{
	eBET_Thread,
	eBET_Fallback,
	eBET_Blocking
};

namespace Fiber
{
//! The alignment of the fibertask stack (currently set to 128 kb).
enum {FIBERTASK_ALIGNMENT = 128U << 10 };

//! The number of switches the fibertask records (default: 32).
enum {FIBERTASK_RECORD_SWITCHES = 32U };

} // namespace JobManager::Fiber

} // namespace JobManager

//! Structures used by the JobManager.
namespace JobManager
{
struct SJobStringHandle
{
	const char*  cpString;       //!< Points into repository, must be first element.
	unsigned int strLen;         //!< String length.
	int          jobId;          //!< Index (also acts as id) of job.
	unsigned int       nJobInvokerIdx; //!< Index of the jobInvoker (used to job switching in the prod/con queue).

	inline bool operator==(const SJobStringHandle& crOther) const;
	inline bool operator<(const SJobStringHandle& crOther) const;

};

//! Handle retrieved by name for job invocation.
typedef SJobStringHandle* TJobHandle;

bool SJobStringHandle::operator==(const SJobStringHandle& crOther) const
{
	return strcmp(cpString, crOther.cpString) == 0;
}

bool SJobStringHandle::operator<(const SJobStringHandle& crOther) const
{
	return strcmp(cpString, crOther.cpString) < 0;
}

//! Struct to collect profiling informations about job invocations and sync times.
struct _declspec(align(16)) SJobProfilingData
{
	typedef CTimeValue TimeValueT;

	TimeValueT nStartTime;
	TimeValueT nEndTime;

	TimeValueT nWaitBegin;
	TimeValueT nWaitEnd;

	TJobHandle jobHandle;
	unsigned long nThreadId;
	unsigned int nWorkerThread;

};

struct SJobProfilingDataContainer
{
	enum {nCapturedFrames = 4};
	enum {nCapturedEntriesPerFrame = 4096 };
	unsigned int       nFrameIdx;
	unsigned int       nProfilingDataCounter[nCapturedFrames];

	inline unsigned int GetFillFrameIdx()       { return nFrameIdx % nCapturedFrames; }
	inline unsigned int GetPrevFrameIdx()       { return (nFrameIdx - 1) % nCapturedFrames; }
	inline unsigned int GetRenderFrameIdx()     { return (nFrameIdx - 2) % nCapturedFrames; }
	inline unsigned int GetPrevRenderFrameIdx() { return (nFrameIdx - 3) % nCapturedFrames; }

	_declspec(align(128)) SJobProfilingData m_DymmyProfilingData;
	SJobProfilingData arrJobProfilingData[nCapturedFrames][nCapturedEntriesPerFrame];
};

//! Special combination of a volatile spinning variable combined with a semaphore.
//! Used if the running state is not yet set to finish during waiting.
struct SJobSyncVariable
{
	SJobSyncVariable();

	bool IsRunning() const volatile;

	//! Interface, should only be used by the job manager or the job state classes.
	void Wait() volatile;
	void SetRunning() volatile;
	bool SetStopped(struct SJobStateBase* pPostCallback = nullptr) volatile;

private:
	friend class CJobManager;

	//! Union used to combine the semaphore and the running state in a single word.
	union SyncVar
	{
		volatile unsigned int wordValue;
		struct
		{
			unsigned short           nRunningCounter;
			TSemaphoreHandle semaphoreHandle;
		};
	};

	SyncVar syncVar;      //!< Sync-variable which contain the running state or the used semaphore.
#if ANGELICA_PLATFORM_64BIT
	char    padding[4];
#endif
};

//! Condition variable like struct to be used for polling if a job has been finished.
struct SJobStateBase
{
public:
	inline bool IsRunning() const { return syncVar.IsRunning(); }
	inline void SetRunning()
	{
		syncVar.SetRunning();
	}
	virtual bool SetStopped()
	{
		return syncVar.SetStopped(this);
	}
	virtual void AddPostJob() {};

	virtual ~SJobStateBase() {}

private:
	friend class CJobManager;

	SJobSyncVariable syncVar;
};

//! For speed, use 16 byte aligned job state.
struct _declspec(align(16)) SJobState: SJobStateBase
{
	//! When profiling, intercept the SetRunning() and SetStopped() functions for profiling informations.
	inline SJobState()
		: m_pFollowUpJob(NULL)
#if defined(JOBMANAGER_SUPPORT_PROFILING)
		  , nProfilerIndex(~0)
#endif
	{
	}

	inline void SetRunning();

	virtual void AddPostJob() override;

	inline void RegisterPostJob(CJobBase* pFollowUpJob) { m_pFollowUpJob = pFollowUpJob; }

	// Non blocking trying to stop state, and run post job.
	inline bool TryStopping()
	{
		if (IsRunning())
		{
			return SetStopped();
		}
		return true;
	}

	inline const bool Wait();

#if defined(JOBMANAGER_SUPPORT_PROFILING)
	unsigned short nProfilerIndex;
#endif

	CJobBase* m_pFollowUpJob;
};

struct SJobStateLambda : public SJobState
{
	void RegisterPostJobCallback(const char* postJobName, const std::function<void()>& lambda, TPriorityLevel priority = eRegularPriority, SJobState* pJobState = 0)
	{
		m_callbackJobName = postJobName;
		m_callbackJobPriority = priority;
		m_callbackJobState = pJobState;
		m_callback = lambda;
	}
private:
	virtual void AddPostJob() override;
private:
	const char*                    m_callbackJobName;
	SJobState*                     m_callbackJobState;
	std::function<void()>          m_callback;
	TPriorityLevel                 m_callbackJobPriority;
	AngelicaCriticalSectionNonRecursive m_stopLock;
};

//! Stores worker utilization stats for a frame.
class CWorkerFrameStats
{
public:
	struct SWorkerStats
	{
		float  nUtilPerc;             //!< Utilization percentage this frame [0.f..100.f].
		unsigned int nExecutionPeriod;      //!< Total execution time on this worker in usec.
		unsigned int nNumJobsExecuted;      //!< Number of jobs executed contributing to nExecutionPeriod.
	};

public:
	inline CWorkerFrameStats(unsigned char workerNum)
		: numWorkers(workerNum)
	{
		workerStats = new SWorkerStats[workerNum];
		Reset();
	}

	inline ~CWorkerFrameStats()
	{
		delete[] workerStats;
	}

	inline void Reset()
	{
		for (int i = 0; i < numWorkers; ++i)
		{
			workerStats[i].nUtilPerc = 0.f;
			workerStats[i].nExecutionPeriod = 0;
			workerStats[i].nNumJobsExecuted = 0;
		}

		nSamplePeriod = 0;
	}

public:
	unsigned char         numWorkers;
	unsigned int        nSamplePeriod;
	SWorkerStats* workerStats;
};

struct SWorkerFrameStatsSummary
{
	unsigned int nSamplePeriod;             //!< Worker sample period.
	unsigned int nTotalExecutionPeriod;     //!< Total execution time on this worker in usec.
	unsigned int nTotalNumJobsExecuted;     //!< Total number of jobs executed contributing to nExecutionPeriod.
	float  nAvgUtilPerc;              //!< Average worker utilization [0.f ... 100.f].
	unsigned char  nNumActiveWorkers;         //!< Active number of workers for this frame.
};

//! Per frame and job specific.
struct _declspec(align(16)) SJobFrameStats
{
	unsigned int usec;                //!< Last frame's job time in microseconds.
	unsigned int count;               //!< Number of calls this frame.
	const char* cpName;               //!< Job name.

	unsigned int usecLast;            //!< Last but one frames Job time in microseconds (written and accumulated by Thread).

	inline SJobFrameStats();
	inline SJobFrameStats(const char* cpJobName);

	inline void Reset();
	inline void operator=(const SJobFrameStats& crFrom);
	inline bool operator<(const SJobFrameStats& crOther) const;

	static bool sort_lexical(const JobManager::SJobFrameStats& i, const JobManager::SJobFrameStats& j)
	{
		return strcmp(i.cpName, j.cpName) < 0;
	}

	static bool sort_time_high_to_low(const JobManager::SJobFrameStats& i, const JobManager::SJobFrameStats& j)
	{
		return i.usec > j.usec;
	}

	static bool sort_time_low_to_high(const JobManager::SJobFrameStats& i, const JobManager::SJobFrameStats& j)
	{
		return i.usec < j.usec;
	}
};

struct _declspec(align(16)) SJobFrameStatsSummary
{
	unsigned int nTotalExecutionTime;             //!< Total execution time of all jobs in microseconds.
	unsigned int nNumJobsExecuted;                //!< Total Number of job executions this frame.
	unsigned int nNumIndividualJobsExecuted;      //!< Total Number of individual jobs.
};

SJobFrameStats::SJobFrameStats(const char* cpJobName) : usec(0), count(0), cpName(cpJobName), usecLast(0)
{}

SJobFrameStats::SJobFrameStats() : cpName("Uninitialized"), usec(0), count(0), usecLast(0)
{}

void SJobFrameStats::operator=(const SJobFrameStats& crFrom)
{
	usec = crFrom.usec;
	count = crFrom.count;
	cpName = crFrom.cpName;
	usecLast = crFrom.usecLast;
}

void SJobFrameStats::Reset()
{
	usecLast = usec;
	usec = count = 0;
}

bool SJobFrameStats::operator<(const SJobFrameStats& crOther) const
{
	//sort from large to small
	return usec > crOther.usec;
}

//! Struct to represent a packet for the consumer producer queue.
struct _declspec(align(16))SAddPacketData
{
	JobManager::SJobState* pJobState;
	unsigned short profilerIndex;
	unsigned char nInvokerIndex;   //!< To keep the struct 16 bytes long, we use a index into the info block.
};

//! Delegator function.
//! Takes a pointer to a params structure and does the decomposition of the parameters, then calls the Job entry function.
typedef void (* Invoker)(void*);

//! Info block transferred first for each job.
//! We have to transfer the info block, parameter block, input memory needed for execution.
struct _declspec(align(128)) SInfoBlock
{
	//! Struct to represent the state of a SInfoBlock.
	struct SInfoBlockState
	{
		//! Union of requiered members and uin32 to allow easy atomic operations.
		union
		{
			struct
			{
				TSemaphoreHandle nSemaphoreHandle;    //!< Handle for the conditon variable to use in case some thread is waiting.
				unsigned short           nRoundID;            //!< Number of rounds over the job queue, to prevent a race condition with SInfoBlock reuse (we have 15 bits, thus 32k full job queue iterations are safe).
			};
			volatile LONG nValue;                     //!< value for easy Compare and Swap.
		};

		void IsInUse(unsigned int nRoundIdToCheckOrg, bool& rWait, bool& rRetry, unsigned int nMaxValue) volatile const
		{
			unsigned int nRoundIdToCheck = nRoundIdToCheckOrg & 0x7FFF;   //!< Clamp nRoundID to 15 bit.
			unsigned int nLoadedRoundID = nRoundID;
			unsigned int nNextLoadedRoundID = ((nLoadedRoundID + 1) & 0x7FFF);
			nNextLoadedRoundID = nNextLoadedRoundID >= nMaxValue ? 0 : nNextLoadedRoundID;

			if (nLoadedRoundID == nRoundIdToCheck)  // job slot free
			{
				rWait = false;
				rRetry = false;
			}
			else if (nNextLoadedRoundID == nRoundIdToCheck)     // job slot need to be worked on by worker
			{
				rWait = true;
				rRetry = false;
			}
			else   // producer was suspended long enough that the worker overtook it
			{
				rWait = false;
				rRetry = true;
			}
		}
	};

	//! data needed to run the job and it's functionality.
	volatile SInfoBlockState jobState;             //!< State of the SInfoBlock in job queue, should only be modified by access functions.
	Invoker jobInvoker;                            //!< Callback function to job invoker (extracts parameters and calls entry function).
	std::function<void()> jobLambdaInvoker;        //!< Alternative way to invoke job with lambda
	union                                          //!< External job state address /shared with address of prod-cons queue.
	{
		JobManager::SJobState*          pJobState;
		JobManager::SProdConsQueueBase* pQueue;
	};
	JobManager::SInfoBlock* pNext;                 //!< Single linked list for fallback jobs in the case the queue is full, and a worker wants to push new work.

	// Per-job settings like cache size and so on.
	unsigned char frameProfIndex;                  //!< Index of SJobFrameStats*.
	unsigned char nflags;
	unsigned char paramSize;                       //!< Size in total of parameter block in 16 byte units.
	unsigned char jobId;                           //!< Corresponding job ID, needs to track jobs.

	// We could also use a union, but this solution is (hopefully) clearer.
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	unsigned short profilerIndex;                  //!< index for the job system profiler.
#endif

	//! Bits used for nflags.
	static const unsigned int scHasQueue = 0x4;

	//! Size of the SInfoBlock struct and how much memory we have to store parameters.
#if ANGELICA_PLATFORM_64BIT
	static const unsigned int scSizeOfSJobQueueEntry = 512;
	static const unsigned int scSizeOfJobQueueEntryHeader = 64;   //!< Please adjust when adding/removing members, keep as a multiple of 16.
	static const unsigned int scAvailParamSize = scSizeOfSJobQueueEntry - scSizeOfJobQueueEntryHeader;
#else
	static const unsigned int scSizeOfSJobQueueEntry = 384;
	static const unsigned int scSizeOfJobQueueEntryHeader = 32;   //!< Please adjust when adding/removing members, keep as a multiple of 16.
	static const unsigned int scAvailParamSize = scSizeOfSJobQueueEntry - scSizeOfJobQueueEntryHeader;
#endif

	//! Parameter data are enclosed within to save a cache miss.
	_declspec(align(16)) unsigned char paramData[scAvailParamSize];    //!< is 16 byte aligned, make sure it is kept aligned.

	inline void AssignMembersTo(SInfoBlock* pDest) const
	{
		pDest->jobInvoker = jobInvoker;
		pDest->jobLambdaInvoker = jobLambdaInvoker;
		pDest->pJobState = pJobState;
		pDest->pQueue = pQueue;
		pDest->pNext = pNext;

		pDest->frameProfIndex = frameProfIndex;
		pDest->nflags = nflags;
		pDest->paramSize = paramSize;
		pDest->jobId = jobId;

#if defined(JOBMANAGER_SUPPORT_PROFILING)
		pDest->profilerIndex = profilerIndex;
#endif

		memcpy(pDest->paramData, paramData, sizeof(paramData));
	}

	inline bool HasQueue() const
	{
		return (nflags & (unsigned char)scHasQueue) != 0;
	}

	inline void SetJobState(JobManager::SJobState* _pJobState)
	{
		assert(!HasQueue());
		pJobState = _pJobState;
	}

	inline JobManager::SJobState* GetJobState() const
	{
		assert(!HasQueue());
		return pJobState;
	}

	inline JobManager::SProdConsQueueBase* GetQueue() const
	{
		assert(HasQueue());
		return pQueue;
	}

	inline unsigned char* const __restrict GetParamAddress()
	{
		assert(!HasQueue());
		return &paramData[0];
	}

	//! Functions to access jobstate of SInfoBlock.
	inline void IsInUse(unsigned int nRoundId, bool& rWait, bool& rRetry, unsigned int nMaxValue) const { return jobState.IsInUse(nRoundId, rWait, rRetry, nMaxValue); }

	void Release(unsigned int nMaxValue);
	void Wait(unsigned int nRoundID, unsigned int nMaxValue);

};

//! State information for the fill and empty pointers of the job queue.
struct _declspec(align(16)) SJobQueuePos
{
	//! Base of job queue per priority level.
	JobManager::SInfoBlock* jobQueue[JobManager::eNumPriorityLevel];

	//! Base of job queue per priority level.
	JobManager::detail::SJobQueueSlotState* jobQueueStates[JobManager::eNumPriorityLevel];

	//! Index use for job slot publishing each priority level is encoded in this single value.
	volatile unsigned long long index;

	//! Util function to increase counter for their priority level only.
	static const unsigned long long eBitsInIndex = sizeof(unsigned long long) * CHAR_BIT;
	static const unsigned long long eBitsPerPriorityLevel = eBitsInIndex / JobManager::eNumPriorityLevel;
	static const unsigned long long eMaskStreamPriority = (1ULL << eBitsPerPriorityLevel) - 1ULL;
	static const unsigned long long eMaskLowPriority = (((1ULL << eBitsPerPriorityLevel) << eBitsPerPriorityLevel) - 1ULL) & ~eMaskStreamPriority;
	static const unsigned long long eMaskRegularPriority = ((((1ULL << eBitsPerPriorityLevel) << eBitsPerPriorityLevel) << eBitsPerPriorityLevel) - 1ULL) & ~(eMaskStreamPriority | eMaskLowPriority);
	static const unsigned long long eMaskHighPriority = (((((1ULL << eBitsPerPriorityLevel) << eBitsPerPriorityLevel) << eBitsPerPriorityLevel) << eBitsPerPriorityLevel) - 1ULL) & ~(eMaskRegularPriority | eMaskStreamPriority | eMaskLowPriority);

	inline static unsigned long long ExtractIndex(unsigned long long currentIndex, unsigned int nPriorityLevel)
	{
		switch (nPriorityLevel)
		{
		case eHighPriority:
			return ((((currentIndex & eMaskHighPriority) >> eBitsPerPriorityLevel) >> eBitsPerPriorityLevel) >> eBitsPerPriorityLevel);
		case eRegularPriority:
			return (((currentIndex & eMaskRegularPriority) >> eBitsPerPriorityLevel) >> eBitsPerPriorityLevel);
		case eLowPriority:
			return ((currentIndex & eMaskLowPriority) >> eBitsPerPriorityLevel);
		case eStreamPriority:
			return (currentIndex & eMaskStreamPriority);
		default:
			return ~0;
		}
	}

	inline static bool IncreasePullIndex(unsigned long long currentPullIndex, unsigned long long currentPushIndex, unsigned long long& rNewPullIndex, unsigned int& rPriorityLevel,
	                                    unsigned int nJobQueueSizeHighPriority, unsigned int nJobQueueSizeRegularPriority, unsigned int nJobQueueSizeLowPriority, unsigned int nJobQueueSizeStreamPriority)
	{
		unsigned int nPushExtractedHighPriority = static_cast<unsigned int>(ExtractIndex(currentPushIndex, eHighPriority));
		unsigned int nPushExtractedRegularPriority = static_cast<unsigned int>(ExtractIndex(currentPushIndex, eRegularPriority));
		unsigned int nPushExtractedLowPriority = static_cast<unsigned int>(ExtractIndex(currentPushIndex, eLowPriority));
		unsigned int nPushExtractedStreamPriority = static_cast<unsigned int>(ExtractIndex(currentPushIndex, eStreamPriority));

		unsigned int nPullExtractedHighPriority = static_cast<unsigned int>(ExtractIndex(currentPullIndex, eHighPriority));
		unsigned int nPullExtractedRegularPriority = static_cast<unsigned int>(ExtractIndex(currentPullIndex, eRegularPriority));
		unsigned int nPullExtractedLowPriority = static_cast<unsigned int>(ExtractIndex(currentPullIndex, eLowPriority));
		unsigned int nPullExtractedStreamPriority = static_cast<unsigned int>(ExtractIndex(currentPullIndex, eStreamPriority));

		unsigned int nRoundPushHighPriority = nPushExtractedHighPriority / nJobQueueSizeHighPriority;
		unsigned int nRoundPushRegularPriority = nPushExtractedRegularPriority / nJobQueueSizeRegularPriority;
		unsigned int nRoundPushLowPriority = nPushExtractedLowPriority / nJobQueueSizeLowPriority;
		unsigned int nRoundPushStreamPriority = nPushExtractedStreamPriority / nJobQueueSizeStreamPriority;

		unsigned int nRoundPullHighPriority = nPullExtractedHighPriority / nJobQueueSizeHighPriority;
		unsigned int nRoundPullRegularPriority = nPullExtractedRegularPriority / nJobQueueSizeRegularPriority;
		unsigned int nRoundPullLowPriority = nPullExtractedLowPriority / nJobQueueSizeLowPriority;
		unsigned int nRoundPullStreamPriority = nPullExtractedStreamPriority / nJobQueueSizeStreamPriority;

		unsigned int nPushJobSlotHighPriority = nPushExtractedHighPriority & (nJobQueueSizeHighPriority - 1);
		unsigned int nPushJobSlotRegularPriority = nPushExtractedRegularPriority & (nJobQueueSizeRegularPriority - 1);
		unsigned int nPushJobSlotLowPriority = nPushExtractedLowPriority & (nJobQueueSizeLowPriority - 1);
		unsigned int nPushJobSlotStreamPriority = nPushExtractedStreamPriority & (nJobQueueSizeStreamPriority - 1);

		unsigned int nPullJobSlotHighPriority = nPullExtractedHighPriority & (nJobQueueSizeHighPriority - 1);
		unsigned int nPullJobSlotRegularPriority = nPullExtractedRegularPriority & (nJobQueueSizeRegularPriority - 1);
		unsigned int nPullJobSlotLowPriority = nPullExtractedLowPriority & (nJobQueueSizeLowPriority - 1);
		unsigned int nPullJobSlotStreamPriority = nPullExtractedStreamPriority & (nJobQueueSizeStreamPriority - 1);

		//NOTE: if this shows in the profiler, find a lockless variant of it
		if (((nRoundPushHighPriority == nRoundPullHighPriority) && nPullJobSlotHighPriority < nPushJobSlotHighPriority) ||
		    ((nRoundPushHighPriority != nRoundPullHighPriority) && nPullJobSlotHighPriority >= nPushJobSlotHighPriority))
		{
			rPriorityLevel = eHighPriority;
			rNewPullIndex = IncreaseIndex(currentPullIndex, eHighPriority);
		}
		else if (((nRoundPushRegularPriority == nRoundPullRegularPriority) && nPullJobSlotRegularPriority < nPushJobSlotRegularPriority) ||
		         ((nRoundPushRegularPriority != nRoundPullRegularPriority) && nPullJobSlotRegularPriority >= nPushJobSlotRegularPriority))
		{
			rPriorityLevel = eRegularPriority;
			rNewPullIndex = IncreaseIndex(currentPullIndex, eRegularPriority);
		}
		else if (((nRoundPushLowPriority == nRoundPullLowPriority) && nPullJobSlotLowPriority < nPushJobSlotLowPriority) ||
		         ((nRoundPushLowPriority != nRoundPullLowPriority) && nPullJobSlotLowPriority >= nPushJobSlotLowPriority))
		{
			rPriorityLevel = eLowPriority;
			rNewPullIndex = IncreaseIndex(currentPullIndex, eLowPriority);
		}
		else if (((nRoundPushStreamPriority == nRoundPullStreamPriority) && nPullJobSlotStreamPriority < nPushJobSlotStreamPriority) ||
		         ((nRoundPushStreamPriority != nRoundPullStreamPriority) && nPullJobSlotStreamPriority >= nPushJobSlotStreamPriority))
		{
			rPriorityLevel = eStreamPriority;
			rNewPullIndex = IncreaseIndex(currentPullIndex, eStreamPriority);
		}
		else
		{
			rNewPullIndex = ~0;
			return false;
		}

		return true;
	}

	inline static unsigned long long IncreasePushIndex(unsigned long long currentPushIndex, unsigned int nPriorityLevel)
	{
		return IncreaseIndex(currentPushIndex, nPriorityLevel);
	}

	inline static unsigned long long IncreaseIndex(unsigned long long currentIndex, unsigned int nPriorityLevel)
	{
		unsigned long long nIncrease = 1ULL;
		unsigned long long nMask = 0;
		switch (nPriorityLevel)
		{
		case eHighPriority:
			nIncrease <<= eBitsPerPriorityLevel;
			nIncrease <<= eBitsPerPriorityLevel;
			nIncrease <<= eBitsPerPriorityLevel;
			nMask = eMaskHighPriority;
			break;
		case eRegularPriority:
			nIncrease <<= eBitsPerPriorityLevel;
			nIncrease <<= eBitsPerPriorityLevel;
			nMask = eMaskRegularPriority;
			break;
		case eLowPriority:
			nIncrease <<= eBitsPerPriorityLevel;
			nMask = eMaskLowPriority;
			break;
		case eStreamPriority:
			nMask = eMaskStreamPriority;
			break;
		}

		// increase counter while preventing overflow
		unsigned long long nCurrentValue = currentIndex & nMask;                        // extract all bits for this priority only
		unsigned long long nCurrentValueCleared = currentIndex & ~nMask;                // extract all bits of other priorities (they shouldn't change)
		nCurrentValue += nIncrease;                                         // increase value by one (already at the right bit position)
		nCurrentValue &= nMask;                                             // mask out again to handle overflow
		unsigned long long nNewCurrentValue = nCurrentValueCleared | nCurrentValue;     // add new value to other priorities
		return nNewCurrentValue;
	}

};

//! Struct covers DMA data setup common to all jobs and packets.
class CCommonDMABase
{
public:
	CCommonDMABase();
	void        SetJobParamData(void* pParamData);
	const void* GetJobParamData() const;
	unsigned short      GetProfilingDataIndex() const;

	//! In case of persistent job objects, we have to reset the profiling data
	void ForceUpdateOfProfilingDataIndex();

protected:
	void*  m_pParamData;      //!< parameter struct, not initialized to 0 for performance reason.
	unsigned short nProfilerIndex;    //!< index handle for Jobmanager Profiling Data.
};

//! Delegation class for each job.
class CJobDelegator : public CCommonDMABase
{
public:

	typedef volatile CJobBase* TDepJob;

	CJobDelegator();
	void                            RunJob(const JobManager::TJobHandle cJobHandle);
	void                            RegisterQueue(const JobManager::SProdConsQueueBase* const cpQueue);
	JobManager::SProdConsQueueBase* GetQueue() const;

	void                            RegisterJobState(JobManager::SJobState* __restrict pJobState);

	//return a copy since it will get overwritten
	void                   SetParamDataSize(const unsigned int cParamDataSize);
	const unsigned int     GetParamDataSize() const;
	void                   SetCurrentThreadId(const unsigned long cID);
	const unsigned long         GetCurrentThreadId() const;

	JobManager::SJobState* GetJobState() const;
	void                   SetRunning();

	inline void             SetDelegator(Invoker pGenericDelecator)
	{
		m_pGenericDelecator = pGenericDelecator;
	}

	Invoker GetGenericDelegator()
	{
		return m_pGenericDelecator;
	}

	unsigned int GetPriorityLevel() const                      { return m_nPrioritylevel; }
	bool         IsBlocking() const                            { return m_bIsBlocking; };

	void         SetPriorityLevel(unsigned int nPrioritylevel) { m_nPrioritylevel = nPrioritylevel; }
	void         SetBlocking()                                 { m_bIsBlocking = true; }

	void         SetLambda(const std::function<void()>& lambda)
	{
		m_lambda = lambda;
	}
	const std::function<void()>& GetLambda() const
	{
		return m_lambda;
	}

protected:
	JobManager::SJobState*                m_pJobState;      //!< Extern job state.
	const JobManager::SProdConsQueueBase* m_pQueue;         //!< Consumer/producer queue.
	unsigned int                          m_nPrioritylevel; //!< Enum represent the priority level to use.
	bool m_bIsBlocking;                                     //!< If true, then the job could block on a mutex/sleep.
	unsigned int                          m_ParamDataSize;  //!< Sizeof parameter struct.
	unsigned long                              m_CurThreadID;    //!< Current thread id.
	Invoker                               m_pGenericDelecator;
	std::function<void()>                 m_lambda;
};

//! Base class for jobs.
class CJobBase
{
public:
	inline CJobBase() : m_pJobProgramData(NULL)
	{
	}

	inline void RegisterJobState(JobManager::SJobState* __restrict pJobState) volatile
	{
		((CJobBase*)this)->m_JobDelegator.RegisterJobState(pJobState);
	}

	inline void RegisterQueue(const JobManager::SProdConsQueueBase* const cpQueue) volatile
	{
		((CJobBase*)this)->m_JobDelegator.RegisterQueue(cpQueue);
	}
	inline void Run()
	{
		m_JobDelegator.RunJob(m_pJobProgramData);
	}

	inline const JobManager::TJobHandle GetJobProgramData()
	{
		return m_pJobProgramData;
	}

	inline void SetCurrentThreadId(const unsigned long cID)
	{
		((CJobBase*)this)->m_JobDelegator.SetCurrentThreadId(cID);
	}

	inline const unsigned long GetCurrentThreadId() const
	{
		return ((CJobBase*)this)->m_JobDelegator.GetCurrentThreadId();
	}

	inline CJobDelegator* GetJobDelegator()
	{
		return &m_JobDelegator;
	}

protected:
	CJobDelegator          m_JobDelegator;        //!< Delegation implementation, all calls to job manager are going through it.
	JobManager::TJobHandle m_pJobProgramData;     //!< Handle to program data to run.

	//! Set the job program data.
	inline void SetJobProgramData(const JobManager::TJobHandle pJobProgramData)
	{
		assert(pJobProgramData);
		m_pJobProgramData = pJobProgramData;
	}

};
} // namespace JobManager

// Interface of the JobManager.

//! Create a Producer consumer queue for a job type.
#define PROD_CONS_QUEUE_TYPE(name, size) JobManager::CProdConsQueue < name, (size) >

//! Declaration for the InvokeOnLinkedStacked util function.
extern "C"
{
	void InvokeOnLinkedStack(void (* proc)(void*), void* arg, void* stack, size_t stackSize);
} // extern "C"

namespace JobManager
{
class IWorkerBackEndProfiler;

//! Base class for the various backends the jobmanager supports.
struct IBackend
{
	// <interfuscator:shuffle>
	virtual ~IBackend(){}

	virtual bool   Init(unsigned int nSysMaxWorker) = 0;
	virtual bool   ShutDown() = 0;
	virtual void   Update() = 0;

	virtual void   AddJob(JobManager::CJobDelegator& crJob, const JobManager::TJobHandle cJobHandle, JobManager::SInfoBlock& rInfoBlock) = 0;
	virtual unsigned int GetNumWorkerThreads() const = 0;
	// </interfuscator:shuffle>

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	virtual IWorkerBackEndProfiler* GetBackEndWorkerProfiler() const = 0;
#endif
};

//! Singleton managing the job queues.
struct IJobManager
{
	// <interfuscator:shuffle>
	// === front end interface ===
	virtual ~IJobManager(){}

	virtual void Init(unsigned int nSysMaxWorker) = 0;

	//! Add a job.
	virtual void AddJob(JobManager::CJobDelegator& RESTRICT_REFERENCE crJob, const JobManager::TJobHandle cJobHandle) = 0;

	//! Add a job as a lambda callback.
	virtual void AddLambdaJob(const char* jobName, const std::function<void()>& lambdaCallback, TPriorityLevel priority = JobManager::eRegularPriority, SJobState* pJobState = nullptr) = 0;

	//! Wait for a job, preempt the calling thread if the job is not done yet.
	virtual const bool WaitForJob(JobManager::SJobState& rJobState) const = 0;

	//! Obtain job handle from name.
	virtual const JobManager::TJobHandle GetJobHandle(const char* cpJobName, const unsigned int cStrLen, JobManager::Invoker pInvoker) = 0;

	//! Obtain job handle from name.
	virtual const JobManager::TJobHandle GetJobHandle(const char* cpJobName, JobManager::Invoker pInvoker) = 0;

	//! Shut down the job manager.
	virtual void                           ShutDown() = 0;

	virtual JobManager::IBackend*          GetBackEnd(JobManager::EBackEndType backEndType) = 0;

	virtual bool                           InvokeAsJob(const char* cJobHandle) const = 0;
	virtual bool                           InvokeAsJob(const JobManager::TJobHandle cJobHandle) const = 0;

	virtual void                           SetJobFilter(const char* pFilter) = 0;
	virtual void                           SetJobSystemEnabled(int nEnable) = 0;

	virtual unsigned int                         GetWorkerThreadId() const = 0;

	virtual JobManager::SJobProfilingData* GetProfilingData(unsigned short nProfilerIndex) = 0;
	virtual unsigned short                         ReserveProfilingData() = 0;

	virtual void                           Update(int nJobSystemProfiler) = 0;
	virtual void                           PushProfilingMarker(const char* pName) = 0;
	virtual void                           PopProfilingMarker() = 0;

	virtual unsigned int                         GetNumWorkerThreads() const = 0;

	//! Get a free semaphore handle from the Job Manager pool.
	virtual JobManager::TSemaphoreHandle AllocateSemaphore(volatile const void* pOwner) = 0;

	//! Return a semaphore handle to the Job Manager pool.
	virtual void DeallocateSemaphore(JobManager::TSemaphoreHandle nSemaphoreHandle, volatile const void* pOwner) = 0;

	//! Increase the refcounter of a semaphore, but only if it is > 0, else returns false.
	virtual bool AddRefSemaphore(JobManager::TSemaphoreHandle nSemaphoreHandle, volatile const void* pOwner) = 0;

	//! Get the actual semaphore object for aquire/release calls.
	virtual SJobFinishedConditionVariable* GetSemaphore(JobManager::TSemaphoreHandle nSemaphoreHandle, volatile const void* pOwner) = 0;

	virtual void                           DumpJobList() = 0;

	virtual void                           SetFrameStartTime(const CTimeValue& rFrameStartTime) = 0;
};
extern "C" JobManager::IJobManager* GetJobManagerInterface();
//! Utility function to get the worker thread id in a job, returns 0xFFFFFFFF otherwise.
inline unsigned int GetWorkerThreadId()
{
	unsigned int nWorkerThreadID = GetJobManagerInterface()->GetWorkerThreadId(); 
	return nWorkerThreadID == ~0 ? ~0 : (nWorkerThreadID & ~0x40000000);
}

//! Utility function to find out if a call comes from the mainthread or from a worker thread.
inline bool IsWorkerThread()
{
	return GetJobManagerInterface()->GetWorkerThreadId() != ~0;
}

//! Utility function to find out if a call comes from the mainthread or from a worker thread.
inline bool IsBlockingWorkerThread()
{
	return (GetJobManagerInterface()->GetWorkerThreadId() != ~0) && ((GetJobManagerInterface()->GetWorkerThreadId() & 0x40000000) != 0);
}

//! Utility function to check if a specific job should really run as job.
inline bool InvokeAsJob(const char* pJobName)
{
#if defined(_RELEASE)
	return true;      // In release mode, always assume that the job should be executed.
#else
	return GetJobManagerInterface()->InvokeAsJob(pJobName);
#endif
}

/////////////////////////////////////////////////////////////////////////////
inline void SJobState::SetRunning()
{
	SJobStateBase::SetRunning();
}

/////////////////////////////////////////////////////////////////////////////
inline void SJobState::AddPostJob()
{
	// Start post job if set.
	if (m_pFollowUpJob)
		m_pFollowUpJob->Run();
}

//////////////////////////////////////////////////////////////////////////
inline void SJobStateLambda::AddPostJob()
{
	if (m_callback)
	{
		// Add post job callback before trying to stop and releasing the semaphore
		GetJobManagerInterface()->AddLambdaJob(m_callbackJobName, m_callback, m_callbackJobPriority, m_callbackJobState);   // Use same job state for post job.
	}
}

/////////////////////////////////////////////////////////////////////////////
inline const bool SJobState::Wait()
{
	return GetJobManagerInterface()->WaitForJob(*this);
}

//! Interface of the Producer/Consumer Queue for JobManager.
//! Producer - consumer queue.
//! - All implemented inline using a template:.
//!     - Ring buffer size (num of elements).
//!     - Instance type of job.
//!     - Param type of job.
//! - Factory with macro instantiating the queue (knowing the exact names for a job).
//! - Queue consists of:.
//!     - Ring buffer consists of 1 instance of param type of job.
//!     - For atomic test on finished and push pointer update, the queue is 128 byte aligned and push, pull ptr and DMA job state are lying within that first 128 byte.
//!     - Volatile push (only modified by producer) /pull (only modified by consumer) pointer, point to ring buffer, both equal in the beginning.
//!     - Job instance (create with def. ctor).
//!     - DMA job state (running, finished).
//!     - AddPacket - method to add a packet.
//!         - Wait on current push/pull if any space is available.
//!         - Need atomically test if a job is running and update the push pointer, if job is finished, start new job.
//!     - Finished method, returns push==pull.
//! Job producer side:
//!     - provide RegisterProdConsumerQueue - method, set flags accordingly.
//! Job side:
//!     - Check if it has  a prod/consumer queue.
//!     - If it is part of queue, it must obtain DMA address for job state and of push/pull (static offset to pointer).
//!     - A flag tells if job state is job state or queue.
//!     - Lock is obtained when current push/pull ptr is obtained, snooping therefore enabled.
//!     - Before FlushCacheComplete, get next parameter packet if multiple packets needs to be processed.
//!     - If all packets were processed, try to write updated pull pointer and set finished state. If it fails, push pointer was updated during processing time, in that case get lock again and with it the new push pointer.
//!     - loop till the lock and finished state was updated successfully.
//!     - no HandleCallback method, only 1 JOb is permitted to run with queue.
//!     - no write back to external job state, always just one inner packet loop.
struct SProdConsQueueBase
{
	// Don't move the state and push ptr to another place in this struct - they are updated together with an atomic operation.
	SJobSyncVariable               m_QueueRunningState;     //!< Waiting state of the queue.
	void*                          m_pPush;                 //!< Push pointer, current ptr to push packets into (written by Producer.

	volatile void*                 m_pPull;                 //!< Pull pointer, current ptr to pull packets from (written by Consumer).
	_declspec(align(16)) unsigned int m_PullIncrement;                   //!< Increment of pull.
	unsigned int                         m_AddPacketDataOffset;   //!< Offset of additional data relative to push ptr.
	INT_PTR                        m_RingBufferStart;       //!< Start of ring buffer.
	INT_PTR                        m_RingBufferEnd;         //!< End of ring buffer.
	SJobFinishedConditionVariable* m_pQueueFullSemaphore;   //!< Semaphore used for yield-waiting when the queue is full.

	SProdConsQueueBase();
};

//! Utility functions namespace for the producer consumer queue.
namespace ProdConsQueueBase
{
inline INT_PTR MarkPullPtrThatWeAreWaitingOnASemaphore(INT_PTR nOrgValue) { assert((nOrgValue & 1) == 0); return nOrgValue | 1; }
inline bool    IsPullPtrMarked(INT_PTR nOrgValue)                         { return (nOrgValue & 1) == 1; }
}

//template<class TJobType, unsigned int Size>
//class _declspec(align(128)) CProdConsQueue: public SProdConsQueueBase
//{
//public:
//	CProdConsQueue();
//	~CProdConsQueue();
//
//	//! Add a new parameter packet with different job type (job invocation).
//	template<class TAnotherJobType>
//	void AddPacket
//	(
//	  const typename TJobType::packet & crPacket,
//	  JobManager::TPriorityLevel nPriorityLevel,
//	  TAnotherJobType * pJobParam,
//	  bool differentJob = true
//	);
//
//	//! Adds a new parameter packet (job invocation).
//	void AddPacket
//	(
//	  const typename TJobType::packet & crPacket,
//	  JobManager::TPriorityLevel nPriorityLevel = JobManager::eRegularPriority
//	);
//
//	//! Wait til all current jobs have been finished and been processed.
//	void WaitFinished();
//
//	//! Returns true if queue is empty.
//	bool IsEmpty();
//
//private:
//	//! Initializes queue.
//	void Init(const unsigned int cPacketSize);
//
//	//! Get incremented pointer, takes care of wrapping.
//	void* const GetIncrementedPointer();
//
//	//! The ring buffer.
//	_declspec(align(128)) void* m_pRingBuffer;
//
//	//! Job instance.
//	TJobType m_JobInstance;
//	int m_Initialized;
//
//};

//! Interface to track BackEnd worker utilisation and job execution timings.
class IWorkerBackEndProfiler
{
public:
	enum EJobSortOrder
	{
		eJobSortOrder_NoSort,
		eJobSortOrder_TimeHighToLow,
		eJobSortOrder_TimeLowToHigh,
		eJobSortOrder_Lexical,
	};

public:
	typedef std::vector<JobManager::SJobFrameStats> TJobFrameStatsContainer;

public:
	virtual ~IWorkerBackEndProfiler(){; }

	virtual void Init(const unsigned short numWorkers) = 0;

	//! Update the profiler at the beginning of the sample period.
	virtual void Update() = 0;
	virtual void Update(const unsigned int curTimeSample) = 0;

	//! Register a job with the profiler.
	virtual void RegisterJob(const unsigned int jobId, const char* jobName) = 0;

	//! Record execution information for a registered job.
	virtual void RecordJob(const unsigned short profileIndex, const unsigned char workerId, const unsigned int jobId, const unsigned int runTimeMicroSec) = 0;

	//! Get worker frame stats.
	virtual void GetFrameStats(JobManager::CWorkerFrameStats& rStats) const = 0;
	virtual void GetFrameStats(TJobFrameStatsContainer& rJobStats, EJobSortOrder jobSortOrder) const = 0;
	virtual void GetFrameStats(JobManager::CWorkerFrameStats& rStats, TJobFrameStatsContainer& rJobStats, EJobSortOrder jobSortOrder) const = 0;

	//! Get worker frame stats summary.
	virtual void GetFrameStatsSummary(SWorkerFrameStatsSummary& rStats) const = 0;
	virtual void GetFrameStatsSummary(SJobFrameStatsSummary& rStats) const = 0;

	//! Returns the index of the active multi-buffered profile data.
	virtual unsigned short GetProfileIndex() const = 0;

	//! Get the number of workers tracked.
	virtual unsigned int GetNumWorkers() const = 0;

public:
	//! Returns a microsecond sample.
	static unsigned int GetTimeSample()
	{
		//panzhijie
		//return static_cast<unsigned int>(gEnv->pTimer->GetAsyncTime().GetMicroSecondsAsInt64());
		return 100;
	}
};

}// namespace JobManager

extern "C"
{
	//! Interface to create the JobManager.
	//! Needed here for calls from Producer Consumer queue.
	JobManager::IJobManager* GetJobManagerInterface();
}

//! Implementation of Producer Consumer functions.
//! Currently adding jobs to a producer/consumer queue is not supported.
//template<class TJobType, unsigned int Size>
//inline JobManager::CProdConsQueue<TJobType, Size>::~CProdConsQueue()
//{
//	if (m_pRingBuffer)
//		_aligned_free(m_pRingBuffer);
//}

///////////////////////////////////////////////////////////////////////////////
inline JobManager::SProdConsQueueBase::SProdConsQueueBase() :
	m_pPush(NULL), m_pPull(NULL), m_PullIncrement(0), m_AddPacketDataOffset(0),
	m_RingBufferStart(0), m_RingBufferEnd(0), m_pQueueFullSemaphore(NULL)
{

}

///////////////////////////////////////////////////////////////////////////////
//template<class TJobType, unsigned int Size>
//inline JobManager::CProdConsQueue<TJobType, Size>::CProdConsQueue() : m_Initialized(0), m_pRingBuffer(NULL)
//{
//	assert(Size > 2);
//	m_JobInstance.RegisterQueue(this);
//}

///////////////////////////////////////////////////////////////////////////////
//template<class TJobType, unsigned int Size>
//inline void JobManager::CProdConsQueue<TJobType, Size >::Init(const unsigned int cPacketSize)
//{
//	assert((cPacketSize & 15) == 0);
//	m_AddPacketDataOffset = cPacketSize;
//	m_PullIncrement = m_AddPacketDataOffset + sizeof(SAddPacketData);
//	m_pRingBuffer = _aligned_malloc(Size * m_PullIncrement, 128);
//
//	assert(m_pRingBuffer);
//	m_pPush = m_pRingBuffer;
//	m_pPull = m_pRingBuffer;
//	m_RingBufferStart = (INT_PTR)m_pRingBuffer;
//	m_RingBufferEnd = m_RingBufferStart + Size * m_PullIncrement;
//	m_Initialized = 1;
//	((TJobType*)&m_JobInstance)->SetParamDataSize(cPacketSize);
//	m_pQueueFullSemaphore = NULL;
//}
//
/////////////////////////////////////////////////////////////////////////////////
//template<class TJobType, unsigned int Size>
//inline void JobManager::CProdConsQueue<TJobType, Size >::WaitFinished()
//{
//	m_QueueRunningState.Wait();
//	// ensure that the pull ptr is set right
//	assert(m_pPull == m_pPush);
//
//}
//
/////////////////////////////////////////////////////////////////////////////////
//template<class TJobType, unsigned int Size>
//inline bool JobManager::CProdConsQueue<TJobType, Size >::IsEmpty()
//{
//	return (INT_PTR)m_pPush == (INT_PTR)m_pPull;
//}
//
/////////////////////////////////////////////////////////////////////////////////
//template<class TJobType, unsigned int Size>
//inline void* const JobManager::CProdConsQueue<TJobType, Size >::GetIncrementedPointer()
//{
//	//returns branch free the incremented wrapped aware param pointer
//	INT_PTR cNextPtr = (INT_PTR)m_pPush + m_PullIncrement;
//#if ANGELICA_PLATFORM_64BIT
//	if ((INT_PTR)cNextPtr >= (INT_PTR)m_RingBufferEnd) cNextPtr = (INT_PTR)m_RingBufferStart;
//	return (void*)cNextPtr;
//#else
//	const unsigned int cNextPtrMask = (unsigned int)(((int)(cNextPtr - m_RingBufferEnd)) >> 31);
//	return (void*)(cNextPtr & cNextPtrMask | m_RingBufferStart & ~cNextPtrMask);
//#endif
//}
//
/////////////////////////////////////////////////////////////////////////////////
//template<class TJobType, unsigned int Size>
//inline void JobManager::CProdConsQueue<TJobType, Size >::AddPacket
//(
//  const typename TJobType::packet& crPacket,
//  JobManager::TPriorityLevel nPriorityLevel
//)
//{
//	AddPacket<TJobType>(crPacket, nPriorityLevel, (TJobType*)&m_JobInstance, false);
//}

///////////////////////////////////////////////////////////////////////////////
//template<class TJobType, unsigned int Size>
//template<class TAnotherJobType>
//inline void JobManager::CProdConsQueue<TJobType, Size >::AddPacket
//(
//  const typename TJobType::packet& crPacket,
//  JobManager::TPriorityLevel nPriorityLevel,
//  TAnotherJobType* pJobParam,
//  bool differentJob
//)
//{
//	const unsigned int cPacketSize = crPacket.GetPacketSize();
//	IF (m_Initialized == 0,0)
//		Init(cPacketSize);
//
//	assert(m_RingBufferEnd == m_RingBufferStart + Size * (cPacketSize + sizeof(SAddPacketData)));
//
//	const void* const cpCurPush = m_pPush;
//
//	// don't overtake the pull ptr
//
//	SJobSyncVariable curQueueRunningState = m_QueueRunningState;
//	IF ((cpCurPush == m_pPull) && (curQueueRunningState.IsRunning()), 0)
//	{
//
//		INT_PTR nPushPtr = (INT_PTR)cpCurPush;
//		INT_PTR markedPushPtr = JobManager::ProdConsQueueBase::MarkPullPtrThatWeAreWaitingOnASemaphore(nPushPtr);
//		TSemaphoreHandle nSemaphoreHandle = GetJobManagerInterface()->AllocateSemaphore(this);
//		m_pQueueFullSemaphore = GetJobManagerInterface()->GetSemaphore(nSemaphoreHandle, this);
//
//		bool bNeedSemaphoreWait = false;
//#if ANGELICA_PLATFORM_64BIT // for 64 bit, we need to atomicly swap 128 bit
//		long long compareValue[2] = { *alias_cast<long long*>(&curQueueRunningState), (long long)nPushPtr };
//		AngelicaInterlockedCompareExchange128((volatile long long*)this, (long long)markedPushPtr, *alias_cast<long long*>(&curQueueRunningState), compareValue);
//		// make sure nobody set the state to stopped in the meantime
//		bNeedSemaphoreWait = (compareValue[0] == *alias_cast<long long*>(&curQueueRunningState) && compareValue[1] == (long long)nPushPtr);
//#else
//		// union used to construct 64 bit value for atomic updates
//		union T64BitValue
//		{
//			long long doubleWord;
//			struct
//			{
//				unsigned int word0;
//				unsigned int word1;
//			};
//		};
//
//		// job system is process the queue right now, we need an atomic update
//		T64BitValue compareValue;
//		compareValue.word0 = *alias_cast<unsigned int*>(&curQueueRunningState);
//		compareValue.word1 = (unsigned int)nPushPtr;
//
//		T64BitValue exchangeValue;
//		exchangeValue.word0 = *alias_cast<unsigned int*>(&curQueueRunningState);
//		exchangeValue.word1 = (unsigned int)markedPushPtr;
//
//		T64BitValue resultValue;
//		resultValue.doubleWord = AngelicaInterlockedCompareExchange64((volatile long long*)this, exchangeValue.doubleWord, compareValue.doubleWord);
//
//		bNeedSemaphoreWait = (resultValue.word0 == *alias_cast<unsigned int*>(&curQueueRunningState) && resultValue.word1 == nPushPtr);
//#endif
//
//		// C-A-S successful, now we need to do a yield-wait
//		IF (bNeedSemaphoreWait, 0)
//			m_pQueueFullSemaphore->Acquire();
//		else
//			m_pQueueFullSemaphore->SetStopped();
//
//		GetJobManagerInterface()->DeallocateSemaphore(nSemaphoreHandle, this);
//		m_pQueueFullSemaphore = NULL;
//	}
//
//	if (crPacket.GetJobStateAddress())
//	{
//		JobManager::SJobState* pJobState = reinterpret_cast<JobManager::SJobState*>(crPacket.GetJobStateAddress());
//		pJobState->SetRunning();
//	}
//
//	//get incremented push pointer and check if there is a slot to push it into
//	void* const cpNextPushPtr = GetIncrementedPointer();
//
//	const SVEC4_UINT* __restrict pPacketCont = crPacket.GetPacketCont();
//	SVEC4_UINT* __restrict pPushCont = (SVEC4_UINT*)cpCurPush;
//
//	//copy packet data
//	const unsigned int cIters = cPacketSize >> 4;
//	for (unsigned int i = 0; i < cIters; ++i)
//		pPushCont[i] = pPacketCont[i];
//
//	// setup addpacket data for Jobs
//	SAddPacketData* const __restrict pAddPacketData = (SAddPacketData*)((unsigned char*)cpCurPush + m_AddPacketDataOffset);
//	pAddPacketData->pJobState = crPacket.GetJobStateAddress();
//
//#if defined(JOBMANAGER_SUPPORT_PROFILING)
//	SJobProfilingData* pJobProfilingData = GetJobManagerInterface()->GetProfilingData(crPacket.GetProfilerIndex());
//	pJobProfilingData->jobHandle = pJobParam->GetJobProgramData();
//	pAddPacketData->profilerIndex = crPacket.GetProfilerIndex();
//	if (pAddPacketData->pJobState) // also store profilerindex in syncvar, so be able to record wait times
//	{
//		pAddPacketData->pJobState->nProfilerIndex = pAddPacketData->profilerIndex;
//	}
//#endif
//
//	// set invoker, for the case the the job changes within the queue
//	pAddPacketData->nInvokerIndex = pJobParam->GetProgramHandle()->nJobInvokerIdx;
//
//	// new job queue, or empty job queue
//	if (!m_QueueRunningState.IsRunning())
//	{
//		m_pPush = cpNextPushPtr;//make visible
//		m_QueueRunningState.SetRunning();
//
//		pJobParam->RegisterQueue(this);
//		pJobParam->SetParamDataSize(((TJobType*)&m_JobInstance)->GetParamDataSize());
//		pJobParam->SetPriorityLevel(nPriorityLevel);
//		pJobParam->Run();
//
//		return;
//	}
//
//	bool bAtomicSwapSuccessfull = false;
//	JobManager::SJobSyncVariable newSyncVar;
//	newSyncVar.SetRunning();
//#if ANGELICA_PLATFORM_64BIT // for 64 bit, we need to atomicly swap 128 bit
//	long long compareValue[2] = { *alias_cast<long long*>(&newSyncVar), (long long)cpCurPush };
//	AngelicaInterlockedCompareExchange128((volatile long long*)this, (long long)cpNextPushPtr, *alias_cast<long long*>(&newSyncVar), compareValue);
//	// make sure nobody set the state to stopped in the meantime
//	bAtomicSwapSuccessfull = (compareValue[0] > 0);
//#else
//	// union used to construct 64 bit value for atomic updates
//	union T64BitValue
//	{
//		long long doubleWord;
//		struct
//		{
//			unsigned int word0;
//			unsigned int word1;
//		};
//	};
//
//	// job system is process the queue right now, we need an atomic update
//	T64BitValue compareValue;
//	compareValue.word0 = *alias_cast<unsigned int*>(&newSyncVar);
//	compareValue.word1 = (unsigned int)(TRUNCATE_PTR)cpCurPush;
//
//	T64BitValue exchangeValue;
//	exchangeValue.word0 = *alias_cast<unsigned int*>(&newSyncVar);
//	exchangeValue.word1 = (unsigned int)(TRUNCATE_PTR)cpNextPushPtr;
//
//	T64BitValue resultValue;
//	resultValue.doubleWord = AngelicaInterlockedCompareExchange64((volatile long long*)this, exchangeValue.doubleWord, compareValue.doubleWord);
//
//	bAtomicSwapSuccessfull = (resultValue.word0 > 0);
//#endif
//
//	// job system finished in the meantime, next to issue a new job
//	if (!bAtomicSwapSuccessfull)
//	{
//		m_pPush = cpNextPushPtr;//make visible
//		m_QueueRunningState.SetRunning();
//
//		pJobParam->RegisterQueue(this);
//		pJobParam->SetParamDataSize(((TJobType*)&m_JobInstance)->GetParamDataSize());
//		pJobParam->SetPriorityLevel(nPriorityLevel);
//		pJobParam->Run();
//	}
//}

//! CCommonDMABase Function implementations.
inline JobManager::CCommonDMABase::CCommonDMABase()
{
	ForceUpdateOfProfilingDataIndex();
}

///////////////////////////////////////////////////////////////////////////////
inline void JobManager::CCommonDMABase::ForceUpdateOfProfilingDataIndex()
{
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	nProfilerIndex = GetJobManagerInterface()->ReserveProfilingData();
#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void JobManager::CCommonDMABase::SetJobParamData(void* pParamData)
{
	m_pParamData = pParamData;
}

///////////////////////////////////////////////////////////////////////////////
inline const void* JobManager::CCommonDMABase::GetJobParamData() const
{
	return m_pParamData;
}

///////////////////////////////////////////////////////////////////////////////
inline unsigned short JobManager::CCommonDMABase::GetProfilingDataIndex() const
{
	return nProfilerIndex;
}

//! Job Delegator Function implementations.
inline JobManager::CJobDelegator::CJobDelegator() : m_ParamDataSize(0), m_CurThreadID(THREADID_NULL)
{
	m_pQueue = NULL;
	m_pJobState = NULL;
	m_nPrioritylevel = JobManager::eRegularPriority;
	m_bIsBlocking = false;
}

///////////////////////////////////////////////////////////////////////////////
inline void JobManager::CJobDelegator::RunJob(const JobManager::TJobHandle cJobHandle)
{
	GetJobManagerInterface()->AddJob(*static_cast<CJobDelegator*>(this), cJobHandle);
}

///////////////////////////////////////////////////////////////////////////////
inline void JobManager::CJobDelegator::RegisterQueue(const JobManager::SProdConsQueueBase* const cpQueue)
{
	m_pQueue = cpQueue;
}

///////////////////////////////////////////////////////////////////////////////
inline JobManager::SProdConsQueueBase* JobManager::CJobDelegator::GetQueue() const
{
	return const_cast<JobManager::SProdConsQueueBase*>(m_pQueue);
}

///////////////////////////////////////////////////////////////////////////////
inline void JobManager::CJobDelegator::RegisterJobState(JobManager::SJobState* __restrict pJobState)
{
	assert(pJobState);
	m_pJobState = pJobState;
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	pJobState->nProfilerIndex = this->GetProfilingDataIndex();
#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void JobManager::CJobDelegator::SetParamDataSize(const unsigned int cParamDataSize)
{
	m_ParamDataSize = cParamDataSize;
}

///////////////////////////////////////////////////////////////////////////////
inline const unsigned int JobManager::CJobDelegator::GetParamDataSize() const
{
	return m_ParamDataSize;
}

///////////////////////////////////////////////////////////////////////////////
inline void JobManager::CJobDelegator::SetCurrentThreadId(const unsigned long cID)
{
	m_CurThreadID = cID;
}

///////////////////////////////////////////////////////////////////////////////
inline const unsigned long JobManager::CJobDelegator::GetCurrentThreadId() const
{
	return m_CurThreadID;
}

///////////////////////////////////////////////////////////////////////////////
inline JobManager::SJobState* JobManager::CJobDelegator::GetJobState() const
{
	return m_pJobState;
}

///////////////////////////////////////////////////////////////////////////////
inline void JobManager::CJobDelegator::SetRunning()
{
	m_pJobState->SetRunning();
}

//! Implementation of SJObSyncVariable functions.
inline JobManager::SJobSyncVariable::SJobSyncVariable()
{
	syncVar.wordValue = 0;
#if ANGELICA_PLATFORM_64BIT
	padding[0] = padding[1] = padding[2] = padding[3] = 0;
#endif
}

/////////////////////////////////////////////////////////////////////////////////
inline bool JobManager::SJobSyncVariable::IsRunning() const volatile
{
	return syncVar.nRunningCounter != 0;
}

/////////////////////////////////////////////////////////////////////////////////
inline void JobManager::SJobSyncVariable::Wait() volatile
{
	if (syncVar.wordValue == 0)
		return;

	IJobManager* pJobManager = GetJobManagerInterface();

	SyncVar currentValue;
	SyncVar newValue;
	SyncVar resValue;
	TSemaphoreHandle semaphoreHandle = GetJobManagerInterface()->AllocateSemaphore(this);

retry:
	// volatile read
	currentValue.wordValue = syncVar.wordValue;

	if (currentValue.wordValue != 0)
	{
		// there is already a semaphore, wait for this one
		if (currentValue.semaphoreHandle)
		{
			// prevent the following race condition: Thread A Gets the semaphore and waits, then Thread B fails the earlier C-A-S
			// thus will wait for the same semaphore, but Thread A already release the semaphore and now another Thread (could even be A again)
			// is using the same semaphore to wait for the job (executed by thread B) which waits for the samephore -> Deadlock
			// this works since the jobmanager semphore function internally are using locks
			// so if the following line succeeds, we got the lock before the release and have increased the use counter
			// if not, it means that thread A release the semaphore, which also means thread B doesn't have to wait anymore

			if (pJobManager->AddRefSemaphore(currentValue.semaphoreHandle, this))
			{
				pJobManager->GetSemaphore(currentValue.semaphoreHandle, this)->Acquire();
				pJobManager->DeallocateSemaphore(currentValue.semaphoreHandle, this);
			}
		}
		else // no semaphore found
		{
			newValue = currentValue;
			newValue.semaphoreHandle = semaphoreHandle;
			resValue.wordValue = AngelicaInterlockedCompareExchange((volatile LONG*)&syncVar.wordValue, newValue.wordValue, currentValue.wordValue);

			// four case are now possible:
			//a) job has finished -> we only need to free our semaphore
			//b) we succeeded setting our semaphore, thus wait for it
			//c) another waiter has set it's sempahore wait for the other one
			//d) another thread has increased/decreased the num runner variable, we need to do the run again

			if (resValue.wordValue != 0) // case a), do nothing
			{
				if (resValue.wordValue == currentValue.wordValue) // case b)
				{
					pJobManager->GetSemaphore(semaphoreHandle, this)->Acquire();
				}
				else // C-A-S not succeeded, check how to proceed
				{
					if (resValue.semaphoreHandle) // case c)
					{
						// prevent the following race condition: Thread A Gets the semaphore and waits, then Thread B fails the earlier C-A-S
						// thus will wait for the same semaphore, but Thread A already release the semaphore and now another Thread (could even be A again)
						// is using the same semaphore to wait for the job (executed by thread B) which waits for the samephore -> Deadlock
						// this works since the jobmanager semphore function internally are using locks
						// so if the following line succeeds, we got the lock before the release and have increased the use counter
						// if not, it means that thread A release the semaphore, which also means thread B doesn't have to wait anymore

						if (pJobManager->AddRefSemaphore(resValue.semaphoreHandle, this))
						{
							pJobManager->GetSemaphore(resValue.semaphoreHandle, this)->Acquire();
							pJobManager->DeallocateSemaphore(resValue.semaphoreHandle, this);
						}
					}
					else // case d
					{
						goto retry;
					}
				}
			}
		}
	}

	// mark an old semaphore as stopped, in case we didn't use use	(if we did use it, this is a nop)
	pJobManager->GetSemaphore(semaphoreHandle, this)->SetStopped();
	pJobManager->DeallocateSemaphore(semaphoreHandle, this);
}

/////////////////////////////////////////////////////////////////////////////////
inline void JobManager::SJobSyncVariable::SetRunning() volatile
{
	SyncVar currentValue;
	SyncVar newValue;

	do
	{
		// volatile read
		currentValue.wordValue = syncVar.wordValue;

		newValue = currentValue;
		newValue.nRunningCounter += 1;

		if (newValue.nRunningCounter == 0)
		{
			//ANGELICA_ASSERT_MESSAGE(0, "JobManager: Atomic counter overflow");
		}

	}
	while (AngelicaInterlockedCompareExchange((volatile LONG*)&syncVar.wordValue, newValue.wordValue, currentValue.wordValue) != currentValue.wordValue);
}

/////////////////////////////////////////////////////////////////////////////////
inline bool JobManager::SJobSyncVariable::SetStopped(SJobStateBase* pPostCallback) volatile
{
	SyncVar currentValue;
	SyncVar newValue;
	SyncVar resValue;

	// first use atomic operations to decrease the running counter
	do
	{
		// volatile read
		currentValue.wordValue = syncVar.wordValue;

		if (currentValue.nRunningCounter == 0)
		{
			//ANGELICA_ASSERT_MESSAGE(0, "JobManager: Atomic counter underflow");
			newValue.nRunningCounter = 1; // Force for potential stability problem, should not happen.
		}

		newValue = currentValue;
		newValue.nRunningCounter -= 1;

		resValue.wordValue = AngelicaInterlockedCompareExchange((volatile LONG*)&syncVar.wordValue, newValue.wordValue, currentValue.wordValue);

	}
	while (resValue.wordValue != currentValue.wordValue);

	// we now successfully decreased our counter, check if we need to signal a semaphore
	if (newValue.nRunningCounter > 0) // return if we still have other jobs on this syncvar
		return false;

	// Post job needs to be added before releasing semaphore of the current job, to allow chain of post jobs to be waited on.
	if (pPostCallback)
		pPostCallback->AddPostJob();

	//! Do we need to release a semaphore?
	if (currentValue.semaphoreHandle)
	{
		// try to set the running counter to 0 atomically
		bool bNeedSemaphoreRelease = true;
		do
		{
			// volatile read
			currentValue.wordValue = syncVar.wordValue;
			newValue = currentValue;
			newValue.semaphoreHandle = 0;

			// another thread increased the running counter again, don't call semaphore release in this case
			if (currentValue.nRunningCounter)
				return false;

		}
		while (AngelicaInterlockedCompareExchange((volatile LONG*)&syncVar.wordValue, newValue.wordValue, currentValue.wordValue) != currentValue.wordValue);
		// set the running successfull to 0, now we can release the semaphore
		GetJobManagerInterface()->GetSemaphore(resValue.semaphoreHandle, this)->Release();
	}

	return true;
}

// SInfoBlock State functions.

inline void JobManager::SInfoBlock::Release(unsigned int nMaxValue)
{
	JobManager::IJobManager* pJobManager = GetJobManagerInterface();

	SInfoBlockState currentInfoBlockState;
	SInfoBlockState newInfoBlockState;
	SInfoBlockState resultInfoBlockState;

	// use atomic operations to set the running flag
	do
	{
		// volatile read
		currentInfoBlockState.nValue = *(const_cast<volatile LONG*>(&jobState.nValue));

		newInfoBlockState.nSemaphoreHandle = 0;
		newInfoBlockState.nRoundID = (currentInfoBlockState.nRoundID + 1) & 0x7FFF;
		newInfoBlockState.nRoundID = newInfoBlockState.nRoundID >= nMaxValue ? 0 : newInfoBlockState.nRoundID;

		resultInfoBlockState.nValue = AngelicaInterlockedCompareExchange((volatile LONG*)&jobState.nValue, newInfoBlockState.nValue, currentInfoBlockState.nValue);
	}
	while (resultInfoBlockState.nValue != currentInfoBlockState.nValue);

	// do we need to release a semaphore
	if (currentInfoBlockState.nSemaphoreHandle)
		GetJobManagerInterface()->GetSemaphore(currentInfoBlockState.nSemaphoreHandle, this)->Release();

	if (jobLambdaInvoker)
	{
		std::function<void()> empty;
		jobLambdaInvoker.swap(empty);
	}
}

/////////////////////////////////////////////////////////////////////////////////
inline void JobManager::SInfoBlock::Wait(unsigned int nRoundID, unsigned int nMaxValue)
{
	bool bWait = false;
	bool bRetry = false;
	jobState.IsInUse(nRoundID, bWait, bRetry, nMaxValue);
	if (!bWait)
		return;

	JobManager::IJobManager* pJobManager = GetJobManagerInterface();

	// get a semaphore to wait on
	TSemaphoreHandle semaphoreHandle = pJobManager->AllocateSemaphore(this);

	// try to set semaphore (info block could have finished in the meantime, or another thread has set the semaphore
	SInfoBlockState currentInfoBlockState;
	SInfoBlockState newInfoBlockState;
	SInfoBlockState resultInfoBlockState;

	currentInfoBlockState.nValue = *(const_cast<volatile LONG*>(&jobState.nValue));

	currentInfoBlockState.IsInUse(nRoundID, bWait, bRetry, nMaxValue);
	if (bWait)
	{
		// is a semaphore already set
		if (currentInfoBlockState.nSemaphoreHandle != 0)
		{
			if (pJobManager->AddRefSemaphore(currentInfoBlockState.nSemaphoreHandle, this))
			{
				pJobManager->GetSemaphore(currentInfoBlockState.nSemaphoreHandle, this)->Acquire();
				pJobManager->DeallocateSemaphore(currentInfoBlockState.nSemaphoreHandle, this);
			}
		}
		else
		{
			// try to set the semaphore
			newInfoBlockState.nRoundID = currentInfoBlockState.nRoundID;
			newInfoBlockState.nSemaphoreHandle = semaphoreHandle;

			resultInfoBlockState.nValue = AngelicaInterlockedCompareExchange((volatile LONG*)&jobState.nValue, newInfoBlockState.nValue, currentInfoBlockState.nValue);

			// three case are now possible:
			//a) job has finished -> we only need to free our semaphore
			//b) we succeeded setting our semaphore, thus wait for it
			//c) another waiter has set it's sempahore wait for the other one
			resultInfoBlockState.IsInUse(nRoundID, bWait, bRetry, nMaxValue);
			if (bWait == true)  // case a) do nothing
			{
				if (resultInfoBlockState.nValue == currentInfoBlockState.nValue) // case b)
				{
					pJobManager->GetSemaphore(semaphoreHandle, this)->Acquire();
				}
				else // case c)
				{
					if (pJobManager->AddRefSemaphore(resultInfoBlockState.nSemaphoreHandle, this))
					{
						pJobManager->GetSemaphore(resultInfoBlockState.nSemaphoreHandle, this)->Acquire();
						pJobManager->DeallocateSemaphore(resultInfoBlockState.nSemaphoreHandle, this);
					}
				}
			}
		}
	}

	pJobManager->GetSemaphore(semaphoreHandle, this)->SetStopped();
	pJobManager->DeallocateSemaphore(semaphoreHandle, this);
}

//! Global helper function to wait for a job.
//! Wait for a job, preempt the calling thread if the job is not done yet.
inline const bool AngelicaWaitForJob(JobManager::SJobState& rJobState)
{
	return GetJobManagerInterface()->WaitForJob(rJobState);
}

// Shorter helper type for job states
typedef JobManager::SJobState       AngelicaJobState;
typedef JobManager::SJobStateLambda AngelicaJobStateLambda;
