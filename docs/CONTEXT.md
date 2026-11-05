# 作業再開メモ

更新: 2026-11-06。要約。PROAGENTS.md・仕様正本・最新利用者指示を優先する。

## 【現在の目的】
Girls Spireを承認済み仕様どおり完成させる。全体は未完成。完成またはトークン限界まで継続し、区切りでゲーム内容を変えない容量整理とcommit→pull --ff-only→pushを行う。

## 【確定事項・重要な設定】
- 計画/実装は2026-09-20承認済み。要件はspecs/001-girls-spire/spec.md、ゲーム概要GAME、実装IMPLEMENTED、残作業PLAN/tasks、証跡VALIDATION/E2E_MATRIX。必要箇所だけ読む。
- 導入済みUE5.7.4/Windowsを使う。bUseUnity=false。パスはcwd/環境/Epic manifest/vswhereから解決。通常ゲームを勝手に起動/強制終了しない。
- 戦国怪異/グリッドターン制/マウス/2Dスプライト。12日、1日1～2イベント、1～4日各1人救出、主人公＋4人、ボス6/12日、Lv10。黒鉄蓮25歳男性固定＋女性8人（全員成人/恋愛候補）。全滅で周回性能を失い、絆/発見/図鑑/外見/難易度/後日談は保持。
- 絆0～100恒久、開始時信頼へ固定。救出3択＋10/0/−5。専用交流は加入/絆80以上・交流1回で完了恒久。勝利/当該周回加入/絆80/専用交流完了で該当者全員か単独の後日談を選ぶ。複数恋愛可、嫉妬等なし、合意/部分的裸体まで/抑制設定。承認本文は接続済み。
- 正式18技能/9必殺/絆100強化、通常20＋専用8連携、性格AI8条件、難易度思考、敵人物技能は実装済み。専用201～208は主人公の承→仲間の結・同敵命中/隣接/絆100、ゲージ20・重複加算なし。数値は承認済みproposal/rulesが正本。
- 未救出4人は周回シードで敵50％/中立50％、城主撃破で勝利。敵12種の開始/途中出現は最初の保存成功時に発見し、敗北/放棄で保持。人物8人は敵図鑑へ重複登録しない。
- Run v19（旧v2～18互換）/Profile v7（旧v1～6互換）。HighestDifficultyは新規0→初級勝利1→中級勝利2、旧v1～4は2で移行。旧v5以前のDiscoveredEnemiesは空。初勝利はVictoryRecordedで後日談前に確定し、旧Epiloguesも根拠にするが難易度全解放から勝利を推測しない。
- 外見案A・9人の配色表・Python/Pillow方式を利用者が明示承認済み。再質問不要。標準＋衣装色違い各1種、顔/髪/体型/衣装形状/模様/名前/肌/露出/武器/金属は固定。主人公初勝利、仲間は専用交流完了で解放。人物別選択も恒久。新規戦闘/会話から反映し、既存保存戦闘の外見は維持する。

## 【現在の進捗】
- 外見データ基盤: Domain/Appearance、Profile.VictoryRecorded/SelectedAppearances[9]（Standard=0/Alternate=1）。旧選択は標準、不正値/未解放選択は拒否。敗北で失わない。初期化告知にも全恒久損失を明記済み。
- GS-082: 主人公hero-ren-dye1.pngを配置。主色#C8CDD2/副色#243B5A。標準原画9枚SHA不変、変更122,858/保護148,289画素、外側RGBA/全alpha/寸法不変。local-04/05/06の再生成一致。正式PNG SHA256=45ABE6AF2E762F48C1038CC50FD7DF7E716B2FEA296276034E85C5DEEE41F00B。設定/確認表はtools/appearance/hero-ren-dye1.jsonとreviewed-portraits.json。
- GS-083: FCharacterPortraits.Get(actor,variant=Standard)と9×2キャッシュをTDD実装。不正ID/列挙値/欠落はnullptrで、標準と偽らない。旧呼出しは標準のまま。主人公の配色別API読込まで確認し、選択UI/保存戦闘固定は未接続。
- 最新全体計測Coverage/aadd0ce54113474487b76a0058e6d238: Passed=true/Missing=[]、行5254/5283=99.45%、関数663/675=98.22%、分岐5434/5956=91.24%。Native全スイート失敗0（外見は294チェック。Native全体件数とは別）。全UE425警告0=6e2a5e2a7a164b95a144ba76b2273a48。通常起動/音声/別プロセス音声再開/音声専用2件も成功。詳細VALIDATION。
- 固定したSource/Tests/人物素材348ファイルSHA不変。計装DLLを最新Coverageへ退避し通常MSVCへ復帰、立ち絵UI13件警告0=7e74352b7a024ee9a397bd0475ce1e2c。Saved/appearance-loader-final-review.logに成功。通常MSVCでの今回の最終再実行範囲は立ち絵UI。
- GS-084: 制作ツールの禁止領域優先/全レイヤー最終再検査/不正名/大小文字重複/出力前拒否をTDD補強。Python23テスト成功。補強後もC++製品147ファイルSHA不変。Python3.14.7をuvから動的解決しSaved/Tools/AppearancePythonへPillow12.0.0を隔離導入。使用法はtools/appearance/README.md。
- 証跡: Saved/appearance-{protected*,final-protection*,layer-name*,loader-*}のログ、ArtPrototypes/Appearanceの候補/マスク/重ね合わせ/report/adoption-audit。主人公候補01～03とimagegen候補は未採用で保全。他8人は標準の一覧を閲覧しただけで差分未制作。通常ゲームは起動していない。

