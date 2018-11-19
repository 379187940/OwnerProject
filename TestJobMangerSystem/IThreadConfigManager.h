// Copyright 2001-2017 Angelicatek GmbH / Angelicatek Group. All rights reserved. 

#pragma once
#define BIT(x)    (1u << (x))
#define BIT64(x)  (1ull << (x))
#define MASK(x)   (BIT(x) - 1U)
#define MASK64(x) (BIT64(x) - 1ULL)
struct SThreadConfig
{
	enum eThreadParamFlag
	{
		eThreadParamFlag_ThreadName    = BIT(0),
		eThreadParamFlag_StackSize     = BIT(1),
		eThreadParamFlag_Affinity      = BIT(2),
		eThreadParamFlag_Priority      = BIT(3),
		eThreadParamFlag_PriorityBoost = BIT(4),
	};

	typedef unsigned int TThreadParamFlag;

	const char*      szThreadName;
	unsigned int           stackSizeBytes;
	unsigned int           affinityFlag;
	int            priority;
	bool             bDisablePriorityBoost;

	TThreadParamFlag paramActivityFlag;
};

class IThreadConfigManager
{
public:
	virtual ~IThreadConfigManager()
	{
	}

	//! Called once during System startup.
	//! Loads the thread configuration for the executing platform from file.
	virtual bool LoadConfig(const char* pcPath) = 0;

	//! Returns true if a config has been loaded.
	virtual bool ConfigLoaded() const = 0;

	//! Gets the thread configuration for the specified thread on the active platform.
	//! If no matching config is found a default configuration is returned (which does not have the same name as the search string).
	virtual const SThreadConfig* GetThreadConfig(const char* sThreadName, ...) = 0;
	virtual const SThreadConfig* GetDefaultThreadConfig() const = 0;

	//! Dump a detailed description of the thread startup configurations for this platform to the log file.
	virtual void DumpThreadConfigurationsToLog() = 0;
};
