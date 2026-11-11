#requires -Version 7.0
# このプロジェクトのゲームウィンドウ専用の操作ツール（撮影・クリック・キー・スクロール）。
# 対象は launch で起動したゲームのプロセスだけ。他のアプリには撮影も入力もしない。
# 出力は文字化けを避けるため英語。使い方は docs/DEV_SKILLS.md を参照。
#   launch [-Exe <path>]   専用の保存先でゲームを窓モードで起動する（利用者の本物のセーブには触れない）
#   status                  対象ウィンドウの有無・位置・大きさ
#   shot -Out <png>         対象ウィンドウの画面を保存する
#   click -X <n> -Y <n> [-Right] [-Double]   ウィンドウ内（クライアント領域）の座標をクリック
#   key -Keys ENTER,ESC,A,F1 ...             キーを順に押す
#   scroll -X <n> -Y <n> -Delta <n>          ホイール（正=上、負=下、1単位=1ノッチ）
#   quit                    通常の終了要求だけを送る（強制終了しない）
[CmdletBinding()]
param(
    [Parameter(Mandatory, Position = 0)][ValidateSet('launch', 'status', 'shot', 'click', 'key', 'scroll', 'quit')][string]$Command,
    [string]$Exe, [string]$Out, [int]$X, [int]$Y, [int]$Delta, [string[]]$Keys,
    [switch]$Right, [switch]$Double,
    [int]$WaitSeconds = 60
)
$ErrorActionPreference = 'Stop'
if (-not $IsWindows) { throw 'Windows only.' }

$ResX = 1280; $ResY = 900                       # 起動時のウィンドウ寸法
$AllowedTitles = @('Spiral RPG', 'GirlsSpire', 'Girls Spire')
$MaxCoordinate = 8192                           # 座標の上限（異常値の拒否）
$MaxKeys = 32                                   # 1回に押せるキー数
$MaxWheelNotches = 20
$projectRoot = Split-Path -Parent $PSScriptRoot
$stateDir = Join-Path $projectRoot 'Saved/DesktopControl'
$statePath = Join-Path $stateDir 'state.json'

Add-Type -AssemblyName System.Drawing
if (-not ('GsDesktop' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class GsDesktop {
    public delegate bool Visitor(IntPtr window, IntPtr data);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] public struct MouseInput { public int dx, dy; public uint data, flags, time; public IntPtr extra; }
    [StructLayout(LayoutKind.Sequential)] public struct KeyInput { public ushort vk, scan; public uint flags, time; public IntPtr extra; }
    [StructLayout(LayoutKind.Explicit)] public struct InputUnion { [FieldOffset(0)] public MouseInput mi; [FieldOffset(0)] public KeyInput ki; }
    [StructLayout(LayoutKind.Sequential)] public struct Input { public uint type; public InputUnion u; }
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool EnumWindows(Visitor v, IntPtr data);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr w, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr w, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr w);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr w);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr w, int cmd);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr w, out Rect r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr w, ref Point p);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr w);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr w);
    [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint from, uint to, bool attach);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", SetLastError = true)] public static extern uint SendInput(uint n, Input[] inputs, int size);
    public static IntPtr FindWindow(uint pid, string[] titles) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((w, d) => {
            uint owner; GetWindowThreadProcessId(w, out owner);
            if (owner != pid || !IsWindowVisible(w)) return true;
            var s = new StringBuilder(256); GetWindowText(w, s, s.Capacity);
            if (Array.IndexOf(titles, s.ToString().Trim()) < 0) return true;
            found = w; return false;
        }, IntPtr.Zero);
        return found;
    }
    // 前面のウィンドウの入力スレッドへ一時的に接続し、前面化の制限を通常の手順で越える（対象はこのツールが起動したゲームだけ）。
    public static void ForceForeground(IntPtr w) {
        IntPtr fg = GetForegroundWindow(); uint pid;
        uint fgThread = fg == IntPtr.Zero ? 0 : GetWindowThreadProcessId(fg, out pid);
        uint me = GetCurrentThreadId(); bool attached = fgThread != 0 && fgThread != me && AttachThreadInput(me, fgThread, true);
        try { BringWindowToTop(w); SetForegroundWindow(w); } finally { if (attached) AttachThreadInput(me, fgThread, false); }
    }
    public static string Title(IntPtr w) { var s = new StringBuilder(256); GetWindowText(w, s, s.Capacity); return s.ToString().Trim(); }
    static uint Send(params Input[] inputs) { return SendInput((uint)inputs.Length, inputs, Marshal.SizeOf(typeof(Input))); }
    static Input Mouse(uint flags, uint data) { var i = new Input { type = 0 }; i.u.mi = new MouseInput { flags = flags, data = data }; return i; }
    public static void Click(bool right, bool dbl) {
        uint down = right ? 0x0008u : 0x0002u, up = right ? 0x0010u : 0x0004u;
        for (int n = 0; n < (dbl ? 2 : 1); n++) { Send(Mouse(down, 0)); System.Threading.Thread.Sleep(40); Send(Mouse(up, 0)); System.Threading.Thread.Sleep(60); }
    }
    public static void Wheel(int notches) { Send(Mouse(0x0800u, unchecked((uint)(notches * 120)))); }
    public static void Key(ushort vk) {
        var d = new Input { type = 1 }; d.u.ki = new KeyInput { vk = vk };
        var u = new Input { type = 1 }; u.u.ki = new KeyInput { vk = vk, flags = 2 };
        Send(d); System.Threading.Thread.Sleep(40); Send(u); System.Threading.Thread.Sleep(60);
    }
}
'@
}
[void][GsDesktop]::SetProcessDPIAware()

