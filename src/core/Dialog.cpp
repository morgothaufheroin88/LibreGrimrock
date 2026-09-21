// Reconstructed from Grimrock.bin.x86 Dialog.cpp (FLTK replaced by an external dialog tool).
#include "core/Dialog.h"
#include "core/Array.h"
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

namespace core
{

constexpr int ExecFailedExitCode = 127; // shell convention for "command not found"

static String runDialog(const char* tool, const Array<String>& args)
{
    int fds[2];
    if (pipe(fds) != 0)
        return String("");
    pid_t pid = fork();
    if (pid == 0)
    {
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        Array<char*> argv;
        argv.push_back((char*)tool);
        for (int i = 0; i < args.size(); ++i)
            argv.push_back((char*)args[i].c_str());
        argv.push_back((char*)0);
        execvp(tool, argv.data());
        _exit(ExecFailedExitCode);
    }
    close(fds[1]);
    String result;
    char buffer[512];
    ssize_t n;
    while ((n = read(fds[0], buffer, sizeof(buffer))) > 0)
        result.push_back(buffer, (int)n);
    close(fds[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return String("");
    while (result.size() > 0 &&
           (result[result.size() - 1] == '\n' || result[result.size() - 1] == '\r'))
        result.erase(result.size() - 1);
    return result;
}

static bool haveTool(const char* tool)
{
    String cmd = formatString("command -v %s >/dev/null 2>&1", tool);
    return system(cmd.c_str()) == 0;
}

// 0x080d3730: fl_file_chooser(description, "*." + pattern, defaultName)
String fileDialog(Window* parent, int mode, const char* title, const char* description,
                  const char* pattern, const char* defaultName)
{
    Array<String> args;
    if (haveTool("kdialog"))
    {
        args.push_back(String("--title"));
        args.push_back(String(title));
        args.push_back(String(mode == FileDialog_Save ? "--getsavefilename" : "--getopenfilename"));
        args.push_back(String(defaultName && *defaultName ? defaultName : "."));
        args.push_back(formatString("*.%s|%s", pattern, description));
        return runDialog("kdialog", args);
    }
    args.push_back(String("--file-selection"));
    args.push_back(formatString("--title=%s", title));
    if (mode == FileDialog_Save)
        args.push_back(String("--save"));
    args.push_back(formatString("--file-filter=%s | *.%s", description, pattern));
    if (defaultName && *defaultName)
        args.push_back(formatString("--filename=%s", defaultName));
    return runDialog("zenity", args);
}

// 0x080d3860: fl_dir_chooser(title, "")
String browseFolderDialog(Window* parent, const char* title)
{
    Array<String> args;
    if (haveTool("kdialog"))
    {
        args.push_back(String("--title"));
        args.push_back(String(title));
        args.push_back(String("--getexistingdirectory"));
        args.push_back(String("."));
        return runDialog("kdialog", args);
    }
    args.push_back(String("--file-selection"));
    args.push_back(String("--directory"));
    args.push_back(formatString("--title=%s", title));
    return runDialog("zenity", args);
}

} // namespace core
