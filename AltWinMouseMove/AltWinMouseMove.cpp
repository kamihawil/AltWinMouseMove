#include <windows.h>
#include <shellapi.h>

#define WM_TRAYICON (WM_USER + 1) // Define tray icon message
#define ID_TRAY_EXIT 1001         // Menu item ID for exit
#define ID_TRAY_PAUSE 1002        // Menu item ID for "Pause/Resume"


// Boolean for pausing the functionality via system tray
bool isPaused = false;


// Variables to track the state of the drag and resize
bool isDragging = false;
bool isResizing = false;
HWND targetWindow = nullptr;
POINT initialClickPosition;
RECT initialWindowRect;

// Enum to represent each corner for resizing
enum ResizeCorner {
    NONE, TOPLEFT, TOPRIGHT, BOTTOMLEFT, BOTTOMRIGHT
};
ResizeCorner resizeCorner = NONE;

// Helper function to determine the nearest corner only
ResizeCorner GetNearestCorner(POINT cursorPos, RECT windowRect) {
    int leftDist = abs(cursorPos.x - windowRect.left);
    int rightDist = abs(cursorPos.x - windowRect.right);
    int topDist = abs(cursorPos.y - windowRect.top);
    int bottomDist = abs(cursorPos.y - windowRect.bottom);

    // Check proximity to corners
    if (leftDist < rightDist) {
        if (topDist < bottomDist) {
            return TOPLEFT;
        }
        else {
            return BOTTOMLEFT;
        }
    }
    else {
        if (topDist < bottomDist) {
            return TOPRIGHT;
        }
        else {
            return BOTTOMRIGHT;
        }
    }
}

// Callback function for the low-level mouse hook
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (isPaused) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam); // Skip functionality if paused
    }

    if (nCode >= 0) {
        if (wParam == WM_LBUTTONDOWN) {
            // Check if the Alt key is being held
            if ((GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState(VK_LWIN) & 0x8000)) {
                // Get the window under the cursor
                POINT cursorPos;
                GetCursorPos(&cursorPos);
                HWND clickedWindow = WindowFromPoint(cursorPos);

                // Get the top-level parent window
                targetWindow = GetAncestor(clickedWindow, GA_ROOT);

                // Start dragging if a window was found
                if (targetWindow != nullptr) {
                    isDragging = true;
                    initialClickPosition = cursorPos;

                    // Get the initial position of the window
                    GetWindowRect(targetWindow, &initialWindowRect);

                    // Make the window in the foreground
                    SetForegroundWindow(targetWindow);

                    // Return non-zero to consume the click event
                    return 1;
                }
            }
        }
        else if (wParam == WM_RBUTTONDOWN) {
            // Check if the Alt key and Windows key is being held
            if ((GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState(VK_LWIN) & 0x8000)) {
                // Get the window under the cursor
                POINT cursorPos;
                GetCursorPos(&cursorPos);
                HWND clickedWindow = WindowFromPoint(cursorPos);

                // Get the top-level parent window
                targetWindow = GetAncestor(clickedWindow, GA_ROOT);

                // Start resizing if a window was found
                if (targetWindow != nullptr) {
                    isResizing = true;
                    initialClickPosition = cursorPos;

                    // Get the initial size of the window
                    GetWindowRect(targetWindow, &initialWindowRect);

                    // Determine the corner to resize from
                    resizeCorner = GetNearestCorner(cursorPos, initialWindowRect);

                    // Make the window in the foreground
                    SetForegroundWindow(targetWindow);

                    // Return non-zero to consume the click event
                    return 1;
                }
            }
        }
        else if (wParam == WM_LBUTTONUP && isDragging) {
            // Stop dragging when the left button is released
            isDragging = false;
            targetWindow = nullptr;
        }
        else if (wParam == WM_RBUTTONUP && isResizing) {
            // Stop resizing when the right button is released
            isResizing = false;
            targetWindow = nullptr;
            resizeCorner = NONE;
        }
        else if (wParam == WM_MOUSEMOVE) {
            POINT cursorPos;
            GetCursorPos(&cursorPos);

            if (isDragging && targetWindow != nullptr) {
                // Calculate new window position based on cursor movement
                int dx = cursorPos.x - initialClickPosition.x;
                int dy = cursorPos.y - initialClickPosition.y;
                SetWindowPos(targetWindow, nullptr, initialWindowRect.left + dx, initialWindowRect.top + dy, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
            }
            else if (isResizing && targetWindow != nullptr) {
                // Calculate new window size based on cursor movement
                int dx = cursorPos.x - initialClickPosition.x;
                int dy = cursorPos.y - initialClickPosition.y;
                int newLeft = initialWindowRect.left;
                int newTop = initialWindowRect.top;
                int newRight = initialWindowRect.right;
                int newBottom = initialWindowRect.bottom;

                // Adjust based on which corner is being resized
                switch (resizeCorner) {
                case TOPLEFT:
                    newLeft = initialWindowRect.left + dx;
                    newTop = initialWindowRect.top + dy;
                    break;
                case TOPRIGHT:
                    newRight = initialWindowRect.right + dx;
                    newTop = initialWindowRect.top + dy;
                    break;
                case BOTTOMLEFT:
                    newLeft = initialWindowRect.left + dx;
                    newBottom = initialWindowRect.bottom + dy;
                    break;
                case BOTTOMRIGHT:
                    newRight = initialWindowRect.right + dx;
                    newBottom = initialWindowRect.bottom + dy;
                    break;
                default:
                    break;
                }

                SetWindowPos(targetWindow, nullptr, newLeft, newTop, newRight - newLeft, newBottom - newTop, SWP_NOZORDER);
            }
        }
    }

    // Call the next hook in the chain
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// Window procedure for handling tray icon messages
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONDOWN) {
            // Show a context menu when right-clicking the tray icon
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_TRAY_PAUSE, isPaused ? L"Resume" : L"Pause");
            AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd); // Required for the menu to display correctly
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_PAUSE:
            isPaused = !isPaused; // Toggle the pause state
            break;

        case ID_TRAY_EXIT:
            PostQuitMessage(0); // Exit the application
            break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Step 1: Create a hidden window for tray icon messages
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"AltWinMouseMoveHiddenWindow";
    RegisterClass(&wc);

    HWND hwnd = CreateWindow(wc.lpszClassName, L"AltWinMouseMove", 0, 0, 0, 0, 0, nullptr, nullptr, wc.hInstance, nullptr);

    // Step 2: Set up the system tray icon
    NOTIFYICONDATA nid = { 0 };
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;              // Handle to the hidden window
    nid.uID = 1;                  // Unique ID for the icon
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON; // Message to handle tray events
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Default application icon
    wcscpy_s(nid.szTip, L"AltWinMouseMove"); // Tooltip text for the tray icon

    Shell_NotifyIcon(NIM_ADD, &nid);

    // Step 3: Set up a low-level mouse hook
    HHOOK mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, nullptr, 0);
    if (!mouseHook) {
        MessageBox(nullptr, L"Failed to set mouse hook", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Step 4: Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Step 5: Clean up
    Shell_NotifyIcon(NIM_DELETE, &nid); // Remove tray icon
    UnhookWindowsHookEx(mouseHook);    // Unhook mouse hook

    return 0;
}
