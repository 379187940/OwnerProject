// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

// Note: we use '#define xxx 1' because in this case both '#if xxx' and '#if defined(xxx)' work for checking.

//! This define was added just to let programmers check that CryPlatformDefines.h is
//! already included (so ANGELICA_PLATFORM_xxx are already available).
#define ANGELICA_PLATFORM 1

// Detecting CPU. See:
// http://nadeausoftware.com/articles/2012/02/c_c_tip_how_detect_processor_type_using_compiler_predefined_macros
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0491c/BABJFEFG.html
#if defined(__x86_64__) || defined(_M_X64)
	#define ANGELICA_PLATFORM_X64       1
	#define ANGELICA_PLATFORM_64BIT     1
	#define ANGELICA_PLATFORM_SSE2      1
#elif defined(__i386) || defined(_M_IX86)
	#define ANGELICA_PLATFORM_X86       1
	#define ANGELICA_PLATFORM_32BIT     1
#elif defined(__arm__)
	#define ANGELICA_PLATFORM_ARM    1
	#define ANGELICA_PLATFORM_32BIT  1
	#if defined(__ARM_NEON__)
		#define ANGELICA_PLATFORM_NEON 1
	#endif
#elif defined(__aarch64__)
	#define ANGELICA_PLATFORM_ARM    1
	#define ANGELICA_PLATFORM_64BIT  1
#else
	#define ANGELICA_PLATFORM_UNKNOWNCPU 1
#endif

#if defined(__APPLE__)

	#define ANGELICA_PLATFORM_APPLE 1
	#define ANGELICA_PLATFORM_POSIX 1
	#include <TargetConditionals.h>
	#if TARGET_OS_IPHONE
		#define ANGELICA_PLATFORM_MOBILE 1
		#define ANGELICA_PLATFORM_IOS    1
		#if !ANGELICA_PLATFORM_UNKNOWNCPU
			#error iOS: Unsupported CPU
		#endif
		#define ANGELICA_PLATFORM_64BIT   1
	#elif TARGET_OS_MAC
		#define ANGELICA_PLATFORM_DESKTOP 1
		#define ANGELICA_PLATFORM_MAC     1
		#if !ANGELICA_PLATFORM_X64
			#error Unsupported Mac CPU (the only supported is x86-64).
		#endif
	#else
		#error Unknown Apple platform.
	#endif

#elif defined(_DURANGO) || defined(_XBOX_ONE)

	#define ANGELICA_PLATFORM_CONSOLE 1
	#define ANGELICA_PLATFORM_DURANGO 1
	#define ANGELICA_PLATFORM_WINAPI  1
	#if !ANGELICA_PLATFORM_X64
		#error Unsupported Durango CPU (the only supported is x86-64).
	#endif
	#define ANGELICA_PLATFORM_SSE4    1
	#define ANGELICA_PLATFORM_F16C    1
	#define ANGELICA_PLATFORM_BMI1    1
	#define ANGELICA_PLATFORM_TBM     1

#elif defined(_ORBIS) || defined(__ORBIS__)

	#define ANGELICA_PLATFORM_CONSOLE 1
	#define ANGELICA_PLATFORM_ORBIS   1
	#define ANGELICA_PLATFORM_POSIX   1
	#if !ANGELICA_PLATFORM_X64
		#error Unsupported Orbis CPU (the only supported is x86-64).
	#endif
	#define ANGELICA_PLATFORM_SSE4    1
	#define ANGELICA_PLATFORM_F16C    1
	#define ANGELICA_PLATFORM_BMI1    1
	#define ANGELICA_PLATFORM_TBM     0

#elif defined(ANDROID) || defined(__ANDROID__)

	#define ANGELICA_PLATFORM_MOBILE  1
	#define ANGELICA_PLATFORM_ANDROID 1
	#define ANGELICA_PLATFORM_POSIX   1
	#if ANGELICA_PLATFORM_ARM && !(ANGELICA_PLATFORM_64BIT || ANGELICA_PLATFORM_32BIT)
		#error Unsupported Android CPU (only 32-bit and 64-bit ARM are supported).
	#endif

