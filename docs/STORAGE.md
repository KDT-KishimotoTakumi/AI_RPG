# 容量整理の記録

実施日: 2026-10-30。利用者の「実装状況を報告後、ゲーム内容を変更せず容量削減」に対応。

## 第1回: 生計測データの削除

- 削除対象: プロジェクト内Saved/Coverage配下のLLVM生計測データ（*.profraw）のみ、112ファイル。
- 削減: 51,438,270,576 bytes（47.906 GiB）。ファイルの論理サイズ合計であり、OSの空き容量増加量そのものの測定ではない。
- プロジェクト合計: 56,454,742,318 → 5,016,471,742 bytes（約52.6 → 4.7 GiB、管理記録の追記前）。
- 保持: Source/Content/Config/Tests/tools/specs/.specify、現在のBinaries、セーブ、失敗調査用ファイル、Automation報告、スクリーンショット、全計測のJSON/集計済みprofdata、LLVMツール。Intermediateも次回ビルドのため保持。
- 整理の直前/直後で保護対象594ファイルのSHA-256が一致。生データの残り0件、最新のcoverage.json/coverage.profdataは存在。
- ゲームコード・素材・設定・セーブの変更なし。実装進捗と本整理結果の管理文書だけを追記する。
- 削除前に試験/計測プロセスの停止を確認し、プロジェクト内の対象パスと拡張子を検証してファイル単位で削除。エンジンやプロジェクト外のフォルダは操作していない。
- 生データは次回の計測で再生成される。既存結果の参照には保持したJSON/集計済みprofdataを用いる。

詳細: Saved/storage-cleanup-result.json、Saved/storage-protected-before.json。SavedはGit管理対象外。

進捗文書の更新後も再照合し、保護対象の差分はspecs/001-girls-spire/tasks.mdの完了チェックのみ。ゲームファイルの差分0、残る生計測データ0、最新計測ゲートのPassed=trueを再確認（Saved/storage-postcheck.json）。

## 第2回: 中間物・重複配布物・過去報告の分析と整理

同日、利用者が追加の容量分析・削減を依頼。ゲーム仕様・内容の変更なし。

| 対象 | 分析・処置 |
| --- | --- |
| Intermediate（2,638,672,379 bytes） | 再生成できるPCH・オブジェクト・生成情報・アセット索引を削除。起動用Binariesは保持。 |
| LLVM配布アーカイブ（939,541,912 bytes） | 展開済みツールと重複。公式SHA-256値との一致、保持するツールの20.1.8起動を確認して配布アーカイブのみ削除。 |
| 過去のcoverage.json 42件 | 473,202,117 → 32,362,715 bytesへgzip可逆圧縮。同じフォルダのcoverage.json.gzへ保存し、展開結果のSHA-256を圧縮前と照合。保管後にも再照合。 |
| LLVMツール本体・最新計測・起動ビルド | 次回の開発/計測・既存結果の検証・起動に必要なため保持。 |
| ソース・素材・設定・仕様・テスト・セーブ・報告・Git履歴 | ゲーム内容または検証記録であり、削除・変更しない。 |

- 合計: 5,016,782,277 → 997,822,156 bytes（4.672 → 0.929 GiB）。追加削減4,018,960,121 bytes（3.743 GiB）。管理記録追記前の論理ファイルサイズ合計。
- 保護対象1,648ファイルのSHA-256が全件一致。仕様/ソース/素材/設定/テスト/ツールスクリプト/LLVM本体/セーブ/テスト報告/現在のBinaries/最新の計測を保持。
- 最新47e702dd48714abf8bb473a201a2e09cのcoverage.json/coverage.profdataと各gateは非圧縮のまま保持。過去の集計済みprofdata・テスト実行物も保持。
- 古い詳細JSONはcoverage.json.gzを展開して参照する。Saved/storage-phase2-archives.jsonに元のパス・サイズ・SHA-256・展開後SHA-256を記録。
- 次回ビルド時にIntermediateは自動再生成され、その分の容量とビルド時間が必要。通常のビルド/計測は保持したLLVM本体を使用する。setup-coverage.ps1を改めて実行する場合は配布物が再ダウンロードされる。
- ゲーム用ファイル無変更のためゲームE2Eの再実行は行わず、前後ハッシュ・gzip全42件の内容照合・保持ツールの起動を確認。最新の既存品質ゲートPassed=trueも保持。
- パスをプロジェクト内へ限定し、動作中の開発プロセスと外部リンクがないことを確認して整理。プロジェクト外・インストール済みUEは操作していない。

