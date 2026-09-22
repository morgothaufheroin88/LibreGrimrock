// The games the launcher knows and where their data is: next to the launcher, in a folder
// the player chose before, in a Steam library or in a GOG install.
#pragma once
#include <string>

namespace launcher
{

struct GameInfo
{
    int number;
    const char* title;
    const char* archive;     // the data file that identifies an install
    const char* module;      // the engine, next to the launcher
    const char* installName; // the folder name of the Steam and GOG installs
    const char* iconFile;    // shipped with the game, or written by tools/log2/icon.py
    const char* windowsExe;  // where the icon is when there is no icon file (or null)
    const char* steamAppId;
};

constexpr int NumGames = 2;
extern const GameInfo Games[NumGames];

const GameInfo* findGame(int number);

struct GameInstall
{
    std::string directory; // empty = not found
    std::string source;    // how it was found, for the player: "Steam", "next to launcher"...
};

class GameLibrary
{
  public:
    // launcherDirectory is where the launcher and the engine modules are
    explicit GameLibrary(const std::string& launcherDirectory);

    // looks for every game again
    void refresh();
    const GameInstall& getInstall(int number) const;
    // a folder the player picked: true when it holds the game's data, and then it is
    // remembered for the next start
    bool setInstall(int number, const std::string& directory);

    const std::string& getLauncherDirectory() const
    {
        return m_launcherDirectory;
    }

    static bool hasData(const GameInfo& game, const std::string& directory);

  private:
    GameInstall detect(const GameInfo& game) const;
    void loadSettings();
    void saveSettings() const;

    std::string m_launcherDirectory;
    std::string m_chosen[NumGames]; // remembered folders, by game
    GameInstall m_installs[NumGames];
};

} // namespace launcher
