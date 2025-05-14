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


//#define PK_CONTEXT			0x0001	/* reporting context */
//#define PK_STATUS				0x0002	/* status bits */
//#define PK_TIME				0x0004	/* time stamp */
//#define PK_CHANGED			0x0008	/* change bit vector */
//#define PK_SERIAL_NUMBER		0x0010	/* packet serial number */
//#define PK_CURSOR				0x0020	/* reporting cursor */
//#define PK_BUTTONS			0x0040	/* button information */
//#define PK_X					0x0080	/* x axis */
//#define PK_Y					0x0100	/* y axis */
//#define PK_Z					0x0200	/* z axis */
//#define PK_NORMAL_PRESSURE	0x0400	/* normal or tip pressure */
//#define PK_TANGENT_PRESSURE	0x0800	/* tangential or barrel pressure */
//#define PK_ORIENTATION		0x1000	/* orientation info: tilts */
//#define PK_ROTATION			0x2000	/* rotation info; 1.1 */

#define PK_ALL 0x3FFF

#define PACKETDATA (PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE | PK_CURSOR)
#define PACKETMODE PK_BUTTONS

#include "MSGPACK.H"
#include "WINTAB.H"
#include "PKTDEF.H"
#include "Utils.h"

#define WM_FOCUS_TABLET (WM_APP + 0x0001)
#define WM_BLUR_TABLET (WM_APP + 0x0002)

const char* version = "1.2.0";

struct TabletInfo {
	int maxPressure;
	char name[256];
};

HWND hWnd = NULL;
HINSTANCE hInst = NULL;
std::map<HCTX, TabletInfo> gContextMap;
int gnOpenContexts = 0;
int gnAttachedDevices = 0;
char buffer[1024];

void SendData(const char* data) {
	int len = strlen(data);
	fwrite(&len, 4, 1, stdout);
	fwrite(data, 1, len, stdout);
	fflush(stdout);
}

void SendError(const char* error) {
	sprintf_s(buffer, "{\"error\":\"%s\"}", error);
	SendData(buffer);
}

BOOL NEAR OpenTabletContexts(HWND hWnd) {
	gnOpenContexts = 0;
	gnAttachedDevices = 0;
	gContextMap.clear();

	gpWTInfoA(WTI_INTERFACE, IFC_NDEVICES, &gnAttachedDevices);
	WACOM_TRACE("Number of attached devices: %i\n", gnAttachedDevices);

	// Open/save contexts until first failure to open a context.
	// Note that gpWTInfoA(WTI_STATUS, STA_CONTEXTS, &nOpenContexts);
	// will not always let you enumerate through all contexts.
	for (int ctxIndex = 0; ctxIndex < gnAttachedDevices; ctxIndex++) {
		LOGCONTEXTA lcMine = { 0 };
		UINT wWTInfoRetVal = 0;
		// AXIS TabletX = { 0 };
		// AXIS TabletY = { 0 };
		AXIS Pressure = { 0 };

		WACOM_TRACE("Getting info on contextIndex: %i ...\n", ctxIndex);

		int foundCtx = gpWTInfoA(WTI_DDCTXS + ctxIndex, 0, &lcMine);

		if (foundCtx == 0) {
			// use global context if can't get local one
			lcMine.lcOptions |= CXO_SYSTEM;
			gpWTInfoA(WTI_DEFSYSCTX, 0, &lcMine);
		}

		//if (foundCtx > 0) {
			lcMine.lcPktData = PACKETDATA;
			lcMine.lcOptions |= CXO_MESSAGES | CXO_SYSTEM;
			lcMine.lcPktMode = PACKETMODE;
			lcMine.lcMoveMask = PACKETDATA;
			lcMine.lcBtnUpMask = lcMine.lcBtnDnMask;

			//lcMine.lcOutOrgX = 0;
			//lcMine.lcOutExtX = lcMine.lcInExtX;
			//lcMine.lcOutOrgY = 0;
			//lcMine.lcOutExtY = -lcMine.lcInExtY;

			//			// Set the entire tablet as active
			//			wWTInfoRetVal = gpWTInfoA(WTI_DEVICES + ctxIndex, DVC_X, &TabletX);
			//
			//			if (wWTInfoRetVal != sizeof(AXIS))
			//			{
			//				WACOM_TRACE("This context should not be opened.\n");
			//				continue;
			//			}
			//
			//			wWTInfoRetVal = gpWTInfoA(WTI_DEVICES + ctxIndex, DVC_Y, &TabletY);

			if (gpWTInfoA(WTI_DEVICES + ctxIndex, DVC_NPRESSURE, &Pressure) != sizeof(AXIS)) {
				WACOM_TRACE("This context should not be opened.\n");
				continue;
			}

			WACOM_TRACE("Pressure: %i, %i\n", Pressure.axMin, Pressure.axMax);

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
			HCTX hCtx = gpWTOpenA(hWnd, &lcMine, FALSE);

			if (hCtx) {
				TabletInfo info = { Pressure.axMax };
				gpWTInfoA(WTI_DEVICES + ctxIndex, CSR_NAME, info.name);
				gContextMap[hCtx] = info;
				WACOM_TRACE("Opened context: 0x%X for ctxIndex: %d\n", hCtx, ctxIndex);
				gnOpenContexts++;
			} else {
				WACOM_TRACE("Did NOT open context for ctxIndex: %d\n", ctxIndex);
			}
		//} else {
		//	WACOM_TRACE("No context info for ctxIndex: %d, found: %d bailing out...\n", ctxIndex, foundCtx);
		//	break;
		//}
	}

	if (gnOpenContexts < gnAttachedDevices) {
		WACOM_TRACE("Did not open a context for each attached device");
	}

	return gnAttachedDevices > 0;
}

