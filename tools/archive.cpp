// grimrock_archive: inspect GRA2 archives with the reconstructed core library.
#include "core/ArchiveFileSystem.h"
#include "core/FileSystem.h"
#include <cstdio>
#include <cstring>

static const char key[16] = {0x4b,       (char)0x8a, 0x11,       0x56,       (char)0xfd, 0x42,
                             (char)0xce, (char)0xf3, 0x00,       (char)0xd7, (char)0xa2, (char)0xdf,
                             (char)0xef, (char)0xd4, (char)0xcc, (char)0xf7};

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: grimrock_archive ARCHIVE list | exists NAME... | read NAME OUT | "
                        "extract-all DIR | verify\n");
        return 2;
    }
    try
    {
        core::ArchiveFileSystem archive(argv[1]);
        archive.setEncryptionKey(key, 16);
        const char* mode = argv[2];
        if (strcmp(mode, "list") == 0)
        {
            const core::Array<core::ArchiveFileSystem::FileItem>& items = archive.items();
            for (int i = 0; i < items.size(); ++i)
                printf("%08x offset=%u stored=%u size=%u flags=%x\n", items[i].nameHash,
                       items[i].offset, items[i].compressedSize, items[i].size, items[i].flags);
            printf("%d entries\n", items.size());
        }
        else if (strcmp(mode, "exists") == 0)
        {
            for (int i = 3; i < argc; ++i)
                printf("%s: %s\n", argv[i], archive.fileExists(argv[i]) ? "yes" : "no");
        }
        else if (strcmp(mode, "read") == 0 && argc == 5)
        {
            core::File* f = archive.openRead(argv[3]);
            if (!f)
            {
                fprintf(stderr, "not found: %s\n", argv[3]);
                return 1;
            }
            int len = f->getFileLength();
            char* data = new char[len];
            f->read(data, len);
            FILE* out = fopen(argv[4], "wb");
            fwrite(data, 1, len, out);
            fclose(out);
            printf("%d bytes\n", len);
            delete[] data;
            delete f;
        }
        else if (strcmp(mode, "extract-all") == 0 && argc == 4)
        {
            // every entry as DIR/<namehash>.bin
            const core::Array<core::ArchiveFileSystem::FileItem>& items = archive.items();
            for (int i = 0; i < items.size(); ++i)
            {
                core::ArchiveFile file("", &archive, &items[i]);
                int len = file.getFileLength();
                char* data = new char[len];
                file.read(data, len);
                char path[1024];
                snprintf(path, sizeof(path), "%s/%08x.bin", argv[3], items[i].nameHash);
                FILE* out = fopen(path, "wb");
                fwrite(data, 1, len, out);
                fclose(out);
                delete[] data;
            }
            printf("%d entries\n", items.size());
        }
        else if (strcmp(mode, "verify") == 0)
        {
            const core::Array<core::ArchiveFileSystem::FileItem>& items = archive.items();
            long long total = 0;
            for (int i = 0; i < items.size(); ++i)
            {
                core::ArchiveFile file("", &archive, &items[i]);
                total += file.getFileLength();
            }
            printf("%d entries, %lld decoded bytes\n", items.size(), total);
        }
    }
    catch (core::Exception& e)
    {
        fprintf(stderr, "error: %s\n", e.getReason());
        return 1;
    }
    return 0;
}
