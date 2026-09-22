// Reconstructed from Grimrock.bin.x86 Sys.cpp. The original called __xstat(3) with the
// legacy 32-bit struct stat; on 64-bit inodes that fails with EOVERFLOW, which is the
// main reason the i386 binary breaks on modern systems. We use the native stat().
#include "core/Sys.h"
#include "core/Exception.h"
#include "core/FileSystem.h"
#include <SDL3/SDL.h>
#include <clocale>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <unistd.h>
#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif

namespace core
{

constexpr int DebugPrintBufferSize = 0x2000;
// FileDate is a Windows FILETIME: 100 ns ticks since 1601-01-01.
constexpr long long FileTimeTicksPerSecond = 10000000LL;
constexpr long long FileTimeUnixEpoch = 0x19db1ded53e8000LL; // 1970-01-01 as FILETIME
// Offset the original adds when converting a FileDate back to a time_t (it is not the
// inverse of FileTimeUnixEpoch; the behaviour is kept as shipped).
constexpr long long FileTimeEpochSkew = 0x49ef6f00LL;
constexpr mode_t DirectoryMode = 0777;

// 0x080ba2f0
void debugPrint(const char* format, ...)
{
    char buffer[DebugPrintBufferSize];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, DebugPrintBufferSize, format, args);
    va_end(args);
    printf("%s", buffer);
}

// 0x080cc8e0
long long sysClock()
{
    return (long long)SDL_GetPerformanceCounter();
}
// 0x080cc890
double sysGetSeconds(long long ticks)
{
    return (double)ticks / (double)SDL_GetPerformanceFrequency();
}
// 0x080cc870
void sysSleep(int milliseconds)
{
    usleep(milliseconds * 1000);
}

// 0x080cc770
bool sysFileExists(const char* filename)
{
    struct stat st;
    return stat(filename, &st) == 0;
}
// 0x080cc740
long long sysFileSize(const char* filename)
{
    struct stat st;
    if (stat(filename, &st) != 0)
        return 0;
    return (long long)st.st_size;
}
// 0x080cc700: seconds since epoch -> 100ns ticks since 1601.
FileDate sysGetFileDate(const char* filename)
{
    struct stat st;
    if (stat(filename, &st) != 0)
        return 0;
    return (long long)st.st_mtim.tv_sec * FileTimeTicksPerSecond + FileTimeUnixEpoch;
}
// 0x080cc7a0
bool sysSetFileDate(const char* filename, FileDate date)
{
    struct timeval times[2];
    times[0].tv_sec =
        (time_t)((unsigned long long)date / FileTimeTicksPerSecond) + FileTimeEpochSkew;
    times[0].tv_usec = 0;
    times[1].tv_sec = times[0].tv_sec;
    times[1].tv_usec = 0;
    return utimes(filename, times) == 0;
}
// 0x080cc430 / 0x080cc440 / 0x080cc450: unimplemented on Linux in the original.
bool sysCopyFile(const char* from, const char* to)
{
    return false;
}
bool sysMoveFile(const char* from, const char* to)
{
    return false;
}
bool sysDeleteFile(const char* filename)
{
    return false;
}

// 0x080cccf0
void sysFindFiles(const char* path, const char* pattern, Array<String>& files, bool recursive)
{
    bool emptyPath = (*path == 0);
    if (emptyPath)
        path = ".";
    DIR* dir = opendir(path);
    if (!dir)
        return;
    while (dirent* entry = readdir(dir))
    {
        const char* name = entry->d_name;
        if (name[0] == '.' || strcmp(name, "..") == 0)
            continue;
        if (entry->d_type == DT_DIR)
        {
            if (!recursive)
                continue;
            String sub(path);
            if (sub.size() > 0 && sub[sub.size() - 1] != '/')
                sub.push_back("/", 1);
            sub.append(name);
            sysFindFiles(sub.c_str(), pattern, files, recursive);
            continue;
        }
        if (strcmp(pattern, "*.*") != 0)
        {
            String ext = getFileExtension(name);
            String patternExt = getFileExtension(pattern);
            if (strcmp(patternExt.c_str(), ext.c_str()) != 0)
                continue;
        }
        if (emptyPath)
            files.push_back(String(name));
        else
            files.push_back(formatString("%s/%s", path, name));
    }
    closedir(dir);
}