void CloseTabletContexts() {
	for (auto& ctx : gContextMap) {
		HCTX hctx = ctx.first;
		WACOM_TRACE("Closing context: 0x%X\n", hctx);
		gpWTClose(hctx);
	}

	gContextMap.clear();
	gnOpenContexts = 0;
	gnAttachedDevices = 0;
}

LRESULT FAR PASCAL MainWndProc(HWND hWnd, unsigned message, WPARAM wParam, LPARAM lParam) {
	PACKET pkt = {};

	switch (message) {
	case WM_FOCUS_TABLET:
		WACOM_TRACE("WM_FOCUS_TABLET.\n");
		for (auto& ctx : gContextMap) {
			gpWTEnable(ctx.first, TRUE);
			gpWTOverlap(ctx.first, TRUE);
		}
		break;

	case WM_BLUR_TABLET:
		WACOM_TRACE("WM_BLUR_TABLET.\n");
		for (auto& ctx : gContextMap) {
			gpWTEnable(ctx.first, FALSE);
			gpWTOverlap(ctx.first, FALSE);
		}
		break;

	case WM_CREATE:
		if (!OpenTabletContexts(hWnd)) {
			WACOM_TRACE("No tablets found.\n");
		} else {
			for (auto& ctx : gContextMap) {
				gpWTEnable(ctx.first, TRUE);
				gpWTOverlap(ctx.first, TRUE);
			}
		}
		break;

	case WT_PACKET: {
		HCTX hctx = (HCTX)lParam;

		if (hctx && gpWTPacket(hctx, wParam, &pkt)) {
			double pressure = (double)pkt.pkNormalPressure / (double)gContextMap[hctx].maxPressure;
			WACOM_TRACE("WT_PACKET x: %d y: %d pres: %d/%d butt: %x\n", pkt.pkX, pkt.pkY, pkt.pkNormalPressure, gContextMap[hctx].maxPressure, pkt.pkButtons);
			sprintf_s(buffer, "{\"p\":%f,\"b\":%d}", pressure, pkt.pkCursor);
			SendData(buffer);
		} else {
			SendError("got pinged by an unknown context");
			WACOM_TRACE("Got pinged by an unknown context: 0x%X\n", hctx);
		}
		break;
	}

	case WT_INFOCHANGE: {
		int nAttachedDevices = 0;
		gpWTInfoA(WTI_INTERFACE, IFC_NDEVICES, &nAttachedDevices);
		WACOM_TRACE("WT_INFOCHANGE detected; number of connected tablets is now: %i\n", nAttachedDevices);

		if (nAttachedDevices != gnAttachedDevices) {
			CloseTabletContexts();

			// Add some delay to give driver a chance to catch up in configuring
			// to the current attached tablets.  
			// 1 second seems to work - your mileage may vary...
			::Sleep(1000);

			// re-enumerate attached tablets
			OpenTabletContexts(hWnd);
		}

		if (gnAttachedDevices == 0) {
			//SendError("no tablets found");
			WACOM_TRACE("No tablets found\n");
		}
		break;
	}

	case WT_PROXIMITY: {
		HCTX hctx = (HCTX)wParam;
		bool entering = (HIWORD(lParam) != 0);

		WACOM_TRACE("PROX %d\n", entering);
		//WACOM_TRACE("WT_PROXIMITY %d %d\n", hctx, entering);
		if (hctx && entering) {
			gpWTEnable(hctx, TRUE);
			if (gContextMap.count(hctx) > 0) {
				auto info = gContextMap[hctx];
				WACOM_TRACE("Tablet name: %s, count: %i\n", info.name, gnAttachedDevices);
				sprintf_s(buffer, "{\"name\":\"%s\"}", info.name);
				SendData(buffer);
			} else {
				SendError("couldn't find context");
				WACOM_TRACE("Couldn't find context: 0x%X\n", hctx);
			}
		}
		break;
	}

	case WM_DESTROY:
		CloseTabletContexts();
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

DWORD WINAPI ThreadFunc(LPVOID lpParam) {
	HINSTANCE hInstance = GetModuleHandle(NULL);

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
	if (!RegisterClass(&wc)) return FALSE;

	hInst = hInstance;

	if (!LoadWintab()) {
		SendError("Wintab not available");
		return FALSE;
	}

	if (!gpWTInfoA(0, 0, NULL)) {
		SendError("WinTab Services Not Available");
		return FALSE;
	}

	hWnd = CreateWindowA("StylusPressurePluginWClass", "test", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
	//hWnd = CreateWindowA("StylusPressurePluginWClass", "test", WS_OVERLAPPEDWINDOW, 0, 0, 500, 500, NULL, NULL, hInstance, NULL);
	//ShowWindow(hWnd, SW_SHOW);

	if (!hWnd) {
		SendError("Could not create window!");
		return FALSE;
	}

	MSG msg;

	while (GetMessageA(&msg, hWnd, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}

int main() {
	UINT length;
	char buffer[1024];

	_setmode(_fileno(stdout), _O_BINARY);
	_setmode(_fileno(stdin), _O_BINARY);

	sprintf_s(buffer, "{\"version\":\"%s\"}", version);
	SendData(buffer);

	DWORD threadId = 0;
	HANDLE thread = CreateThread(NULL, 0, ThreadFunc, NULL, 0, &threadId);

#ifdef DEBUG
	while (1) {
		PostMessage(hWnd, WM_FOCUS_TABLET, 0, 0);
		SetFocus(hWnd);
		::Sleep(5000);
	}
#endif

	PostMessage(hWnd, WM_FOCUS_TABLET, 0, 0);

	while (fread(&length, 4, 1, stdin)) {
		if (length > 1023 || fread(buffer, 1, length, stdin) != length)
			break;

		buffer[length] = '\0';

		if (strcmp(buffer, "\"focus\"") == 0) {
			PostMessage(hWnd, WM_FOCUS_TABLET, 0, 0);
		} else if (strcmp(buffer, "\"blur\"") == 0) {
			PostMessage(hWnd, WM_BLUR_TABLET, 0, 0);
		}
	}

	PostMessage(hWnd, WM_CLOSE, 0, 0);
	WaitForSingleObject(thread, 1000);
	WACOM_TRACE("Cleanup()\n");
	UnloadWintab();
	return 0;
}
