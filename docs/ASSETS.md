# 素材台帳

最終更新: 2026-11-04。通常立ち絵・タイトル背景・音源V2・敵/地形の初期スプライトを接続。正式差分/最終調整・配布検証は未完了。

## 方針

- 主人公、仲間8人、敵10種、ボス2体の和風アニメ調2D素材を用意する。
- 正式素材はオリジナル生成物または利用条件を記録できる素材に限定する。
- 各素材について用途、作成・取得日、作成方法または出典、ライセンス、加工内容を記録する。
- 親密表現を含む差分は全人物が18歳以上であることを守り、部分的な裸体までとする。
- 仮素材と正式素材を区別し、仮素材を配布物へ残さない。

## 登録済み素材

- 上記9枚はFCharacterPortraitsからContent配下を動的に参照し、戦闘中の編成メンバーの名前/HPと共に表示。2026-10-30、実UI11件で実PNGサイズ/対応・9人個別/5人同時/戦闘不能/行動後HP更新を確認し、Saved/Screenshots/Portraits-Party.pngを目視確認。正式差分・パッケージへの同梱は後続であり、以下の制作時点の「未接続」は履歴。
- 最終配置は盤面下。画像欠落時の表示/操作を加えた実UI12件と全118件・通常起動1件に成功し、最終スクリーンショットを再度目視確認（2026-10-30）。通常服以外の正式差分、配布同梱は未完了。
- 追加の女性4人: kaede-v1.png（鷹司楓）、honoka-v1.png（雑賀火乃香）、chikage-v1.png（柳生千景）、sayo-v1.png（鈴鹿小夜）。上記5枚と同じContent/Art/Characters/に保存。2026-10-30、組込みimagegen、hero-ren-v1.pngを画風参照に使用。全員成人、通常服。1024×1536、左上角/左端中央のalpha=0を確認。原画像保持、各.prompt.mdへ生成プロンプト原文を記録。生成サービスの適用条件に従う。画像の配布・画面接続・正式最終デザインの検証は未完了。
- 通常立ち絵の初期画像5枚: Content/Art/Characters/ の hero-ren-v1.png、akane-v1.png、mio-v1.png、oboro-v1.png、shion-v1.png。2026-10-30、組込みimagegenで本プロジェクト向けに生成。成人・通常服、主人公を女性4人の画風参照に使用。外部作品の画像取得・転載なし。原画像を保持してコピーし、各画像と同名の.prompt.mdにプロンプト原文・生成方法を記録。全5枚1024×1536、左上角と左端中央のalpha=0を読取り確認。生成サービスの適用条件に従う。配布時の権利確認・ゲーム表示・差分・残る女性4人は未完了。
- 後日談9種の初期文章: 2026-10-28、本プロジェクト向けに新規執筆。Source/GirlsSpire/Presentation/EpilogueScreen.cppに収録しゲームへ組み込む。外部文章の転載・外部素材の取得なし。単独＋成人の仲間8人の合意に基づく結末で、性的描写は含まない。正式な物語全体との最終整合・推敲は未完了。
- 周回プレビューのフォント・単色ブラシ・ボタン: UE5.7.4付属CoreStyleをコードから参照。2026-09-23。
  外部からの素材取得・複製・加工なし。Engine付属素材の利用条件に従う。製品配布時の素材・ライセンス一覧確認は未完了。
- 検証画像: 上記プレビューの実行画面をUEから生成しSaved/Screenshotsへ保存。正式ゲーム素材には使用しない。

GS-035（2026-10-31）: 最終戦では未救出者を含む全9人物の通常立ち絵を表示。既存の同じPNG/名前を共用し、画像ファイルの複製や差分素材は追加していない。敵/中立を文字で区別する実UI2件と全149件・通常MSVC確認が成功。

