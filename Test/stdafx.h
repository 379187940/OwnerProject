// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <DbgHelp.h>
#include <type_traits>
LONG TestVolatile(volatile LONG* a, LONG b)
{
	return *a + b;
}
inline LONG CryInterlockedAdd(volatile int* pDst, int add)
{
	return TestVolatile((volatile long*)(pDst), (LONG)add);
}

// TODO:  在此处引用程序需要的其他头文件
