#include "LauncherWindow.h"
#include "GameIcon.h"
#include <cstdlib>
#include <mutex>
#include <string>

namespace launcher
{

namespace
{

constexpr int WindowWidth = 900;
constexpr int WindowHeight = 540;
constexpr float Margin = 40.0f;
constexpr float CardGap = 28.0f;
constexpr float CardTop = 128.0f;
constexpr float CardHeight = 340.0f;
constexpr float IconSize = 128.0f;
constexpr float ButtonHeight = 42.0f;
constexpr Uint64 DoubleClickMs = 400;

// a dark stone palette with the gold of the games' menus
constexpr SDL_Color BackgroundTop = {34, 28, 22, 255};
constexpr SDL_Color BackgroundBottom = {12, 10, 8, 255};
constexpr SDL_Color CardColor = {28, 24, 20, 235};
constexpr SDL_Color CardBorder = {74, 62, 46, 255};
constexpr SDL_Color Gold = {214, 176, 102, 255};
constexpr SDL_Color Text = {226, 216, 196, 255};
constexpr SDL_Color DimText = {150, 138, 118, 255};
constexpr SDL_Color ErrorText = {226, 110, 90, 255};
constexpr SDL_Color ButtonColor = {52, 44, 34, 255};
constexpr SDL_Color ButtonHover = {76, 64, 48, 255};
constexpr SDL_Color PrimaryColor = {120, 88, 40, 255};
constexpr SDL_Color PrimaryHover = {150, 110, 50, 255};
constexpr SDL_Color DisabledColor = {40, 36, 32, 255};

bool contains(const SDL_FRect& rect, SDL_FPoint point)
{
    return SDL_PointInRectFloat(&point, &rect);
}

// The folder dialog answers on its own thread, possibly after the window is gone (the
// player closed the launcher with the dialog open), so its state outlives any window.
struct FolderDialog
{
    std::mutex mutex;
    int game = 0; // the game a dialog is open for, 0 = none
    bool answered = false;
    std::string folder; // empty = cancelled
};
FolderDialog g_folderDialog;

void SDLCALL onFolderChosen(void* userdata, const char* const* files, int)
{
    FolderDialog* dialog = (FolderDialog*)userdata;
    std::lock_guard<std::mutex> lock(dialog->mutex);
    dialog->folder = files && files[0] ? files[0] : "";
    dialog->answered = true;
}

} // namespace

LauncherWindow::LauncherWindow(GameLibrary& library)
    : m_library(library), m_window(nullptr), m_renderer(nullptr), m_selected(0),
      m_mouse{-1.0f, -1.0f}, m_lastClickTime(0), m_lastClickCard(-1)
{
    if (!SDL_CreateWindowAndRenderer("LibreGrimrock", WindowWidth, WindowHeight, 0, &m_window,
                                     &m_renderer))
    {
        m_window = nullptr;
        m_renderer = nullptr;
        return;
    }
    SDL_SetRenderVSync(m_renderer, 1);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    m_titleFont.reset(new Font(m_renderer, "serif:bold", 34));
    m_cardFont.reset(new Font(m_renderer, "serif:bold", 24));
    m_textFont.reset(new Font(m_renderer, "sans-serif", 15));
    m_smallFont.reset(new Font(m_renderer, "sans-serif", 13));
    // the first game that is installed is the one Enter plays
    for (int i = NumGames - 1; i >= 0; --i)
        if (isAvailable(i))
            m_selected = i;
    layout();
    loadIcons();
}

LauncherWindow::~LauncherWindow()
{
    for (Card& card : m_cards)
        if (card.icon)
            SDL_DestroyTexture(card.icon);
    m_titleFont.reset();
    m_cardFont.reset();
    m_textFont.reset();
    m_smallFont.reset();
    if (m_renderer)
        SDL_DestroyRenderer(m_renderer);
    if (m_window)
        SDL_DestroyWindow(m_window);
}

void LauncherWindow::layout()
{
    float cardWidth = (WindowWidth - 2 * Margin - CardGap) / NumGames;
    for (int i = 0; i < NumGames; ++i)
    {
        Card& card = m_cards[i];
        card.bounds = {Margin + i * (cardWidth + CardGap), CardTop, cardWidth, CardHeight};
        float buttonWidth = (cardWidth - 3 * 20.0f) / 2;
        float buttonY = card.bounds.y + card.bounds.h - ButtonHeight - 20.0f;
        card.play = {card.bounds.x + 20.0f, buttonY, buttonWidth, ButtonHeight};
        card.locate = {card.play.x + buttonWidth + 20.0f, buttonY, buttonWidth, ButtonHeight};
    }
}

void LauncherWindow::loadIcons()
{
    for (int i = 0; i < NumGames; ++i)
    {
        if (m_cards[i].icon)
            SDL_DestroyTexture(m_cards[i].icon);
        m_cards[i].icon =
            loadGameIcon(m_renderer, Games[i], m_library.getInstall(Games[i].number).directory);
    }
}

bool LauncherWindow::isAvailable(int index) const
{
    return !m_library.getInstall(Games[index].number).directory.empty();
}

int LauncherWindow::run()
{
    // Debugging aid: GRIMROCK_LAUNCHER_SCREENSHOT=<file.bmp> saves the first frame and closes.
    const char* screenshot = getenv("GRIMROCK_LAUNCHER_SCREENSHOT");
    for (;;)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                return 0;
            case SDL_EVENT_MOUSE_MOTION:
                m_mouse = {event.motion.x, event.motion.y};
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
                if (event.button.button != SDL_BUTTON_LEFT)
                    break;
                SDL_FPoint point = {event.button.x, event.button.y};
                for (int i = 0; i < NumGames; ++i)
                {
                    Card& card = m_cards[i];
                    if (!contains(card.bounds, point))
                        continue;
                    m_selected = i;
                    if (contains(card.play, point) && isAvailable(i))
                        return Games[i].number;
                    if (contains(card.locate, point))
                    {
                        locate(i);
                        break;
                    }
                    Uint64 now = SDL_GetTicks();
                    if (m_lastClickCard == i && now - m_lastClickTime < DoubleClickMs &&
                        isAvailable(i))
                        return Games[i].number;
                    m_lastClickCard = i;
                    m_lastClickTime = now;
                }
                break;
            }
            case SDL_EVENT_KEY_DOWN:
                switch (event.key.key)
                {
                case SDLK_ESCAPE:
                    return 0;
                case SDLK_LEFT:
                    m_selected = (m_selected + NumGames - 1) % NumGames;
                    break;
                case SDLK_RIGHT:
                case SDLK_TAB:
                    m_selected = (m_selected + 1) % NumGames;
                    break;
                case SDLK_1:
                case SDLK_2:
                    m_selected = (int)(event.key.key - SDLK_1);
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                case SDLK_SPACE:
                    if (isAvailable(m_selected))
                        return Games[m_selected].number;
                    locate(m_selected);
                    break;
                case SDLK_L:
                    locate(m_selected);
                    break;
                }
                break;
            }
        }
        takeChosenFolder();
        draw();
        if (screenshot)
        {
            if (SDL_Surface* frame = SDL_RenderReadPixels(m_renderer, nullptr))
            {
                SDL_SaveBMP(frame, screenshot);
                SDL_DestroySurface(frame);
            }
            return 0;
        }
        SDL_RenderPresent(m_renderer);
    }
}

