# Starts a game, holds one key against its window long enough for the engine's
# once-per-frame keyboard poll to see it, and saves the window before and after.
#
# Labrador samples keys at the start of each frame (engine/input/keyboard.h): a
# key pressed and released between two polls is never seen, so the hold spans
# many frames and the release comes in a later one. Keys go in as set-1 scan
# codes, which is how the Win32 window reads them, and only reach the
# foreground window, so the script brings the game forward first and refuses to
# continue if it cannot. Windows lets a program take the foreground only just
# after a key or click, so the script taps Alt before asking for it.
#
# From an engine checkout:
#   powershell -ExecutionPolicy Bypass -File tools\input_drive\hold_key.ps1 `
#       -Exe out\build\x64-release\samples\minimal\MinimalSample.exe -Key Right
# From a starter project, whose engine is in external\labrador:
#   powershell -ExecutionPolicy Bypass -File external\labrador\tools\input_drive\hold_key.ps1 `
#       -Exe out\build\x64-debug\game\MyGame.exe -Key Right
# The pictures go to out\input-drive under the folder the command runs in.

param(
    [Parameter(Mandatory = $true)] [string] $Exe,
    [ValidateSet('Left', 'Right', 'Up', 'Down', 'Enter', 'Space', 'Escape')] [string] $Key = 'Right',
    [int] $HoldMilliseconds = 500,
    [string] $OutDirectory = 'out\input-drive'
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class InputDrive
{
    [StructLayout(LayoutKind.Sequential)]
    public struct KEYBDINPUT
    {
        public ushort wVk;
        public ushort wScan;
        public uint dwFlags;
        public uint time;
        public IntPtr dwExtraInfo;
    }

    // INPUT is a tagged union whose largest member, MOUSEINPUT, makes it 40
    // bytes on x64; the keyboard member starts after the 8-byte-aligned tag.
    [StructLayout(LayoutKind.Explicit, Size = 40)]
    public struct INPUT
    {
        [FieldOffset(0)] public uint type;
        [FieldOffset(8)] public KEYBDINPUT ki;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X, Y; }

    [DllImport("user32.dll", SetLastError = true)]
    static extern uint SendInput(uint count, INPUT[] inputs, int size);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr window, out RECT rect);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr window, ref POINT point);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();

    const uint KEYEVENTF_EXTENDEDKEY = 0x0001;
    const uint KEYEVENTF_KEYUP = 0x0002;
    const uint KEYEVENTF_SCANCODE = 0x0008;

    public static void Key(ushort scan, bool extended, bool up)
    {
        INPUT input = new INPUT();
        input.type = 1;
        input.ki.wScan = scan;
        input.ki.dwFlags = KEYEVENTF_SCANCODE | (extended ? KEYEVENTF_EXTENDEDKEY : 0) |
            (up ? KEYEVENTF_KEYUP : 0);
        if (SendInput(1, new[] { input }, Marshal.SizeOf(typeof(INPUT))) != 1)
        {
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        }
    }
}
'@

# Set-1 scan codes; the arrows are the extended (E0) navigation cluster.
$scans = @{
    Left = @(0x4B, $true); Right = @(0x4D, $true); Up = @(0x48, $true)
    Down = @(0x50, $true); Enter = @(0x1C, $false); Space = @(0x39, $false)
    Escape = @(0x01, $false)
}

# Physical pixels, so the capture rectangle matches the window at any scaling.
[void][InputDrive]::SetProcessDPIAware()

function Save-Client([IntPtr] $window, [string] $path) {
    $rect = New-Object InputDrive+RECT
    [void][InputDrive]::GetClientRect($window, [ref]$rect)
    $origin = New-Object InputDrive+POINT
    [void][InputDrive]::ClientToScreen($window, [ref]$origin)
    $bitmap = New-Object System.Drawing.Bitmap $rect.Right, $rect.Bottom
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($origin.X, $origin.Y, 0, 0, $bitmap.Size)
    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose()
    $bitmap.Dispose()
}

New-Item -ItemType Directory -Force $OutDirectory | Out-Null
$exePath = (Resolve-Path -LiteralPath $Exe).Path
$game = Start-Process -FilePath $exePath -WorkingDirectory (Split-Path $exePath) -PassThru
try {
    $deadline = (Get-Date).AddSeconds(10)
    while ($game.MainWindowHandle -eq [IntPtr]::Zero) {
        if ($game.HasExited -or (Get-Date) -gt $deadline) { throw 'The game showed no window.' }
        Start-Sleep -Milliseconds 100
        $game.Refresh()
    }
    $window = $game.MainWindowHandle
    # A key event makes this script's input the last the system saw, which is
    # what lets SetForegroundWindow succeed when the script was not started
    # from the window in front. It reaches that window, not the game, and Alt
    # pressed and released alone types nothing.
    [InputDrive]::Key(0x38, $false, $false)
    [InputDrive]::Key(0x38, $false, $true)
    [void][InputDrive]::SetForegroundWindow($window)
    Start-Sleep -Milliseconds 1000
    if ([InputDrive]::GetForegroundWindow() -ne $window) {
        throw 'The game is not the foreground window, so it would not receive the key.'
    }

    Save-Client $window (Join-Path $OutDirectory 'before.png')
    $scan, $extended = $scans[$Key]
    [InputDrive]::Key($scan, $extended, $false)
    Start-Sleep -Milliseconds $HoldMilliseconds
    [InputDrive]::Key($scan, $extended, $true)
    Start-Sleep -Milliseconds 200
    Save-Client $window (Join-Path $OutDirectory 'after.png')
    Write-Output "Held $Key for $HoldMilliseconds ms; wrote before.png and after.png to $OutDirectory."
} finally {
    if (-not $game.HasExited) {
        [void]$game.CloseMainWindow()
        if (-not $game.WaitForExit(5000)) { $game.Kill() }
    }
}
