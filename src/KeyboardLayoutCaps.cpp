// KeyboardLayoutCaps.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "KeyboardLayoutCaps.h"

#define MAX_LOADSTRING 100

void GetKeyboardLayouts();
LRESULT CALLBACK LowLevelHookProc(int nCode, WPARAM wParam, LPARAM lParam);
HHOOK g_hHook = NULL;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Prevent from two copies of Recaps from running at the same time
    HANDLE mutex = CreateMutex(NULL, FALSE, L"KeyboardLayoutCaps");
    if (!mutex || WaitForSingleObject(mutex, 0) == WAIT_TIMEOUT) {
        MessageBox(NULL, L"KeyboardLayoutCaps is already running.", L"KeyboardLayoutCaps", MB_OK | MB_ICONINFORMATION);
        return 1;
    }

    GetKeyboardLayouts(); 

    // Set hook to capture CapsLock
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelHookProc, GetModuleHandle(NULL), 0);
    PostMessage(NULL, WM_NULL, 0, 0);

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}

#define MAX_LAYOUTS 256
#define MAXLEN 1024

struct KeyboardLayoutInfo {
    WCHAR names[MAX_LAYOUTS][MAXLEN];
    HKL   hkls[MAX_LAYOUTS];
    BOOL  inUse[MAX_LAYOUTS];
    UINT  count;
    UINT  current;
};

KeyboardLayoutInfo g_keyboardInfo = { 0 };


void GetKeyboardLayouts() {
    memset(&g_keyboardInfo, 0, sizeof(KeyboardLayoutInfo));
    g_keyboardInfo.count = GetKeyboardLayoutList(MAX_LAYOUTS, g_keyboardInfo.hkls);
    for (UINT i = 0; i < g_keyboardInfo.count; i++) {
        LANGID language = (LANGID)(((UINT)g_keyboardInfo.hkls[i]) & 0x0000FFFF); // bottom 16 bit of HKL
        LCID locale = MAKELCID(language, SORT_DEFAULT);
        GetLocaleInfo(locale, LOCALE_SLANGUAGE, g_keyboardInfo.names[i], MAXLEN);
        g_keyboardInfo.inUse[i] = TRUE;
    }
}


HWND RemoteGetFocus() {
    HWND hwnd = GetForegroundWindow();
    DWORD remoteThreadId = GetWindowThreadProcessId(hwnd, NULL);
    DWORD currentThreadId = GetCurrentThreadId();
    AttachThreadInput(remoteThreadId, currentThreadId, TRUE);
    HWND focused = GetFocus();
    AttachThreadInput(remoteThreadId, currentThreadId, FALSE);
    return focused;
}

HKL GetCurrentLayout() {
    HWND hwnd = RemoteGetFocus();
    DWORD threadId = GetWindowThreadProcessId(hwnd, NULL);
    return GetKeyboardLayout(threadId);
}

HKL ChangeInputLanguage(UINT newLanguage) {
    HWND hwnd = RemoteGetFocus();
    g_keyboardInfo.current = newLanguage;
    PostMessage(hwnd, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)(g_keyboardInfo.hkls[g_keyboardInfo.current]));
    return g_keyboardInfo.hkls[g_keyboardInfo.current];
}


void SwitchToLayoutNumber(int number) {
    HKL currentLayout = GetCurrentLayout();
    BOOL found = FALSE;
    UINT languageIndex;
    for (UINT i = 0; i < g_keyboardInfo.count; i++) {
        if (!g_keyboardInfo.inUse[i]) {
            continue;
        }
        if (number-- == 0) {
            if (g_keyboardInfo.hkls[i] != currentLayout) {
                found = TRUE;
                languageIndex = i;
            }
            break;
        }
    }

    if (found) {
        ChangeInputLanguage(languageIndex);
    }
}

BOOL IsKeyPressed(int nVirtKey) {
    return (GetKeyState(nVirtKey) & 0x80000000) > 0;
}


LRESULT CALLBACK LowLevelHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0) return CallNextHookEx(g_hHook, nCode, wParam, lParam);

    KBDLLHOOKSTRUCT* data = (KBDLLHOOKSTRUCT*)lParam;

    if ((data->flags & LLKHF_ALTDOWN) == 0) {
        //WM_KEYDOWN WM_KEYUP WM_SYSKEYDOWN WM_SYSKEYUP
        if (wParam == WM_KEYDOWN) {
            switch (data->vkCode) {
            case VK_CAPITAL:
                if (IsKeyPressed(VK_SHIFT))
                {
                    SwitchToLayoutNumber(1);
                }
                else
                {
                    SwitchToLayoutNumber(0);
                }
                return 1; // prevent windows from handling the keystroke
            default:
                break;
            }
        }
    }

    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}
