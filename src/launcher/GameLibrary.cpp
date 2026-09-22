#include "GameLibrary.h"
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace launcher
{

const GameInfo Games[NumGames] = {
    {1, "Legend of Grimrock", "grimrock.dat", "libgrimrock1.so", "Legend of Grimrock",
     "grimrock.png", nullptr, "207170"},
    {2, "Legend of Grimrock 2", "grimrock2.dat", "libgrimrock2.so", "Legend of Grimrock 2",
     "grimrock2.png", "grimrock2.exe", "251730"},
};

const GameInfo* findGame(int number)
{
    for (const GameInfo& game : Games)
        if (game.number == number)
            return &game;
    return nullptr;
}

static std::string homeDirectory()
{
    const char* home = getenv("HOME");
    return home ? home : "";
}

static std::string settingsFile()
{
    const char* config = getenv("XDG_CONFIG_HOME");
    std::string base = config && *config ? config : homeDirectory() + "/.config";
    return base + "/LibreGrimrock/launcher.cfg";
}

static bool isFile(const std::string& path)
{
    struct stat info;
    return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

// The roots of the Steam installs and every library folder they list in
// steamapps/libraryfolders.vdf ("path" "/somewhere/SteamLibrary").
static std::vector<std::string> steamLibraries()
{
    std::string home = homeDirectory();
    const std::string roots[] = {home + "/.local/share/Steam", home + "/.steam/steam",
                                 home + "/.var/app/com.valvesoftware.Steam/.local/share/Steam"};
    std::vector<std::string> libraries;
    auto add = [&libraries](const std::string& path)
    {
        for (const std::string& known : libraries)
            if (known == path)
                return;
        libraries.push_back(path);
    };
    for (const std::string& root : roots)
    {
        std::ifstream vdf(root + "/steamapps/libraryfolders.vdf");
        if (!vdf)
            continue;
        add(root);
        std::string line;
        while (std::getline(vdf, line))
        {
            size_t key = line.find("\"path\"");
            if (key == std::string::npos)
                continue;
            size_t open = line.find('"', key + 6);
            size_t close = open == std::string::npos ? open : line.find('"', open + 1);
            if (close != std::string::npos)
                add(line.substr(open + 1, close - open - 1));
        }
    }
    return libraries;
}

GameLibrary::GameLibrary(const std::string& launcherDirectory)
    : m_launcherDirectory(launcherDirectory)
{
    loadSettings();
    refresh();
}

bool GameLibrary::hasData(const GameInfo& game, const std::string& directory)
{
    return !directory.empty() && isFile(directory + "/" + game.archive);
}

GameInstall GameLibrary::detect(const GameInfo& game) const
{
    const std::string& chosen = m_chosen[game.number - 1];
    if (hasData(game, chosen))
        return {chosen, "chosen folder"};
    if (hasData(game, m_launcherDirectory))
        return {m_launcherDirectory, "next to the launcher"};
    for (const std::string& library : steamLibraries())
    {
        std::string directory = library + "/steamapps/common/" + game.installName;
        if (hasData(game, directory))
            return {directory, "Steam"};
    }
    std::string home = homeDirectory();
    const std::string gogFolders[] = {
        home + "/GOG Games/" + game.installName + "/game",
        home + "/GOG Games/" + game.installName,
        home + "/.wine/drive_c/GOG Games/" + game.installName,
    };
    for (const std::string& directory : gogFolders)
        if (hasData(game, directory))
            return {directory, "GOG"};
    return {};
}

void GameLibrary::refresh()
{
    for (const GameInfo& game : Games)
        m_installs[game.number - 1] = detect(game);
}

const GameInstall& GameLibrary::getInstall(int number) const
{
    return m_installs[number - 1];
}

bool GameLibrary::setInstall(int number, const std::string& directory)
{
    const GameInfo* game = findGame(number);
    if (!game || !hasData(*game, directory))
        return false;
    m_chosen[number - 1] = directory;
    m_installs[number - 1] = {directory, "chosen folder"};
    saveSettings();
    return true;
}

// launcher.cfg: one "gameN=<folder>" line per remembered folder
void GameLibrary::loadSettings()
{
    std::ifstream file(settingsFile());
    std::string line;
    while (std::getline(file, line))
    {
        if (line.size() > 6 && line.compare(0, 4, "game") == 0 && line[5] == '=')
        {
            int number = line[4] - '0';
            if (findGame(number))
                m_chosen[number - 1] = line.substr(6);
        }
    }
}

void GameLibrary::saveSettings() const
{
    std::string path = settingsFile();
    std::string directory = path.substr(0, path.rfind('/'));
    mkdir(directory.substr(0, directory.rfind('/')).c_str(), 0755);
    mkdir(directory.c_str(), 0755);
    std::ofstream file(path);
    for (const GameInfo& game : Games)
        if (!m_chosen[game.number - 1].empty())
            file << "game" << game.number << "=" << m_chosen[game.number - 1] << "\n";
}

} // namespace launcher
