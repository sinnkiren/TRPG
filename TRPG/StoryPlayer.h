#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include "IScene.h"
#include "system/json.hpp"
#include "system/imgui/imgui.h"
#include "EventNode.h"
#include <d3d11.h>
#include <chrono>

using json = nlohmann::json;

struct StoryEvent
{
	std::string text;
	std::string speaking;
	std::string faceImage;
	std::string effect;
	float duration = 1.0f;
	json effectParams = nullptr; // JSON での効果パラメータを保持
};
// キャラクター状態（複数キャラ対応の基盤）
struct CharacterState {
	std::string id;              // 識別子（JSON や StoryEvent の speaker フィールドと連携）
	std::string imagePath;       // 立ち絵のパス
	std::string expression;      // 表情差分やバリアント名
	ImVec2 position = ImVec2(0.1f, 0.7f); // 画面上の位置（0..1 の正規化座標）
	bool visible = false;        // 表示フラグ
	// 将来的にテクスチャハンドルを保持する場
	ImTextureID tex = nullptr;
};

// Forward helpers implemented in StoryPlayer_Editor.cpp
void ParseCharactersJson(const json &cj, std::vector<CharacterState> &out);
void DrawCharacters(const std::vector<CharacterState>& chars, ImDrawList* bg, const ImGuiViewport* vp);
class StoryPlayer : public IScene {
	using EffectHandler = std::function<void(const StoryEvent)>;
public:
	StoryPlayer();
	~StoryPlayer();
	void Initialize() override;
	void Update() override;   // 未使用
	void Render() override;
	void RenderUI() override;

	void UpdateImpl(float dt); // float dt を受け取る更新

	bool LoadFromFile(const std::string& path);
	bool SaveToFile(const std::string& path) const;
	void Play();//再生
	void Pause();//一時停止
	void Next();//次へ
	void Reset();//リセット

	// エフェクト登録 API
	void RegisterEffect(const std::string& name, EffectHandler  handler);
	// イベント完了コールバック
	std::function<void(const StoryEvent&)>onEventFinished;

	// 背景画像設定（汎用 ImTextureID）
	void SetBackgroundTexture(ImTextureID tex) { m_bgTex = tex; }
	ImTextureID GetBackgroundTexture() const { return m_bgTex; }

#ifdef IMGUI_IMPL_DIRECTX11
	// DirectX11 の SRV を渡すヘルパー（StoryPlayer が既存 SRV を解放して管理します）
	void SetBackgroundSRV(ID3D11ShaderResourceView* srv);
#endif

private:
	std::vector<StoryEvent>m_events;
	int m_index = 0;
	float m_timer = 0.0f;
	bool m_playing = false;

	// Graph-based nodes (Phase1 base)
	std::unordered_map<int, EventNode> m_nodeMap; // id -> node
	int m_currentNodeId = -1;
    bool m_waitChoice = false; // true when waiting for player choice in node graph
	bool m_usingNodeGraph = false; // true if loaded graph is active

	// Load node graph from JSON file (uses EventNode::FromJson)
	bool LoadGraphFromFile(const std::string& path);
	// Select a choice from current node (by index)
	void SelectChoice(int choiceIndex);
	// --- Phase2: multiple-character support ---
	std::vector<CharacterState> m_characters;
	// Character management
	void AddOrUpdateCharacter(const CharacterState& s);
	bool RemoveCharacterById(const std::string& id);
	CharacterState* FindCharacter(const std::string& id);
	void SetCharacterVisible(const std::string& id, bool visible);

	std::unordered_map < std::string, EffectHandler>m_effects;
	void TriggerEffect(const StoryEvent& ev);
	void ShowCurrentText();

	// 追加メンバ
	float m_textScale = 1.0f;      // 文字の拡大率（1.0 = 標準）
	ImVec2 m_dialogSize = ImVec2(400.0f, 160.0f); // ダイアログサイズのデフォルト

	// フェード遷移用（story -> battle）
	float m_fsFadeAlpha = 0.0f;
	float m_fsFadeDuration = 0.8f;
	float m_fsFadeElapsed = 0.0f;
	bool  m_fsFadingOut = false;

	// 背景テクスチャ（ImGui 用）
	ImTextureID m_bgTex = nullptr;

#ifdef IMGUI_IMPL_DIRECTX11
	// DirectX11 用に SRV の参照を保持して自動解放
	ID3D11ShaderResourceView* m_bgSrv = nullptr;
#endif

