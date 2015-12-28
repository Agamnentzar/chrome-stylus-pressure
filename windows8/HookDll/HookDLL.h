#pragma once

#define LIBSPEC __declspec(dllexport)
LIBSPEC DWORD InstallHook(HWND hWnd);
LIBSPEC DWORD UnInstallHook(HWND hWnd);
#undef LIBSPEC

#define UWM_POINTERUPDATE_MSG (TEXT("UWM_POINTERUPDATE_USER_MSG"))