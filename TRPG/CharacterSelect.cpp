#include "CharacterSelect.h"
#include "Dice.h"
#include "system/imgui/imgui.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include <regex>
#include <cstring> // strncpy 用
#include <vector>

// 各能力ごとの直近ロール（個々のダイスの出目）を保持する（ファイルスコープ）
static std::vector<std::vector<int>> abilityFaces;

// 簡易初期化：サンプルキャラクターを用意する
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
    charcter = sampleCharacters[0];
    lastDiceRoll =0;

    // 能力値行の初期化
    abilities.clear();
    abilities.push_back({"STR","3D6", charcter.str, false});
    abilities.push_back({"CON","3D6", charcter.con, false});
    abilities.push_back({"POW","3D6", charcter.pow, false});
    abilities.push_back({"DEX","3D6", charcter.dex, false});
    abilities.push_back({"APP","3D6", charcter.app, false});
    abilities.push_back({"SIZ","2D6+6", charcter.siz, false});
    abilities.push_back({"INT","2D6+6", charcter.int_, false});
    abilities.push_back({"EDU","3D6+3", charcter.edu, false});

    // abilityFaces を能力数に合わせて初期化
    abilityFaces.clear();
    abilityFaces.resize(abilities.size());
}

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
static std::vector<int> RollDiceDetailed(const std::string& expr, int& outTotal)
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
        int r = Dice::RollDie(sides);
        faces.push_back(r);
        outTotal += r;
    }
    outTotal += add;
    return faces;
}

void CharcterScene::Update()
{
    // 必要ならここに入力処理や状態更新を記述する
}

void CharcterScene::Render()
{
    ImGui::Begin("Character Sheet");

    // サンプルの簡易選択コンボ
    if (!sampleCharacters.empty()) {
        const char* names[16];
        int count = (int)sampleCharacters.size();
        for (int i =0; i < count; ++i) names[i] = sampleCharacters[i].name.c_str();
        if (ImGui::Combo("Quick Samples", &selectedSampleIndex, names, count)) {
            if (selectedSampleIndex >=0 && selectedSampleIndex < (int)sampleCharacters.size()) {
                charcter = sampleCharacters[selectedSampleIndex];
            }
        }
    }

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
    ImGui::BeginChild("Abilities", ImVec2(0,0), false);
    ImGui::Text("能力");
    ImGui::Separator();

    if (ImGui::Button("Roll All")) {
        for (auto &a : abilities) {
            if (!a.locked) a.value = EvalDiceExpr(a.expr);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        for (auto &a : abilities) { a.locked=false; a.value=0; }
        // Reset faces too
        for (auto &f : abilityFaces) f.clear();
    }

    ImGui::Separator();
    for (int i=0;i<(int)abilities.size();++i) {
        auto &a = abilities[i];
        ImGui::PushID(i);
        ImGui::Text("%s", a.name.c_str()); ImGui::SameLine(120);

        // ロールボタン: 個々の出目を取得して abilityFaces に保存
        if (ImGui::Button(a.expr.c_str())) {
            int total = 0;
            auto faces = RollDiceDetailed(a.expr, total);
            a.value = total;
            lastDiceRoll = total;
            if (i >= 0 && i < (int)abilityFaces.size()) abilityFaces[i] = faces;
        }
        ImGui::SameLine();

        if (ImGui::Checkbox("Lock", &a.locked)) { }
        ImGui::SameLine();

        // ダイス出目画像の表示（アトラス: 3 列 x 2 行 想定）
        if (i >= 0 && i < (int)abilityFaces.size() && !abilityFaces[i].empty()) {
            ImTextureID sheet = TextureManager::GetImGuiTextureID("texture/dice.jpg"); // ← 正しい相対パスに変更
if (!sheet) {
    OutputDebugStringA("CharacterSelect: TextureManager returned NULL for texture/dice.jpg\n");
    // フォールバック：数値表示
    for (size_t fi = 0; fi < abilityFaces[i].size(); ++fi) {
        ImGui::Text("%d ", abilityFaces[i][fi]);
        ImGui::SameLine();
    }
    ImGui::NewLine();
    ImGui::SameLine(120);
    ImGui::Text("%d", a.value);
} else {
    const float cols = 3.0f;
    const float rows = 2.0f;
    for (size_t fi = 0; fi < abilityFaces[i].size(); ++fi) {
        int face = abilityFaces[i][fi];
        if (face < 1 || face > 6) continue;
        int idx = face - 1;
        int col = idx % 3;
        int row = idx / 3;
        ImVec2 uv0(col / cols, row / rows);
        ImVec2 uv1((col + 1) / cols, (row + 1) / rows);
        ImGui::Image(sheet, ImVec2(28,28), uv0, uv1);
        ImGui::SameLine();
    }
    ImGui::NewLine();
    ImGui::SameLine(120); // 必要なら調整
    ImGui::Text("%d", a.value);
        } else {
            ImGui::Text("%d", a.value);
        }

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
    ImGui::Text("Knowledge: %d", EDU*5);//知識
    ImGui::Text("Endurance: %d", (CON+SIZ)/2);//耐久力
    ImGui::Text("Magic Points: %d", POW*1);//マジックポイント
    ImGui::Text("Occupational Skill Points: %d", EDU*20);
    ImGui::Text("Hobby Skill Points: %d", INT*10);
    ImGui::Text("Damage Bonus: %d", STR + SIZ);

    ImGui::Separator();
    if (ImGui::Button("Apply to Character")) {
        //反映ボタン: abilities の値を charcter にコピー
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
        // 計算した耐久力を設定（例: (CON + SIZ) /2 を最大耐久力とする）
        charcter.maxEndurance = (charcter.con + charcter.siz) /2;
        charcter.endurance = charcter.maxEndurance;

        g_SceneManager.SetPlayer(charcter);

        // ★ここにデバッグ表示を追加
        printf("[DEBUG] HP applied: %d / %d (CON=%d, SIZ=%d)\n",
            charcter.endurance, charcter.maxEndurance, charcter.con, charcter.siz);
    }

    ImGui::EndChild();
    ImGui::Columns(1);

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
}