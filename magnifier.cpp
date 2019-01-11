// Ensure that the following definition is in effect before winuser.h is included.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501    
#endif

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "magnification.lib")

#include <windows.h>
#include <wincodec.h>
#include <magnification.h>

// Constants.
#define MAGFACTOR  2.0f
#define TIMERINTERVAL 16 // close to the refresh rate @60hz
#define WINDOWTITLE "Magnifier"
#define WINDOWCLASSNAME "MagnifierWindow"

// Global variables and strings.
HINSTANCE           hInst;
HWND                hwndMag;
HWND                hwndHost;
RECT                magWindowRect;
DWORD               diameter;
FLOAT               magfactor;
BOOL                showCursor;

// Forward declarations.
BOOL                SetupMagnifier(HINSTANCE hinst);
LRESULT CALLBACK    HostWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL                UpdateMagnification();
void                UpdateSize();
void                ToggleCursor();
void                UpdateMagWindow();
void CALLBACK       UpdateMagWindowCallback(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE /*hPrevInstance*/,
                     _In_ LPSTR     /*lpCmdLine*/,
                     _In_ int       nCmdShow)
{
    MSG msg = {};
    UINT_PTR timerId = NULL;

    if (!MagInitialize())
    {
        return 0;
    }

    if (!SetupMagnifier(hInstance))
    {
        return 0;
    }

    ShowWindow(hwndHost, nCmdShow);
    UpdateWindow(hwndHost);

    // Create a timer to update the control.
    timerId = SetTimer(hwndHost, 0, TIMERINTERVAL, UpdateMagWindowCallback);

    // Main message loop.
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Shut down.
    KillTimer(NULL, timerId);
    MagUninitialize();
    return (int)msg.wParam;
}

LRESULT CALLBACK HostWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) 
    {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            // Close window.
            PostQuitMessage(0);
        }
        else if (wParam == VK_OEM_PLUS || wParam == VK_ADD)
        {
            if (GetKeyState(VK_SHIFT) < 0) // Shift key pressed
            {
                // Zoom in.
                magfactor += 1.0f;
                UpdateMagnification();
            }
            else
            {
                // Enlarge window.
                diameter += GetSystemMetrics(SM_CYSCREEN) / 8;
                UpdateSize();
            }
        }
        else if (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT)
        {
            if (GetKeyState(VK_SHIFT) < 0) // Shift key pressed
            {
                // Zoom out.
                magfactor -= 1.0f;
                UpdateMagnification();
            }
            else
            {
                // Shrink window.
                diameter -= GetSystemMetrics(SM_CYSCREEN) / 8;
                UpdateSize();
            }
        }
        else if (wParam == '0' || wParam == VK_NUMPAD0)
        {
            if (GetKeyState(VK_SHIFT) < 0) // Shift key pressed
            {
                // Reset zoom.
                magfactor = 2.0f;
                UpdateMagnification();
            }
            else
            {
                // Reset window size.
                diameter = GetSystemMetrics(SM_CYSCREEN) / 4;
                UpdateSize();
            }
        }
        break;

    case WM_LBUTTONDBLCLK:
        // Toggle magnified cursor.
        ToggleCursor();
        break;

    case WM_LBUTTONDOWN:
        // Drag window.
        ReleaseCapture();
        SendMessage(hWnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
        break;

    case WM_RBUTTONDBLCLK:
    case WM_DESTROY:
        // Close window.
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

BOOL SetupMagnifier(HINSTANCE hinst)
{
    WNDCLASSEX wcex = {};
    HRGN hRgn = NULL;

    // Register the window class for the window that contains the magnification control.
    wcex.cbSize         = sizeof(WNDCLASSEX); 
    wcex.style          = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc    = HostWndProc;
    wcex.hInstance      = hInst;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(1 + COLOR_BTNFACE);
    wcex.lpszClassName  = WINDOWCLASSNAME;
    RegisterClassEx(&wcex);

    // Set bounds of host window according to screen size.
    diameter = GetSystemMetrics(SM_CYSCREEN) / 4;

    // Create the host window.
    hwndHost = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED,
                              WINDOWCLASSNAME,
                              WINDOWTITLE,
                              WS_CLIPCHILDREN | WS_POPUP,
                              0,
                              0,
                              diameter,
                              diameter,
                              NULL,
                              NULL,
                              hInst,
                              NULL);
    if (!hwndHost)
    {
        return FALSE;
    }

    SetCapture(hwndHost);

    // Make the window circular.
    hRgn = CreateEllipticRgn(0, 0, diameter, diameter);
    SetWindowRgn(hwndHost, hRgn, TRUE);

    // Make the window opaque.
    SetLayeredWindowAttributes(hwndHost, 0, 255, LWA_ALPHA);

    // Create a magnifier control that fills the client area.
    GetClientRect(hwndHost, &magWindowRect);
    hwndMag = CreateWindow(WINDOWTITLE,
                           WINDOWCLASSNAME,
                           WS_CHILD | WS_VISIBLE,
                           magWindowRect.left,
                           magWindowRect.top,
                           magWindowRect.right,
                           magWindowRect.bottom,
                           hwndHost,
                           NULL,
                           hInst,
                           NULL);
    if (!hwndMag)
    {
        return FALSE;
    }

    magfactor = MAGFACTOR;
    if (!UpdateMagnification())
    {
        return FALSE;
    }


    showCursor = TRUE;
    ToggleCursor();

    return TRUE;
}

