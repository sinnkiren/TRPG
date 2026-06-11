#include "CharacterSelect.h"
#include "Dice.h"
#include "DiceVisual.h"
#include "system/imgui/imgui.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "ExploreScene.h"
#include <regex>
#include <cstring> // strncpy 用
#include <vector>
#include <sstream>

// 各能力ごとの直近ロール（個々のダイスの出目）を保持する（ファイルスコープ）
static std::vector<std::vector<int>> abilityFaces;

/*
 * CharcterScene::Initialize
 * -------------------------
 * キャラクター選択シーンの初期化処理。
 * - サンプルキャラクターを用意します。
 * - SceneManager に既にロスターがあればそれを編集対象にし、なければサンプルを1体追加します。
 * - 能力行 (abilities) と出目キャッシュ (abilityFaces) の初期化を行います。
 */
void CharcterScene::Initialize()
{
    sampleCharacters.clear();

    CharcterDate a;
    a.name = "Researcher Sample";
    a.job = "Investigator";
    a.str =6; a.con =10; a.dex =8; a.int_ =14; a.pow =12; a.cha =9; a.app =10; a.siz =9; a.edu =12;
    a.sanity =80; a.maxSanity =100;
    a.skills = { "Library Use", "Psychology", "Dodge" };
    sampleCharacters.push_back(a);

    CharcterDate b;
    b.name = "Officer Sample";
    b.job = "Police Officer";
    b.str =12; b.con =11; b.dex =12; b.int_ =10; b.pow =10; b.cha =9; b.app =8; b.siz =11; b.edu =9;
    b.sanity =90; b.maxSanity =100;
    b.skills = { "Firearms", "Negotiation", "Tracking" };
    sampleCharacters.push_back(b);

    CharcterDate c;
    c.name = "Student Sample";
    c.job = "Student";
    c.str =7; c.con =8; c.dex =9; c.int_ =12; c.pow =8; c.cha =11; c.app =10; c.siz =8; c.edu =14;
    c.sanity =95; c.maxSanity =100;
    c.skills = { "Persuade", "Translation", "Stealth" };
    sampleCharacters.push_back(c);

    // 初期選択を設定
    selectedSampleIndex =0;
    // If SceneManager already has players, use active player; otherwise create one from sample
    auto &roster = g_SceneManager.GetPlayers();
    if (!roster.empty()) {
        int idx = g_SceneManager.GetActivePlayerIndex();
        if (idx < 0 || idx >= (int)roster.size()) idx = 0;
        charcter = roster[idx];
        // ensure SceneManager's active index is synced
        g_SceneManager.SetActivePlayerIndex(idx);
    } else {
        charcter = sampleCharacters[0];
        // add initial sample to roster
        int newIdx = g_SceneManager.AddPlayer(charcter);
        g_SceneManager.SetActivePlayerIndex(newIdx);
    }

    lastDiceRoll = 0;

    // 能力値行の初期化
    abilities.clear();
    // 表形式で初期データを定義してから一括で abilities に格納する
    struct AbilityInit { const char* name; const char* expr; int value; bool locked; };
    AbilityInit initList[] = {
        {"STR", "3D6",  charcter.str,  false},
        {"CON", "3D6",  charcter.con,  false},
        {"POW", "3D6",  charcter.pow,  false},
        {"DEX", "3D6",  charcter.dex,  false},
        {"APP", "3D6",  charcter.app,  false},
        {"SIZ", "2D6+6",charcter.siz,  false},
        {"INT", "2D6+6",charcter.int_, false},
        {"EDU", "3D6+3",charcter.edu,  false}
    };
    for (const auto &it : initList) {
        abilities.push_back({ std::string(it.name), std::string(it.expr), it.value, it.locked });
    }

    // abilityFaces を能力数に合わせて初期化
    abilityFaces.clear();
    abilityFaces.resize(abilities.size());
}

