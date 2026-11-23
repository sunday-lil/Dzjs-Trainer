# 机房部署：复制文件、写配置、建快捷方式，可选开启测试签名。
#
# 只动目标目录和 ini，不会碰系统其它位置；除 -EnableTestSigning 外不需要管理员
# （写 Program Files 或建快捷方式时才需要）。
#
# 用法：
#   powershell -File scripts\Deploy-Lab.ps1 -SourceDir D:\Deploy -TargetDir C:\DzjsTrainer -DisableDriver -CreateShortcut
#   powershell -File scripts\Deploy-Lab.ps1 -SourceZip D:\bundle-fix.zip -TargetDir D:\DzjsTrainer -DisableDriver
#   powershell -File scripts\Deploy-Lab.ps1 -SourceDir ... -TargetDir ... -EnableTestSigning

[CmdletBinding(DefaultParameterSetName = 'Dir')]
param(
    [Parameter(Mandatory = $true, ParameterSetName = 'Dir')][string]$SourceDir,
    [Parameter(Mandatory = $true, ParameterSetName = 'Zip')][string]$SourceZip,
    [Parameter(Mandatory = $true)][string]$TargetDir,

    [switch]$DisableDriver,
    [switch]$CreateShortcut,
    [switch]$EnableTestSigning,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

function Write-Section($text) { Write-Host ""; Write-Host "== $text ==" -ForegroundColor Cyan }

# --- 准备源目录 --------------------------------------------------------------
if ($PSCmdlet.ParameterSetName -eq 'Zip') {
    if (-not (Test-Path -LiteralPath $SourceZip)) { throw "找不到 zip：$SourceZip" }
    $unpack = Join-Path $env:TEMP ("dzjs-" + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $unpack | Out-Null
    Expand-Archive -LiteralPath $SourceZip -DestinationPath $unpack -Force
    # zip 里可能带一层顶层目录
    $inner = Get-ChildItem -LiteralPath $unpack -Directory | Select-Object -First 1
    $SourceDir = if ($inner -and (Test-Path (Join-Path $inner.FullName 'DzjsTrainer.exe'))) { $inner.FullName } else { $unpack }
}

$sourceExe = Join-Path $SourceDir 'DzjsTrainer.exe'
if (-not (Test-Path -LiteralPath $sourceExe)) {
    throw "源目录里没有 DzjsTrainer.exe：$SourceDir"
}

# --- 复制 --------------------------------------------------------------------
Write-Section "复制到 $TargetDir"
if ((Test-Path -LiteralPath $TargetDir) -and -not $Force) {
    $existing = Get-ChildItem -LiteralPath $TargetDir -ErrorAction SilentlyContinue
    if ($existing) { Write-Host "目标目录已有内容，继续覆盖（-Force 可跳过此提示）" -ForegroundColor Yellow }
}
New-Item -ItemType Directory -Path $TargetDir -Force | Out-Null
Copy-Item -Path (Join-Path $SourceDir '*') -Destination $TargetDir -Recurse -Force
Write-Host "已复制 $((Get-ChildItem -LiteralPath $TargetDir -Recurse -File).Count) 个文件"

# --- ini ---------------------------------------------------------------------
$iniPath = Join-Path $TargetDir 'DzjsTrainer.ini'
if (-not (Test-Path -LiteralPath $iniPath)) {
    # 与打包脚本一致：ini 用无 BOM UTF-8，GetPrivateProfileString 才能读到第一段。
    [IO.File]::WriteAllText($iniPath,
        "[JTArgeement]`r`nFirstUse=TRUE`r`nArgeed=FALSE`r`n`r`n[JTSettings]`r`nCaptureExclusion=1`r`n",
        (New-Object System.Text.UTF8Encoding($false)))
}

if (-not ('IniWriter' -as [type])) {
    Add-Type @"
using System.Runtime.InteropServices;
public static class IniWriter {
    [DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool WritePrivateProfileString(string section, string key, string value, string file);
}
"@
}

function Set-IniValue([string]$section, [string]$key, [string]$value) {
    # 用 Win32 API 而不是逐行拼接：不会因为 BOM、重复段或大小写把 ini 写坏。
    if (-not [IniWriter]::WritePrivateProfileString($section, $key, $value, $iniPath)) {
        throw "写入 ini 失败：$iniPath [$section] $key"
    }
}

if ($DisableDriver) {
    Write-Section "关闭驱动加载（无驱动模式）"
    Set-IniValue 'JTSettings' 'DisableDriver' 'TRUE'
    Write-Host "已写入：$iniPath -> [JTSettings] DisableDriver=TRUE"
}

# --- 快捷方式 ----------------------------------------------------------------
if ($CreateShortcut) {
    Write-Section "创建桌面快捷方式"
    $desktop = [Environment]::GetFolderPath('CommonDesktopDirectory')
    if (-not $desktop) { $desktop = [Environment]::GetFolderPath('Desktop') }
    $link = Join-Path $desktop 'Dzjs Trainer.lnk'
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($link)
    $shortcut.TargetPath = Join-Path $TargetDir 'DzjsTrainer.exe'
    $shortcut.WorkingDirectory = $TargetDir
    $shortcut.IconLocation = (Join-Path $TargetDir 'DzjsTrainer.exe') + ',0'
    $shortcut.Description = 'Dzjs Trainer'
    $shortcut.Save()
    Write-Host "已创建：$link"
}

# --- 测试签名 ----------------------------------------------------------------
if ($EnableTestSigning) {
    Write-Section "开启测试签名"
    $secureBoot = $null
    try { $secureBoot = Confirm-SecureBootUEFI } catch { $secureBoot = $null }
    if ($secureBoot -eq $true) {
        Write-Host "Secure Boot 已开启：bcdedit 会被固件拒绝。请先进入 BIOS 关闭 Secure Boot，再运行本选项。" -ForegroundColor Yellow
    } else {
        & bcdedit /set testsigning on | Out-Host
        if ($LASTEXITCODE -ne 0) {
            # $ErrorActionPreference 不管原生程序退出码，以前会照样打印"已设置"。
            Write-Host "bcdedit 失败（exit $LASTEXITCODE）：请确认以管理员运行，且固件没有锁定该启动项。" -ForegroundColor Red
        }
        else {
            Write-Host "已设置 testsigning=on，需要重启生效。" -ForegroundColor Yellow
        }
        Write-Host "还原卡机器请务必在母盘上执行并重建还原点，否则重启后会被还原。" -ForegroundColor Yellow
    }
}

# --- 清单 --------------------------------------------------------------------
Write-Section "部署结果"
Write-Host "目标目录：$TargetDir"
foreach ($name in @('DzjsTrainer.exe', 'DzjsTrainer.ini', 'TeacherEndpoint\JiYuTeacherGui.exe')) {
    $p = Join-Path $TargetDir $name
    $state = if (Test-Path -LiteralPath $p) { 'OK' } else { '缺失' }
    Write-Host ("  {0,-36} {1}" -f $name, $state)
}
Write-Host ""
Write-Host "下一步：双击 DzjsTrainer.exe（管理员），确认广播能变窗口；详细核对项见 docs\LAB-DEPLOY.md"
