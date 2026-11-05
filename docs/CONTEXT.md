# 作業再開メモ

更新: 2026-11-05。要約であり、PROAGENTS.md/仕様正本/最新の利用者指示を優先する。

## 【現在の目的】
Girls Spireを承認済み仕様どおり完成させる。全体は未完成。完成またはトークン限界まで継続し、区切りで内容を変えない容量整理とcommit→pull --ff-only→pushを行う。

## 【確定事項・重要な設定】
- 計画/実装は2026-09-20承認済み。AGENTS→PROAGENTS、spec.mdと承認済みproposal/rules、IMPLEMENTED、PLAN/tasks、VALIDATION/E2E_MATRIXを必要箇所だけ読む。
- 導入済みUE5.7.4/Windows、bUseUnity=false。cwd/環境/Epic manifest/vswhereからパス解決。通常ゲームを勝手に起動/強制終了しない。現在は通常MSVCへ復帰、通常ゲームは閉じている。
- 戦国怪異、12日/1日1～2イベント、1～4日各1人救出し主人公＋4人、ボス6/12日、Lv10。黒鉄蓮25歳男性固定＋女性8人（全員成人/恋愛候補）、固定人物画像/マウス操作/2Dスプライト。全滅で周回性能を失い、絆/発見/図鑑/外見/難易度/後日談は保持。
- 絆0～100恒久、開始時信頼へ固定。救出3択＋10/0/−5。専用交流は加入/絆80以上・交流1回、完了恒久。勝利/当該周回加入/絆80/専用交流完了で該当者全員か単独の後日談を選ぶ。複数恋愛可、嫉妬等なし、合意/部分的裸体まで/抑制設定。本文承認・接続済み。
- 正式18技能/9必殺/絆100強化、通常20＋専用8連携、性格AI8条件、難易度思考、敵人物技能は実装済み。専用201～208は主人公の承→仲間の結・同敵命中/隣接/絆100、ゲージ20・重複加算なし。数値正本は各proposal/rules。
- 未救出者は周回シードで敵50%/中立50%、城主撃破で勝利。敵12種は開始/途中出現の最初の保存成功時に発見、敗北/放棄で保持。人物8人は敵図鑑へ重複登録しない。両図鑑/難易度解放は完了。
- 現在Run v19（旧v2～18互換）/Profile v7（旧v1～6互換）。HighestDifficultyは新規0、初級勝利で1/中級勝利で2、旧v1～4は2、旧v5以降の値は保持。DiscoveredEnemiesは固定ID1～12、旧v5以前は空。
- 外見はappearance-unlock-proposal.mdのAとappearance-palette-proposal.mdの9人の配色を2026-11-05に承認済み。再質問不要。標準＋衣装配色差分各1種、顔/髪/体型/衣装形状/模様/名前/肌/露出/武器/金属は固定。主人公は初勝利、仲間は専用交流完了で解放、人物別選択も恒久。新規戦闘/会話から反映し既存保存戦闘の外見を維持する。

