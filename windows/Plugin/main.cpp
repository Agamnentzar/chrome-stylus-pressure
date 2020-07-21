#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h>

#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <memory>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>

#define PACKETDATA (PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE)
#define PACKETMODE PK_BUTTONS

#include "MSGPACK.H"
#include "WINTAB.H"
#include "PKTDEF.H"
#include "Utils.h"

#define WM_FOCUS_TABLET (WM_APP + 0x0001)
#define WM_BLUR_TABLET (WM_APP + 0x0002)

const char* version = "1.0.3";

struct TabletInfo
{
	int maxPressure;
	char name[256];
};

HWND hWnd = NULL;
HINSTANCE hInst = NULL;
std::map<HCTX, TabletInfo> gContextMap;
int gnOpenContexts = 0;
int gnAttachedDevices = 0;
char buffer[1024];

void SendData(const char* data)
{
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

BOOL NEAR OpenTabletContexts(HWND hWnd)
{
	int ctxIndex = 0;
	gnOpenContexts = 0;
	gnAttachedDevices = 0;
	std::stringstream szTabletName;

	gContextMap.clear();

	gpWTInfoA(WTI_INTERFACE, IFC_NDEVICES, &gnAttachedDevices);
	WacomTrace("Number of attached devices: %i\n", gnAttachedDevices);

	// Open/save contexts until first failure to open a context.
	// Note that gpWTInfoA(WTI_STATUS, STA_CONTEXTS, &nOpenContexts);
	// will not always let you enumerate through all contexts.
	do
	{
		LOGCONTEXT lcMine = { 0 };
		UINT wWTInfoRetVal = 0;
		//		AXIS TabletX = { 0 };
		//		AXIS TabletY = { 0 };
		AXIS Pressure = { 0 };

		WacomTrace("Getting info on contextIndex: %i ...\n", ctxIndex);

		int foundCtx = gpWTInfoA(WTI_DDCTXS + ctxIndex, 0, &lcMine);

		if (foundCtx > 0)
		{
			lcMine.lcPktData = PACKETDATA;
			lcMine.lcOptions |= CXO_MESSAGES;
			lcMine.lcOptions |= CXO_SYSTEM;
			lcMine.lcPktMode = PACKETMODE;
			lcMine.lcMoveMask = PACKETDATA;
			lcMine.lcBtnUpMask = lcMine.lcBtnDnMask;

			//			// Set the entire tablet as active
			//			wWTInfoRetVal = gpWTInfoA(WTI_DEVICES + ctxIndex, DVC_X, &TabletX);
			//
			//			if (wWTInfoRetVal != sizeof(AXIS))
			//			{
			//				WacomTrace("This context should not be opened.\n");
			//				continue;
			//			}
			//
			//			wWTInfoRetVal = gpWTInfoA(WTI_DEVICES + ctxIndex, DVC_Y, &TabletY);

			if (gpWTInfoA(WTI_DEVICES + ctxIndex, DVC_NPRESSURE, &Pressure) != sizeof(AXIS))
			{
				WacomTrace("This context should not be opened.\n");
				continue;
			}

			WacomTrace("Pressure: %i, %i\n", Pressure.axMin, Pressure.axMax);

			//			lcMine.lcInOrgX = 0;
			//			lcMine.lcInOrgY = 0;
			//			lcMine.lcInExtX = TabletX.axMax;
			//			lcMine.lcInExtY = TabletY.axMax;
			//
			//			// Guarantee the output coordinate space to be in screen coordinates.  
			//			lcMine.lcOutOrgX = GetSystemMetrics(SM_XVIRTUALSCREEN);
			//			lcMine.lcOutOrgY = GetSystemMetrics(SM_YVIRTUALSCREEN);
			//			lcMine.lcOutExtX = GetSystemMetrics(SM_CXVIRTUALSCREEN);
			//
			//			// In Wintab, the tablet origin is lower left.  Move origin to upper left
			//			// so that it coincides with screen origin.
			//			lcMine.lcOutExtY = -GetSystemMetrics(SM_CYVIRTUALSCREEN);

			// Leave the system origin and extents as received:
			// lcSysOrgX, lcSysOrgY, lcSysExtX, lcSysExtY

			// Open the context enabled.
			HCTX hCtx = gpWTOpenA(hWnd, &lcMine, TRUE);

			if (hCtx)
			{
				TabletInfo info = { Pressure.axMax };

				gpWTInfoA(WTI_DEVICES + ctxIndex, CSR_NAME, info.name);
				gContextMap[hCtx] = info;
				WacomTrace("Opened context: 0x%X for ctxIndex: %i\n", hCtx, ctxIndex);
				gnOpenContexts++;
			}
			else
			{
				WacomTrace("Did NOT open context for ctxIndex: %i\n", ctxIndex);
			}
		}
		else
		{
			WacomTrace("No context info for ctxIndex: %i, bailing out...\n", ctxIndex);
			break;
		}

		ctxIndex++;
	} while (TRUE);

	if (gnOpenContexts < gnAttachedDevices)
	{
		WacomTrace("Oops - did not open a context for each attached device");
	}

	return gnAttachedDevices > 0;
}

void CloseTabletContexts()
{
	for (auto& ctx : gContextMap)
	{
		HCTX hCtx = ctx.first;
		WacomTrace("Closing context: 0x%X\n", hCtx);
		gpWTClose(hCtx);
	}

	gContextMap.clear();
	gnOpenContexts = 0;
	gnAttachedDevices = 0;
}

LRESULT FAR PASCAL MainWndProc(HWND hWnd, unsigned message, WPARAM wParam, LPARAM lParam)
{
	static HCTX hctx = NULL;
	PACKET pkt;

	switch (message)
	{
	case WM_FOCUS_TABLET:
		if (hctx)
			gpWTOverlap(hctx, TRUE);
		break;

	case WM_BLUR_TABLET:
		if (hctx)
			gpWTOverlap(hctx, FALSE);
		break;

	case WM_CREATE:
		if (!OpenTabletContexts(hWnd))
		{
			//SendError("no tablets found");
			WacomTrace("No tablets found.");
		}
		break;

	case WT_PACKET:
	{
		hctx = (HCTX)lParam;

		if (gpWTPacket(hctx, wParam, &pkt))
		{
			double pressure = (double)pkt.pkNormalPressure / (double)gContextMap[hctx].maxPressure;
			sprintf_s(buffer, "{\"p\":%f}", pressure);
			SendData(buffer);
		}
		else
		{
			SendError("got pinged by an unknown context");
			WacomTrace("Oops - got pinged by an unknown context: 0x%X", hctx);
		}
	}
	break;

	case WT_INFOCHANGE:
	{
		int nAttachedDevices = 0;
		gpWTInfoA(WTI_INTERFACE, IFC_NDEVICES, &nAttachedDevices);
		WacomTrace("WT_INFOCHANGE detected; number of connected tablets is now: %i\n", nAttachedDevices);

		if (nAttachedDevices != gnAttachedDevices)
		{
			// kill all current tablet contexts
			CloseTabletContexts();

			// Add some delay to give driver a chance to catch up in configuring
			// to the current attached tablets.  
			// 1 second seems to work - your mileage may vary...
			::Sleep(1000);

			// re-enumerate attached tablets
			OpenTabletContexts(hWnd);
		}

		if (gnAttachedDevices == 0)
		{
			//SendError("no tablets found");
			WacomTrace("No tablets found.");
		}
	}
	break;

	case WT_PROXIMITY:
	{
		hctx = (HCTX)wParam;
		bool entering = (HIWORD(lParam) != 0);

		if (entering)
		{
			if (gContextMap.count(hctx) > 0)
			{
				auto info = gContextMap[hctx];
				WacomTrace("Tablet name: %s count: %i\n", info.name, gnAttachedDevices);
				sprintf_s(buffer, "{\"name\":%s}", info.name);
				SendData(buffer);
			}
			else
			{
				SendError("couldn't find context");
				WacomTrace("Oops - couldn't find context: 0x%X\n", hctx);
			}
		}
	}
	break;

	case WM_DESTROY:
		CloseTabletContexts();
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

BOOL InitInstance(HINSTANCE hInstance)
{
	hInst = hInstance;

	if (!LoadWintab())
	{
		SendError("Wintab not available");
		ShowError("Wintab not available");
		return FALSE;
	}

	if (!gpWTInfoA(0, 0, NULL))
	{
		SendError("WinTab Services Not Available");
		ShowError("WinTab Services Not Available");
		return FALSE;
	}

	hWnd = CreateWindow("StylusPressurePluginWClass", "test", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		SendError("Could not create window!");
		ShowError("Could not create window!");
		return FALSE;
	}

	return TRUE;
}

BOOL InitApplication(HINSTANCE hInstance)
{
	WNDCLASS wc;
	wc.style = 0;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = NULL;
	wc.hCursor = NULL;
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "StylusPressurePluginWClass";
	return RegisterClass(&wc);
}

void Cleanup()
{
	WACOM_TRACE("Cleanup()\n");
	UnloadWintab();
}

DWORD WINAPI ThreadFunc(LPVOID lpParam)
{
	HINSTANCE hInstance = GetModuleHandle(NULL);

	if (!InitApplication(hInstance))
		return FALSE;

	if (!InitInstance(hInstance))
		return FALSE;

	MSG msg;

	while (GetMessage(&msg, NULL, 0, 0))
	{
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

		buffer[length] = '\0';

		if (strcmp(buffer, "\"focus\"") == 0)
			PostMessage(hWnd, WM_FOCUS_TABLET, 0, 0);
		else if (strcmp(buffer, "\"blur\"") == 0)
			PostMessage(hWnd, WM_BLUR_TABLET, 0, 0);
	}

	PostMessage(hWnd, WM_CLOSE, 0, 0);
	WaitForSingleObject(thread, 1000);
	Cleanup();
	return 0;
}
