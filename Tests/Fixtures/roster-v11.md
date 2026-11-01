# 架空の旧Run v11保存データ

`roster-v11.bin` は2026-11-01、保存形式の拡張前にTestData::RosterCaseの100シードから作成した。
実利用者のセーブは含まない。各payloadのFNV64は既存RosterBaseline.hの100値と全て一致。
ファイル全体のSHA-256/サイズはroster-v11.meta.jsonに記録している。

形式はASCII `GSR11` に続き、100件の「uint32 little-endianの長さ＋Run v11 payload」。
末尾の余剰データはない。後の保存形式変更では、本データを再生成せず読込互換を確認する。
生成記録はSaved/DesignReview/CaptureRosterV11.cppとcapture-roster-v11.log。
