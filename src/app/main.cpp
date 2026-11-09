// SPDX-License-Identifier: GPL-3.0-or-later
// SDL3 frontend: window, input, timing. All game logic lives in lemcore.
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "config.h"
#include "screens.h"

using namespace lem;

namespace {

struct App {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    Config config;
    std::unique_ptr<Frontend> frontend;
    Uint64 lastTick = 0;
    bool waitingForFolder = false;
    // Test hooks: --level R:N, --play, --shot file.bmp, --frames N
    int startRating = -1, startLevel = 0;
    bool autoPlay = false;
    std::string shotPath;
    int shotFrames = 0;
    int framesRun = 0;
};

// Stores folders as absolute paths so the app works from any start directory.
std::string absolutePath(const std::string& p) {
#ifdef _WIN32
    char buf[_MAX_PATH];
    return _fullpath(buf, p.c_str(), sizeof buf) ? std::string(buf) : p;
#else
    char buf[PATH_MAX];
    return realpath(p.c_str(), buf) ? std::string(buf) : p;
#endif
}

bool isDataDir(const std::string& dir) {
    return !dir.empty() && fileExists(joinPath(dir, "main.dat")) &&
           fileExists(joinPath(dir, "level000.dat"));
}

void startGame(App& app) {
    try {
        int r = app.startRating >= 0 ? app.startRating : app.config.lastRating;
        int n = app.startRating >= 0 ? app.startLevel : app.config.lastLevel;
        app.frontend = std::make_unique<Frontend>(app.config.dataDir, r, n);
        app.frontend->setSound(app.config.music, app.config.sfx);
        if (app.autoPlay) app.frontend->startLevel();
        SDL_SetWindowTitle(app.window, "Lem_Port");
    } catch (const std::exception& e) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lem_Port", e.what(), app.window);
        app.frontend.reset();
    }
}

void SDLCALL onFolderChosen(void* userdata, const char* const* files, int) {
    App& app = *static_cast<App*>(userdata);
    app.waitingForFolder = false;
    if (!files || !files[0]) return;
    if (!isDataDir(files[0])) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Lem_Port",
                                 "That folder does not contain the DOS Lemmings data files "
                                 "(main.dat, level000.dat, ...).", app.window);
        return;
    }
    app.config.dataDir = files[0];
    saveConfig(app.config);
}

#ifdef SDL_PLATFORM_IOS
// On iOS the data is copied into the app's Documents folder (visible in the
// Files app and in Finder). Accept the files there or in any direct subfolder.
std::string findDocumentsData() {
    const char* docs = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    if (!docs) return {};
    std::string base = docs;
    if (isDataDir(base)) return base;
    int count = 0;
    std::string found;
    if (char** names = SDL_GlobDirectory(docs, "*", 0, &count)) {
        for (int i = 0; i < count && found.empty(); ++i) {
            std::string sub = base + (base.empty() || base.back() == '/' ? "" : "/") + names[i];
            if (isDataDir(sub)) found = sub;
        }
        SDL_free(names);
    }
    return found;
}
#endif

void askForFolder(App& app) {
#ifdef SDL_PLATFORM_IOS
    return;   // no folder picker: the files are copied into Documents instead
#endif
    if (app.waitingForFolder) return;
    app.waitingForFolder = true;
    SDL_ShowOpenFolderDialog(onFolderChosen, &app, app.window, nullptr, false);
}

} // namespace

SDL_AppResult SDL_AppInit(void** state, int argc, char** argv) {
    auto* app = new App;
    *state = app;
    SDL_SetAppMetadata("Lem_Port", "0.1", "io.github.sectroyer.lemport");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    app->config = loadConfig();
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--data") && i + 1 < argc) {
            app->config.dataDir = absolutePath(argv[++i]);
            saveConfig(app->config);
        } else if (!std::strcmp(argv[i], "--scale") && i + 1 < argc) {
            app->config.scale = std::max(1, std::atoi(argv[++i]));
        } else if (!std::strcmp(argv[i], "--level") && i + 1 < argc) {
            std::sscanf(argv[++i], "%d:%d", &app->startRating, &app->startLevel);
            app->startLevel -= 1;
        } else if (!std::strcmp(argv[i], "--play")) {
            app->autoPlay = true;
        } else if (!std::strcmp(argv[i], "--shot") && i + 1 < argc) {
            app->shotPath = argv[++i];
        } else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) {
            app->shotFrames = std::atoi(argv[++i]);
        }
    }
    if (const char* env = SDL_getenv("LEM_DATA"); env && app->config.dataDir.empty())
        app->config.dataDir = env;
#ifdef SDL_PLATFORM_IOS
    if (!isDataDir(app->config.dataDir)) app->config.dataDir = findDocumentsData();
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    const SDL_WindowFlags flags = SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#else
    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
