// Reconstructed from Grimrock.bin.x86 FileSystem.cpp.
#include "core/FileSystem.h"
#include <cstdio>

namespace core
{

static Array<FileSystem*> g_fileSystems;
static Array<String> g_searchPaths;
static StdFileSystem g_defaultFileSys;

// 0x080c7980: the default (native) file system is always mounted first.
static struct FileSystemInit
{
    FileSystemInit()
    {
        g_fileSystems.push_back(&g_defaultFileSys);
    }
} g_fileSystemInit;

// 0x080c7a90
FileSystem::~FileSystem()
{
    for (int i = 0; i < g_fileSystems.size(); ++i)
    {
        if (g_fileSystems[i] == this)
        {
            g_fileSystems.erase(i);
            return;
        }
    }
}

// 0x080c9330
void mount(FileSystem& fs, int position)
{
    for (int i = 0; i < g_fileSystems.size(); ++i)
        if (g_fileSystems[i] == &fs)
            return;
    if (position < 0)
        g_fileSystems.push_back(&fs);
    else if (position <= g_fileSystems.size())
        g_fileSystems.insert(position, &fs);
}
// 0x080c7650
void unmount(FileSystem& fs)
{
    for (int i = 0; i < g_fileSystems.size(); ++i)
    {
        if (g_fileSystems[i] == &fs)
        {
            g_fileSystems.erase(i);
            return;
        }
    }
}
// 0x080c8170
void addSearchPath(const char* path)
{
    String searchPath(path);
    for (int i = 0; i < g_searchPaths.size(); ++i)
        if (g_searchPaths[i] == searchPath)
            return;
    g_searchPaths.push_back(String(path));
}
// 0x080c7cb0
void removeSearchPath(const char* path)
{
    String searchPath(path);
    for (int i = 0; i < g_searchPaths.size(); ++i)
    {
        if (g_searchPaths[i] == searchPath)
        {
            g_searchPaths.erase(i);
            return;
        }
    }
}

// Common lookup helper: try the plain name, then each search path + "/" + name.
template <class Op> static bool lookup(const char* filename, Op& op)
{
    for (int i = 0; i < g_fileSystems.size(); ++i)
        if (op(g_fileSystems[i], filename))
            return true;
    for (int s = 0; s < g_searchPaths.size(); ++s)
    {
        String path(g_searchPaths[s]);
        path.push_back("/", 1);
        String full(path);
        full.append(filename);
        for (int i = 0; i < g_fileSystems.size(); ++i)
            if (op(g_fileSystems[i], full.c_str()))
                return true;
    }
    return false;
}

// 0x080c9080
File* openRead(const char* filename)
{
    struct Op
    {
        File* file;
        bool operator()(FileSystem* fs, const char* name)
        {
            file = fs->openRead(name);
            return file != 0;
        }
    } op = {0};
    if (lookup(filename, op))
        return op.file;
    throw FileNotFoundException("File not found: %s", filename);
}
// 0x080c78e0
File* openWrite(const char* filename)
{
    for (int i = 0; i < g_fileSystems.size(); ++i)
    {
        File* file = g_fileSystems[i]->openWrite(filename);
        if (file)
            return file;
    }
    throw FileOpenFailedException("Failed to open file %s", filename);
}
// 0x080c76e0
void closeFile(File* file)
{
    if (file)
        delete file;
}
// 0x080c8800
bool fileExists(const char* filename)
{
    struct Op
    {
        bool operator()(FileSystem* fs, const char* name)
        {
            return fs->fileExists(name);
        }
    } op;
    return lookup(filename, op);
}
// 0x080c85c0
int fileLength(const char* filename)
{
    struct Op
    {
        int length;
        bool operator()(FileSystem* fs, const char* name)
        {
            if (!fs->fileExists(name))
                return false;
            length = fs->fileLength(name);
            return true;
        }
    } op = {0};
    lookup(filename, op);
    return op.length;
}
// 0x080c8360
FileDate fileDate(const char* filename)
{
    struct Op
    {
        FileDate date;
        bool operator()(FileSystem* fs, const char* name)
        {
            if (!fs->fileExists(name))
                return false;
            date = fs->fileDate(name);
            return true;
        }
    } op = {0};
    lookup(filename, op);
    return op.date;
}
// 0x080c77a0
void setFileDate(const char* filename, FileDate date)
{
    sysSetFileDate(filename, date);
}
// 0x080c92c0
char* readFile(const char* filename, int& length)
{
    File* file = openRead(filename);
    length = file->getFileLength();
    char* buffer = new char[length + 1];
    file->read(buffer, length);
    buffer[length] = 0;
    delete file;
    return buffer;
}
// 0x080c8df0
String locateFile(const char* filename)
{
    struct Op
    {
        String found;
        bool operator()(FileSystem* fs, const char* name)
        {
            if (!fs->fileExists(name))
                return false;
            found = name;
            return true;
        }
    } op;
    lookup(filename, op);
    return op.found;
}
// 0x080c8a00: replaces a run of '#' with a zero padded counter and returns the
// first name that does not exist yet.
String getTempFilename(const char* pattern)
{
    String name(pattern);
    int first = name.find("#", 0);
    int last = name.find_last("#", 0);
    int count = name.count("#", 0);
    if (count == 0)
        return name;
    String prefix = name.substr(0, first - 1);
    String suffix = name.substr(last + 1, -1);
    // Only the counter is formatted. The original built a printf format out of the whole
    // pattern, which breaks on a '%' in the path around it and overflowed a 512 byte buffer
    // on a long one.
    for (int i = 1; i != 0x7fffffff; ++i)
    {
        char counter[32];
        snprintf(counter, sizeof(counter), "%0*d", count < 20 ? count : 20, i);
        String candidate(prefix);
        candidate.append(counter);
        candidate.append(suffix);
        if (!fileExists(candidate.c_str()))
            return candidate;
    }
    return String("");
}
// 0x080c7780
void findFiles(const char* path, const char* pattern, Array<String>& files, bool recursive)
{
    sysFindFiles(path, pattern, files, recursive);
}
// 0x080c77b0
void createPath(const char* path)
{
    if (!sysCreateDirectory(path, true))
        throw Exception("Could not create path: %s", path);
}
// 0x080c7830
void deleteFile(const char* filename)
{
    if (!sysDeleteFile(filename))
        throw Exception("Could not delete file %s", filename);
}
// 0x080c7e70
void moveFile(const char* from, const char* to)
{
    String path = getPath(to);
    if (path.size() > 0 && !sysCreateDirectory(path.c_str(), true))
        throw Exception("Could not create path: %s", path.c_str());
    if (!sysMoveFile(from, to))
        throw Exception("Could not move file from \"%s\" to \"%s\"", from, to);
}
// 0x080c7ff0
void copyFile(const char* from, const char* to)
{
    String path = getPath(to);
    if (path.size() > 0 && !sysCreateDirectory(path.c_str(), true))
        throw Exception("Could not create path: %s", path.c_str());
    if (!sysCopyFile(from, to))
        throw Exception("Could not copy file from \"%s\" to \"%s\"", from, to);
}
// 0x080c7700: everything up to (not including) the last path separator.
String getPath(const char* filename)
{
    int last = 0;
    for (int i = 0; filename[i]; ++i)
        if (filename[i] == '\\' || filename[i] == '/')
            last = i;
    return String(filename, last);
}
// 0x080c7bd0
String stripPath(const char* filename)
{
    const char* name = filename;
    for (const char* p = filename; *p; ++p)
        if (*p == '\\' || *p == '/')
            name = p + 1;
    return String(name);
}
// 0x080c7c40
String getFileExtension(const char* filename)
{
    const char* extension = "";
    for (const char* p = filename; *p; ++p)
        if (*p == '.')
            extension = p + 1;
    return String(extension);
}
// 0x080c82c0
String stripExtension(const char* filename)
{
    const char* dot = 0;
    for (const char* p = filename; *p; ++p)
        if (*p == '.')
            dot = p;
    if (dot)
        return String(filename, (int)(dot - filename));
    return String(filename);
}
// 0x080c78b0
String getCurrentDirectory()
{
    return sysGetCurrentDirectory();
}
// 0x080c78d0
void setCurrentDirectory(const char* path)
{
    sysSetCurrentDirectory(path);
}

} // namespace core
