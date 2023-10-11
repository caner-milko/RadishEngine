#include <Application.h>
#include <Game.h>
#include <Window.h>

#include <DirectXMath.h>
namespace dfr
{

Game::Game(const std::wstring& name, int width, int height, bool vSync)
    : Name(name)
    , Width(width)
    , Height(height)
    , vSync(vSync)
{
}

Game::~Game()
{
    assert(!Window && "Use Game::Destroy() before destruction.");
}

bool Game::Initialize()
{
    // Check for DirectX Math library support.
    if (!DirectX::XMVerifyCPUSupport())
    {
        MessageBoxA(NULL, "Failed to verify DirectX Math library support.", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    Window = Application::Get().CreateRenderWindow(Name, Width, Height, vSync);
    Window->RegisterCallbacks(shared_from_this());
    Window->Show();

    return true;
}

void Game::Destroy()
{
    Application::Get().DestroyWindow(Window);
    Window.reset();
}

void Game::OnUpdate(UpdateEventArgs& e)
{

}

void Game::OnRender(RenderEventArgs& e)
{

}

void Game::OnKeyPressed(KeyEventArgs& e)
{
    // By default, do nothing.
}

void Game::OnKeyReleased(KeyEventArgs& e)
{
    // By default, do nothing.
}

void Game::OnMouseMoved(class MouseMotionEventArgs& e)
{
    // By default, do nothing.
}

void Game::OnMouseButtonPressed(MouseButtonEventArgs& e)
{
    // By default, do nothing.
}

void Game::OnMouseButtonReleased(MouseButtonEventArgs& e)
{
    // By default, do nothing.
}

void Game::OnMouseWheel(MouseWheelEventArgs& e)
{
    // By default, do nothing.
}

void Game::OnResize(ResizeEventArgs& e)
{
    Width = e.Width;
    Height = e.Height;
}

void Game::OnWindowDestroy()
{
    // If the Window which we are registered to is 
    // destroyed, then any resources which are associated 
    // to the window must be released.
    UnloadContent();
}

};