詳細: Saved/storage-phase2-result.json、storage-phase2-protected-before.json、storage-phase2-actions.json、storage-phase2-archives.json。

## 第3回: 過去の検証実行物・集計データの可逆圧縮

同日の継続指示に従い、ゲーム仕様・内容を変えず追加分析した。残る容量の中心はLLVM本体650,643,594 bytesで、今後のビルド・計測に必要なため保持した。

- 最新計測を除くCoreTests.exeとcoverage.profdata、計96ファイルを同じフォルダの.exe.gz/.profdata.gzへ可逆圧縮。
- 86,592,896 → 27,344,852 bytes。各ファイルを展開したSHA-256が元ファイルと一致してから元ファイルを除去。
- プロジェクト合計997,853,173 → 938,645,140 bytes（管理記録追記前）。追加削減59,208,033 bytes（56.47 MiB）、残り約0.874 GiB。論理ファイルサイズ合計。
- 保護対象1,648ファイルのハッシュ全件一致。ゲーム仕様・ソース・素材・設定・テスト・セーブ・起動用Binaries・LLVM本体・最新の計測は変更なし。
- 最新47e702dd48714abf8bb473a201a2e09cのCoreTests.exe/coverage.profdata/coverage.json/gateは通常の形式で保持。過去の検証物を使うときだけgzipを展開する。
- ゲームE2Eは再実行せず、アーカイブ全96件の展開内容と保護対象のハッシュを検証。大きな再生成物・重複配布物の整理は完了し、ゲーム素材や必要な開発ツールの削除は行っていない。

詳細: Saved/storage-phase3-result.json、Saved/storage-phase3-archives.json。圧縮前のパス・サイズ・SHA-256を保持。

2026-10-31: 開発再開後の合格済み3回（GS-027/028/029）のprofraw9件、8,130,120,776 bytes（約7.57 GiB）を整理。各gate.jsonのPassed=true、非空coverage.profdata、coverage.jsonの存在を確認してから既知フォルダの通常ファイルだけ削除。集計/JSON/実行物/セーブ/素材と実行中のGS-030計測は保持。記録Saved/completed-profile-cleanup.json。ゲーム仕様・内容の変更なし。

2026-10-31追加整理: 最新の絡新婦全体ゲートと24地点再開/通常ビルドの完了後、生profraw16件・16296396000 bytes（約15.18GiB）を削減。18.673→3.496GiB。ソース/仕様/素材/設定/テスト/起動ビルド/ツール/セーブ等1,875ファイルは整理前後のSHA-256一致。集計JSON/profdata、実行物、診断ログは保持。Saved/StorageAudit-9fca0259d8cd4bab8a4d15f9f4a1cd4a/result.json。今後tools/test-coverage.ps1は全体ゲート合格後にcleanup-measurements.ps1で当該回の生データのみ整理する。未合格保護/WhatIf/生データ限定削除/集計保持を専用試験で確認済み。Intermediate等の有用なキャッシュは保持。

GS-033: 全体カバレッジ合格後の自動整理を初回実行。7689965f797647c78215b5a92d7def35のprofraw3件2,720,628,248 bytes（約2.53GiB）を削除し、JSON/profdata/実行物を保持。raw-cleanup.jsonへ記録。検証結果やゲームデータを減らさず、成功した当該回だけを対象とした。

GS-034: 全体ゲート合格後、23ba3ccbfbdb465189075f3e047b0e07のprofraw3件2,723,005,184 bytes（約2.54GiB）を自動整理。raw-cleanup.json、gate.json、coverage.json/profdataと実行物を保持。ゲーム素材・仕様・保存データ・検証結果は削除していない。

GS-035: 全体ゲート合格後、aa6ce94affb64ad3af8350412404f0ccのprofraw3件2,725,179,440 bytes（約2.54GiB）を自動整理。raw-cleanup.json、gate.json、coverage.json/profdataと実行物を保持。人物画像は既存素材を共用し、ゲーム内容・仕様・保存データ・検証結果を削除していない。

2026-10-31・コミット前整理: 再生成可能なSharedPCHのpch/objとCachedAssetRegistry_0.binの3件、2,557,134,884 bytes（2.382GiB）を削減。削減対象外5,102ファイルは整理前後のSHA-256一致（変更0件）。起動用Binaries・素材・仕様・設定・セーブ・集計を保持。次回ビルドでPCHの再生成が必要。監査Saved/StorageAudit-d6963e2fa68f495da09898ae3d238acb/result.json。秘密情報を含むConfig/DefaultEngine.iniはローカルの内容を保持し.gitignoreでGit対象外。公開候補の資格情報形式/秘密値代入の検査は検出0件。