2026-11-02 GS-052 初期音素材: Content/Audio/Source/のbgm-castle-v1.wav（20秒）、sfx-confirm-v1.wav（0.16秒）、sfx-impact-v1.wav（0.24秒）。tools/generate-audio.pyで本プロジェクト専用に数式合成したモノラル22050Hz/16-bit PCM。独自の32拍の音列・減衰する倍音・低音・簡易打楽器を使用し、外部の曲/録音/音源/演奏の転載やサンプリングはない。生成条件/原本SHA-256は同フォルダmanifest.json、波形の検査はtools/test-audio-assets.py。ピーク/RMSは順に0.194/0.044、0.235/0.086、0.234/0.041、無音でないこと・範囲・DC偏り・波形端を確認。
UE素材はContent/Audio/BGM_Castle_V1.uasset、SFX_Confirm_V1.uasset、SFX_Impact_V1.uasset。tools/import-audio.pyでPCM設定へ取り込み、BGMのみループ指定。取込時だけPythonScriptPlugin/EditorScriptingUtilitiesをCLI有効化し、プロジェクトプラグイン設定は変更しない。初回は無音コマンドレットのBINKAデコーダー未初期化で停止、音声初期化を有効にしたPCM取込は0エラー（別名パス大文字小文字判定の警告2件）で成功。原記録Saved/audio-import*.log。
初期素材として使用し、最終音楽/ミックスの聴感・長時間ループ・配布同梱/権利一覧の最終確認は未完了。Config/DefaultGame.iniに/Game/Audioの常時cookを追加したが、配布パッケージを実測するまで配布完了としない。
2026-11-03: 救出後の人物別台詞と3返答は本会話でプロジェクト向けに作成し、利用者が8人分を一括承認。本文正本specs/001-girls-spire/rescue-dialogue-proposal.md、実装Source/GirlsSpire/Domain/RescueDialogue.cpp。外部文章の引用なし。画像・音素材を置換せず、会話の数値/加入/時間を維持する。
2026-11-03: 通常交流/専用交流の8人別本文は本会話でプロジェクト向けに作成し、利用者が一括承認。後日談9編は既存試作をそのまま転記して正式承認。正本social-ending-dialogue-proposal.md、共有カタログSource/GirlsSpire/Domain/RelationshipText.cpp。外部文章の引用なし。画像・音素材は変更せず、条件・数値・保存形式を維持する。

GS-064素材（2026-11-04）: Content/Art/Backgrounds/title-castle-v1.png は組込みimagegenで新規生成した独自の戦国風城背景。原画像のコピーと全文プロンプトを同フォルダに保持。既存作品の画像・紋章・人物を取得/転載していない。生成サービスの適用条件に従う。固定人物9枚は変更せず盤面にも参照する。
音素材V2は tools/generate-audio-v2.py の標準ライブラリ合成による独自旋律/和音/弦・金管風音/太鼓/残響。既存曲・録音・サウンドフォントの利用なし。Content/Audio/Source/manifest-v2.json に3原本のSHA256と方式を記録し、V1も保持。BGM40秒・96BPM・32000Hzステレオ16bit、確認0.18秒/打撃0.24秒。UEへPCMで非置換取込しBGMのみループ。波形の形式/署名/ピーク/DC/ループ境界/左右差は test-audio-v2.py で検証。聴感の最終評価とWindows配布での同梱確認は未完了。
配布設定は/Game/AudioのCookを維持し、動的読込みPNGのArt/BackgroundsとArt/CharactersをNonUFS対象へ指定した。これは同梱指定の追加であり、Windowsパッケージの実生成・オフライン表示検証はまだ行っていない。

GS-065（2026-11-04）: 固定人物PNG9枚を無加工で待機/移動表示に再利用。追加画像・外部アニメーション素材・歩行フレームはなし。CombatMotion.cpp/UnitMotion.cppが確定経路と表示時計だけで移動/揺れを計算する。画像欠落や単色時は文字情報と汎用の移動記号（◆）へ退避。原画像/プロンプト/V1/V2音源は変更しない。

GS-066素材（2026-11-04）: Content/Art/Objects/terrain-atlas-v1.pngは草地/石畳/木/石壁/柵/罠/段差/遮蔽の8種、enemy-atlas-v1.pngは足軽/弓兵/鉄砲/騎馬/骨忍/鬼武者/呪詛僧/妖狐/絡新婦/絡繰鎧/落城姫/歴史喰らいの城主の12種を収録。組込みimagegenで本プロジェクト向けに新規生成し原本を無加工でコピー。外部作品の転載なし、生成サービスの適用条件に従う。全文プロンプトは同フォルダatlas-v1.prompt.md。元画像のUV範囲を参照し、PNGは加工しない。主人公/仲間9枚の固定画像、既存背景/音源は変更しない。PNGのNonUFS同梱指定を追加するが、Windows配布での実確認は未完了。初期スプライトであり、歩行コマ/関節・攻撃動作や最終デザイン調整は別途残る。

2026-11-05: 敵図鑑12種の画像にも既存Content/Art/Objects/enemy-atlas-v1.pngとBoardSpritesの同じ切出しを再使用。画像ファイル・出典・利用条件の変更なし。図鑑の検証PNGはSaved/Screenshots/EnemyCodexのGUID配下に保持し、ゲーム配布素材には追加しない。

