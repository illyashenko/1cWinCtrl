#include "stdafx.h"
#include "WindowMngr.h"
#include "json_ext.h"

#ifdef __linux__

#include "XWinBase.h"

class WindowList : public WindowEnumerator
{
protected:
	virtual bool EnumWindow(Window window) {
		JSON j;
		j["Window"] = window;
		j["Owner"] = GetWindowOwner(window);
		j["Class"] = GetWindowClass(window);
		j["Title"] = GetWindowTitle(window);
		j["ProcessId"] = GetWindowPid(window);
		json.push_back(j);
		return true;
	}
public:
	WindowList()
		: WindowEnumerator() {}
};

class GeometryHelper : public WindowHelper
{
public:
	GeometryHelper(Window window) {
		/* geometry */
		Window junkroot;
		int x, y, junkx, junky;
		unsigned int w, h, bw, depth;
		Status status = XGetGeometry(display, window, &junkroot, &junkx, &junky, &w, &h, &bw, &depth);
		if (!status) return;
		Bool ok = XTranslateCoordinates(display, window, junkroot, junkx, junky, &x, &y, &junkroot);
		if (!ok) return;
		json["left"] = x;
		json["top"] = y;
		json["width"] = w;
		json["height"] = h;
		json["right"] = x + w;
		json["bottom"] = y + h;
	}
};

std::wstring WindowManager::GetWindowList(tVariant* paParams, const long lSizeArray)
{
	return WindowList().Enumerate();
}

HWND WindowManager::ActiveWindow()
{
	return WindowHelper().GetActiveWindow();
}

BOOL WindowManager::Activate(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 1) return false;
	Window window = VarToInt(paParams);
	WindowHelper().SetActiveWindow(window);
	return true;
}

BOOL  WindowManager::Restore(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 1) return false;
	Window window = VarToInt(paParams);
	WindowHelper().Maximize(window, false);
	return true;
}

BOOL  WindowManager::Maximize(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 1) return false;
	Window window = VarToInt(paParams);
	WindowHelper().Maximize(window, true);
	return true;
}

BOOL WindowManager::Minimize(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 1) return false;
	Window window = VarToInt(paParams);
	WindowHelper().Minimize(window);
	return true;
}

BOOL WindowManager::SetWindowSize(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 3) return false;
	Window window = VarToInt(paParams);
	int x = VarToInt(paParams + 1);
	int y = VarToInt(paParams + 2);
	WindowHelper().SetWindowSize(window, x, y);
	return true;
}

BOOL WindowManager::SetWindowPos(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 3) return false;
	Window window = VarToInt(paParams);
	int x = VarToInt(paParams + 1);
	int y = VarToInt(paParams + 2);
	WindowHelper().SetWindowPos(window, x, y);
	return true;
}

#else//__linux__

std::wstring WindowManager::GetWindowList(tVariant* paParams, const long lSizeArray)
{
	class Param {
	public:
		DWORD pid = 0;
		JSON json;
	};
	Param param;

	if (lSizeArray > 0) param.pid = VarToInt(paParams);

	BOOL bResult = ::EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL
		{
			if (::IsWindow(hWnd) && ::IsWindowVisible(hWnd)) {
				Param* p = (Param*)lParam;
				DWORD dwProcessId;
				::GetWindowThreadProcessId(hWnd, &dwProcessId);
				if (p->pid == 0 || p->pid == dwProcessId) {
					JSON j;
					j["Window"] = (INT64)hWnd;
					j["ProcessId"] = dwProcessId;
					j["Enabled"] = (boolean)::IsWindowEnabled(hWnd);
					j["Owner"] = (INT64)::GetWindow(hWnd, GW_OWNER);

					WCHAR buffer[256];
					::GetClassName(hWnd, buffer, 256);
					j["Class"] = WC2MB(buffer);

					const int length = GetWindowTextLength(hWnd);
					if (length != 0) {
						std::wstring text;
						text.resize(length);
						::GetWindowText(hWnd, &text[0], length + 1);
						j["Title"] = WC2MB(text);
					}
					p->json.push_back(j);
				}
			}
			return TRUE;
		}, (LPARAM)&param);

	return param.json;
}

HWND WindowManager::ActiveWindow()
{
	return ::GetForegroundWindow();
}

HWND WindowManager::CurrentWindow()
{
	DWORD pid = GetCurrentProcessId();
	std::pair<HWND, DWORD> params = { 0, pid };

	// Enumerate the windows using a lambda to process each window
	BOOL bResult = ::EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL
		{
			auto pParams = (std::pair<HWND, DWORD>*)(lParam);
			WCHAR buffer[256];
			DWORD processId;
			if (IsWindowVisible(hWnd)
				&& ::GetWindowThreadProcessId(hWnd, &processId)
				&& processId == pParams->second
				&& ::GetClassName(hWnd, buffer, 256)
				&& wcscmp(L"V8TopLevelFrameSDI", buffer) == 0
				) {
				// Stop enumerating
				SetLastError(-1);
				pParams->first = hWnd;
				return FALSE;
			}

			// Continue enumerating
			return TRUE;
		}, (LPARAM)&params);

	if (!bResult && GetLastError() == -1 && params.first)
	{
		return params.first;
	}

	return 0;
}

