# 架空のRun v12互換原本

ability-v12.binはGS-046の保存拡張前、RunPayloadSchema==12を静的検査する専用生成器で作成した。
元の製品コードはコミット4f967b9。LegacyとCharacter（開始時絆10～80/固定信頼）の2例。
実セーブは含まない。ファイル6549 bytes、署名GSA12に続く2件のuint32 little-endian長さ＋payload。
SHA-256/各payloadの既知FNV64はability-v12.meta.json。今後再生成せず、旧v12の空強化補完・全状態・乱数を検証する。
生成器/記録はSaved/DesignReview/CaptureAbilityV12.cppとSaved/capture-ability-v12.log。
