# 和上游对齐

## 为什么需要这个

上游的公开进度**不一定在 `main` 上**。这个项目习惯先在 `feat/*` 分支上迭代，再打 tag：

- `main` 停在 `3d61cc0`（2026-11-21 09:02）
- `feat/edu-compat` 到 `a860269`（2026-11-21 20:32），`v1.0.6` 就落在这条线上

所以"从 `main` 开分支"很容易在提 PR 时被判定基线落后。上次六个 PR 被关闭的理由全都是这个。

## 做法

提 PR 前一律用脚本建分支，它会自己挑**提交时间最新的**公开 ref（`main` / `feat/*` / `release/*` / 所有 tag）：

```powershell
powershell -File scripts\Sync-Upstream.ps1 -Name fix/reg-read-string-units
```

脚本做的事：

1. `git fetch origin --tags --prune`（失败只警告，不中断，但会提示基线可能旧了）
2. 在候选 ref 里按 `committerdate` 选最新的一个，打印出来
3. 分支不存在就从它建；已存在就 `git rebase` 到它
4. 打印"提 PR 前再确认一次"的命令

想指定基线时：

```powershell
powershell -File scripts\Sync-Upstream.ps1 -Name fix/xxx -Base origin/main
```

## 已有分支的处理

```powershell
git switch fix/old-branch
git fetch origin --tags
git rebase origin/feat/edu-compat   # 或脚本挑出来的那个 ref
git push --force-with-lease
```

## 提 PR 前的检查清单

- [ ] `git log --oneline <基线>..HEAD` 只包含你这次的提交
- [ ] `git diff --stat <基线>...HEAD` 是一个文件、十几行
- [ ] 在干净 clone 上复现过问题，且改动前后行为有对比
- [ ] 涉及发布形态（内嵌资源、包结构、签名链）的改动先开 issue 问

## 本地分叉说明

本仓库（`sunday-lil/Dzjs-Trainer`）是社区版，`main` 已经合并到上游 `v1.0.6` 线，并保留上游没有的部分：

- `docs/LAB-DEPLOY.md` + `scripts/Deploy-Lab.ps1`（机房无驱动部署）
- `.github/workflows/`（CI 与发布流水线）
- 目录布局（`include/`、`assets/branding/`、`third_party/`）

这些属于本分支的增值内容，不往上游提；往上游提的只放单点修复。
