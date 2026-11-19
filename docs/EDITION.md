# 社区修复版说明（本仓库）

本仓库是 [yunsjxh/Dzjs-Trainer](https://github.com/yunsjxh/Dzjs-Trainer) 的社区维护分支，在上游 `1.0.5.2`（commit `90ef800`）基础上做了整理与修复。原作者为「快乐的梦鱼」（JiYu Trainer）与「云散皆星河」（Dzjs Trainer），本分支保留原有 MIT 许可与署名。

## 相对上游的改动

### 仓库与构建

- 目录整理：品牌资源进 `assets/branding/`，共享头进 `include/`，卸载令牌占位进 `resources/`，未参与编译的 `curl` / `MemoryModule` / `XZip` 进 `third_party/`，2019 年的 x86 驱动进 `DzjsTrainerDriver/legacy-x86/`（原样保留，不参与编译）。
- 补齐干净 clone 的构建缺口：32 位注入器 `DzjsTrainer/DllInjector/`、更新令牌封装脚本 `tools/Protect-DriverUnlockToken.ps1`、Boost 通过 `$(BoostIncludeDir)` 传入。
- 新增 `scripts/build-driver-wdk.cmd`、`scripts/build-av-driver-wdk.cmd`：不依赖 MSBuild 的 WDK 工具集，直接用 WDK 头 + `cl /kernel` 编驱动（Build Tools 环境下 `MSB8020` 的兜底）。
- 新增 `scripts/Sign-Drivers.ps1`：按证书或 pfx 签两个驱动，并打印发布者名/指纹，供 `DriverPublisherAllowlist.h`、`DriverSignerPin.h` 对照。
- 新增 `tools/Build-PortableBundle.ps1`：打包「主程序 + VC++ 运行库 + components 散件 + 教师端」，解决官方包只有 exe、纯净系统打不开的问题。
- 新增 `tools/verify_repo.py` 与 `.github/workflows/ci.yml`：检查冲突标记、`.rc`/`.vcxproj` 资源与 `#include` 可解析、误提交产物、根目录条目，并编译 Hook 工程。
- `LICENSE` 恢复被 Git 冲突标记截断的 MIT 正文；清理 `.vcxproj/.filters` 里指向不存在文件（`..\icon1.ico`、mhook/disasm 时代条目）的项目项。

### 运行时修复

- **选图对话框双击失效**（上游 issue #4）：媒体选择器窗口类补 `CS_DBLCLKS`，否则系统只发单击，进入子目录/选中文件的分支不可达。
- **高 DPI 模糊**：进程声明 Per-Monitor V2，主窗口按 DPI 缩放，绘制走 `ScaleTransform`，鼠标/滚轮坐标换算回逻辑坐标，处理 `WM_DPICHANGED`。独立对话框仍是 96 DPI。
- **`TIMER_RERUN` 与 `TIMER_TRAY_REPAIR` 撞号**（都是 21）：前者改为 25。
- **退出确认的 `injectedInput` 恒为 false**：删除该装饰性判断，保留真实的物理键盘检测。
- **教师端启动**：只认 `class=FLTK` 的窗口，公开的 Python 控制台版永远超时。现改为超时后降级为「独立窗口运行」并保留控制台；路径解析改用 32K 缓冲并记录每个候选路径的属性与错误码。
- **预览分片重组**：`total` 来自报文且被直接用于分配，加入 8 MiB 上限。
- **更新器**：清单没有 `sha256` 时不再「跳过校验」，直接拒绝；`update.php` 在清单缺失时返回 503 而不是签发默认空哈希清单。

## 构建

见 [BUILD.md](BUILD.md)。要点：先编驱动和 Hook（含 x64），再编注入助手与更新器，最后编主程序——`.rc` 会把它们作为资源嵌入。

## 驱动签名与系统要求

- 自签驱动需要测试模式；开启 Secure Boot 的机器必须先在 BIOS 关闭 Secure Boot 才能开启测试模式。
- 无驱动模式（`[JTSettings] DisableDriver=TRUE`）下，窗口化、注入、涂黑/替换等用户态功能仍然可用，仅内核杀进程/自保护/AV 扫描缺失。
- Win7 支持需要在 v143 工具集下重编，并用 `dumpbin /imports` 确认产物不含 Win8+ 独占 API（详见 issue #12）。

## 与上游的关系

改动以 PR 形式提交上游（#5 整理、#6 修复、#7 构建与打包），对应 issue #8–#13。本分支会继续独立维护，上游合并后可视情况同步。
