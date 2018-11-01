// Test.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include <DbgHelp.h>
#include <type_traits>
#include<stdlib.h>
#include<algorithm>
#include<thread>
#include<vector>
//using namespace std;
template <int c ,class T = int ,class T1 = int, class T2 = int >
class stack {
public:
	stack() {
		printf("class stack {\n");
	}
};
template <>
class stack<4, bool , int> {
public:
	stack() {
		printf("class stack<bool, int> {\n");
	}
};
template <class T2>
class stack<3,bool,T2> {
public:
	stack() {
		printf("class stack<bool,T2> {\n");
	}
};
template <class T2,class T3>
class stack<5,bool, T2,T3> {
public:
	stack() {
		printf("class stack<bool, T2,T3>\n");
	}
};
//template <class Tx = int, typename Ty>
//Ty test(Tx x, Ty y)
//{
//	printf("sizeof():%d\n", sizeof(Ty));
//	return y;
//}
//class CTest
//{
//public:
//	void test(int x, int y, int z)
//	{
//
//	}
//private:
//	int m_a;
//};
//template<typename C, typename P0, typename P1, typename P2, typename T>
//void VerifyParameter_3(void (C::* func)(P0, P1, P2), const T&)
//{
//	printf("void VerifyParameter_3(void (C::* func)(P0, P1, P2), const T&)");
//	
//}
//template<typename P0, typename P1, typename P2, typename T>
//void VerifyParameter_3(void (* func)(P0, P1, P2), const T&)
//{
//	printf("void VerifyParameter_3(void (* func)(P0, P1, P2), const T&)");
//}

struct protect_mem_t {
	protect_mem_t(void* addr, size_t size) : addr(addr), size(size), is_protected(FALSE) {
		protect();
	}
	~protect_mem_t() { release(); }
	BOOL protect() {
		if (!is_protected) {
			// To catch only read access you should change PAGE_NOACCESS to PAGE_READONLY
			is_protected = VirtualProtect(addr, size, PAGE_READONLY, &old_protect);
		}
		return is_protected;
	}
	BOOL release() {
		if (is_protected)
			is_protected = !VirtualProtect(addr, size, old_protect, &old_protect);
		return !is_protected;
	}

protected:
	void*   addr;
	size_t  size;
	BOOL    is_protected;
	DWORD   old_protect;
};
class ccccc
{
public:
	ccccc()
	{
		a = 1;
		b = 2;
		c = 3;
	}
	int a, b, c;
};
void  add(int a, int b)
{
	
}
int g_a = 3333;
void t1()
{
	g_a = 3334;
}
struct _False {};
struct _True
{
	char m_a[2];
};
class hhhhh
{
public:
	void add(int a, int b)
	{
		return ;
	}
	static void add1(int a, int b)
	{
		return ;
	}
	int m_a;
};
//typedef void(*func) (int a, int b);
//_False is_member_function(...)
//{
//#pragma message("this is False") 
//	return _False();
//}
template<typename T>
_True is_member_function(void (T::*) (int, int))
{
#pragma message("this is True")
	return _True();
}
template<typename T>
class CTestTypename
{
	typedef typename std::vector<T>::size_type size_type;
};
template<typename DestinationType, typename SourceType> inline DestinationType alias_cast(SourceType pPtr)
{
	union
	{
		SourceType      pSrc;
		DestinationType pDst;
	} conv_union;
	conv_union.pSrc = pPtr;
	return conv_union.pDst;

}


int main()
{
	for (int i = 0; i < 10; i++)
	{
		printf("%d\n", rand());
	}
	hhhhh test;
	char a[120];
	*alias_cast<hhhhh**>(a) = &test;
    return 0;
}