// abilities <-> character の同期ヘルパ（UI から呼ばれる）
void CharcterScene::RefreshAbilitiesFromCharacter()
{
    for (auto &a : abilities) {
        if (a.name == "STR") a.value = charcter.str;
        else if (a.name == "CON") a.value = charcter.con;
        else if (a.name == "POW") a.value = charcter.pow;
        else if (a.name == "DEX") a.value = charcter.dex;
        else if (a.name == "APP") a.value = charcter.app;
        else if (a.name == "SIZ") a.value = charcter.siz;
        else if (a.name == "INT") a.value = charcter.int_;
        else if (a.name == "EDU") a.value = charcter.edu;
    }
}

// UI から確定操作を行う際に abilities の値を character に適用する
void CharcterScene::ApplyAbilitiesToCharacter()
{
    for (auto &a : abilities) {
        if (a.name == "STR") charcter.str = a.value;
        else if (a.name == "CON") charcter.con = a.value;
        else if (a.name == "POW") charcter.pow = a.value;
        else if (a.name == "DEX") charcter.dex = a.value;
        else if (a.name == "APP") charcter.app = a.value;
        else if (a.name == "SIZ") charcter.siz = a.value;
        else if (a.name == "INT") charcter.int_ = a.value;
        else if (a.name == "EDU") charcter.edu = a.value;
    }
}

/*
 * EvalDiceExpr
 * -------------
 * 簡易的なダイス式パーサ: "nDm" または "nDm+k" の形式を受け取り合計値を返します。
 * - ログ出力や個別の出目詳細は RollDiceDetailed を使用してください。
 */
static int EvalDiceExpr(const std::string& expr)
{
    // 簡易的なパーサ: nDm [+ add]
    std::regex re(R"((\d+)D(\d+)(?:\s*\+\s*(\d+))?)", std::regex::icase);
    std::smatch m;
    if (!std::regex_match(expr, m, re)) return 0;
    int n = std::stoi(m[1].str());
    int sides = std::stoi(m[2].str());
    int add =0;
    if (m.size() >=4 && m[3].matched) add = std::stoi(m[3].str());
    int total =0;
    for (int i=0;i<n;++i) total += Dice::RollDie(sides);
    total += add;
    return total;
}

// 出目の詳細を返すヘルパ: 個々のダイスの出目を配列で返し、合計を outTotal に設定する
// If outSides != nullptr, it will be set to the parsed sides value.
/*
 * RollDiceDetailed
 * -----------------
 * ダイス式を解析して個々のダイス出目を生成し、出目配列を返します。
 * - outTotal に合計値を設定します。
 * - outSides が nullptr でなければサイコロの面数を格納します。
 * - ビジュアル用の DiceVisual と組み合わせて使います。
 */
static std::vector<int> RollDiceDetailed(const std::string& expr, int& outTotal, int* outSides = nullptr)
{
    std::vector<int> faces;
    outTotal = 0;
    std::regex re(R"((\d+)D(\d+)(?:\s*\+\s*(\d+))?)", std::regex::icase);
    std::smatch m;
    if (!std::regex_match(expr, m, re)) return faces;
    int n = std::stoi(m[1].str());
    int sides = std::stoi(m[2].str());
    int add = 0;
    if (m.size() >= 4 && m[3].matched) add = std::stoi(m[3].str());
    for (int i = 0; i < n; ++i) {
        int r = Dice::RollDieNoVisual(sides);
        faces.push_back(r);
        outTotal += r;
    }
    if (outSides) *outSides = sides;
    outTotal += add;
    return faces;
}

/*
 * CharcterScene::Update
 * ----------------------
 * 毎フレーム更新処理。
 * - DiceVisual の更新など軽微な状態更新を行います。
 */
void CharcterScene::Update()
{
    // Update Dice visual (use ImGui delta time when available)
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGuiIO& io = ImGui::GetIO();
        float dt = io.DeltaTime;
        // Fallback small dt if zero
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        DiceVisual::Instance().Update(dt);
    }
}