void ToggleCursor()
{
    showCursor = !showCursor;

    if (showCursor)
    {
        SetWindowLong(hwndMag, GWL_STYLE, GetWindowLong(hwndMag, GWL_STYLE) | MS_SHOWMAGNIFIEDCURSOR);
    }
    else
    {
        SetWindowLong(hwndMag, GWL_STYLE, GetWindowLong(hwndMag, GWL_STYLE) & ~MS_SHOWMAGNIFIEDCURSOR);
    }

}

BOOL UpdateMagnification()
{
    MAGTRANSFORM matrix = {};

    if (magfactor < 2.0f)
    {
        magfactor = 2.0f;
    }
    else if (magfactor > 10.0f)
    {
        magfactor = 10.0f;
    }

    // Set the magnification factor.
    memset(&matrix, 0, sizeof(matrix));
    matrix.v[0][0] = magfactor;
    matrix.v[1][1] = magfactor;
    matrix.v[2][2] = 1.0f;

    return MagSetWindowTransform(hwndMag, &matrix);
}

void UpdateSize()
{
    RECT hostWindowRect = {};
    HRGN hRgn = NULL;

    if (diameter < GetSystemMetrics(SM_CYSCREEN) / 4)
    {
        diameter = GetSystemMetrics(SM_CYSCREEN) / 4;
    }
    else if (diameter > GetSystemMetrics(SM_CYSCREEN))
    {
        diameter = GetSystemMetrics(SM_CYSCREEN);
    }

    // Update host window size.
    GetWindowRect(hwndHost, &hostWindowRect);
    SetWindowPos(hwndHost,
                 HWND_TOPMOST,
                 hostWindowRect.left,
                 hostWindowRect.top,
                 hostWindowRect.left + diameter,
                 hostWindowRect.top + diameter,
                 SWP_NOACTIVATE);

    // Update circle size.
    hRgn = CreateEllipticRgn(0, 0, diameter, diameter);
    SetWindowRgn(hwndHost, hRgn, TRUE);

    // Update magnifier size.
    GetClientRect(hwndHost, &magWindowRect);
    SetWindowPos(hwndMag, NULL, magWindowRect.left, magWindowRect.top, magWindowRect.right, magWindowRect.bottom, 0);

    UpdateMagWindow();
}

void UpdateMagWindow()
{
    int width = 0;
    int height = 0;
    int centerx = 0;
    int centery = 0;
    RECT sourceRect = {};
    HRGN hRgn = NULL;

    // Set the source rectangle for the magnifier control.
    GetClientRect(hwndHost, &magWindowRect);
    width = (int)((magWindowRect.right - magWindowRect.left) / magfactor);
    height = (int)((magWindowRect.bottom - magWindowRect.top) / magfactor);
    centerx = magWindowRect.right / 2;
    centery = magWindowRect.bottom / 2;

    GetWindowRect(hwndHost, &magWindowRect);
    sourceRect.left = magWindowRect.left + centerx - width / 2;
    sourceRect.right = sourceRect.left + width;
    sourceRect.top = magWindowRect.top + centery - width / 2;
    sourceRect.bottom = sourceRect.top + height;

    MagSetWindowSource(hwndMag, sourceRect);

    // Reclaim topmost status, to prevent unmagnified menus from remaining in view. 
    SetWindowPos(hwndHost, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

    // Force redraw.
    InvalidateRect(hwndMag, NULL, TRUE);
}

void CALLBACK UpdateMagWindowCallback(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    UpdateMagWindow();
}