function Read-State {
    if (-not (Test-Path -LiteralPath $statePath)) { throw 'No game was launched by this tool. Run: desktop-control.ps1 launch' }
    Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
}
# 対象は、このツールが起動した生存中のプロセスで、実行ファイルがプロジェクト内にあるものだけ。
function Get-Target {
    $state = Read-State
    $process = Get-Process -Id $state.Pid -ErrorAction SilentlyContinue
    if (-not $process) { throw "The launched game (PID $($state.Pid)) is not running." }
    $root = (Resolve-Path -LiteralPath $projectRoot).Path
    if (-not $process.Path -or -not $process.Path.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refused: the process is not an executable inside this project.'
    }
    $window = [GsDesktop]::FindWindow([uint32]$state.Pid, $AllowedTitles)
    if ($window -eq [IntPtr]::Zero) { throw 'The game window was not found (title must be Spiral RPG / GirlsSpire).' }
    [pscustomobject]@{ State = $state; Process = $process; Window = $window }
}
function Get-Client($window) {
    $rect = [GsDesktop+Rect]::new()
    if (-not [GsDesktop]::GetClientRect($window, [ref]$rect)) { throw 'GetClientRect failed.' }
    $origin = [GsDesktop+Point]::new()
    if (-not [GsDesktop]::ClientToScreen($window, [ref]$origin)) { throw 'ClientToScreen failed.' }
    [pscustomobject]@{ Left = $origin.X; Top = $origin.Y; Width = $rect.Right; Height = $rect.Bottom }
}
# 入力の直前に対象を最前面にし、他のアプリが前面のままなら何も送らず中止する。
function Focus-Target($window) {
    if ([GsDesktop]::IsIconic($window)) { [void][GsDesktop]::ShowWindow($window, 9) }
    [GsDesktop]::ForceForeground($window)
    for ($i = 0; $i -lt 20; $i++) {
        if ([GsDesktop]::GetForegroundWindow() -eq $window) { return }
        Start-Sleep -Milliseconds 100
    }
    throw 'Aborted: could not bring the game window to the foreground, so no input was sent.'
}
function Assert-InClient($client, [int]$x, [int]$y) {
    if ($x -lt 0 -or $y -lt 0 -or $x -ge $client.Width -or $y -ge $client.Height -or $x -gt $MaxCoordinate -or $y -gt $MaxCoordinate) {
        throw "Refused: ($x,$y) is outside the client area $($client.Width)x$($client.Height)."
    }
}
function Resolve-Vk([string]$name) {
    $n = $name.Trim().ToUpperInvariant()
    $named = @{ ENTER = 0x0D; ESC = 0x1B; ESCAPE = 0x1B; TAB = 0x09; SPACE = 0x20; BACK = 0x08; UP = 0x26; DOWN = 0x28; LEFT = 0x25; RIGHT = 0x27 }
    if ($named.ContainsKey($n)) { return [uint16]$named[$n] }
    if ($n -match '^F([1-9]|1[0-2])$') { return [uint16](0x6F + [int]$Matches[1]) }
    if ($n -match '^[A-Z0-9]$') { return [uint16][char]$n }
    throw "Unsupported key: $name"
}