void LauncherWindow::locate(int index)
{
    {
        std::lock_guard<std::mutex> lock(g_folderDialog.mutex);
        if (g_folderDialog.game)
            return; // one dialog at a time
        g_folderDialog.game = Games[index].number;
        g_folderDialog.answered = false;
    }
    const GameInstall& install = m_library.getInstall(Games[index].number);
    std::string start =
        install.directory.empty() ? m_library.getLauncherDirectory() : install.directory;
    // unlocked: without a dialog backend SDL answers at once, on this thread
    SDL_ShowOpenFolderDialog(onFolderChosen, &g_folderDialog, m_window, start.c_str(), false);
}

void LauncherWindow::takeChosenFolder()
{
    int game;
    std::string folder;
    {
        std::lock_guard<std::mutex> lock(g_folderDialog.mutex);
        if (!g_folderDialog.game || !g_folderDialog.answered)
            return;
        game = g_folderDialog.game;
        folder = g_folderDialog.folder;
        g_folderDialog.game = 0;
    }
    if (folder.empty())
        return; // cancelled
    Card& card = m_cards[game - 1];
    if (m_library.setInstall(game, folder))
    {
        card.message.clear();
        loadIcons();
    }
    else
    {
        card.message = std::string("No ") + findGame(game)->archive + " in that folder";
    }
}

void LauncherWindow::fillRect(const SDL_FRect& rect, SDL_Color color)
{
    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(m_renderer, &rect);
}

void LauncherWindow::outlineRect(const SDL_FRect& rect, SDL_Color color, float thickness)
{
    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
    SDL_FRect edges[4] = {{rect.x, rect.y, rect.w, thickness},
                          {rect.x, rect.y + rect.h - thickness, rect.w, thickness},
                          {rect.x, rect.y, thickness, rect.h},
                          {rect.x + rect.w - thickness, rect.y, thickness, rect.h}};
    SDL_RenderFillRects(m_renderer, edges, 4);
}

