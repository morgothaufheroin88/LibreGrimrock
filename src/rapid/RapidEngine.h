// The application object, reconstructed from RapidEngine.cpp (0x0812bcd0-0x0812d520).
#pragma once
#include "core/HashMap.h"
#include "core/Image.h"
#include "core/SharedPtr.h"
#include "core/String.h"

class RapidEngine
{
  public:
    RapidEngine();
    ~RapidEngine();
    void setWindowIcon(const char* filename);
    // 0x0812bfd0: project file ("dir/init.lua") -> project folder and file name.
    void setProjectPath(const core::String& path);
    // 0x0812c950: runs the project until sys.exit, restarting on sys.restart/loadProject.
    void enterMainLoop();
    // 0x0812c810: writes the error log, shows the message box and exits.
    void luaError(const char* message);
    core::String writeErrorLog(const char* message);

    const core::String& getRootDirectory() const
    {
        return m_rootDirectory;
    }
    const core::String& getProjectFolder() const
    {
        return m_projectFolder;
    }
#if GRIMROCK_GAME >= 2
    // Grimrock 2 passes a plain list of switches ("skipSplash", "restoreGame") instead
    // of the key/value pairs of the first game (0x0040a4b0 / 0x0040a6d0).
    core::Array<core::String>& getArgList()
    {
        return m_argList;
    }
#endif
    core::HashMap<core::String, core::String>& getArgs()
    {
        return m_args;
    }
    void setCompanyName(const char* name)
    {
        m_companyName = name;
    }
    void setApplicationName(const char* name)
    {
        m_applicationName = name;
    }
    const core::String& getCompanyName() const
    {
        return m_companyName;
    }
    const core::String& getApplicationName() const
    {
        return m_applicationName;
    }
    core::Image* getWindowIcon() const
    {
        return m_windowIcon.get();
    }
    void setMaxFrameRate(int fps)
    {
        m_maxFrameRate = fps;
    }
    void quit()
    {
        m_quit = true;
    }
    void restart()
    {
        m_restart = true;
    }

  private:
    core::String m_rootDirectory;
    core::String m_libPath; // root + "/lib"
    core::String m_projectPath;
    core::String m_projectFolder;
    core::String m_projectFile;
    bool m_quit;
    bool m_restart;
    core::HashMap<core::String, core::String> m_args;
#if GRIMROCK_GAME >= 2
    core::Array<core::String> m_argList;
#endif
    core::String m_companyName;
    core::String m_applicationName;
    core::SharedPtr<core::Image> m_windowIcon;
    int m_maxFrameRate;
};

extern RapidEngine* g_pRapidEngine;
