/*
  Copyright (C) 2001 Kimmo Pekkola

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <string>
#include <algorithm>
#include "../Library/Rainmeter.h"

#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

/*
** Protos
*/
BOOL InitApplication(HINSTANCE hInstance, const WCHAR* WinClass);
HWND InitInstance(HINSTANCE hInstance, const WCHAR* WinClass, const WCHAR* WinName);
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void Bang(const WCHAR* command);
HMODULE RmLoadSystemLibrary(LPCWSTR lpLibFileName);
BOOL IsRunning(HANDLE* hMutex);

/*
** Stuff from the DLL
*/
EXPORT_PLUGIN int Initialize(HWND hWnd, HINSTANCE hInstance, LPCWSTR lpCmdLine);
EXPORT_PLUGIN void Quit();
EXPORT_PLUGIN void ExecuteBang(LPCTSTR szBang);

const WCHAR* WinClass = L"DummyRainWClass";
const WCHAR* WinName = L"Rainmeter control window";

enum RetValue
{
	RetSuccess = 0,
	RetError   = 1
};

/*
** WinMain
**
** The Main-function
**
*/
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	HANDLE hMutex = NULL;
	MSG msg;
	BOOL bRet;

	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
//	_CrtSetBreakAlloc(5055);

	// Avoid loading a dll from current directory
	SetDllDirectory(L"");

	if (lpCmdLine && lpCmdLine[0] == L'!')
	{
		// It's a !bang
		Bang(lpCmdLine);
		return RetSuccess;
	}

	// Check whether Rainmeter.exe is already running
	if (IsRunning(&hMutex))
	{
		//MessageBox(NULL, L"Rainmeter.exe is already running.", L"Rainmeter", MB_ICONWARNING | MB_TOPMOST | MB_OK);
		return RetSuccess;
	}

	if (!InitApplication(hInstance, WinClass)) return RetError;

	HWND hWnd = InitInstance(hInstance, WinClass, WinName);
	if (!hWnd) return RetError;

	// Check that the DLL is available and initialize it
	HMODULE module = GetModuleHandle(L"Rainmeter.dll");
	if (module == NULL || Initialize(hWnd, module, lpCmdLine) == 1)
	{
		DestroyWindow(hWnd);
		return RetError;
	}

	// Run the standard window message loop
	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (bRet == -1)  // error
		{
			Quit();
			break;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (hMutex) ReleaseMutex(hMutex);
	return (int)msg.wParam;
}

/*
** InitApplication
**
** Creates the windowclass
**
*/
BOOL InitApplication(HINSTANCE hInstance, const WCHAR* WinClass)
{
	WNDCLASS wc = {0};
	wc.lpfnWndProc = (WNDPROC)MainWndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = WinClass;

	return RegisterClass(&wc);
}

/*
** InitInstance
**
** Creates the window. This is just an invisible window. The real window
** is created by the DLL.
**
*/
HWND InitInstance(HINSTANCE hInstance, const WCHAR* WinClass, const WCHAR* WinName)
{
	return CreateWindowEx(
		WS_EX_TOOLWINDOW,
		WinClass,
		WinName,
		WS_POPUP | WS_DISABLED,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		hInstance,
		NULL
	);
}

/*
** Bang
**
** Sends bangs to the DLL
**
*/
void Bang(const WCHAR* command)
{
	// Check if Rainmeter is running
	HWND wnd = FindWindow(WinClass, WinName);
	if (wnd != NULL)
	{
		COPYDATASTRUCT copyData;

		copyData.dwData = 1;
		copyData.cbData = (DWORD)((wcslen(command) + 1) * sizeof(WCHAR));
		copyData.lpData = (void*)command;

		// Send the bang to the Rainmeter window
		SendMessage(wnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&copyData);
	}
	else
	{
		if (_wcsicmp(L"!rainmeterquit", command) != 0 &&
			_wcsicmp(L"!quit", command) != 0)
		{
			MessageBox(NULL, L"Rainmeter is not running.\nUnable to send the !bang to it.", L"Rainmeter", MB_OK | MB_TOPMOST | MB_ICONERROR);
		}
	}
}

