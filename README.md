# TRPG — C++ / DirectX 11 で自作したシナリオ駆動型TRPG

外部ゲームエンジンを使わず、**C++20 + DirectX 11 + ImGui** だけで作った個人制作のTRPG風ゲームです。
シナリオ・選択肢・ダイス判定・フラグ・インベントリを **JSON で定義**でき、ゲーム内の **ノード型ストーリーエディタ**でシナリオを作成できます。

> 制作: 荒巻 諒人（HAL大阪 ゲーム制作学科） / 個人制作

<!-- TODO: ここにスクリーンショット or GIF を貼る（最重要） -->
<!-- ![gameplay](docs/gameplay.gif) -->

**プレイ動画:** TODO（YouTube 限定公開リンク）
**実行ファイル:** TODO（GitHub Releases のリンク）

## 特徴

| 機能 | 内容 |
|---|---|
| JSONシナリオエンジン | 台詞・演出（shake / fadein / noise など）・選択肢・ダイス判定をJSONで記述。コードを変更せずシナリオだけ差し替え可能 |
| ストーリーエディタ | ImGui 上のノードグラフでシナリオを編集（Dev Mode 時） |
| ダイス判定 | 面数と閾値をJSONで指定。ダイスの回転アニメーション付き（`DiceVisual`） |
| 探索パート | ノード型の探索マップ。フラグ・所持品による選択肢の出し分け |
| 戦闘 | d100 判定のターン制戦闘（`BattleLogic` に分離） |
| 恐怖演出 | 画面揺れ・ノイズなどのオーバーレイ（`FearEffects`） |

## アーキテクチャ

```
Application ──> SceneManager ──> IScene (インターフェース)
                                   ├─ TitleScene / TRPGSelectScene
                                   ├─ ScenarioScene / CharacterSelect
                                   ├─ StoryPlayer ─ (Runtime / Editor / NodeGraph)
                                   ├─ ExploreScene
                                   └─ BattleScene ──> BattleLogic
共通: TextureManager / AssetManager / Logging / ImGuiFontLoader
```

- **IScene** を実装するだけで新しいシーンを追加でき、`SceneManager` 本体の変更は不要です。
- **BattleLogic** は戦闘の計算だけを担当し、描画や進行から切り離しています。
- **EventNode / Choice** がデータ駆動の最小単位で、条件（flags / inventory / roll）と効果（effects）をJSONから読み込みます。

## ディレクトリ構成

```
TRPG/
  *.cpp, *.h          ゲーム本体（シーン、戦闘、演出、エディタ）
  system/             描画・メッシュ・ImGui 等の基盤コード（一部は外部ライブラリ）
  DirectXTex/         DirectXTex ヘッダ（外部ライブラリ）
  shader/             HLSL
  assets/story/       シナリオJSON
  assets/texture/     テクスチャ
```

`system/imgui`・`system/json.hpp`（nlohmann/json）・`DirectXTex`・Assimp は外部ライブラリです。
ゲームロジック・シーン管理・エディタ・演出は自作です。

## ビルド方法

- Visual Studio 2022 以降（プラットフォームツールセット v143）、Windows 10/11
- `TRPG.sln` を開き、`x64` でビルド
- `DirectXTex.lib` が `TRPG/DirectXTex/x64/<Configuration>/` に必要です（リポジトリには含めていません。DirectXTex を別途ビルドして配置してください）

## 操作

TODO（キーボード/マウス操作を記載）

## 使用ライブラリ

[Dear ImGui](https://github.com/ocornut/imgui) / [nlohmann/json](https://github.com/nlohmann/json) / [DirectXTex](https://github.com/microsoft/DirectXTex) / [Assimp](https://github.com/assimp/assimp) / Noto Sans JP (OFL)
