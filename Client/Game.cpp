#include "pch.h"
#include "Game.h"
#include "DEngine.h"
#include "SceneManager.h"

void Game::Init(const WindowInfo& info)
{
	GDEngine->Init(info);

	GET_SINGLE(SceneManager)->LoadScene(L"TestScene");
}

void Game::Update()
{
	GDEngine->Update();
}
