#define NOMINMAX
#include <Windows.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <psapi.h>
#include <shlobj.h>

#define my_assert(EXP) if (!(EXP)) {MessageBox(NULL, "This expression is false: "#EXP, "Window Tiler Assert", MB_OK|MB_ICONSTOP); exit(1);}
#define limit_string(char_array) char_array[sizeof(char_array)-1] = '\0'
#define limit_count(count,limit) count = std::max(0, std::min(count, limit));

template <typename T> void read_assert(FILE* fptr, T& dst) {
    my_assert(fread(&dst, sizeof(dst), 1, fptr) == 1);
}
template <typename T> void write_assert(FILE* fptr, const T& src) {
    my_assert(fwrite(&src, sizeof(src), 1, fptr) == 1);
}

struct SplashWindow {
    HWND m_hWnd = NULL;

    SplashWindow() {
        WNDCLASS wc = { 0 };
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "WindowTilerSplash";
        RegisterClass(&wc);
        m_hWnd = CreateWindowEx(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE, wc.lpszClassName, "", WS_POPUPWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, wc.hInstance, NULL);
    }
    void set_splash_rect(int centre_x, int centre_y, int width, int height) {
        SetWindowPos(m_hWnd, HWND_TOPMOST, centre_x - width / 2, centre_y - height / 2, width, height, 0);
    }
    void splash_printf(const char* fmt, ...) {
        char tmp[256];
        va_list argptr;
        va_start(argptr, fmt);
        vsprintf_s(tmp, sizeof(tmp), fmt, argptr);
        va_end(argptr);
        ShowWindow(m_hWnd, SW_SHOWNA);
        if (HDC hDC = GetDC(m_hWnd)) {
            RECT rc;
            GetClientRect(m_hWnd, &rc);
            DrawText(hDC, tmp, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            ReleaseDC(m_hWnd, hDC);
        }
        SetTimer(m_hWnd, 1, UINT(1.0 * 1000.0), (TIMERPROC)&hide_window);
    }
private:
    static void CALLBACK hide_window(HWND hWnd, UINT uMsg, UINT timerId, DWORD dwTime) {
        if (timerId == 1) {
            ShowWindow(hWnd, SW_HIDE);
            KillTimer(hWnd, 1);
        }
    }
};
static SplashWindow g_splash;

struct MyMonitor {
    MONITORINFOEX m_info = {};

    MyMonitor() {
    }
    MyMonitor(HMONITOR hMon) {
        my_assert(hMon != NULL);
        m_info.cbSize = sizeof(m_info);
        GetMonitorInfo(hMon, &m_info);
    }
    const RECT& rect() const { return m_info.rcWork; }
    int x() const { return rect().left; }
    int y() const { return rect().top; }
    int width() const { return rect().right - rect().left; }
    int height() const { return rect().bottom - rect().top; }
    bool stream_out(FILE* fptr) const {
        write_assert(fptr, m_info);
        return true;
    }
    bool stream_in(FILE* fptr) {
        read_assert(fptr, m_info);
        return true;
    }
    void debug_print() const {
        auto& r = rect();
        printf("rect:%5i,%5i,%5i,%5i (%5i x%5i) monitor:%-16.16s primary:%-3s\n",
            r.left, r.top, r.right, r.bottom,
            r.right-r.left, r.bottom-r.top,
            m_info.szDevice,
            (m_info.dwFlags&MONITORINFOF_PRIMARY) ? "Yes" : "No");
    }
};

struct MyMonitorGroup {
    std::vector<MyMonitor> m_monitors;
    const MyMonitor* m_hover = nullptr;
    POINT m_cursor = {};