void LauncherWindow::drawButton(const SDL_FRect& bounds, const char* label, bool enabled,
                                bool hovered, bool primary)
{
    SDL_Color fill = !enabled  ? DisabledColor
                     : primary ? (hovered ? PrimaryHover : PrimaryColor)
                               : (hovered ? ButtonHover : ButtonColor);
    fillRect(bounds, fill);
    outlineRect(bounds, enabled ? (primary ? Gold : CardBorder) : CardBorder, 1.0f);
    int width = m_textFont->measure(label);
    m_textFont->draw(label, bounds.x + (bounds.w - width) / 2,
                     bounds.y + (bounds.h - m_textFont->getHeight() * 1.2f) / 2,
                     enabled ? Text : DimText);
}

void LauncherWindow::drawCard(int index)
{
    const GameInfo& game = Games[index];
    const GameInstall& install = m_library.getInstall(game.number);
    Card& card = m_cards[index];
    bool available = isAvailable(index);
    bool selected = index == m_selected;
    bool hovered = contains(card.bounds, m_mouse);

    fillRect(card.bounds, CardColor);
    outlineRect(card.bounds, selected ? Gold : (hovered ? DimText : CardBorder),
                selected ? 2.0f : 1.0f);

    // the icon, or an empty frame with the game's number
    SDL_FRect icon = {card.bounds.x + (card.bounds.w - IconSize) / 2, card.bounds.y + 24.0f,
                      IconSize, IconSize};
    if (card.icon)
    {
        if (!available)
            SDL_SetTextureColorMod(card.icon, 110, 110, 110);
        else
            SDL_SetTextureColorMod(card.icon, 255, 255, 255);
        SDL_RenderTexture(m_renderer, card.icon, nullptr, &icon);
    }
    else
    {
        outlineRect(icon, CardBorder, 1.0f);
        std::string number = std::to_string(game.number);
        m_titleFont->draw(number, icon.x + (icon.w - m_titleFont->measure(number)) / 2,
                          icon.y + (icon.h - m_titleFont->getHeight() * 1.2f) / 2, DimText);
    }

    float y = icon.y + icon.h + 18.0f;
    float textWidth = card.bounds.w - 40.0f;
    m_cardFont->draw(game.title,
                     card.bounds.x + (card.bounds.w - m_cardFont->measure(game.title)) / 2, y,
                     available ? Gold : DimText);
    y += m_cardFont->getHeight() * 1.5f;

    std::string status = available ? "Found: " + install.source : std::string("Not found");
    m_textFont->draw(status, card.bounds.x + 20.0f, y, available ? Text : DimText);
    y += m_textFont->getHeight() * 1.5f;
    if (available)
        m_smallFont->draw(m_smallFont->fit(install.directory, (int)textWidth),
                          card.bounds.x + 20.0f, y, DimText);
    else
        m_smallFont->draw(
            m_smallFont->fit(std::string("Locate the folder with ") + game.archive, (int)textWidth),
            card.bounds.x + 20.0f, y, DimText);
    y += m_smallFont->getHeight() * 1.5f;
    if (!card.message.empty())
        m_smallFont->draw(card.message, card.bounds.x + 20.0f, y, ErrorText);

    drawButton(card.play, "Play", available, available && contains(card.play, m_mouse), true);
    drawButton(card.locate, "Locate\xE2\x80\xA6", true, contains(card.locate, m_mouse), false);
}

void LauncherWindow::draw()
{
    // a vertical gradient behind everything
    auto vertex = [](float x, float y, SDL_Color color)
    {
        SDL_Vertex v;
        v.position = {x, y};
        v.color = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, 1.0f};
        v.tex_coord = {0.0f, 0.0f};
        return v;
    };
    SDL_Vertex background[4] = {vertex(0, 0, BackgroundTop),
                                vertex((float)WindowWidth, 0, BackgroundTop),
                                vertex((float)WindowWidth, (float)WindowHeight, BackgroundBottom),
                                vertex(0, (float)WindowHeight, BackgroundBottom)};
    const int quad[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(m_renderer, nullptr, background, 4, quad, 6);

    const char* title = "LibreGrimrock";
    m_titleFont->draw(title, (WindowWidth - m_titleFont->measure(title)) / 2.0f, 30.0f, Gold);
    const char* subtitle = "Choose a game";
    m_textFont->draw(subtitle, (WindowWidth - m_textFont->measure(subtitle)) / 2.0f, 82.0f,
                     DimText);

    for (int i = 0; i < NumGames; ++i)
        drawCard(i);

    const char* help = "Enter: play     Left / Right: choose     L: locate     Esc: quit";
    m_smallFont->draw(help, (WindowWidth - m_smallFont->measure(help)) / 2.0f, WindowHeight - 34.0f,
                      DimText);
}

} // namespace launcher