#endif

    if (!SDL_CreateWindowAndRenderer("Lem_Port", kScreenW * app->config.scale,
                                     kScreenH * app->config.scale, flags,
                                     &app->window, &app->renderer)) {
        SDL_Log("window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    // Fit any window or screen; the pixel art scaler keeps pixels crisp at
    // non-integer sizes (iPad 1.6x, phones, odd desktop windows).
    SDL_SetRenderLogicalPresentation(app->renderer, kScreenW, kScreenH,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(app->renderer, 1);
    app->texture = SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_XRGB8888,
                                     SDL_TEXTUREACCESS_STREAMING, kScreenW, kScreenH);
    SDL_SetTextureScaleMode(app->texture, SDL_SCALEMODE_PIXELART);
    SDL_HideCursor();

    if (isDataDir(app->config.dataDir)) startGame(*app);
    else askForFolder(*app);
    app->lastTick = SDL_GetTicksNS() / 1000;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* state, SDL_Event* ev) {
    App& app = *static_cast<App*>(state);
    if (ev->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
    if (!app.frontend) {
        if (ev->type == SDL_EVENT_KEY_DOWN && ev->key.key == SDLK_ESCAPE) return SDL_APP_SUCCESS;
        if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_KEY_DOWN) askForFolder(app);
        return SDL_APP_CONTINUE;
    }
    if (ev->type == SDL_EVENT_DID_ENTER_BACKGROUND || ev->type == SDL_EVENT_WILL_ENTER_BACKGROUND) {
        app.frontend->suspend();
        saveConfig(app.config);
        return SDL_APP_CONTINUE;
    }
    if (ev->type == SDL_EVENT_DID_ENTER_FOREGROUND) {
        app.frontend->resume();
        app.lastTick = SDL_GetTicksNS() / 1000;
        return SDL_APP_CONTINUE;
    }
    SDL_ConvertEventToRenderCoordinates(app.renderer, ev);
    if (ev->type == SDL_EVENT_KEY_DOWN && ev->key.key == SDLK_RETURN && (ev->key.mod & SDL_KMOD_ALT)) {
        bool fs = SDL_GetWindowFlags(app.window) & SDL_WINDOW_FULLSCREEN;
        SDL_SetWindowFullscreen(app.window, !fs);
        return SDL_APP_CONTINUE;
    }
    if (!app.frontend->handleEvent(*ev)) return SDL_APP_SUCCESS;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* state) {
    App& app = *static_cast<App*>(state);
#ifdef SDL_PLATFORM_IOS
    // Wait for the files to appear in Documents (checked about once a second).
    static Uint64 lastScan = 0;
    if (!app.frontend && SDL_GetTicks() - lastScan > 1000) {
        lastScan = SDL_GetTicks();
        app.config.dataDir = findDocumentsData();
        if (!app.config.dataDir.empty()) saveConfig(app.config);
    }
#endif
    if (!app.frontend && isDataDir(app.config.dataDir)) startGame(app);

    Uint64 now = SDL_GetTicksNS() / 1000;   // microseconds
    if (app.frontend) {
        // Fixed game frames (68.7 ms), catching up at most a few frames.
        int steps = 0;
        if (!app.shotPath.empty()) {
            // Test mode: run as fast as possible.
            app.frontend->update();
            ++app.framesRun;
        }
        while (app.shotPath.empty() && now - app.lastTick >= Uint64(Game::kFrameMicros) && steps < 4) {
            app.lastTick += Game::kFrameMicros;
            app.frontend->update();
            ++steps;
        }
        if (now - app.lastTick > 1000000) app.lastTick = now;
    }

    void* pixels;
    int pitch;
    if (SDL_LockTexture(app.texture, nullptr, &pixels, &pitch)) {
        if (app.frontend) {
            app.frontend->draw(static_cast<uint32_t*>(pixels), pitch / 4);
            if (!app.shotPath.empty() && app.framesRun >= app.shotFrames) {
                SDL_Surface* s = SDL_CreateSurfaceFrom(kScreenW, kScreenH, SDL_PIXELFORMAT_XRGB8888, pixels, pitch);
                SDL_SaveBMP(s, app.shotPath.c_str());
                SDL_DestroySurface(s);
                SDL_UnlockTexture(app.texture);
                return SDL_APP_SUCCESS;
            }
        } else {
            for (int y = 0; y < kScreenH; ++y)
                std::memset(static_cast<uint8_t*>(pixels) + y * pitch, 0, size_t(kScreenW) * 4);
        }
        SDL_UnlockTexture(app.texture);
    }
    SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
    SDL_RenderClear(app.renderer);
    SDL_RenderTexture(app.renderer, app.texture, nullptr, nullptr);
    if (!app.frontend) {
        SDL_SetRenderDrawColor(app.renderer, 200, 200, 200, 255);
#ifdef SDL_PLATFORM_IOS
        SDL_RenderDebugText(app.renderer, 24, 80, "Copy the DOS Lemmings data files (main.dat,");
        SDL_RenderDebugText(app.renderer, 24, 92, "level000.dat, ...) into the Lem_Port folder");
        SDL_RenderDebugText(app.renderer, 24, 104, "with the Files app or Finder. The game starts");
        SDL_RenderDebugText(app.renderer, 24, 116, "as soon as the files are there.");
#else
        SDL_RenderDebugText(app.renderer, 24, 80, "Choose the folder with the DOS");
        SDL_RenderDebugText(app.renderer, 24, 92, "Lemmings data files (main.dat...)");
        SDL_RenderDebugText(app.renderer, 24, 116, "Click to open the folder dialog");
#endif
    }
    SDL_RenderPresent(app.renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* state, SDL_AppResult) {
    App* app = static_cast<App*>(state);
    if (app) {
        if (app->frontend) {
            app->config.lastRating = app->frontend->rating();
            app->config.lastLevel = app->frontend->levelNumber();
            app->config.music = app->frontend->musicOn();
            app->config.sfx = app->frontend->sfxOn();
        }
        app->frontend.reset();
        saveConfig(app->config);
        delete app;
    }
}