    void enumerate() {
        m_monitors.clear();
        EnumDisplayMonitors(0, 0, MonitorEnum, (LPARAM)this);
        GetCursorPos(&m_cursor);
        m_hover = find_monitor(m_cursor);
        if (m_hover) {
            const auto& r = m_hover->rect();
            g_splash.set_splash_rect((r.left + r.right) / 2, (r.top + r.bottom) / 2, 440, 120);
        }
    }
    const MyMonitor* find_monitor(const char* name) const {
        for (const auto& i : m_monitors) {
            if (!strcmp(name, i.m_info.szDevice))
                return &i;
        }
        return nullptr;
    }
    const MyMonitor* find_monitor(const POINT& p) const {
        for (const auto& i : m_monitors) {
            if (PtInRect(&i.rect(), p))
                return &i;
        }
        return nullptr;
    }
    const MyMonitor* find_monitor(const RECT& r) const {
        RECT isect;
        for (const auto& i : m_monitors) {
            if (IntersectRect(&isect, &r, &i.rect()))
                return &i;
        }
        return nullptr;
    }
    bool stream_out(FILE* fptr) const {
        int count = int(m_monitors.size());
        write_assert(fptr, count);
        for (const auto& i : m_monitors)
            i.stream_out(fptr);
        return true;
    }
    bool stream_in(FILE* fptr) {
        int count = 0;
        read_assert(fptr, count);
        limit_count(count, 500);
        m_monitors.resize(count);
        for (int i = 0; i < count; ++i)
            m_monitors[i].stream_in(fptr);
        return true;
    }
    void debug_print() const {
        printf("Cursor: %i, %i\n", m_cursor.x, m_cursor.y);
        printf("Monitors:-\n");
        for (const auto& i : m_monitors)
            i.debug_print();
    }

private:
    static BOOL CALLBACK MonitorEnum(HMONITOR hMon, HDC hDC, LPRECT rect, LPARAM lParam) {
        ((MyMonitorGroup*)lParam)->m_monitors.push_back(MyMonitor(hMon));
        return TRUE;
    }
};

struct MyWindow {
    HWND m_hWnd = NULL;
    WINDOWPLACEMENT m_placement = {};
    int m_z = 0;
    char m_title[MAX_PATH] = {};
    char m_class_name[MAX_PATH] = {};
    char m_process_name[MAX_PATH] = {};

