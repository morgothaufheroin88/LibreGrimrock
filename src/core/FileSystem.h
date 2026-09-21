// Reconstructed from Grimrock.bin.x86 FileSystem.cpp (0x080c7650-0x080c9560).
// Virtual layouts match the original vtables (File: read, write, seek, skip,
// getFilename, getFilePosition, getFileLength; FileSystem: openRead, openWrite,
// fileExists, fileLength, fileDate).
#pragma once
#include "core/Array.h"
#include "core/Exception.h"
#include "core/String.h"
#include "core/Sys.h"

namespace core
{

class File
{
  public:
    virtual ~File() {}
    virtual void read(void* buffer, int size) = 0;
    virtual void write(const void* buffer, int size) = 0;
    virtual void seek(int pos) = 0;
    virtual void skip(int count) = 0;
    virtual const char* getFilename() const = 0;
    virtual int getFilePosition() const = 0;
    virtual int getFileLength() const = 0;
};

class FileSystem
{
  public:
    FileSystem() {}
    // 0x080c7a90: a file system removes itself from the mount list when destroyed.
    virtual ~FileSystem();
    virtual File* openRead(const char* filename) = 0;
    virtual File* openWrite(const char* filename) = 0;
    virtual bool fileExists(const char* filename) = 0;
    virtual int fileLength(const char* filename) = 0;
    virtual FileDate fileDate(const char* filename) = 0;
};

class StdFileSystem : public FileSystem
{
  public:
    File* openRead(const char* filename);
    File* openWrite(const char* filename);
    bool fileExists(const char* filename);
    int fileLength(const char* filename);
    FileDate fileDate(const char* filename);
};

// Mounted file systems are searched in order; search paths are tried as prefixes.
void mount(FileSystem& fs, int position = -1);
void unmount(FileSystem& fs);
void addSearchPath(const char* path);
void removeSearchPath(const char* path);

File* openRead(const char* filename);  // throws FileNotFoundException
File* openWrite(const char* filename); // throws FileOpenFailedException
void closeFile(File* file);
bool fileExists(const char* filename);
int fileLength(const char* filename);
FileDate fileDate(const char* filename);
void setFileDate(const char* filename, FileDate date);
// Returns a new[]'ed, zero terminated buffer.
char* readFile(const char* filename, int& length);
String locateFile(const char* filename);
String getTempFilename(const char* pattern); // "name####.ext" -> first free number
void findFiles(const char* path, const char* pattern, Array<String>& files, bool recursive);
void createPath(const char* path);
void deleteFile(const char* filename);
void moveFile(const char* from, const char* to);
void copyFile(const char* from, const char* to);
String getPath(const char* filename);
String stripPath(const char* filename);
String getFileExtension(const char* filename);
String stripExtension(const char* filename);
String getCurrentDirectory();
void setCurrentDirectory(const char* path);

} // namespace core
