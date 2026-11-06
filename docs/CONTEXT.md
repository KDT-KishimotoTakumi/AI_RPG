# 作業再開メモ

更新: 2026-11-07。要約。PROAGENTS.md・仕様正本・最新利用者指示を優先。

## 【現在の目的】
Girls Spireを承認済み仕様で完成させる。全体は未完成。完成またはトークン限界まで進め、区切りで内容を変えない容量整理とcommit→pull --ff-only→pushを行う。

## 【確定事項・重要な設定】
- 計画/実装は2026-09-20承認済み。要件specs/001-girls-spire/spec.md、概要GAME、実績IMPLEMENTED、予定PLAN/tasks、証跡VALIDATION/E2E_MATRIX。必要部分だけ読む。
- Windows/導入済みUE5.7.4、bUseUnity=false。パスはcwd/環境/Epic manifest/vswhereで解決。通常ゲームを勝手に起動/強制終了しない。
- 戦国怪異/グリッドターン/マウス/2Dスプライト。12日、1日1～2イベント、1～4日各1人救出、主人公＋4人、6/12日ボス、Lv10。黒鉄蓮25歳男性固定＋成人女性8人全員恋愛候補。全滅時に周回性能を失い、絆/発見/図鑑/外見/難易度/後日談を保持。
- 恒久絆0～100を開始時信頼へ固定。救出3択＋10/0/−5。加入/絆80以上で専用交流（交流1回消費、完了恒久）。勝利/当該周回加入/絆80/専用交流で条件達成者全員または単独の後日談。複数恋愛可/嫉妬なし、合意/部分的裸体まで/抑制設定。
- 正式18技能/9必殺/絆100強化、通常20＋専用8連携、性格AI8条件/難易度思考/敵人物技能は接続済み。専用201～208は主人公の承→仲間の結、同敵命中/隣接/絆100、ゲージ20/重複なし。数値は承認済みproposal/rules。
- Run対応上限20。標準外見のみ/戦闘なしは同一バイトのv19を出力し、配色のある戦闘だけv20。旧v2～19は標準外見で読む。Profile v7（旧v1～6互換）。難易度は新規0→初級勝利1→中級勝利2、旧v1～4は2で移行。旧v5以前の敵図鑑は空。
- 勝利はVictoryRecordedを後日談前に記録。旧Epiloguesも根拠にするがHighestDifficultyだけから推測しない。
- 衣装案A/9人の配色/Python・Pillow方式は明示承認済み。標準＋各1差分、原画/顔/髪/体型/衣装形状/模様/肌/露出/武器/金属は固定。主人公初勝利、仲間専用交流で解放。新規戦闘/会話から反映、保存戦闘は開始時9人分を維持。

## 【現在の進捗】
- GS-091/T034h: 初級/軍配/seed1・9人配色の12日実UI/実IOを、5日外/6日内中断で3つのUEプロセスへ分割して完走。独立期待Run/Profile/進捗/確定世代と実保存を比較しUI再開、全12日/両ボス/6戦/89操作/後日談/恒久再読を確認。3段階各1件、従来3件は警告0。上書き/引継ぎ欠落は期待した1失敗で拒否し原本SHA不変。
- 最新証跡はSaved/campaign-process-{guards-green.log,regression.log,final.json}、ProcessCampaign/a7d09d28b0054b68bb7f75d0f5790cfcの段階ログ/期待データ/guard記録。3段階レポート37d0563439e746a5a36a5277bbbe8b8c / 2b5798585bf748269cb6a0730edecec2 / 20dced36648b42db84cb50442080baab、従来3件c3378ec851b24393aef564d66b359f73。使うコマンドはtools/test-campaign-process.ps1 -Guards。
- GS-089: Native48条件（4役割×3難易度×seed1～4）が12日到達、47勝/最終日1敗（上級刀seed2）。372戦/9324行動/12502保存地点で連続と毎操作再構築が一致。隔離乱数破損も全48＋既存4シードで検出。
- GS-090: seed1の4役割×3難易度の実マウス/実IO12日＋既存3=15件警告0、3e7226c4212344bb91816ab44344e877。全追加12組勝利、追加72戦/1460操作、全体90戦/1727操作。tools/test-campaign.ps1 -Matrix。
- 最新全体計測0c4eb3278a0e4cab9367d4e2b02ad5e2: Passed=true/Missing=[]、行5333/5361=99.48%、関数679/691=98.26%、分岐5485/6004=91.36%。Native全回帰/全UE432警告0、通常起動/音声/別プロセス再開成功。Source/Tests/Content378ファイルの計測時SHA不変、計装DLL/集計保持。通常MSVC復帰後の併用実寸2件も警告0。
- 衣装はGS-079～088で9人の正式差分/選択/取消/取引保存/会話・駒反映/保存戦闘固定/旧互換/別プロセスv20を接続済み。レシピ/SHAはtools/appearance/reviewed-portraits.json。Python3.14.7/Pillow12.0.0はSaved/Tools/AppearancePythonに隔離。詳細はIMPLEMENTED/VALIDATION。
## 【未解決の課題】
- Git同期の実行結果を確認。外見の関連回帰/容量監査は完了。
- 他シード/他役割のプロセス通し/全7ユーザーストーリーの最終受け入れ、生成安全検査、正式敵/地形差分、歩行コマ/攻撃演出/音調整/全設定実寸、Windows配布/オフライン/日本語フォント/性能/45～90分実測。
- 実フルスクリーンtitle.settingsカーソル不一致/title.start未検出、旧battle.start未検出、OS置換エラー5は原因未確定。他経路の成功で解決扱いにしない。

