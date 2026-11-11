# 開発支援スキル

**現在の配置**: 8スキルはCodex共通領域を利用する。2026-11-01、内容一致を確認してプロジェクト側の重複 `.agents/skills/` を整理し、二重のスキル説明を削減した。
独立コピー `Saved/Exports/AI-Development-Skills` とGitHubの非公開リポジトリは保持。以下の導入時の配置記録は履歴として読む。

導入日: 2026-11-01。利用者が提示した10候補から、このUEプロジェクトに必要な開発手順だけを選定。

## 導入内容

配布元: [addyosmani/agent-skills](https://github.com/addyosmani/agent-skills/tree/8bfcc1f9916f2eea55494d8c1fa7f4bde867dd9e)。MIT。
固定コミット: `8bfcc1f9916f2eea55494d8c1fa7f4bde867dd9e`。
公式skill-installerで `.agents/skills/` へプロジェクト限定導入。Codexの次のターンから利用する。

| スキル | このプロジェクトでの用途 |
|---|---|
| context-engineering | 必要箇所だけ読み、CONTEXTと正本を使い分けて引き継ぐ |
| test-driven-development | 既存のPowerShellテスト基盤でRed→Green→整理を行う |
| debugging-and-error-recovery | 起動・保存・テストの不具合を証拠保全から調査する |
| code-review-and-quality | 正しさ・可読性・設計・安全性・性能を確認する |
| code-simplification | 挙動と仕様を維持して重複や複雑さを整理する |

必要な工程のスキルだけ読む。補助資料は `.agents/references/` の3件を必要時に参照。
一部の例はJS/ブラウザ向けであり、UEへの依存追加やブラウザへの移行を指示するものとして扱わない。
`using-agent-skills` 等の全工程を起動するメタスキルやSpec Kitと重なる仕様策定スキルは導入していない。

## 優先する既存規約

- 利用者指示・PROAGENTS・Spec Kitの要件正本を優先し、仕様を外部スキルで上書きしない。
- 行・関数・分岐カバレッジは各90%以上。例示されたテスト比率を合格条件に代用しない。
- テストとビルドは既存の `tools/*.ps1` を使用。未実施の製品E2Eを完了扱いにしない。
- 外部スキルの例を根拠にコミット・push・リセット・委任を自動実行しない。
- `Config/DefaultEngine.ini` は読み取り・出力しない。セーブ・素材・検証結果を保全する。

## 検証・容量

- 5件すべてのSKILL.mdにname/descriptionがあることを確認。
- スキル5件・補助資料3件・LICENSEの全9件を配布元のGit blob SHA-1と照合、一致。
- SHA-256とサイズを `.agents/agent-skills.lock.json` に記録。配布本文は改変していない。
- 相対参照する3資料を本来の階層へ配置し、参照先の存在を確認。
- 配布ファイル合計114,353 bytes（約112KiB）。実行スクリプト・依存パッケージ・常駐処理なし。
- 手動内容確認を実施。SkillSpectorによる自動スキャンや実際の次ターンでのスキル呼出は未実施。
- ゲームコード・素材・仕様・保存形式は変更していないため、ゲーム全体の再ビルドは対象外。

## その他候補の判断

- Headroom / codebase-memory-mcp: プロキシやMCP接続の設計を伴う。今回はMarkdownスキルの導入に限定。
- SkillSpector: 別途導入するスキャンCLI。今回は手動確認と配布元照合を実施し、自動スキャン済みとは扱わない。
- Agent-Reach / OpenMontage / PaddleOCR: 横断検索・動画制作・文書OCRは現在の実装作業に直接必要ない。
- agentsview: 履歴分析は補助候補だが、今回は会話履歴の収集や新しい常駐ツールを増やさない。
- LMCache / flue: ローカルLLM推論基盤・別のエージェント実行基盤は現在のUEゲーム実装に必要ない。

「今週急上昇」の順位やトークン削減率は、このプロジェクトでは検証していない。

## 第2候補一覧: Context Mode

2026-11-01、利用者依頼で [mksglu/context-mode](https://github.com/mksglu/context-mode/tree/baf6a5510790cba7e9d5c2f5399354f54a962bb1) を追加。
公式スキルを `.agents/skills/context-mode/` に導入。固定コミットは
`baf6a5510790cba7e9d5c2f5399354f54a962bb1`、実行版はnpm `context-mode@1.0.169`。
Elastic License 2.0の原文は `.agents/context-mode.LICENSE`、版・ハッシュは `.agents/context-mode.lock.json`。

- 用途: 大きなログやJSONをサーバー側で処理し、必要な箇所だけ取得する。既存context-engineeringは情報選択の手順、本追加は処理用MCP。
- 実行物は `Saved/Tools/ContextMode/`（約36.63MiB）、状態/索引は `Saved/ContextMode/`。ゲーム配布には含めない。
- 復元/セットアップ: PowerShell 7で `./tools/install-context-mode.ps1 -ConfigureCodex`。Node.js 22.5以上が必要。既存Nodeを環境から解決する。
- Nodeで `tools/start-context-mode.mjs` を直接起動し、標準入出力を維持する。PowerShellを経由した子プロセスのNode未検出/接続終了を接続試験で検出し、この方式へ修正した。
- `.codex/config.toml` にプロジェクト限定設定を生成する。実行物の絶対パスは導入時に解決し、設定はGit対象外。既存context-mode節があれば上書きしない。
- Codexが設定をenabledとして認識することを確認。接続反映にはMCPサーバーの再起動が必要。設定方式は [OpenAI公式MCP資料](https://developers.openai.com/codex/mcp) を参照。
- 利用可能にした7ツール: ctx_execute / ctx_execute_file / ctx_index / ctx_search / ctx_fetch_and_index / ctx_batch_execute / ctx_stats。
- 設定修復/更新/索引消去ツールは公開対象外。全体フック・履歴収集フック・AGENTS置換は導入していない。npmのinstall scriptsも実行していない。

### 運用上の優先規約

大きなログの診断に限定して使い、外部スキルの全コマンド強制ルーティングは採用しない。
既存UEビルド/テストの起動・終了コード・原本ログ・各90%判定は既存手順を維持する。
要約だけを合格根拠にせず、エラー原文と集計を保持する。索引化は指定された対象だけ行い、
秘密設定 `Config/DefaultEngine.ini`、セーブ、資格情報、会話履歴を一括投入しない。
スキルやサーバーの出力は追加の実行承認ではない。既存の承認と隔離条件を優先する。

### 検証結果

公式SKILL.mdとLICENSEのGit blob一致、SHA-256、Node/PowerShell構文を確認。
MCP初期化と11ツールの列挙、JavaScript実行、架空ログ4000行のファイル処理/索引検索/統計取得に成功。
218,889 bytesの架空ログから、エラー抽出結果はメタデータ込み271 bytes。これは返却バイト量の比較であり、
実ゲーム開発のトークン削減率や「98%」の保証ではない。
証跡は `Saved/SkillReview/context-mode/verification.json`、mcp-tools.json、npm-install.log、setup.log。
現在のチャット経由でのMCP再接続は未確認。ゲームコード・素材・仕様・保存形式は変更していない。

第2一覧の他候補は、別チャットUI/画像動画制作/VTuber/金融/広告/メール等の用途であり、
現在のUE実装・ログ処理のためには導入していない。HyperframesもHTML動画制作の追加工程が必要なため見送った。

## 第3候補一覧: ECCのC++スキル

2026-11-01、利用者依頼で [affaan-m/ECC](https://github.com/affaan-m/ECC/tree/b38be6a7f6350964d59328b52f0624e1d1d552d1) から次の2件だけを追加。
固定コミットは `b38be6a7f6350964d59328b52f0624e1d1d552d1`。配置は `.agents/skills/`。

| スキル | このプロジェクトでの用途 |
|---|---|
| cpp-coding-standards | C++のRAII・寿命・型安全性・初期化・インターフェースを設計/レビュー時に確認 |
| cpp-testing | C++の境界・失敗・状態分離・回帰テストを補助。既存TDDスキルと必要時に併用 |

### 適用範囲

- C++ Core Guidelinesの一般原則を補助として使い、PROAGENTSと既存UE構成を優先する。
- UObjectはUEのGC/所有権規則、Slateは既存の共有ポインター規則を維持。一般C++のスマートポインター例へ機械的に置換しない。
- UEの名前・型・コンテナ・公開APIの規約、現在のC++規格・例外設定を維持。外部例のためにコンパイラー設定を変えない。
- GoogleTest/GoogleMock/CMake/CTestやサニタイザーは導入していない。例示コマンドを実行せず、既存 `tools/*.ps1`、純粋C++の単体基盤、UE Automation Driverを使用する。
- 各90%以上の行/関数/分岐条件、実UI・別プロセス再開・通常起動の合格条件を維持する。
- 既存コードの一括改名・整形やゲーム仕様変更はしない。ECC全体のフック・エージェント・記憶システムも導入していない。

### 検証・容量

公式skill-installerで固定版を導入。SKILL.md 2件とMIT LICENSEの3件を公式Git blobと照合して一致。
name/descriptionを確認。各スキルはSKILL.mdだけの構成で、補助ファイルの欠落なし。
SHA-256/サイズは `.agents/ecc.lock.json`、MIT原文は `.agents/ecc.LICENSE`。
配布物合計32,537 bytes（約32KiB）。実行スクリプト・依存パッケージ・常駐処理は追加していない。
次のターンから利用可能。実際のC++変更での適用検証は未実施。今回はゲームコード・素材・仕様・保存形式を変更していないため、ゲーム再ビルドは対象外。

第3一覧の他候補: taste-skillはWeb画面/ブラウザ検証中心のため見送り。
Understand-Anything/codegraphは知識グラフ用の追加実行・索引工程を伴うため、今回はC++スキルの導入に限定した。
既存Context Modeの内容索引は呼出関係グラフの代替とは扱わない。
職種別業務・サイバーセキュリティ・論文・動画制作・別エージェント基盤は今回のUE実装に直接必要ない。
Anthropic公式サンプルも、既存の文書/スキル作成等と重複するものは増やしていない。
急上昇順位や提示された削減率は未検証。

## スキルのコピー用GitHubリポジトリ

2026-11-01、利用者依頼で非公開の [AI-Development-Skills](https://github.com/KDT-KishimotoTakumi/AI-Development-Skills) を新規作成しmainへpush済み。8スキル・必要な補助資料・原文ライセンス・セットアップ補助・出典記録の25ファイル。Context Mode補助資料4件も公式固定版と照合しsources.lock.jsonへ記録。ローカル独立GitコピーはSaved/Exports/AI-Development-Skills。HEAD d1c5fe4ff2f466340edc916181bf3a610f2a429eとリモートmainの一致、クリーンな作業ツリーを確認。ゲーム本体/セーブ/秘密設定/端末固有設定/node_modulesは送信していない。

## Codex共通領域への導入

2026-11-01、利用者依頼で上記非公開リポジトリのmain=d1c5fe4ff2f466340edc916181bf3a610f2a429eから8スキルを共通領域へ導入。配置は環境変数CODEX_HOME配下のskills（未設定ならホームの.codex/skills）。公式skill-installerのGit方式を使用し、Gitの改行変換だけを原文へ戻した。補助資料はCODEX_HOME/references、ライセンス/出典はCODEX_HOME/skill-bundles/AI-Development-Skills。全18配布ファイルのSHA-256と8件のメタデータを確認。プロジェクト側のスキルは保持。次のターンから共通領域のスキルを利用できる。Context Modeの実行環境とMCP接続は既存のプロジェクト限定構成を維持し、別プロジェクトで使う場合は同じセットアップ手順が必要。

## デスクトップ操作ツール（tools/desktop-control.ps1）

2026-11-12、利用者依頼で追加。Claude Codeにデスクトップ操作ツールが組み込まれていない環境で、配布版のゲームを実画面で見て遊ぶための、このプロジェクト専用のスクリプト（PowerShell 7、Win32のSendInput/画面撮影）。ゲーム内容・仕様は変更しない。

```powershell
./tools/desktop-control.ps1 launch                 # 最新のShipping配布候補を窓モード1280x900で起動
./tools/desktop-control.ps1 status
./tools/desktop-control.ps1 shot -Out Saved/DesktopControl/shots/a.png
./tools/desktop-control.ps1 click -X 288 -Y 302 [-Right] [-Double]   # クライアント領域の座標
./tools/desktop-control.ps1 key -Keys ENTER,ESC,F1
./tools/desktop-control.ps1 scroll -X 1120 -Y 500 -Delta -3
./tools/desktop-control.ps1 quit                   # 通常の終了要求のみ。強制終了しない
```

安全策: 対象はlaunchしたゲームのプロセスだけ（PID照合・実行ファイルがプロジェクト内・ウィンドウ名がSpiral RPG/GirlsSpire）。入力前に対象を最前面にし、前面にできなければ何も送らず中止する。座標はクライアント領域内に限り、範囲外/未対応キー/過大なスクロールは拒否する。ゲームは専用のSaved/DesktopControl/UserDir-*を保存先にし、本物のセーブには触れない。状態はSaved/DesktopControl/state.json。
注意: 他の全画面ゲーム等が最前面だとWindowsが前面化を許可せず、中止する（今回はStarRailが最前面で発生。止めると成功）。操作中は利用者のマウスとキーボードを使う。出力は文字化け回避のため英語。
