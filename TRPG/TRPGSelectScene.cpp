#include "TRPGSelectScene.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "system/imgui/imgui.h"

// TRPG選択シーン: 画面を左右に分割して2つの選択肢を表示（画像があれば画像を表示）
// ユーザーがどちらかをクリックするとシナリオ選択へ遷移します。

static ImTextureID g_tex_cthulhu = nullptr;
static ImTextureID g_tex_sw25 = nullptr;
static bool g_texturesLoaded = false;
static int g_selectedIndex = -1; // 0 = クトゥルフ, 1 = SW2.5

void TRPGSelectScene::Initialize() {
    // assets/texture/ から表紙画像を読み込もうとします。画像は任意で、無ければ代替表示になります。
    g_tex_cthulhu = TextureManager::GetImGuiTexture("texture/kuto.jpg");
    g_tex_sw25 = TextureManager::GetImGuiTexture("texture/sw.jpg");
    g_texturesLoaded = true;
    g_selectedIndex = -1;
}

void TRPGSelectScene::Update() {
    ImGui::Begin("TRPG Select Scene", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Select TRPG System");
    ImGui::Separator();

    // 画面幅を左右半分に分けてそれぞれの項目を表示するレイアウト
    float availW = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float halfW = (availW - spacing) * 0.5f;
    // 表紙画像は縦長を想定し、幅よりも高めのサイズを使う
    ImVec2 imgSize(halfW, halfW * 1.4f);

    // 左側: クトゥルフ
    ImGui::BeginGroup();
    ImGui::Text("Call of Cthulhu");
    ImVec2 pos1 = ImGui::GetCursorScreenPos();
    bool clicked1 = false;
    if (g_tex_cthulhu) {
        if (ImGui::ImageButton(g_tex_cthulhu, imgSize, ImVec2(0,0), ImVec2(1,1), 0)) clicked1 = true;
    } else {
        if (ImGui::Button("Call of Cthulhu (no image)", imgSize)) clicked1 = true;
    }
    // 選択時のハイライトを描画
    if (g_selectedIndex == 0) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRect(ImVec2(pos1.x - 2, pos1.y - 2), ImVec2(pos1.x + imgSize.x + 2, pos1.y + imgSize.y + 2), IM_COL32(255,200,0,255), 4.0f, 0, 3.0f);
    }
    ImGui::EndGroup();

    ImGui::SameLine();

    // 右側: ソードワールド2.5
    ImGui::BeginGroup();
    ImGui::Text("Sword World 2.5");
    ImVec2 pos2 = ImGui::GetCursorScreenPos();
    bool clicked2 = false;
    if (g_tex_sw25) {
        if (ImGui::ImageButton(g_tex_sw25, imgSize, ImVec2(0,0), ImVec2(1,1), 0)) clicked2 = true;
    } else {
        if (ImGui::Button("Sword World 2.5 (no image)", imgSize)) clicked2 = true;
    }
    if (g_selectedIndex == 1) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRect(ImVec2(pos2.x - 2, pos2.y - 2), ImVec2(pos2.x + imgSize.x + 2, pos2.y + imgSize.y + 2), IM_COL32(255,200,0,255), 4.0f, 0, 3.0f);
    }
    ImGui::EndGroup();

    // クリック処理
    if (clicked1) {
        g_selectedIndex = 0;
        // Move to scenario selection (TRPG type can be stored elsewhere if needed)
        g_SceneManager.ChangeScene(SceneType::SCENARIO_SELECT);
    }
    if (clicked2) {
        g_selectedIndex = 1;
        g_SceneManager.ChangeScene(SceneType::SCENARIO_SELECT);
    }

    ImGui::Separator();
    ImGui::Text("Hint: click a cover to select the system and proceed.");

    ImGui::End();
}

void TRPGSelectScene::Render() {
    // ここでは特別なDirectX描画は不要（Update() 内で ImGui による描画を行っている）
}
