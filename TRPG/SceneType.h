#pragma once

enum class SceneType {
	TITLE,				//タイトル
	TRPG_SELECT,		//TRPGの選択（クトゥルフ、ソードワールド）
	SCENARIO_SELECT,	//シナリオの選択・表示
	CHARACTER_SELECT,	//キャラクター（選択・作成）
	GAME_PLAY,			//本編
	BATTLE,				//戦闘シーン（オプション）
	RESULT,				//リザルト
	RECORD,				//物語の進行状況記録
};