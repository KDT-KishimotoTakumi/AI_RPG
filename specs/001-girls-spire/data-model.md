# データモデル

- 救出返答（GS-023）: RunSnapshot.PendingRescueは未返答の候補ID0～7、なしは−1。実戦の救出勝利で加入/報酬を反映後に設定し、返答で絆＋10／0／−5を一度だけ適用して解除する。返答では日数・イベント・乱数を進めず世代だけ更新する。未返答のまま次の経路へ進めない。Run payload v3へ追加し、v2は未返答なしとして既存状態を維持する。結果のみの検証用経路APIは実戦経路と区別する。
- Unit: 安定ID、陣営、役割、HP/最大HP、8能力、座標、状態、スキル、装備。HPは0～最大HP。
- Battle: マップ、参加者、ラウンド、安定した行動順、目的、乱数状態、行動履歴、ゲージ0～100。
- BattleCheckpoint（GS-020）: State/RandomState/Goal/SurvivalRounds/TurnIndex/LastHitsを保持。Battle::Restoreは新規戦闘コンストラクタの初期化を行わず、検証後の状態とComboLedgerを復元する。BattleValidation.cppで開始/復元共通の地形・人物検証を実施し、復元時は戦闘不能者とのマス重複だけを許可する。順序参照・生存者の欠落・手番・勝敗・クールダウン・連携参照・直近命中結果を検査する。UIのOrders/Trust/ChoiceRandom、周回との整合、ファイルI/Oは別の未接続領域。
- Intent: 攻勢/護衛/支援/妨害、任意対象、任意地点、合法候補、確率。候補なしなら待機。
- CombatControlCheckpoint（GS-020）: Orders[9]・Trust[9]・ChoiceRandomState・ProfileDrivenを保持する。CombatControl::Restoreは戦場を参照して対象/地点と値域を検証し、独立した仲間乱数と開始時信頼を復元する。現在のProfileを参照しない。倒れた対象への指示は残す。画面とファイルI/Oへの接続は未実装。
- Recipe: ID、参加者条件、構文列、位置/対象条件、効果。参加人数2/3/5、人物重複不可。
- Run: ID、seed、日1～12、難易度、経路、救出者最大4、主人公役割、資源。
- RunCheckpoint（GS-020）: RunSnapshot、RandomState、AfterRewardを保持し、Profileを含めない。Run::Restoreは別引数のProfileと周回の範囲/構造を検証し、失敗時nullopt、成功時は独立したRunを返す。生成済み経路・三択を再生成せず、内部乱数の消費位置と最終報酬後のWon遷移を復元する。ファイル形式、保存世代の整合、中断消費、UIへの接続は別工程。
- Shop（GS-017）: PurseはCoins初期100・Medicines初期0/上限5、ShopStockはPower/Guard各1・Medicines3。AccessoryはNone/Power/Guardの排他的1枠。Buyは価格・在庫・所持上限・HP/負傷を検証後に引数を更新し、失敗時は全引数を保持する。RunSnapshotはWallet/Stock/Accessoriesを保持し、Purchaseが所属・店舗・世代を検証してコピー上で購入し成功時だけ更新する。入店時に在庫を初期化、購入時はRevisionだけ進めて日程を維持。実戦勝利の銭は決着検証後に1回だけ加算する。敗北/最終報酬取得後は財布0・装備なし。Encounterで装飾品を能力へ合成。商店画面はRunの複製で予告し、確定だけを本体へ適用する。
- Medicine（GS-017）: BattleSnapshot.Medicinesは部隊の傷薬残数。EncounterがWalletから受け渡し、PreviewMedicine/UseMedicineが手番・行動・対象・距離・世代を検証して回復と消費を確定する。Runは戦闘結果の残数が0以上かつ持込数以下であることを検証後に引き継ぐ。予告・取消は消費せず、無効/古い操作も状態を保持する。
- GrowthProgress（初期接続）: 人物ID0～8ごとにHP/攻撃/防御/素早さの強化累計と追加技3種の取得状態。周回内のみ保持し、Encounter生成時に基礎能力へ一度加算する。
- LevelProgress: 人物ID0～8ごとのレベル1～10と次レベルへの経験値0～199。上限10では経験値0に正規化し画面はMAX。三択強化と分離し、最大HP＋5・攻撃/防御＋1をレベル差分から一度だけ合成する。実戦結果のコピー上で経験値→回復→加入の順に反映し、無効/古い結果では本体を変えない。新規加入者は成長後の主人公レベル・経験値0、周回終了で全員初期値へ戻す。ディスク保存は未接続。
- HealthProgress: 人物ID0～8の現在HPと負傷段階0～4。HP=-1は未初期化（新規参加時は有効最大HP）、0～3は通常負傷数、4は重傷。基礎最大HP＋成長－負傷で有効最大HPを計算し最低10。戦闘結果の所属/人数/HP/決着とrevisionを検証して一括更新し、勝利回復は一度のみ。休息予告はRunのコピーへ同じ処理を適用する。HP・負傷は周回終了時に消去し、ディスク保存は未接続。
- Reward（初期接続）: 勝利→受取人未選択→加入済み1人に固定→三択→1個取得→次経路（最終勝利なら結果）。受取人・候補をRunSnapshotに保持しRevisionで二重操作を拒否する。ディスク保存は未接続。
- Profile: 人物別絆0～100（初回は8人全員0）、人物別専用交流完了記録、既発見連携、図鑑、外見、難易度、文字の後日談。戦闘中の信頼は人物別絆と同値を参照し、別の周回内信頼値は保持しない。絆と専用交流完了記録は周回を越えて引き継ぐ。
- Profile実装（GS-018/020/021/022）: Bonds[8]、SpecialConversations[8]、Epilogues[9]（0は単独、1～8は人物ID）。RunSnapshotから分離した値型をRunが所有し、交流の所属・ノード・世代検証後にイベント進行と一括更新する。専用交流の完了は絆を追加加算しない。恒久payload v3へ保存し、v1は絆、v2は絆/専用交流を維持して追加記録をfalseで補う。既発見連携等の他の恒久項目は後続。
- SaveEnvelope: schemaVersion、generation、checksum、payload、resumeConsumed。周回と恒久を別管理。
- GS-020保存包絡: Infrastructure/SaveEnvelopeはRun/Profile種別、1以上のuint64世代、最大16MiBのバイナリpayloadを保持する。固定リトルエンディアンの形式版1とFNV-1aでヘッダを含む破損を検出。復号結果は正常/破損/非対応版/別種別を区別し、異常時にpayloadを公開しない。中断消費・ゲーム状態・ファイルI/Oはまだ未接続で、包絡だけでそれらを保証しない。
- BattleBond（GS-019）: 人物ID1～8に対応するCombo/Aid配列。各0～5/0～3。BattleSnapshot.BondGainsへ成功した連携/他者回復だけを集計する。Runの決着・所属・世代検証後にProfileへ一括適用し100で止める。戦闘中の信頼は開始Profileを維持する。
- ComboLedger復元（GS-020）: RestoreはComboSnapshotのラウンド・ゲージ・最新/解決済み番号・保留行動の昇順と人物IDを検証し、成功時だけ置換する。未消費の古い行動と同一人物の複数行動は保持する。乱数を使わず、復元だけで連携を再解決しない。戦場参照の整合性検証、ディスク読み込み、中断消費はこのAPIの責務ではなく未接続。
- 遷移: Title→RoleSelection→Route→Battle/Event→Route→Result→Epilogue→Title。
- 保存復旧: 検証→復旧候補選択→消費状態永続化→再開。結果適用は世代番号で重複排除。

