#pragma once
#include "SceneType.h"
#include <memory>
#include <string>
#include <Windows.h>
#include "system/imgui/imgui.h"
#include "IScene.h"
#include <unordered_map>

// CharacterSelect's CharcterDate is used for storing player data
#include "CharacterSelect.h"

class IScene;

class SceneManager {
public:
	void Initialize();	//初期化処理
	void Update();		//入力ロジック更新
	void Render();		//描画処理
	void Finalize();
	void SetWindowHandle(HWND hwnd)
	{
		m_hWnd = hwnd;
	} // ウィンドウハンドルを渡す関数

	void ChangeScene(SceneType Next);	//シーン切り替え
	SceneType GetCurrentScene() const;	//現在のシーン確認
	void HandleInput();

	// プレイヤーデータの設定/取得
	void SetPlayer(const CharcterScene::CharcterDate& p) { playerData = p; }
	CharcterScene::CharcterDate& GetPlayer() { return playerData; }
	const CharcterScene::CharcterDate& GetPlayer() const { return playerData; }

private:
	std::unique_ptr<IScene>currentScene;	//現在のシーンインスタンス	
	SceneType currentType;					//現在のシーン種類
	HWND m_hWnd = nullptr; // ウィンドウハンドルを保持
	bool spacePressedLastFrame = false; // 前フレームの状態を保持

	//シーンの移動をマップにしてコード纏めた
	std::unordered_map<SceneType, SceneType> nextSceneMap = {
	{ SceneType::TITLE, SceneType::TRPG_SELECT },
	{ SceneType::TRPG_SELECT, SceneType::SCENARIO_SELECT },
	{ SceneType::SCENARIO_SELECT, SceneType::CHARACTER_SELECT },
	{ SceneType::CHARACTER_SELECT, SceneType::GAME_PLAY },
	{ SceneType::GAME_PLAY, SceneType::RESULT },
	{ SceneType::RESULT, SceneType::TITLE},
	// 他のシーンもここに追加
	};

	CharcterScene::CharcterDate playerData;
	// プレイヤーデータを保持（シーン間で共有）


};
extern SceneManager g_SceneManager; // 実体は別ファイルに