## 【現在の進捗】
- GS-079/T026b1の外見データ基盤を検証完了。Domain/Appearance.h/.cpp、Profile.VictoryRecorded/SelectedAppearances[9]（Standard=0/Alternate=1）。初勝利を後日談前に記録。旧選択は標準/新旗false、既存Epiloguesも根拠に使うが難易度全解放から勝利を推測しない。不正値/未解放選択は拒否。敗北で失わない。
- Red183中75失敗→焦点Green184→全Native293成功→補強Native294成功。固定1200戦場指紋/通常能力12日4シードの保存再構築一致も維持。Sourceの変更は基盤/勝利記録/codecのみで戦闘性能/RNGは不変。
- Coverage/79a55e5a602043dab0dc8d2d768177c4はPassed=true/Missing=[]、行5250/5279=99.45%、関数663/675=98.22%、分岐5425/5950=91.18%。全UE424警告0、通常起動/音声/別プロセス音声再開も成功。JSON/profdata/計装DLLを保全し成功profraw6件4,301,423,320 bytesを整理。
- 初回Clangは前工程CampaignTestsのCombat省略初期化を拒否、明示初期化で訂正。次回は実行成功後の宣言SHA正規化差で集計停止。RunAutomation.cppのLF正規化値を訂正し、同一原本/実行物をappearance-aggregate.ps1で再集計、Source/Testsの集計前後SHAと、補強テスト後の製品ソース147件SHA不変を確認。宣言SHAはtest-coverage.ps1と同じLF正規化UTF8で計算しRaw Get-FileHashを使わない。
- 通常MSVCへ復帰済み。保存UI23、別UEプロセス60地点/世代復旧/独立設定、12日UI3件（メモリ/実ディスク/9人の外見選択データ入り実ディスク）、音声/独立音量/別プロセス再開が成功。Saved/appearance-final-review.logに『外見基盤の最終確認成功』。計測後の補強はテストだけで製品コード不変。
- Save UI=04a0c01045ec421abbc8bd4dc15ab6b7、12日=3447ec7dc21c42b3abff1681a7abfed0、ProcessResume/0f1d2fc4854440b2bcb50f44f0ef555c、音声=f9a2c0ff94a3462f998255bf9290e7d1/fed3399901954d00a4ce29436288cede。詳細VALIDATION。
- 変異コピーで初勝利根拠を難易度2へ誤用すると検出。Saved/AppearanceMutation-99d0c9428aeb4d4bb36b1b20b69bffc1/result.json、期待終了1/原本不変。製品Appearance.cpp SHA=EB18E7DF9EF4C64214A168C4944C20CC3686F26D8858E4B037B47D854DC20525。
- 主人公の生成画像候補は布模様差で未採用。Saved/ArtPrototypes/Appearance/hero-ren-dye1-candidate01.pngと原文.prompt.mdを保全、Content未配置・原画未変更。他8人未生成。衣装が変わるUI/画像E2Eは未完了。
- GS-076/077/078で通常能力12日Native4シードと実マウス/実ディスク通し・内外中断再構築を検証。初期Run/達成済みProfileだけAPI準備し、その後は実マウス。別UEプロセスの12日通し/全条件の代替ではない。

## 【未解決の課題】
- T026b全体: 選択UI/予告取消/取引保存、固定人物・模様を維持した9人の画像差分、保存戦闘の固定外見、新規戦闘/会話への反映。初期化警告には勝利/外見と既存の図鑑/難易度等の恒久損失も明記する。
- 正式人物/敵/地形差分、歩行コマ/攻撃演出、音調整、字幕/演出/親密差分と全設定実寸回帰、全7ストーリー/全武器・難易度/実戦12日総合、Windows配布/オフライン/日本語フォント/性能/45～90分実測。
- 実フルスクリーンのtitle.settingsカーソル不一致/title.start未検出、旧battle.start未検出、OS置換エラー5は原因未確定。失敗原本を保持し、別経路成功を原因修正と断定しない。