## 【未解決の課題】
- T026b全体: 他8人の差分制作、選択UI/予告取消/取引保存、新規戦闘/会話への反映、保存戦闘の固定外見、9人の実画像E2E。主人公素材配置やAPI読込だけを外見全体の完成としない。
- 正式敵/地形差分、歩行コマ/攻撃演出、音調整、全設定の実寸回帰、全7ストーリー/全武器・難易度/実戦12日総合、Windows配布/オフライン/日本語フォント/性能/45～90分実測。
- 実fullscreen title.settingsカーソル不一致/title.start未検出、旧battle.start未検出、OS置換エラー5の原因は未確定。今回のoffscreen成功を原因修正と断定しない。

## 【重要な制約・注意点】
- TDDと全自作C++の行/関数/分岐各90%以上。実UI/実IO/別プロセス/全体を区別。ビルド/計測中はSource/Testsを凍結。宣言ハッシュはLF正規化UTF8（Raw Get-FileHashではない）。計測後は通常MSVCへ戻す。
- Config/DefaultEngine.iniは秘密情報。内容の読取/出力/ステージ禁止。整理ではサイズ/更新日時だけ。原画/セーブ/固定原本/集計/失敗証跡を保全し、現行UE Intermediate/DDCを保持。
- 固定原本Tests/Fixtures/{roster-v11,ability-v12,skill-v13,ultimate-v14,bond-v15,personality-v16,enemy-v17,enemy-v18}.binは再生成しない。enemy-v18は3423 bytes/FNV1a64=7282346423887295310/SHA256=465FE4FECCCFF16BC18D4459AB99D959D814413D27F555F07CD7618EF72E93CA。
- EnemyEncounterFingerprint.h/.mdの1200戦場Run v19指紋は9375212860804229285（基準edc5b5a）。外見固定の保存拡張でも旧原本/戦闘処理不変を検証し、比較値を都合よく更新しない。
- 旧v5合成テストはv7の外見37＋空の敵一覧4を除き、旧v4はさらに難易度4を除く。独立v6レイアウトもAppearanceTestsで確認済み。
- UIは-UI/-RenderOffscreen、-windowedを追加しない。実寸はクライアント/撮影寸法でも確認。通常ゲームを勝手に起動/強制終了せず、所有する隔離試験だけ実行する。
- 必要なスキル/資料だけ使用し、秘密設定/セーブの一括索引・全体フック・無断委任をしない。Spec Kit全体の「曖昧さなし」1件は未確認だが、継続承認済みの確定範囲を実装する。
- Gitはmain、origin=https://github.com/KDT-KishimotoTakumi/AI_RPG.git。秘密設定/セーブ/生成物を除外しcommit→pull --ff-only→push、clean/0・0を確認。同期の最新状態はgit status/logを正とする。

## 【次に行う作業】
1. 今回の容量監査/Git同期の実行結果を確認。合格済みC++/UIを理由なく繰り返さない。最新計測の成功profraw6件4,302,835,720 bytesは集計保持後に整理済み（全体純減とは別）。
2. 未完のT026b2/3/4へ進む。主人公原画/配色/方式の再質問、未採用候補の接続、原画の上書きをしない。残る人物は布地色が異なるため主人公のマスクを流用せず保護領域を確認する。
3. 区切りで保全付き容量整理とGit同期。集計/実行物/セーブ/失敗証跡を保持する。

削除しても支障がない情報: 重複継続指示、正本へ記録済みの過去ログ再掲。
失うと困る情報: 承認範囲、保存互換/RNG、固定原本/指紋、最新証跡/未解決、秘密設定の非参照、次の確定作業。

容量監査完了: StorageAudit-14311055c5e9469ba7480d180f04a61b。今回生計測6件4,302,835,720 bytes/追加obj0件、保全17,325ファイルSHAと秘密設定メタデータ不変。原画/正式差分/セーブ/実行物/結果/失敗証跡/現行UE Intermediate/DDC保持。Git同期の最終出力はSaved/appearance-sync.log（生成物でGit管理外）。
