// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   Win32specific.h
//  Version:     v1.00
//  Created:     31/03/2003 by Sergiy.
//  Compilers:   Visual Studio.NET
//  Description: Specific to Win32 declarations, inline functions etc.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

// Ensure WINAPI version is consistent everywhere


//////////////////////////////////////////////////////////////////////////
// Standard includes.
//////////////////////////////////////////////////////////////////////////
#include <malloc.h>
#include <io.h>
#include <new.h>
#include <direct.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <float.h>
//////////////////////////////////////////////////////////////////////////

// Special intrinsics
#include <intrin.h> // moved here to prevent assert from being redefined when included elsewhere
#include <process.h>

//////////////////////////////////////////////////////////////////////////
// Define platform independent types.
//////////////////////////////////////////////////////////////////////////

#define THREADID_NULL 0

//typedef unsigned char BYTE;
//typedef unsigned int  unsigned long;
//typedef unsigned long DWORD;
//typedef double        real;  //!< Biggest float-type on this machine.
//typedef long          LONG;



typedef void* THREAD_HANDLE;
typedef void* EVENT_HANDLE;

#ifndef FILE_ATTRIBUTE_NORMAL
	#define FILE_ATTRIBUTE_NORMAL 0x00000080
#endif

#define FP16_TERRAIN
#define TARGET_DEFAULT_ALIGN (0x4U)
inline signed long long GetRealTicks()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}
#define STATIC_CHECK(expr, msg) static_assert((expr) != 0, # msg)