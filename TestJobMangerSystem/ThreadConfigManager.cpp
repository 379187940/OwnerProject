// Copyright 2001-2017 Angelicatek GmbH / Angelicatek Group. All rights reserved. 

#include "StdAfx.h"
#include "ThreadConfigManager.h"

namespace
{
const char* sCurThreadConfigFilename = "";
const unsigned int sPlausibleStackSizeLimitKB = (1024 * 100); // 100mb
}

//////////////////////////////////////////////////////////////////////////
CThreadConfigManager::CThreadConfigManager()
{
	m_defaultConfig.szThreadName = "AngelicaThread_Unnamed";
	m_defaultConfig.stackSizeBytes = 0;
	m_defaultConfig.affinityFlag = -1;
	m_defaultConfig.priority = THREAD_PRIORITY_NORMAL;
	m_defaultConfig.bDisablePriorityBoost = false;
	m_defaultConfig.paramActivityFlag = (SThreadConfig::TThreadParamFlag)~0;
}

//////////////////////////////////////////////////////////////////////////
const SThreadConfig* CThreadConfigManager::GetThreadConfig(const char* szThreadName, ...)
{
	return &m_defaultConfig;
}

//////////////////////////////////////////////////////////////////////////
//const SThreadConfig* CThreadConfigManager::GetThreadConfigImpl(const char* szThreadName)
//{
//	// Get thread config for platform
//	return &m_defaultConfig;
//}

//////////////////////////////////////////////////////////////////////////
const SThreadConfig* CThreadConfigManager::GetDefaultThreadConfig() const
{
	return &m_defaultConfig;
}

