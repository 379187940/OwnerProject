// Copyright 2001-2017 Angelicatek GmbH / Angelicatek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   MSVCSpecific.h
//  Version:     v1.00
//  Created:     5/4/2005 by Scott
//  Compilers:   Visual Studio.NET 2003
//  Description: Settings for all builds under MS Visual C++ compiler
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef _MSC_VER
	#error This file should only be included on MSVC compiler
#endif

//! Compiler version
#define ANGELICA_COMPILER_MSVC    1
#define ANGELICA_COMPILER_VERSION (_MSC_VER)
#if ANGELICA_COMPILER_VERSION < 1700
	#error This version of MSVC is not supported
#elif ANGELICA_COMPILER_VERSION < 1900
// patch alignof and noexcept (VS2012 + VS2013 only)
	#define _ALLOW_KEYWORD_MACROS
	#define alignof  __alignof
	#define noexcept throw()
#endif

//! Compiler features
#if defined(_CPPUNWIND)
	#define ANGELICA_COMPILER_EXCEPTIONS 1
#endif
#if defined(_CPPRTTI)
	#define ANGELICA_COMPILER_RTTI 1
#endif

//! __FUNC__ is like __func__, but it has the class name
#define __FUNC__               __FUNCTION__
#define ANGELICA_FUNC_HAS_SIGNATURE 0

//! PREfast heleprs
#define PREFAST_SUPPRESS_WARNING(W) __pragma(warning(suppress: W))
#ifdef _PREFAST_
	#define PREFAST_ASSUME(cond)      __analysis_assume(cond)
#else
	#define PREFAST_ASSUME(cond)
#endif

//! Deprecation helper
#define ANGELICA_DEPRECATED(func) __declspec(deprecated) func

//! Portable alignment helper, can be placed after the struct/class/union keyword, or before the type of a declaration.
//! Example: struct ANGELICA_ALIGN(16) { ... }; ANGELICA_ALIGN(16) char myAlignedChar;
#define ANGELICA_ALIGN(bytes) __declspec(align(bytes))

//! Restricted reference (similar to restricted pointer), use like: SFoo& RESTRICT_REFERENCE myFoo = ...;
#define RESTRICT_REFERENCE

//! Compiler-supported type-checking helper
#define PRINTF_PARAMS(...)
#define SCANF_PARAMS(...)

//! Barrier to prevent R/W reordering by the compiler.
//! Note: This does not emit any instruction, and it does not prevent CPU reordering!
#define MEMORY_RW_REORDERING_BARRIER _ReadWriteBarrier()

//! Static branch-prediction helpers
#define IF(condition, hint)    if (condition)
#define IF_UNLIKELY(condition) if (condition)
#define IF_LIKELY(condition)   if (condition)

//! Inline helpers
#define NO_INLINE        __declspec(noinline)
#define NO_INLINE_WEAK   __declspec(noinline) inline
#define ANGELICA_FORCE_INLINE __forceinline

//! Packing helper, the preceding declaration will be tightly packed.
#define __PACKED

// Suppress undefined behavior sanitizer errors on a function.
#define ANGELICA_FUNCTION_CONTAINS_UNDEFINED_BEHAVIOR

//! Unreachable code marker for helping error handling and optimization
#define UNREACHABLE() __assume(0)

// Disable (and enable) specific compiler warnings.
// MSVC compiler is very confusing in that some 4xxx warnings are shown even with warning level 3,
// and some 4xxx warnings are NOT shown even with warning level 4.

#pragma warning(disable: 4018)  // signed/unsigned mismatch
#pragma warning(disable: 4127)  // conditional expression is constant
#pragma warning(disable: 4201)  // nonstandard extension used : nameless struct/union
#pragma warning(disable: 4512)  // assignment operator could not be generated (in STLPort with const constructs)
#pragma warning(disable: 4530)  // C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc
#pragma warning(disable: 4503)  // decorated name length exceeded, name was truncated
#pragma warning(disable: 6255)  // _alloca indicates failure by raising a stack overflow exception. Consider using _malloca instead. (Note: _malloca requires _freea.)
#pragma warning(disable: 4838)  // conversion from 'T1' to 'T2' requires a narrowing conversion (C++11)
#pragma warning(disable: 4577)  // 'noexcept' used with no exception handling mode specified, this is OK since we don't use exceptions

// TODO: This should be re-enabled and all the warnings fixed
#pragma warning(disable: 4316)  // 'T' : object allocated on the heap may not be aligned X

// Turn on the following very useful warnings.
#pragma warning(3: 4264)        // no override available for virtual member function from base 'class'; function is hidden
#pragma warning(3: 4266)        // no override available for virtual member function from base 'type'; function is hidden
