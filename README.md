# Girls Spire

更新: 2026-11-07。Windows/UE5.7.4で開発中の戦国怪異・グリッドターン制ローグライクRPGです。ゲーム全体は未完成です。

## 現在の状態

- 全12日、1～4日目の救出、主人公＋仲間4人、6/12日目のボス、経路・交流・商店・成長を接続。
- 固定人物9人、正式技能/個人必殺/全員必殺、通常20＋専用8連携、仲間作戦・信頼・性格、難易度別敵AIを接続。
- 独立2Dスプライトの全画面戦場で、駒クリックから行動を選び予告→確定。仲間には作戦を指示。
- 保存/中断/再開/確認付き復旧、恒久絆・難易度解放・連携/敵図鑑・交流本文・後日談の再読を接続。
- 9人の衣装配色差分と、タイトルの「衣装」からの予告・取消・人物別保存を接続。新しい戦闘/会話から反映し、保存中の戦闘の外見を維持します。関連UI・保存互換・12日通しを検証済みです。
- 正式素材差分/演出/音調整/全設定の実寸回帰、全武器・難易度・ストーリーの総合E2E、配布/オフライン/性能/45～90分実測などが残っています。詳細は[開発予定](docs/PLAN.md)。

## 起動

PowerShell 7でプロジェクトルートから実行します。

```powershell
./tools/play.ps1
```

通常構成へビルドし、画面サイズを自動解決してフルスクリーンで起動します。終了はタイトルの「ゲームを終了する」。戦闘中は「保存して中断する」でタイトルへ戻ってから終了します。保存先はUEのProjectSavedDir配下のSaveGamesです。

導入済みUE5.7とVisual Studio 2022のC++ビルドツールを環境から検出します。端末固有の絶対パスは指定しません。配布物と配布手順は未確定です。

周回の開発用プレビュー:

```powershell
./tools/build-unreal.ps1
./tools/preview.ps1
```

プレビューには戦闘結果を省略する開発用入力があります。通常ゲームの操作とは区別します。

## 検証

```powershell
./tools/test-core.ps1
./tools/build-unreal.ps1
./tools/test-unreal.ps1 -UI -Filter 'GirlsSpire'
./tools/test-resume.ps1
./tools/test-campaign.ps1 -Matrix
./tools/test-runtime.ps1 -Audio
```

- test-core: Nativeのルール/生成/保存/固定原本/12日統合。
- test-unreal: Slateの実マウス操作。指定したフィルターの範囲を検証。
- test-resume: 別UEプロセスの60地点再開、世代復旧、独立設定。
- test-campaign: 初級/軍配seed1の12日実戦をマウスで完走。メモリ/実ディスク/外見選択データ入り実ディスクの3件。初期Run/履歴ProfileだけAPI準備し、別プロセスの12日通しとは区別します。
- test-runtime -Audio: 通常ゲームの実戦・独立音量・別プロセス音声再開。実フルスクリーン検証は -Fullscreen を追加します。

全自作C++の行/関数/分岐を各90%以上で判定します。

```powershell
# 初回の計測ツール準備
./tools/setup-coverage.ps1
./tools/test-coverage.ps1
# 計測後、通常構成へ戻す
./tools/build-unreal.ps1
```

最新の結果・実行範囲・未検証事項は[検証記録](docs/VALIDATION.md)と[E2E対応表](docs/E2E_MATRIX.md)を参照してください。現在の保存形式はRun v19（旧v2～18互換）/Profile v7（旧v1～6互換）です。

## 資料と管理

[作業規約](PROAGENTS.md) / [引継ぎ](docs/CONTEXT.md) / [ゲーム仕様](docs/GAME.md) / [要件正本](specs/001-girls-spire/spec.md) / [タスク](specs/001-girls-spire/tasks.md) / [実装実績](docs/IMPLEMENTED.md) / [素材](docs/ASSETS.md) / [容量整理](docs/STORAGE.md) / [開発スキル](docs/DEV_SKILLS.md)

秘密設定Config/DefaultEngine.ini、個人セーブ、生成物はGit管理しません。原画・固定テスト原本・検証結果を保全し、再生成可能な不要物だけを整理します。
