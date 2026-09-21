// GRA2 archive reader, reconstructed from ArchiveFileSystem.cpp (0x080afef0-0x080b0e30).
#pragma once
#include "core/FileSystem.h"

namespace core
{

class ArchiveFileSystem;

class ArchiveFile : public File
{
  public:
    struct FileItem
    {
        unsigned int nameHash;       // FNV-1a of the path
        unsigned int offset;         // from the start of the archive
        unsigned int compressedSize; // 0 when stored uncompressed
        unsigned int size;
        unsigned int flags; // FileFlag_* bits
    };
    enum FileFlag
    {
        FileFlag_Compressed = 1,      // zlib
        FileFlag_Encrypted = 0x10000, // ARC4
    };
    ArchiveFile(const char* filename, ArchiveFileSystem* archive, const FileItem* item);
    ~ArchiveFile();
    void read(void* buffer, int size);
    void write(const void* buffer, int size);
    void seek(int pos);
    void skip(int count);
    const char* getFilename() const;
    int getFilePosition() const;
    int getFileLength() const;
    const char* data() const
    {
        return m_pData;
    }

  private:
    String m_filename;
    [[maybe_unused]] ArchiveFileSystem* m_pArchive; // owner, kept for the original layout
    const char* m_pData;
    char* m_pBuffer; // owned decoded data, if any
    int m_pos;
    int m_length;
};

class ArchiveFileSystem : public FileSystem
{
  public:
    typedef ArchiveFile::FileItem FileItem;
    explicit ArchiveFileSystem(const char* filename);
    ~ArchiveFileSystem();
    void setEncryptionKey(const char* key, int length);
    File* openRead(const char* filename);
    File* openWrite(const char* filename);
    bool fileExists(const char* filename);
    int fileLength(const char* filename);
    FileDate fileDate(const char* filename);

    const FileItem* findFileItem(const char* filename) const;
    const Array<FileItem>& items() const
    {
        return m_items;
    }
    const char* mappedData() const
    {
        return m_pData;
    }
    const String& encryptionKey() const
    {
        return m_key;
    }

  private:
    int m_fd;
    const char* m_pData;
    size_t m_dataSize;
    Array<FileItem> m_items;
    String m_key;
};

} // namespace core