    MyWindow() {
    }
    MyWindow(HWND hWnd, const MyMonitorGroup& monitors) : m_hWnd(hWnd) {
        my_assert(hWnd != NULL);
        m_placement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(hWnd, &m_placement);
        m_z = 0;
        auto h = hWnd; while (h = GetWindow(h, GW_HWNDNEXT)) ++m_z;
        GetWindowText(hWnd, m_title, sizeof(m_title));
        GetClassName(hWnd, m_class_name, sizeof(m_class_name));
        DWORD dwProcId = 0;
        GetWindowThreadProcessId(hWnd, &dwProcId);
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwProcId);
        DWORD len = sizeof(m_process_name);
        QueryFullProcessImageName(hProc, 0, m_process_name, &len);
        CloseHandle(hProc);
    }
    MyWindow(const MyWindow& rhs) : m_hWnd(rhs.m_hWnd), m_placement(rhs.m_placement), m_z(rhs.m_z) {
        strcpy_s(m_title, sizeof(m_title), rhs.m_title);
        strcpy_s(m_class_name, sizeof(m_class_name), rhs.m_class_name);
        strcpy_s(m_process_name, sizeof(m_process_name), rhs.m_process_name);
    }
    const RECT& rect() const { return m_placement.rcNormalPosition; }
    int x() const { return rect().left; }
    int y() const { return rect().top; }
    int width() const { return rect().right - rect().left; }
    int height() const { return rect().bottom - rect().top; }
    POINT get_centre() const { return POINT{ x() + width() / 2, y() + height() / 2 }; }
    void flush_placement() const {
        SetWindowPlacement(m_hWnd, &m_placement);
    }
    void flush_z(HWND hWndInsertAfter) const {
        SetWindowPos(m_hWnd, hWndInsertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    void reposition(int x, int y, int right_limit, int bottom_limit) {
        auto& rect = m_placement.rcNormalPosition;
        rect.right = x + (rect.right - rect.left);
        rect.bottom = y + (rect.bottom - rect.top);
        rect.left = x;
        rect.top = y;
        rect.right = std::min(rect.right, LONG(right_limit));
        rect.bottom = std::min(rect.bottom, LONG(bottom_limit));
    }
    bool stream_out(FILE* fptr) const {
        write_assert(fptr, m_placement);
        write_assert(fptr, m_z);
        write_assert(fptr, m_title);
        write_assert(fptr, m_class_name);
        write_assert(fptr, m_process_name);
        return true;
    }
    bool stream_in(FILE* fptr) {
        read_assert(fptr, m_placement);
        read_assert(fptr, m_z);
        read_assert(fptr, m_title); limit_string(m_title);
        read_assert(fptr, m_class_name); limit_string(m_class_name);
        read_assert(fptr, m_process_name); limit_string(m_process_name);
        return true;
    }
    void debug_print() const {
        auto& r = rect();
        printf("rect:%5i,%5i,%5i,%5i (%5i x%5i) iconic:%-3s z:%3i class:%-32.32s process:%-64.64s title:%s\n",
            r.left, r.top, r.right, r.bottom,
            r.right-r.left, r.bottom-r.top,
            m_placement.showCmd == SW_SHOWMINIMIZED ? "Yes" : "No",
            m_z,
            m_class_name,
            m_process_name,
            m_title);
    }
};

struct MyWindowGroup {
    std::vector<MyWindow> m_windows;
    MyMonitorGroup m_monitors;

    void enumerate() {
        m_monitors.enumerate();
        m_windows.clear();
        EnumWindows(WindowEnum, (LPARAM)this);
    }
    void sort() {
        std::sort(m_windows.begin(), m_windows.end(), [](MyWindow& a, MyWindow& b) {return a.m_z > b.m_z; });
    }
    void flush() {
        sort();
        for (auto& i : m_windows)
            i.flush_placement();
        HWND last = NULL;
        for (auto& i : m_windows) {
            i.flush_z(last);
            last = i.m_hWnd;
        }
    }
    bool stream_out(FILE* fptr) const {
        if (m_monitors.stream_out(fptr)) {
            int count = int(m_windows.size());
            write_assert(fptr, count);
            for (const auto& i : m_windows)
                i.stream_out(fptr);
            return true;
        }
        return false;
    }
    bool stream_in(FILE* fptr) {
        if (m_monitors.stream_in(fptr)) {
            int count = 0;
            read_assert(fptr, count);
            limit_count(count, 500);
            m_windows.resize(count);
            for (int i = 0; i < count; ++i)
                m_windows[i].stream_in(fptr);
            return true;
        }
        return false;
    }
    MyWindow* find_match(const std::string& class_name, const std::string& process_name, const std::string& title) {
        MyWindow* found = nullptr;
        for (auto& i : m_windows) {
            if (class_name == i.m_class_name && process_name == i.m_process_name) {
                found = &i; // this could be the one
                if (title == i.m_title)
                    break;  // yep, this is definitely the one
            }
        }
        return found;
    }
    void debug_print() const {
        m_monitors.debug_print();
        printf("Windows:-\n");
        for (const auto& i : m_windows)
            i.debug_print();
    }
private:
    static BOOL CALLBACK WindowEnum(HWND hWnd, LPARAM lParam) {
        auto group = (MyWindowGroup*)lParam;
        if (group && IsWindowVisible(hWnd) && !GetWindow(hWnd, GW_OWNER)) {
            bool on_taskbar = false;
            auto style = GetWindowLong(hWnd, GWL_STYLE);
            auto ex_style = GetWindowLong(hWnd, GWL_EXSTYLE);
            if (!(style&WS_CHILD)) {
                if (ex_style&WS_EX_APPWINDOW)
                    on_taskbar = true;
                else if (   !(style & WS_DISABLED) &&
                            (style & WS_CAPTION) &&
                            !(ex_style&WS_EX_TOOLWINDOW) &&
                            !(ex_style&WS_EX_NOACTIVATE) &&
                            !(ex_style&WS_EX_TRANSPARENT) &&
                            !(ex_style&WS_EX_TOOLWINDOW))
                    on_taskbar = true;
            }
            if (on_taskbar) {
                MyWindow win(hWnd, group->m_monitors);
                if (*win.m_title)
                    group->m_windows.push_back(win);
            }
        }
        return TRUE;
    }
};

static const char MAGIC_STRING[] = "WINDOW_TILER_FILE_01";

struct Manager {
    MyWindowGroup m_desktop_windows;
    std::map<int, MyWindowGroup> m_layouts;

    // gather all desktop windows onto the monitor where the cursor currently is
    void gather_windows_to_monitor() {
        m_desktop_windows.enumerate();
        if (!m_desktop_windows.m_monitors.m_hover)
            return;

        const auto& monitor_rect = m_desktop_windows.m_monitors.m_hover->rect();

        for (auto& i : m_desktop_windows.m_windows) {
            i.reposition(monitor_rect.left, monitor_rect.top, monitor_rect.right, monitor_rect.bottom);
            i.flush_placement();
        }

        g_splash.splash_printf("Gathered windows to this display");
    }

    // copy specified layout to desktop windows
    void restore_layout(int layout_index) {
        auto layout_iter = m_layouts.find(layout_index);
        if (layout_iter == m_layouts.end()) {
            g_splash.splash_printf("Layout %i doesn't exist. Press CTRL-ALT-SHIFT-%i to create it.", layout_index + 1, layout_index + 1);
            return;
        }
        const auto& layout = layout_iter->second;

        m_desktop_windows.enumerate();

        // go through the windows in the layout trying to find a match with one of the desktop windows.
        // If we find a match, set its position, size and z-order to the values in the layout.
        for (const auto& layout_window : layout.m_windows) {
            auto desktop_window = m_desktop_windows.find_match(layout_window.m_class_name, layout_window.m_process_name, layout_window.m_title);
            if (!desktop_window)
                continue;

            desktop_window->m_placement = layout_window.m_placement;

            // we must modify the placement rect to account for monitor position and DPI changes since the layout was saved...
            // each window will follow the position and DPI of the monitor where its centre lies. Windows spanning monitors will not work well.

            // search the layout for a monitor the window should be associated with (I'm just using the centre of the window to decide)
            auto& desktop_rect = desktop_window->m_placement.rcNormalPosition;
            if (auto layout_monitor = layout.m_monitors.find_monitor(layout_window.get_centre())) {
                // find a matching monitor in the desktop monitor list (using its name)
                if (auto desktop_monitor = m_desktop_windows.m_monitors.find_monitor(layout_monitor->m_info.szDevice)) {
                    // now convert the rect from layout workspace to current desktop workspace

                    float dpi_scale_x = float(desktop_monitor->width()) / float(layout_monitor->width());
                    float dpi_scale_y = float(desktop_monitor->height()) / float(layout_monitor->height());

                    desktop_rect.left -= layout_monitor->x();
                    desktop_rect.right -= layout_monitor->x();
                    desktop_rect.top -= layout_monitor->y();
                    desktop_rect.bottom -= layout_monitor->y();

                    desktop_rect.left = int(float(desktop_rect.left) * dpi_scale_x);
                    desktop_rect.right = int(float(desktop_rect.right) * dpi_scale_x);
                    desktop_rect.top = int(float(desktop_rect.top) * dpi_scale_y);
                    desktop_rect.bottom = int(float(desktop_rect.bottom) * dpi_scale_y);

                    desktop_rect.left += desktop_monitor->x();
                    desktop_rect.right += desktop_monitor->x();
                    desktop_rect.top += desktop_monitor->y();
                    desktop_rect.bottom += desktop_monitor->y();
                }
            }

            desktop_window->m_z = layout_window.m_z;
        }

        m_desktop_windows.flush();

        show_layout_message("Restored Layout ", layout_index);
    }

    // copy current desktop window states to specified layout, and save to disk
    void store_layout(int layout_index) {
        m_desktop_windows.enumerate();
        m_desktop_windows.sort();
        m_layouts[layout_index] = m_desktop_windows;
        save_layouts(layout_index);

        show_layout_message("Stored Layout ", layout_index);
    }

    // show some state info to the user
    void show_info() {
        m_desktop_windows.enumerate();
        m_desktop_windows.sort();
        m_desktop_windows.debug_print();
    }

    void save_layouts(int single_layout = -1) {
        FILE* fptr;
        for (auto& layout : m_layouts) {
            if (single_layout >= 0 && single_layout != layout.first)
                continue;
            if (fopen_s(&fptr, get_layout_file_path(layout.first), "wb") == 0) {
                write_assert(fptr, MAGIC_STRING);
                layout.second.stream_out(fptr);
                fclose(fptr);
            }
        }
    }

    void load_layouts(int single_layout = -1) {
        FILE* fptr;
        for (int layout_index = 0; layout_index < 9; ++layout_index) {
            if (single_layout >= 0 && single_layout != layout_index)
                continue;
            if (fopen_s(&fptr, get_layout_file_path(layout_index), "rb") == 0) {
                char tmp[sizeof(MAGIC_STRING)] = {};
                read_assert(fptr, tmp); limit_string(tmp);
                if (strcmp(tmp, MAGIC_STRING) == 0)
                    m_layouts[layout_index].stream_in(fptr);
                fclose(fptr);
            }
        }
    }

    void show_layout_message(const char* text, int layout_index) {
        ++layout_index;
#define A(N) N==layout_index ? "["#N"]":(m_layouts.find(N-1)!=m_layouts.end() ? " "#N" ":" _ ")
        g_splash.splash_printf("%s %s%s%s%s%s%s%s%s%s", text, A(1), A(2), A(3), A(4), A(5), A(6), A(7), A(8), A(9));
    }

    const char* get_layout_file_path(int layout_index) {
        static char path[MAX_PATH] = { 0 };
        if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
            strcat_s(path, sizeof(path), "\\WindowTiler");
            CreateDirectory(path, NULL);
            auto len = strlen(path);
            sprintf_s(path + len, MAX_PATH - len, "\\layout%i.txt", layout_index + 1);
        }
        return path;
    }
};

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow) {

#ifdef _DEBUG
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
#endif
    Manager manager;
    manager.load_layouts();
    UINT reading_keys[] = {'1','2','3','4','5','6','7','8','9','G','0'};
    int reading_keys_id = 0;
    int reading_key_count = int(sizeof(reading_keys) / sizeof(reading_keys[0]));
    for (int i = 0; i < reading_key_count; ++i) {
        if (!RegisterHotKey(NULL, reading_keys_id + i, MOD_ALT | MOD_CONTROL | MOD_NOREPEAT, reading_keys[i])) {
            char tmp[256];
            sprintf_s(tmp, sizeof(tmp), "Failed to register hotkey CTRL-ALT-%c.\nAnother application may have registerd the same one (maybe another Window Tiler instance?)", (unsigned char)reading_keys[i]);
            MessageBox(NULL, tmp, "Window Tiler", MB_ICONERROR | MB_OK);
            return 1;
        }
    }
    UINT writing_keys[] = {'1','2','3','4','5','6','7','8','9'};
    int writing_keys_id = reading_keys_id + reading_key_count;
    int writing_key_count = int(sizeof(writing_keys) / sizeof(writing_keys[0]));
    for (int i = 0; i < writing_key_count; ++i) {
        if (!RegisterHotKey(NULL, writing_keys_id + i, MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, writing_keys[i])) {
            char tmp[256];
            sprintf_s(tmp, sizeof(tmp), "Failed to register hotkey CTRL-ALT-SHIFT-%c.\nAnother application may have registerd the same one (maybe another Window Tiler instance?)", (unsigned char)writing_keys[i]);
            MessageBox(NULL, tmp, "Window Tiler", MB_ICONERROR | MB_OK);
            return 1;
        }
    }
    while (1) {
        MSG msg = { 0 };
        while (GetMessage(&msg, NULL, 0, 0) != 0) {
            if (msg.message == WM_HOTKEY) {
                int i = (int)msg.wParam;
                if (i >= reading_keys_id && i < reading_keys_id + reading_key_count) {
                    i -= reading_keys_id;
                    if (i < 9)
                        manager.restore_layout(i);
                    else {
                        switch (reading_keys[i]) {
                        case 'G': manager.gather_windows_to_monitor(); break;
                        case '0': manager.show_info(); break;
                        }
                    }
                }
                else if (i >= writing_keys_id && i < writing_keys_id + writing_key_count) {
                    i -= writing_keys_id;
                    if (i < 9)
                        manager.store_layout(i);
                }
                
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(500);
    }
    return 0;
}
