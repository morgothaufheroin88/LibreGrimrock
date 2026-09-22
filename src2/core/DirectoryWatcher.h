// Directory watching and the file resource registry of Legend of Grimrock 2
// (0x0049c9f0-0x0049eb20). The developer mode watches the asset directories and reloads
// what changed; the original uses ReadDirectoryChangesW on a worker thread, this uses
// inotify.
#pragma once
#include "core/Array.h"
#include "core/String.h"

namespace core
{

// 0x48 bytes
class DirectoryWatcher
{
  public:
    // 0x0049caa0: watches the directory and everything below it
    explicit DirectoryWatcher(const char* path);
    ~DirectoryWatcher();
    // 0x0049cc20: the next changed file since the last call, false when nothing changed
    bool getChangedFile(String& filename);

  private:
    void addWatch(const String& path);
    String m_path;
    int m_fd;
    Array<std::pair<int, String>> m_watches; // watch descriptor -> directory
};

// 0x0049eab0: notifies the resources loaded from the file (textures, meshes, shaders,
// fonts and samples register themselves in the original); reloading is a developer mode
// feature, so the registry is empty unless something registers.
class FileResource
{
  public:
    FileResource();
    virtual ~FileResource();
    virtual void reload() {}
    const String& getResourceFilename() const
    {
        return m_resourceFilename;
    }
    void setResourceFilename(const char* filename)
    {
        m_resourceFilename = filename;
    }

  private:
    String m_resourceFilename;
};

void fileChanged(const char* filename);

} // namespace core
