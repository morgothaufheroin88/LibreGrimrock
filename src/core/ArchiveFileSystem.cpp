// Reconstructed from Grimrock.bin.x86 ArchiveFileSystem.cpp. The original used
// __fxstat(3) with the old 32-bit struct stat and stored the mapping size as int;
// both are 64-bit safe here.
#include "core/ArchiveFileSystem.h"
#include "core/Utils.h"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace core
{

constexpr unsigned int ArchiveMagic = 0x32415247; // "GRA2"

// 0x080b0740
ArchiveFileSystem::ArchiveFileSystem(const char* filename) : m_fd(-1), m_pData(0), m_dataSize(0)
{
    FILE* file = fopen(filename, "rb");
    if (!file)
        throw FileOpenFailedException("Failed to open file %s", filename);
    unsigned int magic = 0;
    if (fread(&magic, 4, 1, file) == 0)
        throw FileReadFailedException("Read failed on file %s", filename);
    if (magic != ArchiveMagic)
        throw BrokenArchiveException("Archive file %s is corrupted", filename);
    int numFiles = 0;
    if (fread(&numFiles, 4, 1, file) == 0)
        throw FileReadFailedException("Read failed on file %s", filename);
    for (int i = 0; i < numFiles; ++i)
    {
        FileItem& item = m_items.push_back();
        if (fread(&item.nameHash, 4, 1, file) == 0 || fread(&item.offset, 4, 1, file) == 0 ||
            fread(&item.compressedSize, 4, 1, file) == 0 || fread(&item.size, 4, 1, file) == 0 ||
            fread(&item.flags, 4, 1, file) == 0)
            throw FileReadFailedException("Read failed on file %s", filename);
    }
    fclose(file);

    m_fd = open(filename, O_RDONLY);
    if (m_fd < 0)
        throw Exception("Could not memory map file %s (open failed)", filename);
    struct stat status;
    if (fstat(m_fd, &status) != 0)
        throw Exception("Could not memory map file %s (fstat failed)", filename);
    m_dataSize = (size_t)status.st_size;
    void* mapped = mmap(0, m_dataSize, PROT_READ, MAP_PRIVATE, m_fd, 0);
    if (mapped == MAP_FAILED)
        throw Exception("Could not memory map file %s", filename);
    m_pData = (const char*)mapped;
}

// 0x080b00e0
ArchiveFileSystem::~ArchiveFileSystem()
{
    if (m_pData)
        munmap((void*)m_pData, m_dataSize);
    if (m_fd >= 0)
        close(m_fd);
}

// 0x080aff20 (inlined String assignment in the original)
void ArchiveFileSystem::setEncryptionKey(const char* key, int length)
{
    m_key = String(key, length);
}

// 0x080aff50: FNV-1a hash of the exact path bytes, linear search.
const ArchiveFileSystem::FileItem* ArchiveFileSystem::findFileItem(const char* filename) const
{
    unsigned int hash = hashString(filename);
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items[i].nameHash == hash)
            return &m_items[i];
    return 0;
}
// 0x080b0680
File* ArchiveFileSystem::openRead(const char* filename)
{
    const FileItem* item = findFileItem(filename);
    if (!item)
        return 0;
    return new ArchiveFile(filename, this, item);
}
// 0x080afef0
File* ArchiveFileSystem::openWrite(const char* filename)
{
    return 0;
}
// 0x080b0600
bool ArchiveFileSystem::fileExists(const char* filename)
{
    return findFileItem(filename) != 0;
}
// 0x080b0580
int ArchiveFileSystem::fileLength(const char* filename)
{
    const FileItem* item = findFileItem(filename);
    return item ? (int)item->size : 0;
}
// 0x080aff00
FileDate ArchiveFileSystem::fileDate(const char* filename)
{
    return 0;
}

// 0x080b03e0: decrypt first, then decompress.
ArchiveFile::ArchiveFile(const char* filename, ArchiveFileSystem* archive, const FileItem* item)
    : m_filename(filename), m_pArchive(archive), m_pBuffer(0), m_pos(0)
{
    m_length = (int)(item->compressedSize ? item->compressedSize : item->size);
    m_pData = archive->mappedData() + item->offset;
    unsigned int flags = item->flags;
    if (flags & ArchiveFile::FileFlag_Encrypted)
    {
        char* decrypted = new char[item->compressedSize];
        decryptARC4(m_pData, decrypted, m_length, archive->encryptionKey().c_str(),
                    archive->encryptionKey().size());
        m_pData = decrypted;
        m_pBuffer = decrypted;
    }
    if (flags & ArchiveFile::FileFlag_Compressed)
    {
        int length = 0;
        char* decoded = uncompress(m_pData, (int)item->compressedSize, length);
        if (m_pBuffer)
            delete[] m_pBuffer;
        m_pBuffer = decoded;
        m_pData = decoded;
        m_length = (int)item->size;
    }
}
// 0x080b00a0
ArchiveFile::~ArchiveFile()
{
    if (m_pBuffer)
        delete[] m_pBuffer;
}
// 0x080b0340
void ArchiveFile::read(void* buffer, int size)
{
    if (size >= 0 && m_pos + size <= m_length)
    {
        memcpy(buffer, m_pData + m_pos, size);
        m_pos += size;
        return;
    }
    throw FileReadFailedException("Read failed on file %s", m_filename.c_str());
}
// 0x080b02d0
void ArchiveFile::write(const void* buffer, int size)
{
    throw FileWriteFailedException("Write failed on file %s", m_filename.c_str());
}
// 0x080b0240
void ArchiveFile::seek(int pos)
{
    if (pos >= 0 && pos <= m_length)
    {
        m_pos = pos;
        return;
    }
    throw FileSeekFailedException("Seek failed on file %s", m_filename.c_str());
}
// 0x080b01b0
void ArchiveFile::skip(int count)
{
    int pos = m_pos + count;
    if (pos >= 0 && pos <= m_length)
    {
        m_pos = pos;
        return;
    }
    throw FileSeekFailedException("Seek failed on file %s", m_filename.c_str());
}
// 0x080b0df0
const char* ArchiveFile::getFilename() const
{
    return m_filename.c_str();
}
// 0x080b0e10
int ArchiveFile::getFilePosition() const
{
    return m_pos;
}
// 0x080b0e20
int ArchiveFile::getFileLength() const
{
    return m_length;
}

} // namespace core
