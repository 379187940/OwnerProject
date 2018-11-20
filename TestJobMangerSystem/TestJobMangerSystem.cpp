// TestJobMangerSystem.cpp : 定义应用程序的入口点。
//

#include "stdafx.h"
#include "AngelicaPlatformDefines.h"
#include "TestJobMangerSystem.h"
#include "MSVCspecific.h"
#include "Win32specific.h"
#include "IJobManager.h"
#include "IThreadManager.h"
#include "IJobManager_JobDelegator.h"
#define MAX_LOADSTRING 100

// 全局变量: 
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名

// 此代码模块中包含的函数的前向声明: 
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
//CRITICAL_SECTION cs1;
//CRITICAL_SECTION cs2;

//class CWindowsConsoleInputThread : public IThread
//{
//public:
//	CWindowsConsoleInputThread(){}
//
//	~CWindowsConsoleInputThread(){}
//
//	// Start accepting work on thread
//	void ThreadEntry()
//	{
//		GetJobManagerInterface()->WaitForJob(g_JobState);
//		OutputDebugStringA("CWindowsConsoleInputThread end!!!!\n");
//	}
//
//	// Signals the thread that it should not accept anymore work and exit
//	void SignalStopWork()
//	{
//
//	}
//
//	void Interrupt()
//	{
//
//	}
//
//};
//class CWindowsConsoleInputThread2 : public IThread
//{
//public:
//	CWindowsConsoleInputThread2() {}
//
//	~CWindowsConsoleInputThread2() {}
//
//	// Start accepting work on thread
//	void ThreadEntry()
//	{
//		GetJobManagerInterface()->WaitForJob(g_JobState);
//		OutputDebugStringA("CWindowsConsoleInputThread2 end!!!!\n");
//	}
//
//	// Signals the thread that it should not accept anymore work and exit
//	void SignalStopWork()
//	{
//
//	}
//
//	void Interrupt()
//	{
//
//	}
//
//};
class CTest
{
public:
	CTest(int a)
	{
		m_a = a;
	}
	void Print()
	{
		while (true)
		{
			Sleep(10);
			char log[30];
			sprintf_s(log, "%d  perfect world!!!!\n", m_a);
			OutputDebugStringA(log); 
			if (GetAsyncKeyState(VK_SPACE) && 0x8000)
				break;
		}
	}
private:
	int m_a;
	
};
static JobManager::SJobState g_JobState1, g_JobState2, g_JobState3, g_JobState4, g_JobState5;
DECLARE_JOB("Test", TTestJob, CTest::Print);
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	GetJobManagerInterface()->Init(4);
	CTest a(1),b(2),c(3),d(4),e(5);
	TTestJob job1,job2,job3,job4,job5;
	job1.SetClassInstance(&a);
	job1.RegisterJobState(&g_JobState1);
	job1.Run();

	job2.SetClassInstance(&b);
	job2.RegisterJobState(&g_JobState2);
	job2.Run();

	job3.SetClassInstance(&c);
	job3.RegisterJobState(&g_JobState3);
	job3.Run();

	job4.SetClassInstance(&d);
	job4.RegisterJobState(&g_JobState4);
	job4.Run();

	job5.SetClassInstance(&e);
	job5.RegisterJobState(&g_JobState5);
	job5.Run();
	/*CWindowsConsoleInputThread th1;
	CWindowsConsoleInputThread2 th2;
	GetGlobalThreadManager()->SpawnThread(&th1, "thread1");
	GetGlobalThreadManager()->SpawnThread(&th2, "thread2");*/
	//GetJobManagerInterface()->WaitForJob(g_JobState);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 在此放置代码。

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_TESTJOBMANGERSYSTEM, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化: 
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TESTJOBMANGERSYSTEM));

    MSG msg;

    // 主消息循环: 
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目的: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TESTJOBMANGERSYSTEM));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_TESTJOBMANGERSYSTEM);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目的: 保存实例句柄并创建主窗口
//
//   注释: 
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 将实例句柄存储在全局变量中

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的:    处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 分析菜单选择: 
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: 在此处添加使用 hdc 的任何绘图代码...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
