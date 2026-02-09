#include <windows.h>
#include <commctrl.h>

typedef struct {
    BOOL enabled;
    int cps;
    DWORD lastClickTime;
    int toggle_hotkey;
    BOOL mouse_down;
} ClickerSettings;

ClickerSettings leftClicker = {0, 10, 0, 0, 0};
ClickerSettings rightClicker = {0, 10, 0, 0, 0};
HWND g_hMainWnd = NULL;
HWND g_targetWindow = NULL;
HHOOK g_keyboardHook = NULL;
HHOOK g_mouseHook = NULL;
BOOL g_targetWindowActive = FALSE;

#define IDC_LEFT_ENABLE       101
#define IDC_RIGHT_ENABLE      102
#define IDC_LEFT_CPS_SLIDER   103
#define IDC_RIGHT_CPS_SLIDER  104
#define IDC_LEFT_CPS_TEXT     105
#define IDC_RIGHT_CPS_TEXT    106
#define IDC_LEFT_HOTKEY       107
#define IDC_RIGHT_HOTKEY      108
#define IDC_STATUS_TEXT       109
#define IDT_CLICK_TIMER       110
#define IDT_WINDOW_CHECK      111

static int g_settingHotkeyFor = 0;

const char* GetKeyName(int vkCode) {
    static char keyName[20];
    
    if (vkCode >= 'A' && vkCode <= 'Z') {
        keyName[0] = (char)vkCode;
        keyName[1] = 0;
        return keyName;
    }
    if (vkCode >= '0' && vkCode <= '9') {
        keyName[0] = (char)vkCode;
        keyName[1] = 0;
        return keyName;
    }
    
    switch (vkCode) {
        case VK_F1: return "F1"; case VK_F2: return "F2"; case VK_F3: return "F3"; case VK_F4: return "F4";
        case VK_F5: return "F5"; case VK_F6: return "F6"; case VK_F7: return "F7"; case VK_F8: return "F8";
        case VK_F9: return "F9"; case VK_F10: return "F10"; case VK_F11: return "F11"; case VK_F12: return "F12";
        case VK_HOME: return "Home"; case VK_END: return "End"; 
        case VK_PRIOR: return "PgUp"; case VK_NEXT: return "PgDn";
        case VK_INSERT: return "Ins"; case VK_DELETE: return "Del";
        default: return "Key";
    }
}

void FindTargetWindow() {
    g_targetWindow = FindWindowA("LWJGL", NULL);
}

BOOL IsTargetWindowActive() {
    if (!g_targetWindow) return FALSE;
    return GetForegroundWindow() == g_targetWindow;
}

void HandleClick(ClickerSettings* clicker, UINT downMsg, UINT upMsg) {
    if (!g_targetWindowActive || !clicker->enabled || !clicker->mouse_down || !g_targetWindow) return;
    
    DWORD now = GetTickCount();
    int cps = clicker->cps < 1 ? 1 : clicker->cps;
    DWORD delay = 1000 / cps;
    
    if ((now - clicker->lastClickTime) >= delay) {
        PostMessageA(g_targetWindow, downMsg, downMsg==WM_LBUTTONDOWN?MK_LBUTTON:MK_RBUTTON, 0);
        Sleep(1);
        PostMessageA(g_targetWindow, upMsg, 0, 0);
        clicker->lastClickTime = now;
    }
}

LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKeyboard = (KBDLLHOOKSTRUCT*)lParam;
        
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            int vkCode = pKeyboard->vkCode;
            
            if (g_settingHotkeyFor) {
                if (g_settingHotkeyFor == 1) {
                    leftClicker.toggle_hotkey = vkCode;
                    SetWindowTextA(GetDlgItem(g_hMainWnd, IDC_LEFT_HOTKEY), GetKeyName(vkCode));
                    g_settingHotkeyFor = 0;
                } else if (g_settingHotkeyFor == 2) {
                    rightClicker.toggle_hotkey = vkCode;
                    SetWindowTextA(GetDlgItem(g_hMainWnd, IDC_RIGHT_HOTKEY), GetKeyName(vkCode));
                    g_settingHotkeyFor = 0;
                }
                return 1;
            }
            
            if (vkCode == leftClicker.toggle_hotkey && leftClicker.toggle_hotkey) {
                if (g_targetWindowActive) {
                    leftClicker.enabled = !leftClicker.enabled;
                    CheckDlgButton(g_hMainWnd, IDC_LEFT_ENABLE, leftClicker.enabled ? 1 : 0);
                }
                return 1;
            }
            
            if (vkCode == rightClicker.toggle_hotkey && rightClicker.toggle_hotkey) {
                if (g_targetWindowActive) {
                    rightClicker.enabled = !rightClicker.enabled;
                    CheckDlgButton(g_hMainWnd, IDC_RIGHT_ENABLE, rightClicker.enabled ? 1 : 0);
                }
                return 1;
            }
        }
    }
    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && g_targetWindowActive) {
        if (wParam == WM_LBUTTONDOWN) leftClicker.mouse_down = 1;
        else if (wParam == WM_LBUTTONUP) leftClicker.mouse_down = 0;
        else if (wParam == WM_RBUTTONDOWN) rightClicker.mouse_down = 1;
        else if (wParam == WM_RBUTTONUP) rightClicker.mouse_down = 0;
    }
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hMainWnd = hwnd;
            FindTargetWindow();
            
            // 超简界面 - 每行一个功能
            CreateWindowA("BUTTON", "L: Enable", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
                          10, 10, 80, 25, hwnd, (HMENU)IDC_LEFT_ENABLE, 0, 0);
            HWND hLeftSlider = CreateWindowA(TRACKBAR_CLASS, "", WS_CHILD | WS_VISIBLE | TBS_HORZ, 
                          100, 10, 120, 25, hwnd, (HMENU)IDC_LEFT_CPS_SLIDER, 0, 0);
            SendMessage(hLeftSlider, TBM_SETRANGE, 1, MAKELPARAM(1, 50));
            SendMessage(hLeftSlider, TBM_SETPOS, 1, 10);
            CreateWindowA("STATIC", "10", WS_CHILD | WS_VISIBLE, 230, 13, 20, 20, 
                          hwnd, (HMENU)IDC_LEFT_CPS_TEXT, 0, 0);
            CreateWindowA("BUTTON", "Set Key", WS_CHILD | WS_VISIBLE, 260, 10, 80, 25, 
                          hwnd, (HMENU)IDC_LEFT_HOTKEY, 0, 0);
            
            CreateWindowA("BUTTON", "R: Enable", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
                          10, 45, 80, 25, hwnd, (HMENU)IDC_RIGHT_ENABLE, 0, 0);
            HWND hRightSlider = CreateWindowA(TRACKBAR_CLASS, "", WS_CHILD | WS_VISIBLE | TBS_HORZ, 
                          100, 45, 120, 25, hwnd, (HMENU)IDC_RIGHT_CPS_SLIDER, 0, 0);
            SendMessage(hRightSlider, TBM_SETRANGE, 1, MAKELPARAM(1, 50));
            SendMessage(hRightSlider, TBM_SETPOS, 1, 10);
            CreateWindowA("STATIC", "10", WS_CHILD | WS_VISIBLE, 230, 48, 20, 20, 
                          hwnd, (HMENU)IDC_RIGHT_CPS_TEXT, 0, 0);
            CreateWindowA("BUTTON", "Set Key", WS_CHILD | WS_VISIBLE, 260, 45, 80, 25, 
                          hwnd, (HMENU)IDC_RIGHT_HOTKEY, 0, 0);
            
            CreateWindowA("STATIC", g_targetWindow ? "Ready (LWJGL)" : "No Target", 
                          WS_CHILD | WS_VISIBLE, 10, 80, 330, 20, hwnd, (HMENU)IDC_STATUS_TEXT, 0, 0);
            
            g_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, 0, 0);
            g_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, 0, 0);
            
            SetTimer(hwnd, IDT_CLICK_TIMER, 1, 0);
            SetTimer(hwnd, IDT_WINDOW_CHECK, 100, 0);
            break;
        }
        
        case WM_HSCROLL: {
            HWND hSlider = (HWND)lParam;
            char text[4];
            if (hSlider == GetDlgItem(hwnd, IDC_LEFT_CPS_SLIDER)) {
                leftClicker.cps = (int)SendMessage(hSlider, TBM_GETPOS, 0, 0);
                wsprintfA(text, "%d", leftClicker.cps);
                SetWindowTextA(GetDlgItem(hwnd, IDC_LEFT_CPS_TEXT), text);
            }
            else if (hSlider == GetDlgItem(hwnd, IDC_RIGHT_CPS_SLIDER)) {
                rightClicker.cps = (int)SendMessage(hSlider, TBM_GETPOS, 0, 0);
                wsprintfA(text, "%d", rightClicker.cps);
                SetWindowTextA(GetDlgItem(hwnd, IDC_RIGHT_CPS_TEXT), text);
            }
            break;
        }
        
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            
            if (id == IDC_LEFT_ENABLE) {
                if (g_targetWindowActive) leftClicker.enabled = IsDlgButtonChecked(hwnd, IDC_LEFT_ENABLE) == 1;
                else CheckDlgButton(hwnd, IDC_LEFT_ENABLE, 0);
            }
            else if (id == IDC_RIGHT_ENABLE) {
                if (g_targetWindowActive) rightClicker.enabled = IsDlgButtonChecked(hwnd, IDC_RIGHT_ENABLE) == 1;
                else CheckDlgButton(hwnd, IDC_RIGHT_ENABLE, 0);
            }
            else if (id == IDC_LEFT_HOTKEY) {
                g_settingHotkeyFor = 1;
                SetWindowTextA(GetDlgItem(hwnd, IDC_STATUS_TEXT), "Press key for L");
            }
            else if (id == IDC_RIGHT_HOTKEY) {
                g_settingHotkeyFor = 2;
                SetWindowTextA(GetDlgItem(hwnd, IDC_STATUS_TEXT), "Press key for R");
            }
            break;
        }
        
        case WM_TIMER: {
            if (wParam == IDT_CLICK_TIMER) {
                if (leftClicker.enabled && leftClicker.mouse_down) HandleClick(&leftClicker, WM_LBUTTONDOWN, WM_LBUTTONUP);
                if (rightClicker.enabled && rightClicker.mouse_down) HandleClick(&rightClicker, WM_RBUTTONDOWN, WM_RBUTTONUP);
            }
            else if (wParam == IDT_WINDOW_CHECK) {
                BOOL wasActive = g_targetWindowActive;
                g_targetWindowActive = IsTargetWindowActive();
                if (wasActive != g_targetWindowActive) {
                    SetWindowTextA(GetDlgItem(hwnd, IDC_STATUS_TEXT), 
                        g_targetWindowActive ? "Active - Click Ready" : "Inactive");
                    if (!g_targetWindowActive) {
                        leftClicker.mouse_down = 0;
                        rightClicker.mouse_down = 0;
                    }
                }
            }
            break;
        }
        
        case WM_DESTROY: {
            if (g_mouseHook) UnhookWindowsHookEx(g_mouseHook);
            if (g_keyboardHook) UnhookWindowsHookEx(g_keyboardHook);
            PostQuitMessage(0);
            break;
        }
        
        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);
    WNDCLASSEXA wc;
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "AC";
    wc.hIconSm = NULL;
    
    RegisterClassExA(&wc);
    HWND hwnd = CreateWindowExA(0, "AC", "AutoClicker", 
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                100, 100, 360, 125, 0, 0, hInstance, 0);
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}