## 【重要な制約・注意点】
- TDD/自作C++行・関数・分岐各90%以上。実UI/実IO/別プロセス/全体を区別。ビルド/計測中Source/Tests凍結、宣言ハッシュはLF正規化UTF8。計測後通常MSVCへ戻す。
- Config/DefaultEngine.iniは秘密。内容の読取/出力/ステージ禁止、容量監査はサイズ/更新日時のみ。原画/セーブ/固定原本/集計/失敗証跡と現行UE Intermediate/DDCを保持。
- Tests/Fixturesのroster-v11/ability-v12/skill-v13/ultimate-v14/bond-v15/personality-v16/enemy-v17/enemy-v18.binは再生成禁止。enemy-v18は3423 bytes/FNV7282346423887295310/SHA256=465FE4FECCCFF16BC18D4459AB99D959D814413D27F555F07CD7618EF72E93CA。
- EnemyEncounterFingerprint.h/.mdの1200戦場Run v19指紋9375212860804229285（基準edc5b5a）を更新して帳尻を合わせない。
- 旧v5合成は外見37＋空敵一覧4を除去、旧v4はさらに難易度4を除去。独立v6レイアウトも検証済み。
- UIは-UI/-RenderOffscreen、-windowed不可。非入力の盤面画像はDriver.IsVisibleでなく実Widget/撮影画素で検証。実寸は撮影寸法も確認。
- Spec Kit/スキルは工程別に必要部分だけ読む。秘密/セーブの一括索引・全体フック・無断委任はしない。未確定仕様は事実扱いしない。
- Git main、origin=https://github.com/KDT-KishimotoTakumi/AI_RPG.git。秘密設定/セーブ/生成物を除外し明示ステージ、commit→pull --ff-only→push後clean/0・0を確認。

## 【次に行う作業】
1. Git同期結果を確認してPLANの残る総合受け入れへ進む。完了した衣装/通し検証を再実装しない。
2. 他条件のプロセス通し/全7ユーザーストーリー/全設定/正式演出・素材/配布・性能・所要時間の未検証を進める。
3. 容量整理の最新実績は末尾とSTORAGEを参照。過去整理量を再計上せず、最新集計/全セーブ/失敗証跡を保持する。
削除しても支障がない情報: 重複継続指示、正本へ記録済みの日報/過去説明。
失うと困る情報: 承認範囲、互換/RNG/固定原本と指紋、最新ゲート/未解決、秘密設定の非参照、次の確定作業。

旧合格4a6fc7ddb58f45ffb9e92cbe1a87f6e6/79a55e5a602043dab0dc8d2d768177c4のJSON/profdata/実行物は各.gzへ可逆保管。使用時のみ展開する。最新b64は通常保持。過去の削減を再計上しない。過去監査はSTORAGEを参照。

GS-091補足: 戦闘外自動保存が既に再開可能なら同一状態の中断は世代を維持する。初回期待値の世代強制増加を、ResumeAvailable=1/Generation35とPersistStateの重複保存省略から診断してテストだけ訂正。失敗原本は保持。製品変更なし。
今回容量整理: 旧合格e67de344b2a04c3aa3bb2e8545f6a8bb / 1c37fb16c3624566851da1542ea6dd42のJSON/profdata/CoreTests.exe計6件を.gzへ可逆保管。45734052→4663365 bytes、論理削減41070687 bytes。保全19296ファイルSHA/秘密メタデータ不変、最新b64/現行UE Intermediate/DDC/全セーブ保持。監査StorageAudit-6a0394b8148a4ff2b0f045364add3fc7。使う時だけ展開、過去削減は再計上しない。次はGit同期結果を確認してPLANの残作業へ進む。

GS-092/T031i完了: 最大文字/字幕/軽減/抑制/消音をタイトル実UIで保存し、単色/危険域と併用して1920×1080/2560×1440の12日を完走。仲間手番でRefreshIntentへ早期returnして表示操作/危険域説明が欠落する実不具合を再現し、CombatScreen.cpp/.hのRefreshBoardCuesを主人公/仲間で共用して修正。戦闘/保存/RNG/数値/素材は不変。通常MSVC最終2件4600b799ae3849219025698f52a050fe / 36678aba9d214e539d96fc3c2548711c、全UE d6856454b36f400abd4c98a78b72cbf9。証跡Saved/campaign-accessibility-{coverage.log,msvc-final.log,frozen-result.json,final-review.log}と各失敗ログ/撮影を保持。別モデル読み取りレビュー指摘なし。
最新0c4の成功生計測6件4439902368 bytesを集計保持後に整理、追加obj0件。保全19443ファイルSHA/秘密メタデータ不変、現行UE Intermediate/DDC/全セーブ保持。監査StorageAudit-06fa14d3999544f387e12e642540769a。処理量はプロジェクト全体純減とは別。旧計測b64は今回の製品変更前の記録で、最新と混同しない。
未完T031j: 撮影で最大文字の長い戦闘ボタン文言末尾欠けを確認、ActionのSTextBlockに折返し指定なし。SC-012/全項目視認は未完了。ゲーム文言/効果を変えず実寸TDDで修正する。まず今回Git同期結果を確認し、その後この修正を進める。
