#include "pch.h"
#include "Game.h"
#include "DEngine.h"

void Game::Init(const WindowInfo& info)
{
	GDEngine->Init(info);
}

void Game::Update()
{
	GDEngine->Render();
}
