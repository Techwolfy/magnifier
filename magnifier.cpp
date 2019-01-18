// Ensure that the following definition is in effect before winuser.h is included.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#endif

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "magnification.lib")

#include <windows.h>
#include <wincodec.h>
#include <magnification.h>

// Constants.
#define BORDERWIDTH 1
#define MAGFACTOR  2.0f
#define TIMERINTERVAL 16 // close to the refresh rate @60hz
#define WINDOWTITLE "Magnifier"
#define WINDOWCLASSNAME "MagnifierWindow"

// Global variables and strings.
HWND                hwndMag;
HWND                hwndHost;
DWORD               diameter;
FLOAT               magfactor;
BOOL                showCursor;
BOOL                showBorder;

// Forward declarations.
BOOL                SetupMagnifier(HINSTANCE hInstance);
LRESULT CALLBACK    HostWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void                ToggleCursor();
BOOL                UpdateMagnification();
void                OnPaint();
void                UpdateSize();
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
    RedrawWindow(hwndHost, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);

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
        else if (wParam == 'B')
        {
            showBorder = !showBorder;
            RedrawWindow(hwndHost, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
        }
        else if (wParam == 'C')
        {
            ToggleCursor();
        }
        else if (wParam == VK_OEM_PLUS || wParam == VK_ADD)
        {
            if (GetKeyState(VK_CONTROL) < 0) // Control key pressed
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
            if (GetKeyState(VK_CONTROL) < 0) // Control key pressed
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
            if (GetKeyState(VK_CONTROL) < 0) // Control key pressed
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

    case WM_MBUTTONDOWN:
        // Toggle magnified cursor.
        ToggleCursor();
        break;

    case WM_LBUTTONDOWN:
        // Drag window.
        ReleaseCapture();
        SendMessage(hWnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
        break;

    case WM_HOTKEY:
        // Minimize/restore window.
        if (IsIconic(hwndHost))
        {
            ShowWindow(hwndHost, SW_RESTORE);
        }
        else
        {
            ShowWindow(hwndHost, SW_MINIMIZE);
        }
        break;

    case WM_LBUTTONDBLCLK:
    case WM_DESTROY:
        // Close window.
        PostQuitMessage(0);
        break;

    case WM_PAINT:
        // Repaint window.
        OnPaint();
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

BOOL SetupMagnifier(HINSTANCE hInstance)
{
    WNDCLASSEX wcex = {};
    HRGN hRgn = NULL;
    RECT hostWindowRect = {};

    // Register the window class for the window that contains the magnification control.
    wcex.cbSize         = sizeof(WNDCLASSEX); 
    wcex.style          = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc    = HostWndProc;
    wcex.hInstance      = hInstance;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = NULL;
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
                              hInstance,
                              NULL);
    if (!hwndHost)
    {
        return FALSE;
    }

    SetCapture(hwndHost);

    // Set a global hotkey to show/hide the window.
    RegisterHotKey(hwndHost, 0, MOD_NOREPEAT | MOD_CONTROL | MOD_ALT, 'M');

    // Make the window opaque.
    SetLayeredWindowAttributes(hwndHost, RGB(0, 0, 255), 255, LWA_COLORKEY);

    // Create a magnifier control that fills the client area.
    GetClientRect(hwndHost, &hostWindowRect);
    hwndMag = CreateWindow(WINDOWTITLE,
                           WINDOWCLASSNAME,
                           WS_CHILD | WS_VISIBLE,
                           BORDERWIDTH,
                           BORDERWIDTH,
                           diameter - (BORDERWIDTH * 2),
                           diameter - (BORDERWIDTH * 2),
                           hwndHost,
                           NULL,
                           hInstance,
                           NULL);
    if (!hwndMag)
    {
        return FALSE;
    }

    UpdateSize();

    magfactor = MAGFACTOR;
    if (!UpdateMagnification())
    {
        return FALSE;
    }

    showCursor = TRUE;
    ToggleCursor();

    // Repaint host window.
    showBorder = TRUE;
    RedrawWindow(hwndHost, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);

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

void OnPaint()
{
    PAINTSTRUCT ps = {};
    HPEN hPenBg = NULL;
    HPEN hPenDark = NULL;
    HPEN hPenLight = NULL;
    HBRUSH hBrushBg = NULL;
    HBRUSH hBrushDark = NULL;
    HBRUSH hBrushLight = NULL;

    BeginPaint(hwndHost, &ps);
    hPenBg = CreatePen(PS_SOLID, diameter, RGB(0, 0, 255));
    hPenDark = CreatePen(PS_SOLID, 0, RGB(63, 63, 63));
    hPenLight = CreatePen(PS_SOLID, 0, RGB(127, 127, 127));
    hBrushBg = CreateSolidBrush(RGB(0, 0, 255));
    hBrushDark = CreateSolidBrush(RGB(63, 63, 63));
    hBrushLight = CreateSolidBrush(RGB(127, 127, 127));

    // Make entire window transparent.
    SelectObject(ps.hdc, hPenBg);
    SelectObject(ps.hdc, hBrushBg);
    Rectangle(ps.hdc,
              0,
              0,
              diameter,
              diameter);

    //Draw border.
    if (showBorder)
    {
        SelectObject(ps.hdc, hPenDark);
        SelectObject(ps.hdc, hBrushDark);
        Ellipse(ps.hdc,
                0,
                0,
                diameter,
                diameter);

        SelectObject(ps.hdc, hPenLight);
        SelectObject(ps.hdc, hBrushLight);
        Ellipse(ps.hdc,
                1,
                1,
                diameter - 1,
                diameter - 1);
    }

    DeleteObject(hPenBg);
    DeleteObject(hPenDark);
    DeleteObject(hPenLight);
    DeleteObject(hBrushBg);
    DeleteObject(hBrushDark);
    DeleteObject(hBrushLight);
    EndPaint(hwndHost, &ps);
}

void UpdateSize()
{
    HRGN hRgn = NULL;
    RECT hostWindowRect = {};

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
                 diameter,
                 diameter,
                 SWP_NOACTIVATE);

    // Update host window circle size.
    hRgn = CreateEllipticRgn(0, 0, diameter + 1, diameter + 1);
    SetWindowRgn(hwndHost, hRgn, TRUE);

    // Update magnifier circle size.
    hRgn = CreateEllipticRgn(BORDERWIDTH,
                             BORDERWIDTH,
                             diameter - (BORDERWIDTH * 2),
                             diameter - (BORDERWIDTH * 2));
    SetWindowRgn(hwndMag, hRgn, TRUE);

    // Update magnifier size.
    SetWindowPos(hwndMag,
                 NULL,
                 BORDERWIDTH,
                 BORDERWIDTH,
                 diameter - (BORDERWIDTH * 2),
                 diameter - (BORDERWIDTH * 2),
                 0);

    UpdateMagWindow();

    // Repaint host window.
    RedrawWindow(hwndHost, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

void UpdateMagWindow()
{
    RECT magWindowRect = {};
    RECT sourceRect = {};
    int width = 0;
    int height = 0;
    int centerx = 0;
    int centery = 0;

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
}

void CALLBACK UpdateMagWindowCallback(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    UpdateMagWindow();
}