## 【重要な制約・注意点】
- TDD、全自作C++の行/関数/分岐各90%以上。実UI/実IO/別プロセス/製品全体を区別し未検証を完了にしない。ビルド/計測中はソース/テストを凍結する。
- Config/DefaultEngine.iniは秘密情報。内容の読取/出力/ステージ禁止。整理はサイズ/更新日時だけ。ゲーム/原画/セーブ/集計/失敗証跡を保持。
- Tests/Fixtures/{roster-v11,ability-v12,skill-v13,ultimate-v14,bond-v15,personality-v16,enemy-v17,enemy-v18}.binを再生成しない。enemy-v18は3423 bytes、FNV1a64=7282346423887295310、SHA256=465FE4FECCCFF16BC18D4459AB99D959D814413D27F555F07CD7618EF72E93CA。
- EnemyEncounterFingerprint.h/.mdは1200戦場のRun v19保存指紋9375212860804229285。基準edc5b5a。比較値を都合よく更新しない。将来の外見固定の保存拡張でも旧原本と戦闘処理不変の検証を残す。
- 旧v5テスト生成はv7の外見37＋空の敵一覧4を除き、旧v4はさらに難易度4を除く。独立した旧v6レイアウトもAppearanceTestsで確認。テスト原本の期待値を変えて不具合を隠さない。
- UIは-UI/-RenderOffscreen、-windowedを追加しない。実解像度はクライアント/撮影寸法も断言する。計測後は通常MSVCへ戻す。
- 必要なスキルだけを使い秘密設定/セーブの索引化、全体フック、無断委任をしない。Spec Kitの全体『曖昧さなし』1件は未確認、既存の継続承認で確定範囲を実装する。
- Gitはmain、origin=https://github.com/KDT-KishimotoTakumi/AI_RPG.git。秘密設定/セーブ/生成物を除外しcommit→pull --ff-only→push、clean/0・0を確認する。

## 【次に行う作業】
1. GS-079の容量整理/Git同期の最新状態を確認。通常MSVCの最終確認は成功済み、同じ確認を理由なく繰り返さない。
2. T026b2/3/4と製品総合検証へ進む。外見条件/具体色は再質問しない。未採用候補をゲームへ接続しない。原画を上書きしない。
3. 区切りで成功生計測/再生成可能なNative objを整理。現行UE Intermediate/DDCを次の開発用に保持、結果/実行物/セーブを保全してGit同期。

削除しても支障がない情報: 重複した継続指示、正本に記録済みの過去ログの再掲。
失うと困る情報: 承認範囲、保存互換/RNG、固定原本/指紋、最新証跡/未解決、秘密設定の非参照、次の確定作業。

GS-079容量整理完了: 成功profraw6件4,301,423,320＋再生成obj271件89,898,244＝処理量4,391,321,564 bytes（全体純減とは別）。保全17,124ファイル/秘密設定メタデータ不変、ゲーム/原画/セーブ/結果/未採用画像を保持。監査StorageAudit-9306ecfabe5641b0ae1fcf371d5f540b。次はGit同期を確認しT026b2/3/4へ進む。

GS-080/T026b2a 最新: 初期化警告へ全恒久損失を明記し、周回初期化は恒久全体保持と明示。削除/復旧/保存形式不変。豊富なProfileでRed不足6項目→Green保存UI23件警告0。全Native294/全UE424成功。Coverage/4a6fc7ddb58f45ffb9e92cbe1a87f6e6は行99.45%/関数98.22%/分岐91.23%、Passed=true/Missing=[]。通常MSVCビルド成功、最終確認の結果とGit状態は下へ追記。READMEの古い重複進捗を集約。衣装画像/選択UI/保存戦闘固定は未完了。
衣装画像方式はappearance-authoring-method.mdでPython/Pillowの布地マスク内だけ色替えを提案し回答待ち。image_gen原則からの別方式には明示指示が必要なため、画像編集/依存追加は未実行。uvから既存Python3.14.7を動的解決、Pillow未導入。未採用生成候補を接続しない。
通常MSVC最終確認成功: 保存UI23警告0（3b44411bc8f54ff5827192d9609980f4）、音声起動/独立音量/別プロセス再開（26f8228dfac94e3392efd13efcd1cc36 / 5d74f24a4975458080602c976159a558）。計装DLLを最新Coverageへ退避し通常DLLへ復帰。通常ゲームは起動していない。次は保全付き整理/Git同期。
GS-080整理完了: 成功生計測6件4,301,435,160 bytes、追加obj0件。保全17,212ファイルSHA/秘密設定メタデータ不変。監査StorageAudit-6da0ebc6d7d84328ad98b82f661b8799。ゲーム/セーブ/画像/集計/実行物/現行UE Intermediate/DDC保持。次はGit同期、未完のT026b2/3/4へ進む。
