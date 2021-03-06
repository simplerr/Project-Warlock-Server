#include <crtdbg.h> 
#include <assert.h>
#include <time.h>
#include "Game.h"
#include "Graphics.h"
#include "Input.h"
#include "World.h"
#include "vld.h"
#include "Primitive.h"
#include "Camera.h"
#include "Effects.h"
#include "CameraFPS.h"
#include "CameraRTS.h"
#include "Server.h"
#include <fstream>
#include "Database.h"
#include "Sound.h"
#include "Console.h"

using namespace GLib;

// Set global to NULL.
GLib::Runnable* GLib::GlobalApp = nullptr;
ServerCvars* gCvars = nullptr;
Sound*	gSound = nullptr;
Console* gConsole = nullptr;

//! The program starts here.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	// Create a Game instance.
	Game game(hInstance, "Server", 800, 600);
	GLib::GlobalApp = &game;

	// Init the app.
	game.Init();

	// Run the app.
	return GLib::GlobalApp->Run();
}

Game::Game(HINSTANCE hInstance, string caption, int width, int height)
	: Runnable(hInstance, caption, width, height)
{
	// Cap the fps to 100.
	//SetFpsCap(100.0f);
	gCvars = new ServerCvars();

	// Create a console window.
	gConsole = new Console();
	gConsole->Startup();

	SetConsoleTitle("Warlock Server");

	SetFpsCap(100.0f);
}

Game::~Game()
{
	delete mPeer;
	delete gCvars;
	delete gConsole;
}

void Game::Init()
{
	// Important to run Systems Init() function.
	Runnable::Init();

	ShowWindow(GetHwnd(), false);

	// Add a camera.
	GLib::CameraRTS* camera = new GLib::CameraRTS();
	camera->SetLocked(true);
	GetGraphics()->SetCamera(camera);

	// Create the peer.
	mPeer = new Server();
	mPeer->StartServer();
	
	// Set the fog color.
	GetGraphics()->SetFogColor(XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f));
}

void Game::Update(GLib::Input* pInput, float dt)
{
	mPeer->Update(pInput, dt);
	GetGraphics()->Update(pInput, dt);

	char buffer[256];
	sprintf(buffer, "Warlock Server (FPS: %.1f)", GetCurrentFps());
	SetConsoleTitle(buffer);
}

void Game::Draw(GLib::Graphics* pGraphics)
{
	return;
	// Clear the render target and depth/stencil.
	pGraphics->ClearScene();

	mPeer->Draw(pGraphics);
	pGraphics->DrawBillboards();

	// Present the backbuffer.
	pGraphics->Present();

	// Unbind the SRVs from the pipeline so they can be used as DSVs instead.
	ID3D11ShaderResourceView *const nullSRV[4] = {NULL, NULL, NULL, NULL};
	pGraphics->GetContext()->PSSetShaderResources(0, 4, nullSRV);
	Effects::BasicFX->Apply(GetD3DContext());
}

//! Called when the window gets resized.
void Game::OnResize(int width, int height)
{

}

LRESULT Game::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Toggle screen mode?
	if(msg == WM_CHAR) {
		if(wParam == 'f')	
			SwitchScreenMode();
	}

	return Runnable::MsgProc(hwnd, msg, wParam, lParam);
}