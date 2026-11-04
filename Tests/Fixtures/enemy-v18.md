# Run v18 固定原本：敵人物技能AIの導入前

2026-11-04に現行v18保存処理で一度だけ記録した架空データ。再生成・上書きしない。
既存のv11～17固定原本は変更していない。

- 入力はTestData::AbilityContextCase(true)（RosterCaseのシード42）。
- AbilityRules=CompleteCharacters、PersonalityAI=true、EnemyAI=Basic、連携ゲージ50。
- 開始時の絆10～80とProfileDrivenの信頼を保持。人物技能AIの新しい保存識別はまだ存在しない。
- 保存コードは基準コミットd5cf834から未変更。GS-071の被害予測修正後、正しいBattle.cppを再コンパイルし、隔離Nativeオブジェクトで出力した。変異版Battle.objはリンク対象から除外した。
- 出力前にEncodeRunPayload→DecodeRunPayloadで全状態の一致を確認した。個人セーブは使用していない。
- 生産記録はSaved/capture-enemy-v18.ps1、capture-enemy-v18-build.log、capture-enemy-v18.log、Saved/DesignReview/ThreatMove-*/CaptureV18.cpp。
- 3,423 bytes、FNV-1a64=7282346423887295310。
- SHA256=465FE4FECCCFF16BC18D4459AB99D959D814413D27F555F07CD7618EF72E93CA。

各生存敵を未行動の手番に切り替え、当時のChooseBasicActionも記録した。

| 敵ID | 行動 | 対象座標 | Ability |
|---|---|---|---|
| 2 | Move（Kind=1） | 7,2 | PowerStrike（未使用の既定値0） |
| 100 | Attack（Kind=2） | 8,2 | PowerStrike（未使用の既定値0） |

同じ値をenemy-v18.meta.jsonに記録。将来の版では、この原本を読み込んだ戦闘に敵人物技能AIを自動適用しないことを検証する。