/*
** RmLoadSystemLibrary
**
** Loads a system dll from system32 directory.
**
*/
HMODULE RmLoadSystemLibrary(LPCWSTR lpLibFileName)
{
	WCHAR buffer[MAX_PATH];
	std::wstring path;

	if (GetSystemDirectory(buffer, MAX_PATH))
	{
		path = buffer;
		path += L"\\";
		path += lpLibFileName;

		return LoadLibrary(path.c_str());
	}

	return NULL;
}

/*
** IsRunning
**
** Checks whether Rainmeter.exe is running.
**
*/
BOOL IsRunning(HANDLE* hMutex)
{
	typedef struct
	{
		ULONG         i[2];
		ULONG         buf[4];
		unsigned char in[64];
		unsigned char digest[16];
	} MD5_CTX;

	typedef void (WINAPI *FPMD5INIT)(MD5_CTX *context);
	typedef void (WINAPI *FPMD5UPDATE)(MD5_CTX *context, const unsigned char *input, unsigned int inlen);
	typedef void (WINAPI *FPMD5FINAL)(MD5_CTX *context);

	// Create MD5 digest from command line
	HMODULE hCryptDll = RmLoadSystemLibrary(L"cryptdll.dll");
	if (!hCryptDll)  // Unable to check the mutex
	{
		*hMutex = NULL;
		return FALSE;
	}

	FPMD5INIT MD5Init = (FPMD5INIT)GetProcAddress(hCryptDll, "MD5Init");
	FPMD5UPDATE MD5Update = (FPMD5UPDATE)GetProcAddress(hCryptDll, "MD5Update");
	FPMD5FINAL MD5Final = (FPMD5FINAL)GetProcAddress(hCryptDll, "MD5Final");
	if (!MD5Init || !MD5Update || !MD5Final)  // Unable to check the mutex
	{
		FreeLibrary(hCryptDll);
		*hMutex = NULL;
		return FALSE;
	}

	std::wstring cmdLine = GetCommandLine();
	std::transform(cmdLine.begin(), cmdLine.end(), cmdLine.begin(), ::towlower);

	MD5_CTX ctx = {0};

	MD5Init(&ctx);
	MD5Update(&ctx, (LPBYTE)cmdLine.c_str(), cmdLine.length() * sizeof(WCHAR));
	MD5Final(&ctx);

	FreeLibrary(hCryptDll);

	// Convert MD5 digest to mutex string (e.g. "Rainmeter@0123456789abcdef0123456789abcdef")
	const WCHAR szHexTable[] = L"0123456789abcdef";
	WCHAR szMutex[64] = {0};
	wcscpy(szMutex, L"Rainmeter@");

	WCHAR* pos = szMutex + wcslen(szMutex);

	for (size_t i = 0; i < 16; ++i)
	{
		*(pos++) = *(szHexTable + ((ctx.digest[i] >> 4) & 0xF));
		*(pos++) = *(szHexTable + (ctx.digest[i] & 0xF));
	}

	// Create mutex
	HANDLE hMutexTemp = CreateMutex(NULL, FALSE, szMutex);
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		// Rainmeter.exe is already running
		*hMutex = NULL;
		return TRUE;
	}

	// Rainmeter.exe is not running
	*hMutex = hMutexTemp;
	return FALSE;
}

/*
** MainWndProc
**
** The main window procedure
**
*/
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {

	case WM_DESTROY:
		{
			Quit();
			PostQuitMessage(0);
		}
		break;

	case WM_COPYDATA:
		{
			COPYDATASTRUCT* pCopyDataStruct = (COPYDATASTRUCT*) lParam;
			if (pCopyDataStruct && (pCopyDataStruct->dwData == 1) && (pCopyDataStruct->cbData > 0))
			{
				ExecuteBang((const WCHAR*)pCopyDataStruct->lpData);
			}
		}
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}
