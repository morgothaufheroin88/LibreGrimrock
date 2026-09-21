// Platform layer, reconstructed from Grimrock.bin.x86 Sys.cpp (0x080cc430-0x080cd4b0).
#pragma once
#include "core/Array.h"
#include "core/String.h"
#include <cstdint>

namespace core
{

typedef long long FileDate; // Windows FILETIME style: 100ns ticks since 1601-01-01

enum MessageBoxType
{
    MessageBox_Ok = 0,
    MessageBox_OkCancel = 1,
    MessageBox_YesNo = 2,
    MessageBox_YesNoCancel = 3,
    MessageBox_RetryCancel = 4
};
// sysMessageBox returns the 1 based index of the pressed button.
enum MessageBoxResult
{
    MessageBox_ResultOk = 1,
    MessageBox_ResultYes = 1,
    MessageBox_ResultCancel = 2,
    MessageBox_ResultNo = 2
};

void debugPrint(const char* format, ...) __attribute__((format(printf, 1, 2)));

long long sysClock();
double sysGetSeconds(long long ticks);
void sysSleep(int milliseconds);

bool sysFileExists(const char* filename);
long long sysFileSize(const char* filename);
FileDate sysGetFileDate(const char* filename);
bool sysSetFileDate(const char* filename, FileDate date);
bool sysCopyFile(const char* from, const char* to);
bool sysMoveFile(const char* from, const char* to);
bool sysDeleteFile(const char* filename);
void sysFindFiles(const char* path, const char* pattern, Array<String>& files, bool recursive);
bool sysCreateDirectory(const char* path, bool recursive);
String sysGetCurrentDirectory();
void sysSetCurrentDirectory(const char* path);
String sysGetDocumentsDirectory();
String sysCreateGuid();

String sysGetClipboard();
void sysSetClipboard(const char* text);
void sysGetDesktopDisplayMode(int& width, int& height);
// Refresh rate of the primary display in Hz, 0 when unknown.
float sysGetDisplayRefreshRate();
int sysMessageBox(const char* title, const char* message, MessageBoxType type);

// Grimrock 2 additions (grimrock2.exe Sys.cpp). SystemInfo mirrors the Win32 structure the
// original fills from GetComputerName/GetVersionEx/GetSystemInfo/cpuid/GlobalMemoryStatusEx/
// EnumDisplayDevices (0x00450c10).
struct SystemInfo
{
    String computerName;
    String osVersion;
    unsigned int oemId;
    unsigned int processorCount;
    unsigned int logicalProcessorCount;
    unsigned int pageSize;
    String cpuVendor;
    String cpuBrand;
    unsigned long long totalPhysicalMemory;
    unsigned long long availablePhysicalMemory;
    Array<String> displayDevices;
};
struct MemoryStatus
{
    unsigned long long availablePhysical;
    unsigned long long availableVirtual;
    unsigned long long totalPhysical;
    unsigned long long totalVirtual;
};
void sysGetSystemInfo(SystemInfo& info);
void sysGetMemoryStatus(MemoryStatus& status);
void sysOpenURL(const char* url);
// Desktop area not covered by panels (SystemParametersInfo(SPI_GETWORKAREA)).
bool sysGetWorkArea(int& left, int& top, int& right, int& bottom);
// setlocale for the whole process: "C" or the user's locale.
void sysSetLocale(bool user);

} // namespace core
