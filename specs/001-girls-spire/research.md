# 技術調査

更新: 2026-09-23。

- 決定: 既存インストール台帳とBuild.versionで確認したUE5.7.4を使用。
  理由: 2026-09-23の利用者指定。旧5.8導入計画を置き換える。ブラウザ移植は行わない。
  出典: https://dev.epicgames.com/documentation/unreal-engine/install-unreal-engine
- 決定: ルールを純粋C++としてUEから分離し、LLVM source coverageで測定。
  理由: 固定シードの高速なRed/Greenと行・関数・分岐の個別ゲートを可能にする。
  代替: 行のみの計測は要件不足。
  出典: https://clang.llvm.org/docs/SourceBasedCodeCoverage.html
- 決定: UI E2EはAutomation Driverの識別子付き操作とGauntletで起動・結果収集。
  理由: ルール呼出しだけの統合テストをUI E2Eと混同しない。
  出典: https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-driver-in-unreal-engine
- 決定: UE接続層のWindows計装を別途実証し、未測定なら品質ゲート不合格。
  理由: UBTのbCodeCoverageの存在だけではWindowsでの三指標取得を保証できない。
  出典: https://dev.epicgames.com/documentation/en-us/unreal-engine/build-configuration-for-unreal-engine
