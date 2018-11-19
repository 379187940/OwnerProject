// Copyright 2001-2017 Angelicatek GmbH / Angelicatek Group. All rights reserved. 

#pragma once

#include "AngelicaThread.h"

// Include architecture specific code.
#if _WIN32
	#include "AngelicaAtomics_impl_win32.h"
	#include "AngelicaThreadImpl_win32.h"
#endif

// vim:ts=2
