# 布地差分の制作

利用者承認済みのPython/Pillow方式。ゲーム本体とは独立した開発用ツールです。
主人公のlocal-04/05は同一PNGを目視比較してhero-ren-dye1.pngへ配置済みです。
確認情報はreviewed-portraits.json。候補01～03/他8人/画面接続は別途です。
自動検証だけで顔・髪・肌・金属・模様の目視比較を代替してはいけません。

プロジェクトルートでPowerShell 7から実行します。

```powershell
$python = (& uv --offline python find).Trim()
$environment = Join-Path (Get-Location) 'Saved/Tools/AppearancePython'
uv venv --python $python $environment
$python = Join-Path $environment 'Scripts/python.exe'
uv pip install --python $python -r tools/appearance/requirements.txt
& $python -m unittest discover -s Tests/Tools -p test_dye_cloth.py
$output = Join-Path (Get-Location) ('Saved/ArtPrototypes/Appearance/hero-ren-' + [guid]::NewGuid().ToString('N'))
& $python tools/appearance/dye_cloth.py --config tools/appearance/hero-ren-dye1.json --output $output
```

環境が既に存在する場合は再作成せず、そのPythonを再使用します。
原画を上書きせず、出力先はSaved/ArtPrototypes配下の新しいフォルダだけです。
SHAや画像寸法が変わった原画を自動で許可し直してはいけません。

出力は候補画像、各布地マスク、合成マスク、重ね合わせ、検証JSONです。
自動検証はマスク外RGBA/全alphaの不変を保証しますが、マスク自体の適切さは
原画との目視比較が必要です。境界・模様・肌・金属・髪を確認してから素材として採用します。
配色と解放条件はspecs/001-girls-spireの承認済み資料を参照し、数値や物語は変更しません。

protected_polygonsは色抽出より優先され、全レイヤー合成後にもRGBA一致を確認します。

マスク名は英字で始まる64文字以内の英数字/ハイフン/下線だけを使い、大小文字違いを含む重複を拒否します。不正設定では出力フォルダを作りません。現在の全23テストには禁止領域と不正名の失敗再現を含みます。
