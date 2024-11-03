#include <string>
#include <vector>

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include "fmt/include/fmt/format.h"
#include "fmt/include/fmt/xchar.h"

#define BUTTON_ID_INJECT 1
#define TARGET_PROCESS_NAME "th07.exe"

const int max_process_modules_default = 1;

std::wstring window_text = L"東方 hax";
std::wstring dllPath;

HANDLE process_handle;

// Converts a win32 error code to a string
std::string win32_error_as_string(DWORD error) {
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer, 0, NULL);
    
    std::string message{messageBuffer, size};

    LocalFree(messageBuffer);

    return message;
}

// Prints a win32 error nicely
void print_error(const std::string &func) {
    auto code = GetLastError();

    fmt::print("{} failed ({}): {}\n", func, code, win32_error_as_string(code));
}

// Search for the process name in the list of running processes
int find_target_process() {
    auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        print_error("CreateToolhelp32Snapshot");
        return -1;
    }

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(entry);

    if (!Process32First(snapshot, &entry)) {
        print_error("Process32First");
        CloseHandle(snapshot);

        return -1;
    }

    do {
        if (strstr(entry.szExeFile, TARGET_PROCESS_NAME) != 0) {
            fmt::print("found '{}': {}\n", entry.szExeFile, entry.th32ProcessID);
            CloseHandle(snapshot);

            return entry.th32ProcessID;
        }
    } while (Process32Next(snapshot, &entry));

    CloseHandle(snapshot);
    return -1;
}

// Perform the actual DLL injection
int inject_dll(HANDLE process) {
    auto path_buffer_len = (dllPath.length() + 1) * sizeof(wchar_t);

    // Write the path string
    auto buffer = VirtualAllocEx(process, NULL, path_buffer_len, MEM_COMMIT, PAGE_READWRITE);
    if (buffer == NULL) {
        print_error("VirtualAllocEx");
        return 1;
    }

    WriteProcessMemory(process, buffer, dllPath.c_str(), path_buffer_len, NULL);

    // Load kernel32.dll and get LoadLibraryW
    auto load_library_proc = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (load_library_proc == NULL) {
        print_error("GetProcAddress");
        return 1;
    }

    // Create a thread in the target process, calling the library and loading the DLL
    auto thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)load_library_proc, buffer, 0, NULL);
    if (thread == NULL) {
        print_error("CreateRemoteThread");
        return 1;
    }

    fmt::print("Injection succeeded!");
    WaitForSingleObject(thread, INFINITE);

    CloseHandle(thread);
}

// Handles window events
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE:
            CreateWindowW(L"BUTTON", L"Inject", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                10, 320 - 50 - 48 - 10, 100, 50, hwnd, (HMENU)BUTTON_ID_INJECT, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            FillRect(hdc, &ps.rcPaint, (HBRUSH)COLOR_BACKGROUND);

            DrawTextW(hdc, window_text.c_str(), -1, &ps.rcPaint, DT_TOP | DT_LEFT | DT_WORDBREAK);

            EndPaint(hwnd, &ps);

            break;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == BUTTON_ID_INJECT) {
                fmt::print("Injecting...");
                inject_dll(process_handle);
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return 0;
}

HWND create_window(HINSTANCE hinstance, int nCmdShow) {
    WNDCLASSEXW wc;

    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hinstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = L"hax";
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassExW(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    auto hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, L"hax", L"東方 hax", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 320, NULL, NULL, NULL, NULL);

    if (hwnd == NULL) {
        print_error("CreateWindowExW");
        return NULL;
    }
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    return hwnd;
}

std::vector<HMODULE> get_process_modules(HANDLE process) {
    std::vector<HMODULE> modules{};

    modules.resize(max_process_modules_default);

    DWORD needed;
    if (!EnumProcessModules(process, modules.data(), modules.size() * sizeof(HMODULE), &needed)) {
        print_error("EnumProcessModules");
        exit(1);
    }

    // Keep getting modules until we have the complete list
    while (needed > modules.size() * sizeof(HMODULE)) {
        modules.resize(needed / sizeof(HMODULE));

        if (!EnumProcessModules(process, modules.data(), modules.size() * sizeof(HMODULE),
                &needed)) {
            print_error("EnumProcessModules");
            exit(1);
        }
    }

    return modules;
}

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    int num_args;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &num_args);
    if (argv == NULL) {
        print_error("CommandLineToArgv");
        return 1;
    }

    int pid;
    if (num_args < 2) {
        pid = find_target_process();
    } else {
        pid = std::stoi(argv[1]);
    }

    dllPath.resize(MAX_PATH + 1);

    DWORD path_len = GetFullPathNameW(L"hax.dll", MAX_PATH, dllPath.data(), NULL);
    if (path_len == 0) {
        print_error("GetFullPathNameW");
        return 1;
    }

    dllPath.resize(path_len);

    auto process = OpenProcess(PROCESS_ALL_ACCESS, false, pid);
    if (process == NULL) {
        print_error("OpenProcess");
        return 1;
    }

    process_handle = process;

    // Print process name
    wchar_t processName[MAX_PATH];
    if(GetModuleFileNameExW(process, NULL, processName, MAX_PATH)) {
        fmt::print(L"Process name: '{}'\n", processName);
    } else {
        print_error("GetModuleFileNameExW");
        return 1;
    }

    auto payload_msg = fmt::format(L"DLL to inject: {}", dllPath);
    auto target_msg = fmt::format(L"Injecting into: '{}'\n", processName);

    window_text = payload_msg + L"\n" + target_msg;

    fmt::print(L"{}\n{}", payload_msg, target_msg);

    auto modules = get_process_modules(process);
    fmt::print("Found {} modules", modules.size());

    auto win = create_window(GetModuleHandleW(NULL), nCmdShow);

    MSG m;
    while(GetMessage(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }

    CloseHandle(process);

    return 0;
}
