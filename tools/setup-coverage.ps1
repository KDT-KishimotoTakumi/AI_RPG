#requires -Version 7.0
$ErrorActionPreference = 'Stop'
$version = '20.1.8'
$package = "clang+llvm-$version-x86_64-pc-windows-msvc"
$expectedHash = 'f229769f11d6a6edc8ada599c0cda964b7dee6ab1a08c6cf9dd7f513e85b107f'
$destination = Join-Path (Split-Path -Parent $PSScriptRoot) 'Saved/Tools'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
$archive = Join-Path $destination "$package.tar.xz"
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest "https://github.com/llvm/llvm-project/releases/download/llvmorg-$version/$package.tar.xz" -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedHash) {
    throw 'LLVMアーカイブが公式SHA256値と一致しません。展開しません。'
}
# OSのPATHやレジストリを変更せず、検証に必要な配布物だけ展開する。
& tar -xf $archive -C $destination '*/bin/clang.exe' '*/bin/clang-cl.exe' '*/bin/llvm-cov.exe' '*/bin/llvm-profdata.exe' '*/bin/lld-link.exe' '*/bin/*.dll' '*/lib/clang/*'
if ($LASTEXITCODE) { throw 'LLVM展開に失敗しました。' }
& (Join-Path $destination "$package/bin/clang-cl.exe") --version
if ($LASTEXITCODE) { throw 'LLVM起動検証に失敗しました。' }
