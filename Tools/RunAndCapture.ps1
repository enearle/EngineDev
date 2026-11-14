param(
    [string]$Exe,
    [string]$WorkDir,
    [string]$Out,
    [int]$Seconds = 6,
    [string]$ExeArgs = ""
)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
[Win]::SetProcessDPIAware() | Out-Null

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $Exe
$psi.Arguments = $ExeArgs
$psi.WorkingDirectory = $WorkDir
$psi.UseShellExecute = $false
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$p = New-Object System.Diagnostics.Process
$p.StartInfo = $psi
$stdout = New-Object System.Text.StringBuilder
$stderr = New-Object System.Text.StringBuilder
Register-ObjectEvent -InputObject $p -EventName OutputDataReceived -Action { if ($EventArgs.Data) { $Event.MessageData.AppendLine($EventArgs.Data) | Out-Null } } -MessageData $stdout | Out-Null
Register-ObjectEvent -InputObject $p -EventName ErrorDataReceived -Action { if ($EventArgs.Data) { $Event.MessageData.AppendLine($EventArgs.Data) | Out-Null } } -MessageData $stderr | Out-Null
$p.Start() | Out-Null
$p.BeginOutputReadLine()
$p.BeginErrorReadLine()

Start-Sleep -Seconds $Seconds

if (-not $p.HasExited) {
    $p.Refresh()
    $h = $p.MainWindowHandle
    if ($h -ne [IntPtr]::Zero) {
        [Win]::SetForegroundWindow($h) | Out-Null
        Start-Sleep -Milliseconds 400
        $r = New-Object Win+RECT
        [Win]::GetClientRect($h, [ref]$r) | Out-Null
        $pt = New-Object Win+POINT
        [Win]::ClientToScreen($h, [ref]$pt) | Out-Null
        $w = $r.R - $r.L; $hh = $r.B - $r.T
        if ($w -gt 0 -and $hh -gt 0) {
            $bmp = New-Object System.Drawing.Bitmap $w, $hh
            $g = [System.Drawing.Graphics]::FromImage($bmp)
            $g.CopyFromScreen($pt.X, $pt.Y, 0, 0, $bmp.Size)
            $bmp.Save("$Out.png", [System.Drawing.Imaging.ImageFormat]::Png)
            $g.Dispose(); $bmp.Dispose()
        }
    }
    $p.CloseMainWindow() | Out-Null
    if (-not $p.WaitForExit(5000)) { $p.Kill(); "KILLED" | Out-File "$Out.killed" }
}
Start-Sleep -Milliseconds 500
"exit=$($p.ExitCode)" | Out-File -Encoding utf8 "$Out.exit"
$stdout.ToString() | Out-File -Encoding utf8 "$Out.stdout.txt"
$stderr.ToString() | Out-File -Encoding utf8 "$Out.stderr.txt"
