#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../HookDLL/HookDLL.h"

static UINT UWM_POINTERUPDATE = ::RegisterWindowMessage(UWM_POINTERUPDATE_MSG);

#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <memory>
#include <sstream>
#include <fstream>

const char* version = "2.0.0";

HWND hWnd = NULL;
HINSTANCE hInst = NULL;
char buffer[1024];

std::string GetErrorMessage(int errorCode)
{
	LPTSTR buffer;

	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&buffer, 0, NULL) == 0)
	{
		return "Unknown";
	}

	char buff[20];
	auto result = std::string(buffer);
	LocalFree(buffer);
	_itoa_s(errorCode, buff, sizeof(buff), 10);
	return std::string("(") + buff + std::string(") ") + result;
}

void SendData(const char* data)
{
	//printf("%s\n", data);
	int len = strlen(data);
	fwrite(&len, 4, 1, stdout);
	fwrite(data, 1, len, stdout);
	fflush(stdout);
}

void SendError(const char* error)
{
	sprintf_s(buffer, "{\"error\":\"%s\"}", error);
	SendData(buffer);
}

LRESULT FAR PASCAL MainWndProc(HWND hWnd, unsigned message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_CREATE:
	{
		DWORD result = InstallHook(hWnd);

		if (result != S_OK) {
			sprintf_s(buffer, "{\"error\":\"failed to install hook (%s)\"}", GetErrorMessage(result).c_str());
			SendData(buffer);
		}
	}
	break;
	case WM_DESTROY:
		if (UnInstallHook(hWnd) != S_OK)
			SendError("failed to uninstall hook");
		PostQuitMessage(0);
		break;
	default:
		if (message == UWM_POINTERUPDATE) {
			sprintf_s(buffer, "{\"p\":%f,\"t\":\"%x\"}", wParam / 1024.0, lParam);
			SendData(buffer);
			break;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

BOOL InitInstance(HINSTANCE hInstance) {
	hInst = hInstance;
	hWnd = CreateWindow(TEXT("Agamnentzar.PressurePlugin"), TEXT("test"), 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);

	if (!hWnd) {
		SendError("Could not create window!");
		return FALSE;
	}

	return TRUE;
}

BOOL InitApplication(HINSTANCE hInstance) {
	WNDCLASS wc = { 0, };
	wc.lpfnWndProc = MainWndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = TEXT("Agamnentzar.PressurePlugin");
	return RegisterClass(&wc);
}

DWORD WINAPI ThreadFunc(LPVOID lpParam) {
	HINSTANCE hInstance = GetModuleHandle(NULL);

	if (!InitApplication(hInstance))
		return FALSE;

	if (!InitInstance(hInstance))
		return FALSE;

	MSG msg;

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}

int main()
{
	UINT length;
	char buffer[1024];

	_setmode(_fileno(stdout), _O_BINARY);
	_setmode(_fileno(stdin), _O_BINARY);

	sprintf_s(buffer, "{\"version\":\"%s\"}", version);
	SendData(buffer);

	DWORD threadId = 0;
	HANDLE thread = CreateThread(NULL, 0, ThreadFunc, NULL, 0, &threadId);

	while (fread(&length, 4, 1, stdin))
	{
		if (length > 1023 || fread(buffer, 1, length, stdin) != length)
			break;
	}

	PostMessage(hWnd, WM_CLOSE, 0, 0);
	WaitForSingleObject(thread, 1000);
	return 0;
}
