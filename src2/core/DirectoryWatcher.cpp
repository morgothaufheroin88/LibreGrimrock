// Reconstructed from grimrock2.exe DirectoryWatcher / FileResource.
#include "core/DirectoryWatcher.h"
#include "core/FileSystem.h"
#include "core/Sys.h"
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <unistd.h>

namespace core
{

static Array<FileResource*> g_fileResources;

// 0x0049caa0
DirectoryWatcher::DirectoryWatcher(const char* path) : m_path(path), m_fd(-1)
{
    m_fd = inotify_init1(IN_NONBLOCK);
    if (m_fd < 0)
    {
        debugPrint("DirectoryWatcher: inotify_init failed (%s)\n", strerror(errno));
        return;
    }
    addWatch(m_path);
}
DirectoryWatcher::~DirectoryWatcher()
{
    if (m_fd >= 0)
        ::close(m_fd);
}
// the original watches the subtree with one handle, inotify needs one watch per directory
void DirectoryWatcher::addWatch(const String& path)
{
    int wd = inotify_add_watch(m_fd, path.c_str(), IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE);
    if (wd < 0)
        return;
    m_watches.push_back(std::pair<int, String>(wd, path));
    DIR* dir = opendir(path.c_str());
    if (!dir)
        return;
    while (struct dirent* entry = readdir(dir))
    {
        if (entry->d_name[0] == '.')
            continue;
        String child = path + "/" + entry->d_name;
        if (entry->d_type == DT_DIR)
            addWatch(child);
    }
    closedir(dir);
}
// 0x0049cc20
bool DirectoryWatcher::getChangedFile(String& filename)
{
    if (m_fd < 0)
        return false;
    char buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    for (;;)
    {
        ssize_t length = read(m_fd, buffer, sizeof(buffer));
        if (length <= 0)
            return false;
        for (char* p = buffer; p < buffer + length;)
        {
            const struct inotify_event* event = (const struct inotify_event*)p;
            p += sizeof(struct inotify_event) + event->len;
            if (event->len == 0)
                continue;
            String directory = m_path;
            for (int i = 0; i < m_watches.size(); ++i)
                if (m_watches[i].first == event->wd)
                    directory = m_watches[i].second;
            String path = directory + "/" + event->name;
            if (event->mask & IN_ISDIR)
            {
                if (event->mask & IN_CREATE)
                    addWatch(path);
                continue;
            }
            if (event->mask & IN_CREATE)
                continue; // the write is reported separately
            filename = path;
            return true;
        }
    }
}

// ---- FileResource -----------------------------------------------------------------

FileResource::FileResource()
{
    g_fileResources.push_back(this);
}
FileResource::~FileResource()
{
    g_fileResources.remove(this);
}
// 0x0049eab0
void fileChanged(const char* filename)
{
    for (int i = 0; i < g_fileResources.size(); ++i)
        if (strcmp(g_fileResources[i]->getResourceFilename().c_str(), filename) == 0)
            g_fileResources[i]->reload();
}

} // namespace core
