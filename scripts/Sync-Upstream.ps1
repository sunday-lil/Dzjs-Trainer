# 从"上游最新的公开提交"开工作分支，避免 PR 因为基线落后被退回。
#
# 上游的公开进度不一定在 main 上：本项目发版常常先落在 feat/* 分支再打 tag，
# 例如 v1.0.6 在 feat/edu-compat 上，比 main 新。所以这里不写死 main，而是
# 在 main、feat/*、release/* 和所有 tag 里挑提交时间最新的那个。
#
# 用法：
#   powershell -File scripts\Sync-Upstream.ps1 -Name fix/some-bug
#   powershell -File scripts\Sync-Upstream.ps1 -Name fix/some-bug -Remote origin
#   powershell -File scripts\Sync-Upstream.ps1 -Name fix/some-bug -Base origin/main
#
# 行为：fetch → 选出最新基线 → 从它建分支（已存在则 rebase 到它）→ 打印基线信息。

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [string]$Remote = 'origin',
    [string]$Base = '',
    [switch]$NoRebase
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    # 网络抖动（尤其访问 GitHub）很常见：拿不到就当用本地已有的 ref，
    # 但要明确提示基线可能是旧的。
    $fetchOk = $true
    try { git fetch $Remote --tags --prune 2>&1 | Out-Host } catch { $fetchOk = $false }
    if (-not $fetchOk -or $LASTEXITCODE -ne 0) {
        Write-Host "警告：fetch $Remote 失败，将使用本地已有的 ref；基线可能不是最新的。" -ForegroundColor Yellow
    }

    if (-not $Base) {
        # 候选：main、feat/*、release/*、以及所有 tag 指向的提交
        $refs = @()
        foreach ($pattern in @("refs/remotes/$Remote/main", "refs/remotes/$Remote/feat/*", "refs/remotes/$Remote/release/*", "refs/tags/*")) {
            $refs += git for-each-ref --sort=-committerdate --format='%(refname:short)|%(committerdate:iso8601)|%(objectname:short)' $pattern
        }
        $best = $refs |
            Where-Object { $_ -and $_ -notmatch '\^\{\}$' } |
            ForEach-Object {
                $parts = $_ -split '\|'
                [pscustomobject]@{ Ref = $parts[0]; Date = [datetime]$parts[1]; Sha = $parts[2] }
            } |
            Sort-Object Date -Descending |
            Select-Object -First 1
        if (-not $best) { throw "在 $Remote 上找不到任何候选基线" }
        $Base = $best.Ref
        Write-Host ("最新公开基线：{0}  ({1:yyyy-MM-dd HH:mm}, {2})" -f $best.Ref, $best.Date, $best.Sha) -ForegroundColor Cyan
    }
    else {
        Write-Host "使用指定基线：$Base" -ForegroundColor Cyan
    }

    $baseSha = git rev-parse --short $Base
    Write-Host ("基线提交：{0}" -f $baseSha)

    $existing = git rev-parse --verify --quiet "refs/heads/$Name"
    if ($existing) {
        git switch $Name | Out-Host
        if (-not $NoRebase) {
            git rebase $Base 2>&1 | Out-Host
            if ($LASTEXITCODE -ne 0) {
                Write-Host "rebase 有冲突：解决后 git rebase --continue，或 git rebase --abort 重来" -ForegroundColor Yellow
                exit 1
            }
        }
    }
    else {
        git switch -c $Name $Base | Out-Host
    }

    Write-Host ""
    Write-Host "准备就绪。提 PR 前再确认一次基线没变：" -ForegroundColor Green
    Write-Host "  git fetch $Remote --tags && git rebase $Base"
    Write-Host "  git log --oneline $Base..HEAD"
}
finally {
    Pop-Location
}