2026-11-05: 衣装配色差分の具体色はspecs/001-girls-spire/appearance-palette-proposal.mdの9人分を利用者が一括承認。標準原画9件を上書きせず別アセット化する方針。現時点で新画像は未生成・ゲーム未接続。人物同一性/肌/武器/金属/模様を維持し、作成後に比較検証を記録する。

2026-11-05 主人公の衣装配色候補01: imagegenスキルの組込みimage_genで標準原画を参照編集し、白鼠/藍の布地を試作。布模様の差を目視確認したため未採用・Content未配置・ゲーム未接続。候補と原文プロンプトはSaved/ArtPrototypes/Appearance/hero-ren-dye1-candidate01.{png,prompt.md}に保全。原画を上書きしていない。他8人の差分は未生成。

2026-11-05 GS-081: Python/Pillow12.0.0のローカル布地マスク方式を利用者が明示承認。tools/appearance/hero-ren-dye1.jsonへ原画SHA・主色#C8CDD2/副色#243B5A・抽出領域を記録。Saved/ArtPrototypes/Appearance/hero-ren-local-01/02へcandidate.png、mask/重ね合わせ、report.jsonを保全。原画の模様を描き直していないが、候補は境界/模様の見え方が未達のため両方未採用・ゲーム未接続。他8人は未生成。最終マスクと目視品質を確定後に展開する。新たな配布素材の追加はない。

2026-11-06 GS-082: 主人公の白鼠/藍の衣装差分hero-ren-dye1.pngを配置（画面未接続）。布地マスクを修正し、顔/髪/手/主要鎧など148,289画素の明示保護を追加。標準原画9枚SHA不変、122,858画素だけ変更、マスク外RGBA/全alpha/寸法不変、local-04/05のPNG再生成一致を確認して制作側で目視比較。差分SHA256=45ABE6AF2E762F48C1038CC50FD7DF7E716B2FEA296276034E85C5DEEE41F00B。配色/作成方法は承認済みで新たな人物・模様・露出・能力を追加しない。設定/確認表はtools/appearance/hero-ren-dye1.jsonとreviewed-portraits.json。監査はSaved/ArtPrototypes/Appearance/hero-ren-local-05/adoption-audit.json。候補01～03は未採用で保全。他8人の差分は未制作。戦場表示/選択UI/保存戦闘固定への接続は別工程。

GS-084補強後もlocal-06で主人公の同一PNGを再生成し、正式差分SHAが不変であることを確認。外部画像生成サービスは利用せず、Pillowの開発環境だけを使用。

## 2026-11-06 GS-085 仲間8人の衣装差分

承認済み配色/ローカルPillow方式で残る8人を制作し、正式9人が揃った。原画9枚/既存主人公差分SHA不変。候補の目視比較後に再生成PNGの一致、外側RGBA/全alpha/指定保護領域/寸法を検査した。模様を描き直さず、布地の画素色だけを変換。澪は上衣/袖/帯、火乃香は外套/腰紐を対象とし、別布地/内衣/裏地/鞄を維持。

|人物|正式画像|変更画素|保護画素|
|---|---|---:|---:|
|真田 茜|Content/Art/Characters/akane-dye1.png|52,144|440,947|
|白峰 澪|Content/Art/Characters/mio-dye1.png|107,081|180,339|
|朧|Content/Art/Characters/oboro-dye1.png|384,207|271,743|
|土御門 紫苑|Content/Art/Characters/shion-dye1.png|543,871|191,274|
|鷹司 楓|Content/Art/Characters/kaede-dye1.png|142,636|243,342|
|雑賀 火乃香|Content/Art/Characters/honoka-dye1.png|163,911|259,790|
|柳生 千景|Content/Art/Characters/chikage-dye1.png|146,437|290,959|
|鈴鹿 小夜|Content/Art/Characters/sayo-dye1.png|334,830|188,193|

全SHA/設定/目視記録はtools/appearance/reviewed-portraits.json、原画照合はSaved/appearance-roster-adoption.json、候補/マスク/再現検証はSaved/ArtPrototypes/Appearance/<人物>-final-01。未採用01候補も保全。個人外見の選択UI/保存戦闘固定/ゲーム表示は未接続。

- 2026-11-07のWindows Shipping候補で、配布画像21点のSHA256が原本と一致し、現行V2のBGM・確認・打撃音がIoStoreコンテナに入ることを確認。隔離した梱包元から制作メモ11件を退避。最終聴感・演出・実機表示は継続。

- GS-103: 現行BGMの実音声デバイス試験で82.13秒、累積再生フレーム比2.053を記録。40秒ループの2境界を停止・再開始なしで通過。ループ接続とミックスの聴感は継続。
