// minizip backed read-only file system, from ZipFileSystem.cpp (0x080b1d70-0x080b2450).
#pragma once
#include "core/FileSystem.h"

namespace core
{

class ZipFileSystem : public FileSystem
{
  public:
    explicit ZipFileSystem(const char* filename);
    ~ZipFileSystem();
    File* openRead(const char* filename);
    File* openWrite(const char* filename);
    bool fileExists(const char* filename);
    int fileLength(const char* filename);
    FileDate fileDate(const char* filename);

  private:
    void* m_unzFile;
};

} // namespace core
