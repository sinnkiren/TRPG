#pragma once

enum class SceneType {
	TITLE,				//タイトル
	TRPG_SELECT,		//TRPGの選択（クトゥルフ、ソードワールド）
	SCENARIO_SELECT,	//シナリオの選択・表示
	CHARACTER_SELECT,	//キャラクター（選択・作成）
	EXPLORE,				// 探索パート（ノード型）
    STORY_EDITOR,        // Story 作成/編集用シーン (Dev only)
	GAME_PLAY,			//本編
	BATTLE,				//戦闘シーン（オプション）
	RESULT,				//リザルト
	RECORD,				//物語の進行状況記録
};