BOOL WindowManager::SetWindowSize(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 3) return false;
	HWND hWnd = VarToHwnd(paParams);
	int w = VarToInt(paParams + 1);
	int h = VarToInt(paParams + 2);
	::SetWindowPos(hWnd, 0, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER);
	::UpdateWindow(hWnd);
	return true;
}

BOOL WindowManager::SetWindowPos(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 3) return false;
	HWND hWnd = VarToHwnd(paParams);
	int x = VarToInt(paParams + 1);
	int y = VarToInt(paParams + 2);
	::SetWindowPos(hWnd, 0, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
	::UpdateWindow(hWnd);
	return true;
}

BOOL WindowManager::EnableResizing(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 2) return false;
	HWND hWnd = VarToHwnd(paParams);
	BOOL enable = VarToInt(paParams + 1);
	LONG style = ::GetWindowLong(hWnd, GWL_STYLE);
	style = enable ? (style | WS_SIZEBOX) : (style & ~WS_SIZEBOX);
	::SetWindowLong(hWnd, GWL_STYLE, style);
	::UpdateWindow(hWnd);
	return true;
}

std::wstring WindowManager::GetText(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 1) return false;
	HWND hWnd = VarToHwnd(paParams);
	const int length = ::GetWindowTextLength(hWnd);
	std::wstring text;
	if (length != 0) {
		text.resize(length);
		::GetWindowText(hWnd, &text[0], length + 1);
	}
	return text;
}

BOOL WindowManager::SetText(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 2) return false;
	HWND hWnd = VarToHwnd(paParams);
	return ::SetWindowText(hWnd, (paParams + 1)->pwstrVal);
}

BOOL WindowManager::Minimize(tVariant* paParams, const long lSizeArray)
{
	HWND hWnd = 0;
	if (lSizeArray > 0) hWnd = VarToHwnd(paParams);
	if (hWnd == 0) hWnd = ::GetForegroundWindow();
	return SetWindowState(hWnd, 0, true);
}

BOOL WindowManager::Restore(tVariant* paParams, const long lSizeArray)
{
	HWND hWnd = 0;
	if (lSizeArray > 0) hWnd = VarToHwnd(paParams);
	if (hWnd == 0) hWnd = ::GetForegroundWindow();
	return SetWindowState(hWnd, 1, true);
}

BOOL WindowManager::Maximize(tVariant* paParams, const long lSizeArray)
{
	HWND hWnd = 0;
	if (lSizeArray > 0) hWnd = VarToHwnd(paParams);
	if (hWnd == 0) hWnd = ::GetForegroundWindow();
	return SetWindowState(hWnd, 2, true);
}

BOOL WindowManager::Activate(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 1) return false;
	HWND hWnd = VarToHwnd(paParams);

	if (IsWindow(hWnd)) {
		if (IsWindowVisible(hWnd)) {
			WINDOWPLACEMENT place;
			memset(&place, 0, sizeof(WINDOWPLACEMENT));
			place.length = sizeof(WINDOWPLACEMENT);
			GetWindowPlacement(hWnd, &place);
			if (place.showCmd == SW_SHOWMINIMIZED) ShowWindow(hWnd, SW_RESTORE);
			SetForegroundWindow(hWnd);
		}
	}
	return true;
}

long WindowManager::GetWindowState(tVariant* paParams, const long lSizeArray)
{
	HWND hWnd = 0;
	if (lSizeArray > 0) hWnd = VarToHwnd(paParams);
	if (hWnd == 0) hWnd = ::GetForegroundWindow();

	WINDOWPLACEMENT place;
	memset(&place, 0, sizeof(WINDOWPLACEMENT));
	place.length = sizeof(WINDOWPLACEMENT);
	if (!GetWindowPlacement(hWnd, &place)) return -1;

	switch (place.showCmd) {
	case SW_SHOWMINIMIZED:
		return 0;
	case SW_SHOWNORMAL:
		return 1;
	case SW_SHOWMAXIMIZED:
		return 2;
	default:
		return -1;
	}
}

BOOL WindowManager::SetWindowState(tVariant* paParams, const long lSizeArray)
{
	HWND hWnd = 0;
	int iMode = 1;
	bool bActivate = true;
	if (lSizeArray > 0) hWnd = VarToHwnd(paParams);
	if (lSizeArray > 1) iMode = (paParams + 1)->intVal;
	if (lSizeArray > 2) bActivate = (paParams + 2)->bVal;
	if (hWnd == 0) hWnd = ::GetForegroundWindow();
	return SetWindowState(hWnd, iMode, bActivate);
}

BOOL WindowManager::SetWindowState(HWND hWnd, int iMode, bool bActivate)
{
	if (IsWindow(hWnd)) {
		if (IsWindowVisible(hWnd)) {
			int nCmdShow;
			if (bActivate) {
				switch (iMode) {
				case 0: nCmdShow = SW_SHOWMINIMIZED; break;
				case 2: nCmdShow = SW_SHOWMAXIMIZED; break;
				default: nCmdShow = SW_RESTORE;
				}
			}
			else {
				switch (iMode) {
				case 0: nCmdShow = SW_SHOWMINNOACTIVE; break;
				case 2: nCmdShow = SW_MAXIMIZE; break;
				default: nCmdShow = SW_RESTORE;
				}
			}
			::ShowWindow(hWnd, nCmdShow);
		}
	}
	return true;
}

#endif//__linux__