	// 遅延ロード用メンバ（追加）
	std::string m_bgPath;             // 読み込む画像のパス（遅延読み込み用）
	bool m_bgLoadedAttempted = false; // 一度読み込み試行済みフラグ

	// 内部ヘルパー（遅延読み込み）
	void LoadBackgroundTextureIfNeeded();
	// load character textures lazily
	void LoadCharacterTexturesIfNeeded();

	// フェード開始
	void StartFadeToBattle(float duration = 0.8f);

    // 最後のロード/保存エラーを保持（UI表示用）
    std::string m_lastLoadError;

    // 遅延ロード再試行制御
    int m_bgLoadAttempts = 0;          // 試行回数
    int m_bgMaxLoadAttempts = 8;       // 最大試行回数（-1 で無制限）
    float m_bgLastAttemptTime = 0.0f;  // 最終試行時刻 (ImGui::GetTime())
    float m_bgRetryInterval = 2.0f;    // 再試行間隔（秒）

	// Dev UI: toggle for overlay/debug window
	bool m_showDevWindow = false;
    // Dev helper: node jump input
    int m_devNodeInput = 0;
    // Expose some info to centralized dev panel
public:
    void RenderDevPanelContents();
	// Separate editor UI (used by StoryEditorScene). Only shows in Dev mode.
	void RenderEditorUI();

	// Runtime: update/render the current node (display text and choices)
	void UpdateNode();

	// Editor sub-renderers (refactored from RenderEditorUI)
	void RenderEventEditor();
	void RenderNodeEditor();
	void RenderNodeGraphCanvas();
	void RenderSelectedEventEditor();
	void RenderChoiceEditor(EventNode &node, int choiceIndex);
	void RenderEffectsEditor(Choice &c, int choiceIndex);

	// Node graph editor API
	bool SaveGraphToFile(const std::string& path) const;
	int CreateNode(); // returns new node id
	bool DeleteNode(int nodeId);
	bool AddChoiceToNode(int nodeId, const Choice& choice);
	bool RemoveChoiceFromNode(int nodeId, int choiceIndex);

	// Editor mode: 0 = Events, 1 = NodeGraph
	int m_editorMode = 0;
	// Node editor state
	int m_nodeEditorSelectedId = -1;
	// ノードの画面上の位置（0..1 の正規化座標）。Node Graph ビューで編集・保存します。
	std::unordered_map<int, ImVec2> m_nodePositions;

	// Node graph view interaction state
	ImVec2 m_nodeGraphPan = ImVec2(0.0f, 0.0f); // パンオフセット（ピクセル）
	float m_nodeGraphZoom = 1.0f; // ズーム係数
	// 接続作成中の状態
	bool m_draggingConnection = false;
	int m_dragSourceNode = -1;
	int m_dragSourceChoiceIndex = -1; // 接続ドラッグの元となる choice インデックス（-1 = 新規作成）
	// ポップアップ用: 接続開始で選択候補を表示するノード ID
	int m_pendingChoicePopupNode = -1;

	// 選択されたエッジ（接続）の情報。None の場合は -1。
	int m_selectedEdgeSourceNode = -1;
	int m_selectedEdgeChoiceIndex = -1;
	int m_selectedEdgeTargetNode = -1;

	// Editor state for creating/editing stories
	int m_editorSelected = -1;                // 選択中のイベントインデックス
	std::string m_editorEffectParamsBuf;      // 選択中イベントの effectParams をテキストで編集するバッファ
	std::string m_editorStoryPath;            // 編集時の読み書き用パス（UIで編集可）

	// Event preview API (for editor)
	void PreviewEvent(int index);
	void StopPreview();

	// Runtime story state: flags, inventory, variables
	std::unordered_set<std::string> m_flags; // simple set of flags
	std::vector<std::string> m_inventory;    // list of inventory items
	std::unordered_map<std::string,int> m_vars; // numeric variables

private:
	// Preview state
	bool m_previewActive = false;
	int m_previewPrevIndex = 0;
	bool m_previewPrevPlaying = false;
	float m_previewPrevTimer = 0.0f;
	float m_previewElapsed = 0.0f;
	float m_previewDuration = 0.0f;
};