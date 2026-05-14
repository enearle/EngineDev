#pragma once

#ifdef GAME_EXPORTS
#define GAME_API __declspec(dllexport)
#else
#define GAME_API __declspec(dllimport)
#endif

extern "C" {
    GAME_API void GameInit();
    GAME_API void GameRunFrame(float dt);
    GAME_API void GameShutdown();
}