//////////////////////////////////////////////////////////////////////////
bool CThreadConfigManager::LoadConfig(const char* pcPath)
{
	
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CThreadConfigManager::ConfigLoaded() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////
//bool CThreadConfigManager::LoadPlatformConfig(const XmlNodeRef& rXmlRoot, const char* sPlatformId)
//{
//
//	// Validate node
//	if (!rXmlRoot->isTag("ThreadConfig"))
//	{
//		AngelicaWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: Unable to find root xml node \"ThreadConfig\"");
//		return false;
//	}
//
//	// Find active platform
//	const unsigned int numPlatforms = rXmlRoot->getChildCount();
//	for (unsigned int i = 0; i < numPlatforms; ++i)
//	{
//		const XmlNodeRef xmlPlatformNode = rXmlRoot->getChild(i);
//
//		// Is platform node
//		if (!xmlPlatformNode->isTag("Platform"))
//		{
//			continue;
//		}
//
//		// Is has Name attribute
//		if (!xmlPlatformNode->haveAttr("Name"))
//		{
//			continue;
//		}
//
//		// Is platform of interest
//		const char* platformName = xmlPlatformNode->getAttr("Name");
//		if (_stricmp(sPlatformId, platformName) == 0)
//		{
//			// Load platform
//			LoadThreadDefaultConfig(xmlPlatformNode);
//			LoadPlatformThreadConfigs(xmlPlatformNode);
//			return true;
//		}
//	}
//
//	return false;
//}
//
////////////////////////////////////////////////////////////////////////////
//void CThreadConfigManager::LoadPlatformThreadConfigs(const XmlNodeRef& rXmlPlatformRef)
//{
//	// Get thread configurations for active platform
//	const unsigned int numThreads = rXmlPlatformRef->getChildCount();
//	for (unsigned int j = 0; j < numThreads; ++j)
//	{
//		const XmlNodeRef xmlThreadNode = rXmlPlatformRef->getChild(j);
//
//		if (!xmlThreadNode->isTag("Thread"))
//		{
//			continue;
//		}
//
//		// Ensure thread config has name
//		if (!xmlThreadNode->haveAttr("Name"))
//		{
//			AngelicaWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Thread node without \"name\" attribute encountered.");
//			continue;
//		}
//
//		// Load thread config
//		SThreadConfig loadedThreadConfig = SThreadConfig(m_defaultConfig);
//		LoadThreadConfig(xmlThreadNode, loadedThreadConfig);
//
//		// Get thread name and check if it contains wildcard characters
//		const char* szThreadName = xmlThreadNode->getAttr("Name");
//		bool bWildCard = strchr(szThreadName, '*') ? true : false;
//		ThreadConfigMap& threadConfig = bWildCard ? m_wildcardThreadConfig : m_threadConfig;
//
//		// Check for duplicate and override it with new config if found
//		if (threadConfig.find(szThreadName) != threadConfig.end())
//		{
//			//AngelicaLogAlways("<ThreadConfigInfo>: [XML Parsing] Thread with name \"%s\" already loaded. Overriding with new configuration", szThreadName);
//			threadConfig[szThreadName] = loadedThreadConfig;
//			continue;
//		}
//
//		// Store new thread config
//		std::pair<ThreadConfigMapIter, bool> res;
//		res = threadConfig.insert(ThreadConfigMapPair(AngelicaFixedStringT<THREAD_NAME_LENGTH_MAX>(szThreadName), loadedThreadConfig));
//
//		// Store name (ref to key)
//		SThreadConfig& rMapThreadConfig = res.first->second;
//		rMapThreadConfig.szThreadName = res.first->first.GetBuffer(0);
//	}
//}
//
////////////////////////////////////////////////////////////////////////////
//bool CThreadConfigManager::LoadThreadDefaultConfig(const XmlNodeRef& rXmlPlatformRef)
//{
//	// Find default thread config node
//	const unsigned int numNodes = rXmlPlatformRef->getChildCount();
//	for (unsigned int j = 0; j < numNodes; ++j)
//	{
//		const XmlNodeRef xmlNode = rXmlPlatformRef->getChild(j);
//
//		// Load default config
//		if (xmlNode->isTag("ThreadDefault"))
//		{
//			LoadThreadConfig(xmlNode, m_defaultConfig);
//			return true;
//		}
//	}
//	return false;
//}
//
////////////////////////////////////////////////////////////////////////////
//void CThreadConfigManager::LoadAffinity(const XmlNodeRef& rXmlThreadRef, unsigned int& rAffinity, SThreadConfig::TThreadParamFlag& rParamActivityFlag)
//{
//	const char* szValidCharacters = "-,0123456789";
//	unsigned int affinity = 0;
//
//	// Validate node
//	if (!rXmlThreadRef->haveAttr("Affinity"))
//		return;
//
//	// Validate token
//	AngelicaFixedStringT<32> affinityRawStr(rXmlThreadRef->getAttr("Affinity"));
//	if (affinityRawStr.empty())
//	{
//		AngelicaWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Empty attribute \"Affinity\" encountered");
//		return;
//	}
//
//	if (affinityRawStr.compareNoCase("ignore") == 0)
//	{
//		// Param is inactive, clear bit
//		rParamActivityFlag &= ~SThreadConfig::eThreadParamFlag_Affinity;
//		return;
//	}
//
//	AngelicaFixedStringT<32>::size_type nPos = affinityRawStr.find_first_not_of(" -,0123456789");
//	if (nPos != AngelicaFixedStringT<32>::npos)
//	{
//		AngelicaWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING,
//		           "<ThreadConfigInfo>: [XML Parsing] Invalid character \"%c\" encountered in \"Affinity\" attribute. Valid characters:\"%s\" Offending token:\"%s\"", affinityRawStr.at(nPos),
//		           szValidCharacters, affinityRawStr.GetBuffer(0));
//		return;
//	}
//
//	// Tokenize comma separated string
//	int pos = 0;
//	AngelicaFixedStringT<32> affnityTokStr = affinityRawStr.Tokenize(",", pos);
//	while (!affnityTokStr.empty())
//	{
//		affnityTokStr.Trim();
//
//		long affinityId = strtol(affnityTokStr.GetBuffer(0), NULL, 10);
//		if (affinityId == LONG_MAX || affinityId == LONG_MIN)
//		{
//			AngelicaWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Unknown value \"%s\" encountered for attribute \"Affinity\"", affnityTokStr.GetBuffer(0));
//			return;
//		}
//
//		// Allow scheduler to pick thread
//		if (affinityId == -1)
//		{
//			affinity = ~0;
//			break;
//		}
//
//		// Set affinity bit
//		affinity |= BIT(affinityId);
//
//		// Move to next token
//		affnityTokStr = affinityRawStr.Tokenize(",", pos);
//	}
//
//	// Set affinity reference
//	rAffinity = affinity;
//}
//
////////////////////////////////////////////////////////////////////////////
//void CThreadConfigManager::LoadPriority(const XmlNodeRef& rXmlThreadRef, int& rPriority, SThreadConfig::TThreadParamFlag& rParamActivityFlag)
//{
//	const char* szValidCharacters = "-,0123456789";
//
//	// Validate node
//	if (!rXmlThreadRef->haveAttr("Priority"))
//		return;
//
//	// Validate token
//	AngelicaFixedStringT<32> threadPrioStr(rXmlThreadRef->getAttr("Priority"));
//	threadPrioStr.Trim();
//	if (threadPrioStr.empty())
//	{
//		AngelicaWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Empty attribute \"Priority\" encountered");
//		return;
//	}
//
//	if (threadPrioStr.compareNoCase("ignore") == 0)
//	{
//		// Param is inactive, clear bit
//		rParamActivityFlag &= ~SThreadConfig::eThreadParamFlag_Priority;
//		return;
//	}
//
//	// Test for character string (no numbers allowed)
//	if (threadPrioStr.find_first_of(szValidCharacters) == AngelicaFixedStringT<32>::npos)
//	{
//		threadPrioStr.MakeLower();
//
//		// Set priority
//		if (threadPrioStr.compare("below_normal") == 0)
//		{
//			rPriority = THREAD_PRIORITY_BELOW_NORMAL;
//		}
//		else if (threadPrioStr.compare("normal") == 0)
//		{
//			rPriority = THREAD_PRIORITY_NORMAL;
//		}
//		else if (threadPrioStr.compare("above_normal") == 0)
//		{
//			rPriority = THREAD_PRIORITY_ABOVE_NORMAL;
//		}
//		else if (threadPrioStr.compare("idle") == 0)
//		{
//			rPriority = THREAD_PRIORITY_IDLE;
//		}
//		else if (threadPrioStr.compare("lowest") == 0)
//		{
//			rPriority = THREAD_PRIORITY_LOWEST;
//		}
//		else if (threadPrioStr.compare("highest") == 0)
//		{
//			rPriority = THREAD_PRIORITY_HIGHEST;
//		}
//		else if (threadPrioStr.compare("time_critical") == 0)
//		{
//			rPriority = THREAD_PRIORITY_TIME_CRITICAL;
//		}
//		else
//		{
//			AngelicaWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Platform unsupported value \"%s\" encountered for attribute \"Priority\"", threadPrioStr.GetBuffer(0));
//			return;
//		}
//	}
//	// Test for number string (no alphabetical characters allowed)
//	else if (threadPrioStr.find_first_not_of(szValidCharacters) == AngelicaFixedStringT<32>::npos)
//	{
//		long numValue = strtol(threadPrioStr.GetBuffer(0), NULL, 10);
//		if (numValue == LONG_MAX || numValue == LONG_MIN)
//		{
//			AngelicaWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Unsupported number type \"%s\" for for attribute \"Priority\"", threadPrioStr.GetBuffer(0));
//			return;
//		}
//
//		// Set priority
//		rPriority = numValue;
//	}
//	else
//	{
//		// String contains characters and numbers
//		AngelicaWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Unsupported type \"%s\" encountered for attribute \"Priority\". Token containers numbers and characters", threadPrioStr.GetBuffer(0));
//		return;
//	}
//}
//
////////////////////////////////////////////////////////////////////////////
//void CThreadConfigManager::LoadDisablePriorityBoost(const XmlNodeRef& rXmlThreadRef, bool& rPriorityBoost, SThreadConfig::TThreadParamFlag& rParamActivityFlag)
//{
//	const char* sValidCharacters = "-,0123456789";
//
//	// Validate node
//	if (!rXmlThreadRef->haveAttr("DisablePriorityBoost"))
//	{
//		return;
//	}
//
//	// Extract bool info
//	AngelicaFixedStringT<16> sAttribToken(rXmlThreadRef->getAttr("DisablePriorityBoost"));
//	sAttribToken.Trim();
//	sAttribToken.MakeLower();
//
//	if (sAttribToken.compare("ignore") == 0)
//	{
//		// Param is inactive, clear bit
//		rParamActivityFlag &= ~SThreadConfig::eThreadParamFlag_PriorityBoost;
//		return;
//	}
//	else if (sAttribToken.compare("true") == 0 || sAttribToken.compare("1") == 0)
//	{
//		rPriorityBoost = true;
//	}
//	else if (sAttribToken.compare("false") == 0 || sAttribToken.compare("0") == 0)
//	{
//		rPriorityBoost = false;
//	}
//	else
//	{
//		AngelicaWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Unsupported bool type \"%s\" encountered for attribute \"DisablePriorityBoost\"",
//		           rXmlThreadRef->getAttr("DisablePriorityBoost"));
//		return;
//	}
//}
//
////////////////////////////////////////////////////////////////////////////
//void CThreadConfigManager::LoadStackSize(const XmlNodeRef& rXmlThreadRef, unsigned int& rStackSize, SThreadConfig::TThreadParamFlag& rParamActivityFlag)
//{
//	const char* sValidCharacters = "0123456789";
//
//	if (rXmlThreadRef->haveAttr("StackSizeKB"))
//	{
//		int nPos = 0;
//
//		// Read stack size
//		AngelicaFixedStringT<32> stackSize(rXmlThreadRef->getAttr("StackSizeKB"));
//
//		// Validate stack size
//		if (stackSize.empty())
//		{
//			AngelicaWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Empty attribute \"StackSize\" encountered");
//			return;
//		}
//		else if (stackSize.compareNoCase("ignore") == 0)
//		{
//			// Param is inactive, clear bit
//			rParamActivityFlag &= ~SThreadConfig::eThreadParamFlag_StackSize;
//			return;
//		}
//		else if (stackSize.find_first_not_of(sValidCharacters) == AngelicaFixedStringT<32>::npos)
//		{
//			// Convert string to long
//			long stackSizeVal = strtol(stackSize.GetBuffer(0), NULL, 10);
//			if (stackSizeVal == LONG_MAX || stackSizeVal == LONG_MIN)
//			{
//				AngelicaWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Invalid number for \"StackSize\" encountered. \"%s\"", stackSize.GetBuffer(0));
//				return;
//			}
//			else if (stackSizeVal <= 0 || stackSizeVal > sPlausibleStackSizeLimitKB)
//			{
//				AngelicaWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] \"StackSize\" value not plausible \"%" PRId64 "KB\"", (signed long long)stackSizeVal);
//				return;
//			}
//
//			// Set stack size
//			rStackSize = stackSizeVal * 1024; // Convert to bytes
//		}
//	}
//}
//
////////////////////////////////////////////////////////////////////////////
//void CThreadConfigManager::LoadThreadConfig(const XmlNodeRef& rXmlThreadRef, SThreadConfig& rThreadConfig)
//{
//	LoadAffinity(rXmlThreadRef, rThreadConfig.affinityFlag, rThreadConfig.paramActivityFlag);
//	LoadPriority(rXmlThreadRef, rThreadConfig.priority, rThreadConfig.paramActivityFlag);
//	LoadDisablePriorityBoost(rXmlThreadRef, rThreadConfig.bDisablePriorityBoost, rThreadConfig.paramActivityFlag);
//	LoadStackSize(rXmlThreadRef, rThreadConfig.stackSizeBytes, rThreadConfig.paramActivityFlag);
//}
//
////////////////////////////////////////////////////////////////////////////
//const char* CThreadConfigManager::IdentifyPlatform()
//{
//#if defined(DURANGO)
//	return "durango";
//#elif defined(ORBIS)
//	return "orbis";
//	// ANDROID **needs** to be tested before LINUX because both are defined for android
//#elif defined(ANDROID)
//	return "android";
//#elif defined(LINUX)
//	return "linux";
//#elif defined(APPLE)
//	return "mac";
//#elif defined(WIN32) || defined(WIN64)
//	return "pc";
//#else
//	#error "Undefined platform"
//#endif
//}
//
////////////////////////////////////////////////////////////////////////////
//void CThreadConfigManager::DumpThreadConfigurationsToLog()
//{
//#if !defined(RELEASE)
//
//	// Print header
//	//AngelicaLogAlways("== Thread Startup Config List (\"%s\") ==", IdentifyPlatform());
//
//	// Print loaded default config
//	//AngelicaLogAlways("  (Default) 1. \"%s\" (StackSize:%uKB | Affinity:%u | Priority:%i | PriorityBoost:\"%s\")", m_defaultConfig.szThreadName, m_defaultConfig.stackSizeBytes / 1024,
//	             m_defaultConfig.affinityFlag, m_defaultConfig.priority, m_defaultConfig.bDisablePriorityBoost ? "disabled" : "enabled");
//
//	// Print loaded thread configs
//	int listItemCounter = 1;
//	ThreadConfigMapConstIter iter = m_threadConfig.begin();
//	ThreadConfigMapConstIter iterEnd = m_threadConfig.end();
//	for (; iter != iterEnd; ++iter)
//	{
//		const SThreadConfig& threadConfig = iter->second;
//		//AngelicaLogAlways("%3d.\"%s\" %s (StackSize:%uKB %s | Affinity:%u %s | Priority:%i %s | PriorityBoost:\"%s\" %s)", ++listItemCounter,
//		             threadConfig.szThreadName, (threadConfig.paramActivityFlag & SThreadConfig::eThreadParamFlag_ThreadName) ? "" : "(ignored)",
//		             threadConfig.stackSizeBytes / 1024u, (threadConfig.paramActivityFlag & SThreadConfig::eThreadParamFlag_StackSize) ? "" : "(ignored)",
//		             threadConfig.affinityFlag, (threadConfig.paramActivityFlag & SThreadConfig::eThreadParamFlag_Affinity) ? "" : "(ignored)",
//		             threadConfig.priority, (threadConfig.paramActivityFlag & SThreadConfig::eThreadParamFlag_Priority) ? "" : "(ignored)",
//		             !threadConfig.bDisablePriorityBoost ? "enabled" : "disabled", (threadConfig.paramActivityFlag & SThreadConfig::eThreadParamFlag_PriorityBoost) ? "" : "(ignored)");
//	}
//#endif
//}
