// The launcher's window: a card per game with its icon, where it was found and what can be
// done with it (Play, Locate...). Mouse: click a button, double click a card to play.
// Keyboard: left/right choose a card, Enter plays, L locates, Esc quits.
#pragma once
#include "Font.h"
#include "GameLibrary.h"
#include <SDL3/SDL.h>
#include <memory>
#include <mutex>
#include <string>

namespace launcher
{

class LauncherWindow
{
  public:
    explicit LauncherWindow(GameLibrary& library);
    ~LauncherWindow();

    // runs until the player picks a game (its number) or closes the window (0)
    int run();

  private:
    struct Card
    {
        SDL_FRect bounds;
        SDL_FRect play;
        SDL_FRect locate;
        SDL_Texture* icon = nullptr;
        std::string message; // a line for the player after Locate..., shown in red
    };

    void layout();
    void loadIcons();
    void draw();
    void drawCard(int index);
    void drawButton(const SDL_FRect& bounds, const char* label, bool enabled, bool hovered,
                    bool primary);
    void fillRect(const SDL_FRect& rect, SDL_Color color);
    void outlineRect(const SDL_FRect& rect, SDL_Color color, float thickness);
    bool isAvailable(int index) const;
    // opens the folder dialog for a game; the answer arrives on the dialog's thread
    void locate(int index);
    static void SDLCALL onFolderChosen(void* userdata, const char* const* files, int filter);
    void takeChosenFolder();

    GameLibrary& m_library;
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    std::unique_ptr<Font> m_titleFont;
    std::unique_ptr<Font> m_cardFont;
    std::unique_ptr<Font> m_textFont;
    std::unique_ptr<Font> m_smallFont;
    Card m_cards[NumGames];
    int m_selected;
    SDL_FPoint m_mouse;
    Uint64 m_lastClickTime;
    int m_lastClickCard;

    std::mutex m_dialogMutex;
    int m_dialogGame;           // the game a folder dialog is open for, 0 = none
    std::string m_chosenFolder; // written by the dialog thread
    bool m_folderAnswered;
};

} // namespace launcher
