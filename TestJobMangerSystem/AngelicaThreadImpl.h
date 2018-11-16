// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "AngelicaThread.h"

// Include architecture specific code.
#if _WIN32
	#include "AngelicaAtomics_impl_win32.h"
	#include "AngelicaThreadImpl_win32.h"
#elif CRY_PLATFORM_ORBIS
	#include <CryThreading/CryThreadImpl_sce.h>
#elif CRY_PLATFORM_POSIX
	#include <CryThreading/CryThreadImpl_posix.h>
#else
// Put other platform specific includes here!
#endif

// vim:ts=2