// 0x080cd320
bool sysCreateDirectory(const char* path, bool recursive)
{
    if (!recursive)
        return mkdir(path, DirectoryMode) == 0;
    String fullPath(path);
    int pos = 0;
    for (;;)
    {
        pos = fullPath.find("/", pos);
        if (pos != 0)
        {
            String dir = fullPath.substr(0, pos > 0 ? pos - 1 : -1);
            struct stat status;
            if (stat(dir.c_str(), &status) != 0)
            {
                debugPrint("creating directory %s\n", dir.c_str());
                if (mkdir(dir.c_str(), DirectoryMode) != 0)
                    return false;
            }
            if (pos < 0)
                return true;
        }
        ++pos;
    }
}
// 0x080cd240
String sysGetCurrentDirectory()
{
    char buffer[1024];
    if (!getcwd(buffer, sizeof(buffer)))
        throw Exception("getcwd failed");
    return String(buffer);
}
// 0x080cc800
void sysSetCurrentDirectory(const char* path)
{
    if (chdir(path) != 0)
        throw Exception("chdir failed");
}
// 0x080cd4b0
String sysGetDocumentsDirectory()
{
    const char* xdg = getenv("XDG_DATA_HOME");
    if (xdg)
        return String(xdg);
    const char* home = getenv("HOME");
    if (!home)
        throw Exception("HOME not set");
    String result(home);
    result.push_back("/.local/share", 13);
    return result;
}
// 0x080cd0e0
String sysCreateGuid()
{
    FILE* urandom = fopen("/dev/urandom", "r");
    if (!urandom)
        return String("");
    unsigned int words[4] = {0, 0, 0, 0};
    size_t wordsRead = fread(words, 4, 4, urandom);
    (void)wordsRead;
    fclose(urandom);
    char buffer[40];
    sprintf(buffer, "%04x%04x%04x%04x", words[0], words[1], words[2], words[3]);
    return String(buffer);
}

// 0x080cc490
String sysGetClipboard()
{
    String result;
    if (SDL_HasClipboardText())
    {
        char* text = SDL_GetClipboardText();
        result = text;
        SDL_free(text);
    }
    return result;
}
// 0x080cc530
void sysSetClipboard(const char* text)
{
    SDL_SetClipboardText(text);
}
// 0x080cc540
void sysGetDesktopDisplayMode(int& width, int& height)
{
    const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    width = mode ? mode->w : 0;
    height = mode ? mode->h : 0;
}
float sysGetDisplayRefreshRate()
{
    // the display showing the game window, the primary one before the window exists
    SDL_DisplayID display = 0;
    int numWindows = 0;
    SDL_Window** windows = SDL_GetWindows(&numWindows);
    if (windows)
    {
        if (numWindows > 0)
            display = SDL_GetDisplayForWindow(windows[0]);
        SDL_free(windows);
    }
    if (display == 0)
        display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(display);
    return mode ? mode->refresh_rate : 0.0f;
}
// grimrock2.exe 0x00450c10
void sysGetSystemInfo(SystemInfo& info)
{
    char hostname[256] = "";
    gethostname(hostname, sizeof(hostname) - 1);
    info.computerName = hostname;
    struct utsname name;
    if (uname(&name) == 0)
        info.osVersion = formatString("%s %s", name.sysname, name.release);
    info.oemId = 0;
    info.pageSize = (unsigned int)sysconf(_SC_PAGESIZE);
    info.processorCount = (unsigned int)sysconf(_SC_NPROCESSORS_ONLN);
    info.logicalProcessorCount = info.processorCount;
    info.cpuVendor = "";
    info.cpuBrand = "";
#if defined(__x86_64__) || defined(__i386__)
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid(0, &eax, &ebx, &ecx, &edx))
    {
        char vendor[13];
        memcpy(vendor, &ebx, 4);
        memcpy(vendor + 4, &edx, 4);
        memcpy(vendor + 8, &ecx, 4);
        vendor[12] = 0;
        info.cpuVendor = vendor;
    }
    if (__get_cpuid(0x80000000u, &eax, &ebx, &ecx, &edx) && eax >= 0x80000004u)
    {
        char brand[49];
        unsigned int* words = (unsigned int*)brand;
        for (unsigned int i = 0; i < 3; ++i)
            __get_cpuid(0x80000002u + i, &words[i * 4], &words[i * 4 + 1], &words[i * 4 + 2],
                        &words[i * 4 + 3]);
        brand[48] = 0;
        info.cpuBrand = brand;
    }