switch ($Command) {
    'launch' {
        if (Test-Path -LiteralPath $statePath) {
            $old = Read-State
            if (Get-Process -Id $old.Pid -ErrorAction SilentlyContinue) { throw "A game launched by this tool is already running (PID $($old.Pid)). Run quit first." }
        }
        if (-not $Exe) {
            $candidates = Get-ChildItem (Join-Path $projectRoot 'Saved/ReleaseCandidates') -Directory -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending |
                ForEach-Object { Join-Path $_.FullName 'Project/Distribution/Windows/GirlsSpire/Binaries/Win64/GirlsSpire-Win64-Shipping.exe' } |
                Where-Object { Test-Path -LiteralPath $_ }
            $Exe = $candidates | Select-Object -First 1
        }
        if (-not $Exe -or -not (Test-Path -LiteralPath $Exe)) { throw 'No Shipping executable found. Run tools/package-windows.ps1 first.' }
        $exeFull = (Resolve-Path -LiteralPath $Exe).Path
        $root = (Resolve-Path -LiteralPath $projectRoot).Path
        if (-not $exeFull.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Refused: the executable is outside this project.' }
        $id = [guid]::NewGuid().ToString('N')
        $userDir = Join-Path $projectRoot "Saved/DesktopControl/UserDir-$id"
        New-Item -ItemType Directory -Force -Path $userDir | Out-Null
        $arguments = @('-windowed', "-ResX=$ResX", "-ResY=$ResY", '-NoSplash', "-UserDir=`"$userDir/`"")
        $process = Start-Process -FilePath $exeFull -ArgumentList $arguments -WorkingDirectory (Split-Path -Parent $exeFull) -PassThru
        New-Item -ItemType Directory -Force -Path $stateDir | Out-Null
        [pscustomobject]@{ Pid = $process.Id; Exe = $exeFull; UserDir = $userDir; Started = (Get-Date).ToString('o') } | ConvertTo-Json | Set-Content -LiteralPath $statePath
        $deadline = (Get-Date).AddSeconds($WaitSeconds)
        do {
            Start-Sleep -Milliseconds 500
            if ($process.HasExited) { throw "The game exited during startup (exit code $($process.ExitCode))." }
            $window = [GsDesktop]::FindWindow([uint32]$process.Id, $AllowedTitles)
        } while ($window -eq [IntPtr]::Zero -and (Get-Date) -lt $deadline)
        if ($window -eq [IntPtr]::Zero) { throw "The game window did not appear within $WaitSeconds seconds (PID $($process.Id) is still running)." }
        "launched pid=$($process.Id) title='$([GsDesktop]::Title($window))' userdir=$userDir"
    }
    'status' {
        $t = Get-Target; $c = Get-Client $t.Window
        "running pid=$($t.State.Pid) title='$([GsDesktop]::Title($t.Window))' client=$($c.Width)x$($c.Height) at ($($c.Left),$($c.Top)) foreground=$([GsDesktop]::GetForegroundWindow() -eq $t.Window)"
    }
    'shot' {
        if (-not $Out) { throw 'shot requires -Out <png>.' }
        $t = Get-Target; Focus-Target $t.Window; Start-Sleep -Milliseconds 150
        $c = Get-Client $t.Window
        if ($c.Width -lt 100 -or $c.Height -lt 100 -or $c.Width -gt $MaxCoordinate -or $c.Height -gt $MaxCoordinate) { throw "Unexpected client size $($c.Width)x$($c.Height)." }
        $dir = Split-Path -Parent ([IO.Path]::GetFullPath($Out)); New-Item -ItemType Directory -Force -Path $dir | Out-Null
        $bitmap = [System.Drawing.Bitmap]::new($c.Width, $c.Height)
        try {
            $g = [System.Drawing.Graphics]::FromImage($bitmap)
            try { $g.CopyFromScreen($c.Left, $c.Top, 0, 0, $bitmap.Size) } finally { $g.Dispose() }
            $bitmap.Save([IO.Path]::GetFullPath($Out), [System.Drawing.Imaging.ImageFormat]::Png)
        } finally { $bitmap.Dispose() }
        "saved $([IO.Path]::GetFullPath($Out)) size=$($c.Width)x$($c.Height)"
    }
    'click' {
        $t = Get-Target; $c = Get-Client $t.Window; Assert-InClient $c $X $Y
        Focus-Target $t.Window
        [void][GsDesktop]::SetCursorPos($c.Left + $X, $c.Top + $Y); Start-Sleep -Milliseconds 80
        [GsDesktop]::Click([bool]$Right, [bool]$Double)
        "clicked ($X,$Y)$(if ($Right) { ' right' })$(if ($Double) { ' double' })"
    }
    'scroll' {
        $t = Get-Target; $c = Get-Client $t.Window; Assert-InClient $c $X $Y
        if ($Delta -eq 0 -or [math]::Abs($Delta) -gt $MaxWheelNotches) { throw "Delta must be 1..$MaxWheelNotches in absolute value." }
        Focus-Target $t.Window
        [void][GsDesktop]::SetCursorPos($c.Left + $X, $c.Top + $Y); Start-Sleep -Milliseconds 80
        [GsDesktop]::Wheel($Delta)
        "scrolled $Delta at ($X,$Y)"
    }
    'key' {
        $list = @($Keys | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
        if ($list.Count -eq 0 -or $list.Count -gt $MaxKeys) { throw "Provide 1..$MaxKeys keys." }
        $codes = $list | ForEach-Object { Resolve-Vk $_ }
        $t = Get-Target; Focus-Target $t.Window
        foreach ($code in $codes) { [GsDesktop]::Key([uint16]$code) }
        "pressed $($list -join ',')"
    }
    'quit' {
        $t = Get-Target
        [void][GsDesktop]::PostMessage($t.Window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)   # WM_CLOSE（通常の終了要求のみ）
        $t.Process.WaitForExit(15000) | Out-Null
        if ($t.Process.HasExited) { "exited normally (close request accepted)" }
        else { "close requested; still running (PID $($t.State.Pid)). Close it from the game's title menu. It was NOT force-killed." }
    }
}
