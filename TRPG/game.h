#pragma once
#include "SceneManager.h"
#include <d3d11.h>

// game.h
void gameinit(HWND hwnd, 
	ID3D11Device* device, 
	ID3D11DeviceContext* deviceContext);
void gamedispose();
void gameloop();