#elif defined(_WIN32)

	#define ANGELICA_PLATFORM_DESKTOP 1
	#define ANGELICA_PLATFORM_WINDOWS 1
	#define ANGELICA_PLATFORM_WINAPI  1
	#if defined(_WIN64)
		#if !ANGELICA_PLATFORM_X64
			#error Unsupported Windows 64 CPU (the only supported is x86-64).
		#endif
	#else
		#if !ANGELICA_PLATFORM_X86
			#error Unsupported Windows 32 CPU (the only supported is x86).
		#endif
	#endif

#elif defined(__linux__) || defined(__linux)

	#define ANGELICA_PLATFORM_DESKTOP 1
	#define ANGELICA_PLATFORM_LINUX   1
	#define ANGELICA_PLATFORM_POSIX   1
	#if !ANGELICA_PLATFORM_X64 && !ANGELICA_PLATFORM_X86
		#error Unsupported Linux CPU (the only supported are x86 and x86-64).
	#endif

#else

	#error Unknown target platform.

#endif

#if ANGELICA_PLATFORM_AVX
#define ANGELICA_PLATFORM_ALIGNMENT 32
#elif ANGELICA_PLATFORM_SSE2 || ANGELICA_PLATFORM_SSE4 || ANGELICA_PLATFORM_NEON
#define ANGELICA_PLATFORM_ALIGNMENT 16U
#else
#define ANGELICA_PLATFORM_ALIGNMENT 1U
#endif

// Validation

#if defined(ANGELICA_PLATFORM_X64) && ANGELICA_PLATFORM_X64 != 1
	#error Wrong value of ANGELICA_PLATFORM_X64.
#endif

#if defined(ANGELICA_PLATFORM_X86) && ANGELICA_PLATFORM_X86 != 1
	#error Wrong value of ANGELICA_PLATFORM_X86.
#endif

#if defined(ANGELICA_PLATFORM_ARM) && ANGELICA_PLATFORM_ARM != 1
	#error Wrong value of ANGELICA_PLATFORM_ARM.
#endif

#if defined(ANGELICA_PLATFORM_UNKNOWN_CPU) && ANGELICA_PLATFORM_UNKNOWN_CPU != 1
	#error Wrong value of ANGELICA_PLATFORM_UNKNOWN_CPU.
#endif

#if ANGELICA_PLATFORM_X64 + ANGELICA_PLATFORM_X86 + ANGELICA_PLATFORM_ARM + ANGELICA_PLATFORM_UNKNOWNCPU != 1
	#error Invalid CPU type.
#endif

#if defined(ANGELICA_PLATFORM_64BIT) && ANGELICA_PLATFORM_64BIT != 1
	#error Wrong value of ANGELICA_PLATFORM_64BIT.
#endif

#if defined(ANGELICA_PLATFORM_32BIT) && ANGELICA_PLATFORM_32BIT != 1
	#error Wrong value of ANGELICA_PLATFORM_32BIT.
#endif

#if ANGELICA_PLATFORM_64BIT + ANGELICA_PLATFORM_32BIT != 1
	#error Invalid ANGELICA_PLATFORM_xxBIT.
#endif

#if defined(ANGELICA_PLATFORM_DESKTOP) && ANGELICA_PLATFORM_DESKTOP != 1
	#error Wrong value of ANGELICA_PLATFORM_DESKTOP.
#endif

#if defined(ANGELICA_PLATFORM_MOBILE) && ANGELICA_PLATFORM_MOBILE != 1
	#error Wrong value of ANGELICA_PLATFORM_MOBILE.
#endif

#if defined(ANGELICA_PLATFORM_CONSOLE) && ANGELICA_PLATFORM_CONSOLE != 1
	#error Wrong value of ANGELICA_PLATFORM_CONSOLE.
#endif

#if ANGELICA_PLATFORM_DESKTOP + ANGELICA_PLATFORM_CONSOLE + ANGELICA_PLATFORM_MOBILE != 1
	#error Invalid ANGELICA_PLATFORM_(DESKTOP/CONSOLE/MOBILE)
#endif
