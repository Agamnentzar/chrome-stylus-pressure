#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <algorithm>
#include "HookDLL.h"

#pragma data_seg(".Segment")
HWND hWndServer = NULL;
//std::vector<HWND> receivers;
UINT UWM_POINTERUPDATE = 0;
#pragma data_seg()
#pragma comment(linker, "/section:.Segment,rws")

HINSTANCE hInst = NULL;
HHOOK hook = NULL;

static LRESULT CALLBACK HookProc(UINT nCode, WPARAM wParam, LPARAM lParam);

__declspec(dllexport) DWORD InstallHook(HWND hWndParent)
{
	//receivers.push_back(hWndParent);

	if (hWndServer)
		return E_FAIL;

	hook = SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)HookProc, hInst, 0);

	if (hook) {
		hWndServer = hWndParent;
		return S_OK;
	} else {
		return GetLastError();
	}
}

__declspec(dllexport) DWORD UnInstallHook(HWND hWndParent)
{
	//receivers.erase(std::remove(receivers.begin(), receivers.end(), hWndParent), receivers.end());

	if (hWndParent != hWndServer || hWndParent == NULL)
		return E_FAIL;

	//if (receivers.size() > 0)
	//	return S_OK;

	BOOL unhooked = UnhookWindowsHookEx(hook);

	if (unhooked)
		hWndServer = NULL;

	return unhooked ? S_OK : E_FAIL;
}

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		hInst = hModule;
		UWM_POINTERUPDATE = ::RegisterWindowMessage(UWM_POINTERUPDATE_MSG);
		break;
	case DLL_PROCESS_DETACH:
		UnInstallHook(hWndServer);
		//UnhookWindowsHookEx(hook);
		break;
	default:
		break;
	}
	return TRUE;
}

void SendPressure(UINT32 pressure, UINT32 section) {
	//for (auto hwnd : receivers)
	//	SendMessage(hwnd, UWM_POINTERUPDATE, pressure, section);
	SendMessage(hWndServer, UWM_POINTERUPDATE, pressure, section);
}

static LRESULT CALLBACK HookProc(UINT nCode, WPARAM wParam, LPARAM lParam)
{
	if (!IsWindow(hWndServer))
		UnInstallHook(hWndServer);

	if (nCode < 0) {
		CallNextHookEx(hook, nCode, wParam, lParam);
		return 0;
	}

	LPMSG msg = (LPMSG)lParam;

	if (msg->message == WM_POINTERUPDATE) {
		POINTER_PEN_INFO penInfo;
		UINT32 pointerId = GET_POINTERID_WPARAM(msg->wParam);
		POINTER_INPUT_TYPE pointerType = PT_POINTER;

		if (!GetPointerType(pointerId, &pointerType))
			pointerType = PT_POINTER;

		if (pointerType == PT_PEN && GetPointerPenInfo(pointerId, &penInfo))
			SendPressure(penInfo.pressure, 0);
		//else
		//	SendPressure(penInfo.pressure, 1);
	} 
	//else if (msg->message == WM_POINTERDOWN ||
	//	msg->message == WM_POINTERUP || 
	//	msg->message == WM_POINTERENTER || 
	//	msg->message == WM_POINTERLEAVE || 
	//	msg->message == WM_POINTERCAPTURECHANGED) {
	//	SendPressure(0, 2);
	//} else if (msg->message == WM_NCPOINTERUPDATE) {
	//	SendPressure(0, 3);
	//} else {
	//	//SendPressure(0, msg->message);
	//}

	return CallNextHookEx(hook, nCode, wParam, lParam);
}
