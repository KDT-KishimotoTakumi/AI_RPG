# UI・ルール契約

UIはCommandを送信し、ルールが検証したSnapshot/Eventだけで更新する。
Command: StartRun、ChooseNode、SetIntent、Move、UseSkill、UseItem、Wait、Suspend、Resume、SelectEpilogue。
無効操作は状態・乱数を進めず理由を返す。演出後の二重入力を拒否する。
SpecialConverse: 交流ノード・加入済み・絆80以上・未完了・現在世代を検証し、1イベント消費と人物別恒久完了を同時更新する。
ConverseRescue: 未返答の救出対象と現在世代を検証し、絆だけを変更して未返答を解除する。予告/取消は無変更、日数・イベント・乱数の追加消費なし。次の経路選択は返答確定後に許可する。
SelectEpilogue: 勝利後だけ単独0と条件達成者全員1～8を提示。予告は無変更、確定は周回消去と後日談記録を同じ保存取引で行う。失敗中は固定した選択の再試行だけ許可し、記録済みの本はタイトルから再読できる。
PreviewとExecuteは同一判定を共有し、選択確定時の状態世代が変化したら再予告する。
UI識別子: title.start、role.select、route.node、battle.cell、battle.confirm、battle.intent、save.suspend、
title.resume、result.continue、epilogue.select、settings.textScale、settings.reduceMotion。
セーブ互換エラーと破損は別表示。恒久データへのエラー伝播を禁止する。
