// Reconstructed from Grimrock.bin.x86 StdFileSystem.cpp (0x080bbdc0-0x080bc430).
#include "core/FileSystem.h"
#include <cstdio>

namespace
{

// StdFile lives in the global namespace in the original.
class StdFile : public core::File
{
  public:
    // 0x080bc200
    StdFile(const char* filename, const char* mode) : m_filename(filename)
    {
        m_pFile = fopen(filename, mode);
        if (!m_pFile)
            throw core::FileOpenFailedException("Failed to open file %s", filename);
        m_length = -1;
    }
    // 0x080bc1a0
    ~StdFile()
    {
        fclose(m_pFile);
    }
    // 0x080bc100
    void read(void* buffer, int size)
    {
        if (size > 0 && fread(buffer, size, 1, m_pFile) == 0)
            throw core::FileReadFailedException("Read failed on file %s", m_filename.c_str());
    }
    // 0x080bc060
    void write(const void* buffer, int size)
    {
        if (size > 0 && fwrite(buffer, size, 1, m_pFile) == 0)
            throw core::FileWriteFailedException("Write failed on file %s", m_filename.c_str());
    }
    // 0x080bbfc0
    void seek(int pos)
    {
        if (fseek(m_pFile, pos, SEEK_SET) != 0)
            throw core::FileSeekFailedException("Seek failed on file %s", m_filename.c_str());
    }
    // 0x080bbf20
    void skip(int count)
    {
        if (fseek(m_pFile, count, SEEK_CUR) != 0)
            throw core::FileSeekFailedException("Seek failed on file %s", m_filename.c_str());
    }
    // 0x080bbdc0
    const char* getFilename() const
    {
        return m_filename.c_str();
    }
    // 0x080bbf00
    int getFilePosition() const
    {
        return (int)ftell(m_pFile);
    }
    // 0x080bbe80: cached after the first query.
    int getFileLength() const
    {
        if (m_length >= 0)
            return m_length;
        long pos = getFilePosition();
        fseek(m_pFile, 0, SEEK_END);
        m_length = (int)ftell(m_pFile);
        fseek(m_pFile, pos, SEEK_SET);
        return m_length;
    }

  private:
    FILE* m_pFile;
    core::String m_filename;
    mutable int m_length;
};

} // namespace

namespace core
{

// 0x080bc340
File* StdFileSystem::openRead(const char* filename)
{
    if (!fileExists(filename))
        return 0;
    return new StdFile(filename, "rb");
}
// 0x080bc2f0
File* StdFileSystem::openWrite(const char* filename)
{
    return new StdFile(filename, "wb");
}
// 0x080bbe60
bool StdFileSystem::fileExists(const char* filename)
{
    return sysFileExists(filename);
}
// 0x080bbe40
int StdFileSystem::fileLength(const char* filename)
{
    return (int)sysFileSize(filename);
}
// 0x080bbe10
FileDate StdFileSystem::fileDate(const char* filename)
{
    return sysGetFileDate(filename);
}

} // namespace core
