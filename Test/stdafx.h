// stdafx.h : ��׼ϵͳ�����ļ��İ����ļ���
// ���Ǿ���ʹ�õ��������ĵ�
// �ض�����Ŀ�İ����ļ�
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

// TODO:  �ڴ˴����ó�����Ҫ������ͷ�ļ�
