#pragma once

#include "RadishCommon.h"

#include <SDL2/SDL.h>

namespace rad
{

struct InputManager : Singleton<InputManager>
{
	// Keep last & current sdl key states
	enum KEY_STATE
	{
		KEY_UP = 0,
		KEY_DOWN = 1,
		KEY_PRESSED = 2,
		KEY_RELEASED = 3
	};
	KEY_STATE CUR_KEYS[SDL_NUM_SCANCODES];

	bool CursorEnabled = false;

	struct Immediate
	{
		float MouseWheelDelta = 0.0f;
		glm::vec2 MouseDelta = {0, 0};
	} Immediate;

	bool Init();

	bool IsKeyDown(SDL_Scancode key)
	{
		return CUR_KEYS[key] == KEY_DOWN || CUR_KEYS[key] == KEY_PRESSED;
	}

	bool IsKeyPressed(SDL_Scancode key)
	{
		return CUR_KEYS[key] == KEY_PRESSED;
	}

	bool IsKeyReleased(SDL_Scancode key)
	{
		return CUR_KEYS[key] == KEY_RELEASED;
	}

	bool IsKeyUp(SDL_Scancode key)
	{
		return CUR_KEYS[key] == KEY_UP || CUR_KEYS[key] == KEY_RELEASED;
	}
};

} // namespace rad