// Reconstructed from Grimrock.bin.x86 ZipFileSystem.cpp.
#include "core/ZipFileSystem.h"
#include <cstring>
#include <minizip/unzip.h>

namespace
{

// ZipFile lives in the global namespace in the original; the whole entry is
// inflated into memory when opened.
class ZipFile : public core::File
{
  public:
    // 0x080b2130
    ZipFile(const char* filename, char* data, int length)
        : m_filename(filename), m_pData(data), m_length(length), m_pos(0)
    {
    }
    // 0x080b20f0
    ~ZipFile()
    {
        if (m_pData)
            delete[] m_pData;
    }
    // 0x080b2050
    void read(void* buffer, int size)
    {
        if (size >= 0 && m_pos + size <= m_length)
        {
            memcpy(buffer, m_pData + m_pos, size);
            m_pos += size;
            return;
        }
        throw core::FileReadFailedException("Read failed on file %s", m_filename.c_str());
    }
    // 0x080b1f10
    void write(const void* buffer, int size)
    {
        throw core::FileWriteFailedException("Write failed on file %s", m_filename.c_str());
    }
    // 0x080b1e80
    void seek(int pos)
    {
        if (pos >= 0 && pos <= m_length)
        {
            m_pos = pos;
            return;
        }
        throw core::FileSeekFailedException("Seek failed on file %s", m_filename.c_str());
    }
    // 0x080b1df0
    void skip(int count)
    {
        int pos = m_pos + count;
        if (pos >= 0 && pos <= m_length)
        {
            m_pos = pos;
            return;
        }
        throw core::FileSeekFailedException("Seek failed on file %s", m_filename.c_str());
    }
    const char* getFilename() const
    {
        return m_filename.c_str();
    }
    int getFilePosition() const
    {
        return m_pos;
    }
    int getFileLength() const
    {
        return m_length;
    }

  private:
    core::String m_filename;
    char* m_pData;
    int m_length;
    int m_pos;
};

// 0x080b21b0: zip entries always use forward slashes.
int unzLocateFileEx(unzFile file, const char* filename, int caseSensitivity)
{
    char name[512];
    strncpy(name, filename, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    for (char* p = name; *p; ++p)
        if (*p == '\\')
            *p = '/';
    return unzLocateFile(file, name, caseSensitivity);
}

} // namespace

namespace core
{

// 0x080b1fc0
ZipFileSystem::ZipFileSystem(const char* filename)
{
    m_unzFile = unzOpen(filename);
    if (!m_unzFile)
        throw FileNotFoundException("File not found: %s", filename);
}
// 0x080b1f80
ZipFileSystem::~ZipFileSystem()
{
    unzClose((unzFile)m_unzFile);
}
// 0x080b2450
File* ZipFileSystem::openRead(const char* filename)
{
    if (unzLocateFileEx((unzFile)m_unzFile, filename, 0) == UNZ_END_OF_LIST_OF_FILE)
        return 0;
    unz_file_info info;
    char name[512];
    unzGetCurrentFileInfo((unzFile)m_unzFile, &info, name, sizeof(name), 0, 0, 0, 0);
    if (unzOpenCurrentFile((unzFile)m_unzFile) != UNZ_OK)
        throw BrokenArchiveException("Archive file %s is corrupted", filename);
    int length = (int)info.uncompressed_size;
    char* data = new char[length];
    if (unzReadCurrentFile((unzFile)m_unzFile, data, length) < 0)
        throw BrokenArchiveException("Archive file %s is corrupted", filename);
    if (unzCloseCurrentFile((unzFile)m_unzFile) == UNZ_CRCERROR)
        throw BrokenArchiveException("Archive file %s is corrupted", filename);
    return new ZipFile(filename, data, length);
}
// 0x080b1db0
File* ZipFileSystem::openWrite(const char* filename)
{
    return 0;
}
// 0x080b2290
bool ZipFileSystem::fileExists(const char* filename)
{
    return unzLocateFileEx((unzFile)m_unzFile, filename, 0) != UNZ_END_OF_LIST_OF_FILE;
}
// 0x080b2330
int ZipFileSystem::fileLength(const char* filename)
{
    if (unzLocateFileEx((unzFile)m_unzFile, filename, 0) == UNZ_END_OF_LIST_OF_FILE)
        return 0;
    unz_file_info info;
    char name[512];
    unzGetCurrentFileInfo((unzFile)m_unzFile, &info, name, sizeof(name), 0, 0, 0, 0);
    return (int)info.uncompressed_size;
}
// 0x080b22b0
FileDate ZipFileSystem::fileDate(const char* filename)
{
    return 0;
}

} // namespace core