GS-034保存拡張（全単体・全UE・別プロセス再開で検証済み）: RunPayloadSchema=11、ProfilePayloadSchema=3。BattleUnit.SummonerId（通常−1/召喚兵は城主ID）とBattleCheckpoint.EnemyHpPercent（80/100/120）を追加。BattleRole::CastleLord=11、BattleGoal::DefeatLord=3。旧Run v2～v10を受け入れ、所有者なし/HP補正100を補完する。旧戦闘の能力や目標は書き換えない。

GS-035（実装・検証済み）: 未救出者は加入者との差集合（Run::Unrecruited）から選び、既存人物ID1～8と同じBattleUnitで最終戦へ配置。Sideに敵/中立の判定結果を保存し、読み込み時の再抽選はしない。名前・初期役割/技能・立ち絵は同じ人物資源を共用。Run v11/Profile v3は変更しない。

GS-036（実装・検証済み）: Profile.DiscoveredCombosに正のレシピIDの厳密昇順・重複なしの配列を追加。ProfilePayloadSchema=4、旧v1～v3は既存値を維持し空の発見列を補う。RunPayloadSchema=11は変更しない。発見更新は戦闘スナップショットと同じ保存取引で確定し、失敗中は固定結果の再試行以外の進行を止める。

GS-037（実装・検証済み）: 承認済み20連携の定義はDomain/ComboCatalogへ集約。ID2/3/5を維持し、追加は101～117。照合と名前/理由表示で共用する。既存のBattleCheckpoint/ComboResolution/Profile.DiscoveredCombosで保存できるためRun v11/Profile v4は変更しない。

GS-044: BattleSnapshotにAbilityRulesとStartingBonds[8]を追加し、Run v12へ保存。旧v11以下はLegacy/全0を補完。正式状態は固定信頼と整合させ、恒久Profileから復元時に置換しない。Profile v4は不変。技能効果/一時強化/絆100の実行接続は未完了。

GS-046: BattleUnitに攻撃/防御/命中の独立TimedBoostを保持。Run v13へ強度と除外期限を保存し旧v12以下は空で補完。Battle初期化で消去、AdvanceTurnで退出者だけ期限更新。通常/技能/必殺の共通計算と安全予測へ反映。Characterのみに非空を許可する。正式技能による付与は別工程。v12原本ability-v12.binは再生成しない。

GS-047: CharacterSkills(2)とRun v14を追加。旧識別の技能解釈は不変。SkillPreviewに支援/強化/解除の予告情報を追加（予告用で保存対象ではない）。初期2枠の18技能だけ人物別に解釈し、共通報酬技能/旧敵汎用技能は維持。CD/強化/弱体/構文は既存戦闘状態で保存する。

GS-048: CharacterAbilities(3)は正式通常技能と正式個人必殺を使用し、0/1/2の旧必殺は不変。Run v15を使用する。AttackPreviewへ味方支援予告と自身防御強化情報を追加（非永続）。結果は既存HP/TimedBoost/弱体/ゲージ/構文/RNGへ一括反映して保存する。詳細character-ultimates-rules.md。