#endif
    MemoryStatus memory;
    sysGetMemoryStatus(memory);
    info.totalPhysicalMemory = memory.totalPhysical;
    info.availablePhysicalMemory = memory.availablePhysical;
    // the original lists the display adapters; the DRM devices are the equivalent
    info.displayDevices.clear();
    for (int i = 0; i < 16; ++i)
    {
        String device = formatString("/sys/class/drm/card%d/device", i);
        if (!sysFileExists(device.c_str()))
            continue;
        String ids;
        const char* files[2] = {"vendor", "device"};
        for (int k = 0; k < 2; ++k)
        {
            FILE* file = fopen(formatString("%s/%s", device.c_str(), files[k]).c_str(), "r");
            if (!file)
                continue;
            char line[64] = "";
            if (fgets(line, sizeof(line), file))
            {
                line[strcspn(line, "\r\n")] = 0;
                ids.append(line);
                ids.append(" ");
            }
            fclose(file);
        }
        info.displayDevices.push_back(
            formatString("Device name: card%d\nDevice string: %s\n", i, ids.c_str()));
    }
}
// grimrock2.exe 0x0040b290 (GlobalMemoryStatusEx)
void sysGetMemoryStatus(MemoryStatus& status)
{
    struct sysinfo si;
    memset(&si, 0, sizeof(si));
    sysinfo(&si);
    status.totalPhysical = (unsigned long long)si.totalram * si.mem_unit;
    status.availablePhysical = (unsigned long long)si.freeram * si.mem_unit;
    status.totalVirtual = status.totalPhysical + (unsigned long long)si.totalswap * si.mem_unit;
    status.availableVirtual =
        status.availablePhysical + (unsigned long long)si.freeswap * si.mem_unit;
}
// grimrock2.exe 0x0040b260 (ShellExecute "open")
void sysOpenURL(const char* url)
{
    String command = formatString("xdg-open '%s' >/dev/null 2>&1 &", url);
    if (system(command.c_str()) != 0)
        debugPrint("sysOpenURL: could not open %s\n", url);
}
bool sysGetWorkArea(int& left, int& top, int& right, int& bottom)
{
    SDL_Rect rect;
    if (!SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &rect))
        return false;
    left = rect.x;
    top = rect.y;
    right = rect.x + rect.w;
    bottom = rect.y + rect.h;
    return true;
}
// grimrock2.exe 0x0040aaf0: "C" or the language_COUNTRY.codepage of the user
void sysSetLocale(bool user)
{
    setlocale(LC_ALL, user ? "" : "C");
}

// 0x080cc570
int sysMessageBox(const char* title, const char* message, MessageBoxType type)
{
    // the text also goes to the log, the dialog is easy to miss
    debugPrint("%s: %s\n", title, message);
    fflush(stdout);
    SDL_MessageBoxButtonData buttons[3];
    int numButtons = 1;
    buttons[0].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    buttons[0].buttonID = 1;
    buttons[0].text = "OK";
    buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    buttons[1].buttonID = 2;
    buttons[1].text = "Cancel";
    switch (type)
    {
    case MessageBox_Ok:
        numButtons = 1;
        break;
    case MessageBox_OkCancel:
        numButtons = 2;
        break;
    case MessageBox_RetryCancel:
        numButtons = 2;
        buttons[0].text = "Retry";
        break;
    case MessageBox_YesNo:
        numButtons = 2;
        buttons[0].text = "Yes";
        buttons[1].text = "No";
        break;
    case MessageBox_YesNoCancel:
        numButtons = 3;
        buttons[0].text = "Yes";
        buttons[1].text = "No";
        buttons[2].flags = 0;
        buttons[2].buttonID = 3;
        buttons[2].text = "Cancel";
        break;
    }
    // The game window is usually fullscreen with the mouse grabbed and the cursor
    // hidden; the dialog would open behind it and its event loop would wait forever.
    // Release everything and drop the window out of the way first.
    int numWindows = 0;
    SDL_Window** windows = SDL_GetWindows(&numWindows);
    for (int i = 0; windows && i < numWindows; ++i)
    {
        SDL_SetWindowRelativeMouseMode(windows[i], false);
        SDL_SetWindowMouseGrab(windows[i], false);
        SDL_SetWindowKeyboardGrab(windows[i], false);
        SDL_SetWindowFullscreen(windows[i], false);
        SDL_MinimizeWindow(windows[i]);
    }
    SDL_free(windows);
    SDL_ShowCursor();
    SDL_PumpEvents();
    SDL_MessageBoxData data;
    memset(&data, 0, sizeof(data));
    data.flags = SDL_MESSAGEBOX_INFORMATION;
    data.title = title;
    data.message = message;
    data.numbuttons = numButtons;
    data.buttons = buttons;
    // 1 based button index like fl_choice: 1 = ok/yes/retry, 2 = cancel/no, 3 = cancel
    int result = 0;
    if (!SDL_ShowMessageBox(&data, &result))
        return 1;
    return result;
}

} // namespace core
