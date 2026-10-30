# 検証手順

前提: UE5.7.4、対応MSVC/Windows SDK、LLVM 20.1.8 coverage tools。現在の環境状態はdocs/VALIDATION.md。

1. tools/test-core.ps1でネイティブ単体テストを実行（導入後）。失敗時は終了コード非0。
2. UE EditorでGirlsSpire Automationを実行し、Saved/Automationへ結果を保存（UE接続後）。
3. GauntletでTests/E2EのシナリオをUI入力で実行（実装後）。
4. 全自作C++の行・関数・分岐各90%以上、全E2E対応表合格を確認。
5. パッケージをネットワーク無効環境で起動し、12日、敗北、中断復旧、後日談を確認。

本書は検証手順であり、未作成コマンドや未実行試験を完了とするものではない。