/*
 * CharcterScene::SetPortraitPath
 * -------------------------------
 * 開発モード時のみ呼び出し可能。指定されたパスを当該キャラクターの portraitPath に設定し、
 * テクスチャを予めロードしてキャッシュをウォームアップします。
 */
void CharcterScene::SetPortraitPath(const std::string& path)
{
    // Only allow in dev mode
    if (!g_SceneManager.IsDevMode()) return;
    if (path.empty()) return;
    charcter.portraitPath = path;
    // Warm the texture cache
    TextureManager::LoadTexture(charcter.portraitPath);
    // Also update SceneManager's copy
        g_SceneManager.SetPlayer(charcter);
        // persist player roster/state immediately
        ExploreScene::SaveStateNow();
}

/*
 * CharcterScene::Render
 * ----------------------
 * キャラクター作成/編集 UI を描画します。
 * - パーティロスター、能力ロール、派生値、Apply ボタン等を表示します。
 * - Apply 操作で SceneManager にキャラ情報を反映し、Explore の Save を呼び出して永続化します。
 */
void CharcterScene::Render()
{
    // 安全: ImGui が初期化されていなければ何もしない
    if (ImGui::GetCurrentContext() == nullptr) {
        if (g_SceneManager.IsDevMode()) OutputDebugStringA("CharacterSelect::Render skipped - ImGui context not initialized\n");
        return;
    }
    ImGui::Begin("Character Sheet");
    // Refresh ability UI only when the selected character changes to avoid
    // overwriting recent rolls/edits every frame.
    static int s_lastRefreshedIndex = -999;
    if (selectedSampleIndex != s_lastRefreshedIndex) {
        RefreshAbilitiesFromCharacter();
        s_lastRefreshedIndex = selectedSampleIndex;
    }
    // Roster panel: list existing characters, allow new/remove/select
    // Roster displayed horizontally with scrollbar
    ImGui::BeginChild("Roster", ImVec2(0, 80), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::Text("Party Roster");
    ImGui::Separator();
    auto &roster = g_SceneManager.GetPlayers();
    int active = g_SceneManager.GetActivePlayerIndex();
    // render each character as an inline button with a small Act button next to it
    for (int i = 0; i < (int)roster.size(); ++i) {
        bool isActive = (i == active);
        ImGui::PushID(i);
        // make selected (editing) characters visually distinct by button color
        ImVec4 btnCol = selectedSampleIndex == i ? ImVec4(0.26f, 0.59f, 0.98f, 1.0f) : ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
        if (ImGui::Button(roster[i].name.c_str())) {
            selectedSampleIndex = i;
            charcter = roster[i];
            g_SceneManager.SetActivePlayerIndex(i);
            RefreshAbilitiesFromCharacter();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (isActive) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
        if (ImGui::SmallButton("Act")) {
            g_SceneManager.SetActivePlayerIndex(i);
        }
        if (isActive) ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PopID();
    }
    ImGui::NewLine();
    // controls for roster management
    if (ImGui::Button("New Empty")) {
        CharcterDate nd;
        nd.name = "New Character";
        int idx = g_SceneManager.AddPlayer(nd);
        g_SceneManager.SetActivePlayerIndex(idx);
        selectedSampleIndex = idx;
        charcter = nd;
        RefreshAbilitiesFromCharacter();
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete") && selectedSampleIndex >= 0 && selectedSampleIndex < (int)roster.size()) {
        int delIdx = selectedSampleIndex;
        g_SceneManager.RemovePlayer(delIdx);
        // adjust selection
        if (!roster.empty()) {
            int newIdx = std::min(delIdx, (int)roster.size() - 1);
            selectedSampleIndex = newIdx;
            charcter = roster[newIdx];
            g_SceneManager.SetActivePlayerIndex(newIdx);
            RefreshAbilitiesFromCharacter();
        } else {
            selectedSampleIndex = -1;
            charcter = sampleCharacters.empty() ? CharcterDate() : sampleCharacters[0];
            RefreshAbilitiesFromCharacter();
        }
    }
    ImGui::EndChild();


    // Accept drag and drop files onto the main window (Dev mode only)
    if (g_SceneManager.IsDevMode()) {
        // ImGui built-in drag and drop accepts payloads from ImGui widgets, not OS.
        // We provide a small helper UI to show a drop target and, when activated, ask the
        // platform layer (Application WndProc) to perform the file drop via WM_DROPFILES.
        ImGui::Text("(Dev) You can drag image files from Explorer onto the application window to set Portrait.");
    }

    // サンプルの簡易選択コンボ
    if (!sampleCharacters.empty()) {
        const char* names[16];
        int total = (int)sampleCharacters.size();
        int count = std::min(total, 16);
        for (int i = 0; i < count; ++i) names[i] = sampleCharacters[i].name.c_str();
        if (ImGui::Combo("Quick Samples", &selectedSampleIndex, names, count)) {
            if (selectedSampleIndex >= 0 && selectedSampleIndex < (int)sampleCharacters.size()) {
                charcter = sampleCharacters[selectedSampleIndex];
            }
        }
    }

    // Helper functions are implemented as member methods

    ImGui::Separator();

    // ----- 名前と職業を編集可能にする -----
    {
        // バッファサイズは必要に応じて拡張してください
        char nameBuf[128];
        char jobBuf[128];
        std::memset(nameBuf, 0, sizeof(nameBuf));
        std::memset(jobBuf, 0, sizeof(jobBuf));

#if defined(_MSC_VER)
        // MSVC 環境: strncpy_s を使って安全にコピー（終端は自動保証）
        strncpy_s(nameBuf, sizeof(nameBuf), charcter.name.c_str(), _TRUNCATE);
        strncpy_s(jobBuf, sizeof(jobBuf), charcter.job.c_str(), _TRUNCATE);
#else
        // 非 MSVC: strncpy を使いつつ明示的に終端を保証する
        std::strncpy(nameBuf, charcter.name.c_str(), sizeof(nameBuf) - 1);
        nameBuf[sizeof(nameBuf) - 1] = '\0';
        std::strncpy(jobBuf, charcter.job.c_str(), sizeof(jobBuf) - 1);
        jobBuf[sizeof(jobBuf) - 1] = '\0';
#endif

        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            charcter.name = std::string(nameBuf);
        }
        if (ImGui::InputText("Job", jobBuf, sizeof(jobBuf))) {
            charcter.job = std::string(jobBuf);
        }
    }
    ImGui::Separator();

    // レイアウト: 左列能力、右列派生値
    ImGui::Columns(2, nullptr, true);

    // 左: 能力テーブル
    ImGui::BeginChild("Abilities", ImVec2(0, 0), false);
    ImGui::Text("能力");
    ImGui::Separator();

    if (ImGui::Button("Roll All")) {
        // abilityFaces が不足していたら揃える
        if (abilityFaces.size() < abilities.size()) abilityFaces.resize(abilities.size());

        int lastTotal = 0;
        for (size_t i = 0; i < abilities.size(); ++i) {
            auto& a = abilities[i];
            if (!a.locked) {
                int total = 0;
                // 個々の出目を取得して faces を保存（画像表示と数値表示のため）
                auto faces = RollDiceDetailed(a.expr, total);
                a.value = total;
                if (i < abilityFaces.size()) abilityFaces[i] = faces;
                lastTotal = total;
            }
        }
        if (lastTotal != 0) lastDiceRoll = lastTotal;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        for (auto& a : abilities) { a.locked = false; a.value = 0; }
        // Reset faces too
        for (auto& f : abilityFaces) f.clear();
    }

    ImGui::Separator();

    // レイアウト改良: ボタン幅を固定し絶対位置で配置して重なりを防ぐ
    for (int i = 0; i < (int)abilities.size(); ++i) {
        auto& a = abilities[i];
        ImGui::PushID(i);

        // 名前列
        ImGui::Text("%s", a.name.c_str());
        ImGui::SameLine(90); // 名前欄の幅（必要に応じて調整）

        // ダイス式ボタン（固定幅）
        if (ImGui::Button(a.expr.c_str(), ImVec2(64, 0))) {
            int total = 0;
            int sides = 6;
            auto faces = RollDiceDetailed(a.expr, total, &sides);
            a.value = total;
            lastDiceRoll = total;
            if (i >= 0 && i < (int)abilityFaces.size()) abilityFaces[i] = faces;
            if (!faces.empty()) DiceVisual::Instance().StartRollFaces(sides, faces);
        }

        // ロックチェックボックス（位置固定）
        ImGui::SameLine(170);
        ImGui::Checkbox("Lock", &a.locked);

        // 出目表示領域
        ImGui::SameLine(240);
        if (i >= 0 && i < (int)abilityFaces.size() && !abilityFaces[i].empty()) {
            // 出目文字列を作成（例: "1+4+3+6"、加算分があれば末尾に +6 など）
            int sumFaces = 0;
            std::string facesStr;
            {
                std::ostringstream oss;
                for (size_t fi = 0; fi < abilityFaces[i].size(); ++fi) {
                    if (fi) oss << "+";
                    oss << abilityFaces[i][fi];
                    sumFaces += abilityFaces[i][fi];
                }
                int add = a.value - sumFaces;
                if (add > 0) oss << "+" << add;
                facesStr = oss.str();
            }

            ImTextureID sheet = TextureManager::GetImGuiTextureID("texture/dice.jpg");
            if (!sheet) {
                OutputDebugStringA("CharacterSelect: TextureManager returned NULL for texture/dice.jpg\n");
                // フォールバック：数値を並べて表示
                for (size_t fi = 0; fi < abilityFaces[i].size(); ++fi) {
                    ImGui::Text("%d", abilityFaces[i][fi]);
                    ImGui::SameLine();
                }
                ImGui::NewLine();
                ImGui::SameLine(240);
                ImGui::Text("%s", facesStr.c_str());
            }
            else {
                const float cols = 3.0f;
                const float rows = 2.0f;
                // 画像を横並びに表示
                for (size_t fi = 0; fi < abilityFaces[i].size(); ++fi) {
                    int face = abilityFaces[i][fi];
                    if (face < 1 || face > 6) continue;
                    int idx = face - 1;
                    int col = idx % 3;
                    int row = idx / 3;
                    ImVec2 uv0(col / cols, row / rows);
                    ImVec2 uv1((col + 1) / cols, (row + 1) / rows);
                    ImGui::Image(sheet, ImVec2(20, 20), uv0, uv1);
                    ImGui::SameLine();
                }
                ImGui::SameLine();
                ImGui::Text("%s", facesStr.c_str());
            }
        }
        else {
            // 未ロール時は出目欄を空けておく（位置合わせ）
            ImGui::SameLine(240);
            ImGui::Text("-");
        }

        // 合計値は常に右端に表示（位置固定）
        ImGui::SameLine(420);
        ImGui::Text("%d", a.value);

        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::NextColumn();

    //右: 派生値
    ImGui::BeginChild("Derived", ImVec2(0,0), false);
    ImGui::Text("SAN and Derived Values");
    ImGui::Separator();

    // ローカルで現在の能力値を参照
    auto findVal = [&](const std::string &name)->int {
        for (auto &a : abilities) if (a.name == name) return a.value;
        return 0;
    };

    int POW = findVal("POW");
    int INT = findVal("INT");
    int EDU = findVal("EDU");
    int CON = findVal("CON");
    int SIZ = findVal("SIZ");
    int STR = findVal("STR");

    ImGui::Text("SAN (Sanity): %d", POW*5);//正気度
    ImGui::Text("Luck: %d", POW*5);//幸運
    ImGui::Text("Idea: %d", INT*5);//アイデア
    ImGui::Text("Occupational Skill Points: %d", EDU*20);
    ImGui::Text("Hobby Skill Points: %d", EDU*10);

    // Apply derived values to the character and notify SceneManager
    if (ImGui::Button("Apply to Character")) {
        ApplyAbilitiesToCharacter();

        // simple derived updates
        charcter.sanity = static_cast<int>(trpg::clamp(charcter.pow * 5, 0, 9999));
        charcter.maxSanity = 100;
        charcter.endurance = (charcter.con + charcter.siz) / 2;
        charcter.maxEndurance = charcter.endurance;

        g_SceneManager.SetPlayer(charcter);
    }

    ImGui::EndChild();

    // restore single column layout
    ImGui::Columns(1);

    ImGui::Separator();
    ImGui::Text("Last Dice Roll: %d", lastDiceRoll);
    ImGui::Text("Endurance: %d", (CON+SIZ)/2);//耐久力
    ImGui::Text("Magic Points: %d", POW*1);//マジックポイント
    ImGui::Text("Occupational Skill Points: %d", EDU*20);
    ImGui::Text("Hobby Skill Points: %d", INT*10);
    ImGui::Text("Damage Bonus: %d", STR + SIZ);

    // Portrait selection: only show the path input in Dev mode
    if (g_SceneManager.IsDevMode()) {
        ImGui::Separator();
        ImGui::Text("Portrait:");
        char bufPath[512] = {};
        if (!charcter.portraitPath.empty()) strncpy_s(bufPath, sizeof(bufPath), charcter.portraitPath.c_str(), _TRUNCATE);
        if (ImGui::InputText("Portrait Path", bufPath, sizeof(bufPath))) {
            charcter.portraitPath = std::string(bufPath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Portrait")) {
            // Try to load via TextureManager to warm cache; store path regardless
            g_SceneManager.SetPlayer(charcter); // ensure SceneManager has latest
            ExploreScene::SaveStateNow();
            TextureManager::LoadTexture(charcter.portraitPath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Portrait")) {
            charcter.portraitPath.clear();
            g_SceneManager.SetPlayer(charcter);
            ExploreScene::SaveStateNow();
        }
    }

    // end of derived column area

    ImGui::Separator();

    // 現在のHP表示（耐久力）
    float hpRatio = (charcter.maxEndurance>0) ? float(charcter.endurance) / float(charcter.maxEndurance) :0.0f;
    ImGui::ProgressBar(hpRatio, ImVec2(-1,0), "HP");
    ImGui::Text("HP: %d / %d", charcter.endurance, charcter.maxEndurance);

    ImGui::Separator();

    // 簡易判定UI
    if (ImGui::Button("Roll d100")) {
        lastDiceRoll = Dice::RollDie(100);
    }
    ImGui::SameLine();
    if (ImGui::Button("POW Check (d100)")) {
        lastDiceRoll = Dice::RollDie(100);
        bool success = (lastDiceRoll <= charcter.pow *5);
        if (!success) {
            // サンプルでは耐久力を減らす（ダメージ確認用）
            charcter.ApplyEnduranceLoss(1);
        }
    }
    ImGui::Text("Last roll: %d", lastDiceRoll);

    ImGui::Separator();

    // シーン開始ボタン
    if (ImGui::Button("Start Play (Quick)")) {
        // HPが未設定なら再計算
        charcter.maxEndurance = (charcter.con + charcter.siz) / 2;
        charcter.endurance = charcter.maxEndurance;

        g_SceneManager.SetPlayer(charcter);
        if (RequestSceneChange)
            RequestSceneChange(1);
        else
            g_SceneManager.ChangeScene(SceneType::GAME_PLAY);
    }

    ImGui::SameLine();
    if (ImGui::Button("Open Character Editor")) {
        if (RequestSceneChange) RequestSceneChange(2);
    }

    ImGui::End();

    // Render dice visual overlay (foreground)
    if (ImGui::GetCurrentContext() != nullptr) DiceVisual::Instance().Render();
}