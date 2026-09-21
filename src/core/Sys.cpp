// Reconstructed from Grimrock.bin.x86 Sys.cpp. The original called __xstat(3) with the
// legacy 32-bit struct stat; on 64-bit inodes that fails with EOVERFLOW, which is the
// main reason the i386 binary breaks on modern systems. We use the native stat().
#include "core/Sys.h"
#include "core/Exception.h"
#include "core/FileSystem.h"
#include <SDL3/SDL.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

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
    const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    return mode ? mode->refresh_rate : 0.0f;
}
// 0x080cc570
int sysMessageBox(const char* title, const char* message, MessageBoxType type)
{
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
