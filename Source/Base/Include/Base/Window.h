#pragma once

#include "Base/Common.h"

namespace dfr
{
struct WindowGraphicsInterface
{
	virtual void InitWindow(struct HWND* hwnd, glm::uvec2 extent) = 0;
	virtual void Resized(glm::uvec2 oldExtent, glm::uvec2 newExtent) = 0;
	virtual void Closed(struct HWND* hwnd) = 0;
};
class Window
{
	Window();
	~Window();

	struct SDL_Window* SDLWindowHandle;
};
}