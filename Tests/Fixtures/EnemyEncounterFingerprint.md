# 敵生成の変更前指紋

基準: edc5b5aのゲーム処理（GS-074の閲覧UIのみ作業中）、Run payload v19 / Profile v5。
通常/正式API×3難易度×4役割×10シード×5戦場種=1200戦場。
保存バイト列のFNV1a64: **9375212860804229285**。能力・編成・配置・地形・手番・乱数を含む。
EnemyEncounterFingerprint.hで同じ架空入力を生成し、固定値との一致を単体テストする。ゲーム内容を維持する変更では固定値を更新しない。
採取: Saved/enemy-codex-reference.cpp/.ps1、enemy-codex-reference-valid.log、同時点のゲームソースSHA256一覧。既存MSVC純粋C++の正規オブジェクトをリンク。最初の採取用入力では非救出ノードのCandidateを1にしたため保存検証で拒否された。-1へ修正して上記全1200戦場が有効となった。ゲーム原本は変更していない。
