# Changelog

All notable changes to Dzjs Trainer are documented in this file.

## [1.0.5.2-dzjsfix2] - 2026-11-23

静态审计一轮后修的缺陷，全部为健壮性/正确性修复，没有新增功能。

### Fixed — 用户态核心

- `UnDecryptJiyuKnock`：按 `Data[0]` 偏移、按 0 字结束且没有长度上限，注册表值被改过就能越界读并写穿
  栈缓冲区；改为显式定界并封顶 34 个 WORD。
- `HandleMessageFromVirus`：`wtf` 分支在只有两段时仍取 `arr[2]`（任意进程可发 `WM_COPYDATA` 触发）。
- `KillProcess`：忽略入参 `pid`，一直按 `_StudentMainPid` 操作；现在参数优先，PID 为 0 时直接失败。
- `ClearFilterdPid`：`erase(it)` 之后仍 `it++`，会跳过元素并在末尾越界。
- `ReadTopDomanPassword`：值短于 6 字符时 `size()-6` 下溢，`substr` 抛异常导致崩溃。
- `CloseSourceDir`：同一个句柄连续 `NtClose` 两次，第二次可能误关被复用的句柄。
- `MRegReadKeyString` / `MRegReadKeyString64And32`：`cbData` 传的是字符数而不是字节数，长值永远读不到；
  `new[]` 配 `delete`，且未保证 NUL 结尾。
- `TrainerWorker`：`hWndMain`、`_CurrentBroadcastWnd`、`_CurrentBlackScreenWnd` 未初始化。
- `DllInjector`：远程线程超时后仍 `VirtualFreeEx`，目标进程可能仍在读路径字符串。

### Fixed — Hook / 内核

- `JdrvHeartbeat`：METHOD_BUFFERED 下请求与响应共用缓冲，先清零响应把 `version` 一起抹掉，
  心跳永远 `STATUS_REVISION_MISMATCH`；改为先取请求字段再清零。
- `JiYuAvKernel`：`DriverObject->MajorFunction` 只填了三个，其余为 NULL，`ReadFile` 等会 call NULL；
  补上兜底分发。
- Hook 卸载：`WaitForDrain` 超时结果只写日志，随后仍释放全部 trampoline；现在超时则不释放。
- `WM_STATUS_APPEND`：任意进程 `SendMessage` 一个地址进来会被 `delete[]`；改为握手值 + 同进程校验。
- `GuardProcess`：注入超时后释放远端路径缓冲，与 `DllInjector` 同类问题，已一并修正。

### Fixed — 更新与 UI

- 回滚：`DeleteFile && MoveFileEx` 短路，删除失败就不还原；改为先 `MOVEFILE_REPLACE_EXISTING`。
- `.old` 清理：任何进程启动就删全部 `.old`，把唯一回滚副本也删了；10 分钟内的不再清理。
- 清单：`version/url/sha256` 未转义，`url` 可注入第二行 `sha256=`；写入转义、解析改为重复键只认第一次。
- 清单响应体没有上限（提权进程里可被无限流打爆内存），加 4 MiB 上限。
- 临时更新包文件名直接拼 `version`（可含 `..`/分隔符），改为白名单字符。
- 错误报告：按 ANSI 读 UTF-8 日志导致中文乱码；固定 32K 缓冲静默截断；`EndDialog` 后才读勾选框，
  重启分支从未执行。
- 教师端：`LayoutTeacherControls` 里 `return;` 使其后的布局全部不可达（刷新/画面/聊天/锁屏按钮永远不可见）；
  宿主框在控制台模式下从不显示；发送按钮挂在 STATIC 上 `WM_COMMAND` 到不了主窗口；关闭时先关管道再等读线程。
- 高 DPI：进程选择器原生输入框按物理坐标定位、绘制按逻辑坐标；教师端/网络页控件未随 `dpiScale` 缩放。
- `StopAvProcessScan`：`INFINITE` 等待会让退出永久卡在 `WM_DESTROY`，改为 5 秒超时。
- 预览图：逐像素处理没有尺寸上限，小体积 JPEG 可声明超大尺寸锁死教师端；加 8192 边长 / 4000 万像素上限。

### Fixed — 脚本

- `Complete-DriverSigning.ps1` 第二次运行会因令牌已存在而中断（缺 `-Force`）。
- `Deploy-Lab.ps1` 不检查 `bcdedit` 退出码，失败也打印"已设置"。
- 打包/部署脚本用 `Set-Content -Encoding UTF8` 写 ini（带 BOM），`GetPrivateProfileString` 读不到第一段。
- `Build-PortableBundle.ps1` 的清理没有白名单，改为只允许 `dist/` 下的目录。

## [1.0.5.2-dzjsfix1] - 2026-11-21

### Added

- Built-in console host for the teacher endpoint: the public build is a console program with no
  FLTK window, so embedding used to end in a 10 s timeout. The child's stdin/stdout are now piped
  into a log view, a command line and a send button inside the network page; an FLTK build still
  gets its real window attached.
- `docs/LAB-DEPLOY.md` and `scripts/Deploy-Lab.ps1`: computer-lab deployment (copy, write
  `DisableDriver`, optional shortcut and test signing, verification checklist).
- `scripts/Sign-Drivers.ps1`: sign both drivers by store thumbprint or PFX, print the publisher and
  thumbprint the client policy needs.
- `docs/EDITION.md`: what this fork changes relative to upstream.
- GitHub Actions: `ci.yml` (repo hygiene, script syntax, hooks x86/x64) and `release.yml`
  (WDK + Boost, drivers, hooks, updater, injector, UI, main, portable bundle).

### Changed

- The process declares Per-Monitor V2 awareness and scales the main window by DPI; previously the
  whole window was bitmap-stretched on scaled displays.
- A driver rejected by the signature policy (577 / 1275) switches the session to no-driver mode and
  logs the remedy instead of only "启动时加载驱动失败".
- The bug report dialog writes its content to `桌面\DzjsTrainer-bugreport-<time>.txt`; it used to
  `Sleep(2000)` and claim the report had been submitted.
- The teacher endpoint is launched without `CREATE_NO_WINDOW` so a console-only build stays usable
  when it cannot be embedded.

### Fixed

- `WM_LBUTTONDBLCLK` never arrived in the media picker (missing `CS_DBLCLKS`), so double-clicking a
  folder did nothing (upstream issue #4).
- `--heartbeat-recovery` was not parsed: the watchdog's replacement instance hit the single-instance
  check and exited silently.
- `TIMER_RERUN` collided with `TIMER_TRAY_REPAIR` (both 21).
- Teacher endpoint path resolution used a fixed `MAX_PATH` buffer and logged nothing on failure; it
  now uses a 32K buffer and logs every candidate with its attribute mask and Win32 error.

## [1.0.5.2] - 2026-11-15

### Added

- The about page and the startup log show the run environment: Windows version with its patch
  number, native architecture, elevation, UI access and session type.

### Changed

- Screen captures and desktop sharing see through the trainer window instead of getting a black
  rectangle. `[JTSettings] CaptureExclusion` in the ini picks the behaviour: 1 see-through
  (default), 2 black block, 0 visible as usual. Systems older than Windows 10 2004 fall back to
  visible, because there the see-through flag renders as a black block.

### Fixed

- The sidebar hover no longer flashes brighter before settling darker in the light theme; both
  ends of the hover gradient now sit on the same side of the sidebar colour.

## [1.0.5.1] - 2026-11-15

### Fixed

- The first "check for updates" click no longer looks dead: the apex host is tried first, each
  request allows 6 seconds instead of 8, and the bundled updater is reused instead of rewritten,
  so no full antivirus scan runs on every click.
- The toast at the bottom of the window follows the theme. It was drawing the light palette in
  dark mode, which is why it always came out near-white.

### Added

- The about page and the update-probe log line show the build date, read from the PE header
  timestamp so incremental builds do not report a stale day.

## [1.0.5] - 2026-11-15

### Added

- The protection page shows a driver-status chip next to "卸载主驱动": whether this process holds
  the device handle, whether the service exists and is running, and its name. The sample is cached
  for 1.5 seconds and invalidated on load/unload.
- The exit dialog's verification code is valid for 30 seconds. When it expires a new six-digit code
  is drawn, whatever has been typed is cleared, and a `27 秒后刷新` countdown sits at the right end
  of the code row. Only the countdown rectangle repaints each second.
- Four hooks close coverage gaps: `PrintWindow`, `GetDIBits` and `StretchBlt` in the display domain,
  and `SetWindowsHookExW` in the input domain. They reuse `AllowMonitor` and `AllowControl`.
- Image replacement runs on the generic GDI hooks instead of only inside JiYu's own encoder, so
  `StretchBlt`, `PrintWindow` and `GetDIBits` all return the chosen image. `GetDIBits` paints
  24- and 32-bpp buffers and keeps the solid fill for 8/16-bpp.

### Changed

- Both keys behind driver unload authorization were rotated: the ECDSA P-256 pair that signs the
  token and the AES-256 key that wraps it. Old tokens are rejected by the new driver.
- Image replacement is no longer confined to JiYu's own processes. Inject any program and its screen
  captures come back as the chosen image. `BitBlt` still requires a `SRCCOPY` from a desktop DC into
  a memory DC, so a host's own drawing is untouched. `[CustomInject] Mode=minimal` still skips the
  display domain, and replacement does not work in that mode.
- The blurred desktop behind the exit dialog was rendering as a flat dark green field; all three
  tinted layers are now neutral dark grey, and the blur is tightened to 320×180 at radius 3.
- The exit dialog's full-screen surface no longer carries `WS_EX_DLGMODALFRAME` or `WS_BORDER`,
  which drew a 1px system frame around the dimmed wallpaper.
- The exit dialog's code field sits inside a self-drawn 1px rounded outline instead of the native
  3D sunken border, and its fill matches the panel.
- The exit dialog's inner gaps are a uniform 16px; the error line stays tight against the hint.
- The exit dialog's two buttons are registered for hover tracking, and the dialog draws with
  antialiased geometry and text.
- The exit dialog's panel geometry is a single block of constants shared by `PaintExitConfirm` and
  `WM_CREATE`.
- The wallpaper blur is separable, one horizontal pass then a vertical one, cutting the sample count
  from 2.62M to 1.04M.
- The update flow lives entirely in the standalone `DzjsTrainerUpdater.exe`; the About page extracts
  the embedded updater and starts it.
- The updater checks the published version, downloads the package, verifies its SHA-256, and waits
  for the main program to close before replacing it.
- The updater runs from the install directory instead of `%TEMP%` and requests elevation itself, so
  a declined prompt is reported instead of being treated as success.
- Certificate validation is enabled on the update channel.
- Every hook is record-only by default: a detour with its switch off writes one `RECORD` line and
  calls the original. Two switches are new, "拦截远程窗口操作" (`BlockWindowOps`) and
  "拦截极域驱动访问" (`BlockDriverAccess`). Existing installations are migrated once with
  `EnforceMode=record`.
- The window theme follows the Windows app theme, title bar included, and repaints on
  `WM_SETTINGCHANGE`. Clicking the theme button switches to a manual choice for the rest of the
  session.

### Fixed

- The driver signature policy rejected the program's own embedded driver. The pin still carried the
  old test certificate (`JiYuTrainerDriver-Yunsjxh`) while the release driver was signed with
  `DzjsTrainerDriver By:Yunsjxh`. The pin now holds the current subject and thumbprint, and the
  decision checks the trust status before consulting it.
- `Build-DzjsTrainerRelease.ps1` derived the pin with `Subject -replace '^CN='`, which wrote the
  whole distinguished name into the pin. It now uses `GetNameInfo(SimpleName)`, the same call the
  runtime makes.
- `GuardTerminator` waited the full 60 seconds for a guard process that was not installed, then
  logged an error every cycle. It returns immediately when nothing is found, and a guard that ends
  without being respawned is a warning.
- Unloading custom injection at exit opened processes that had already terminated and reported
  `20002` / `0xC000000B` for each. Gone targets are now skipped.
- The process picker's search field could not be typed into under a Chinese input method and showed
  no caret. It is now a real `EDIT` child window with a cue banner, so focus, caret, selection, the
  clipboard, the context menu and IME composition all come from the control. `EN_CHANGE` feeds the
  filter, and a subclass forwards the keys the list owns plus the wheel. `Tab` now moves the focus
  ring instead of being swallowed.
- `CreateDCW(L"DISPLAY")` was a hole in the capture filter: `WindowFromDC` returns no window for a
  device DC, so the path was waved through as a memory DC. The filter now distinguishes window DCs
  from display DCs and requires a memory-DC destination for the latter; the `GetDIBits` branch
  additionally requires a screen-sized bitmap.
- 16-bpp `GetDIBits` targets were filled solid black instead of getting the replacement image. It
  now packs 555 for `BI_RGB` and 565 for `BI_BITFIELDS`, and still falls back to the solid fill for
  other masks.
- Owner-drawn buttons showed a pale halo around their rounded corners, because `WM_CTLCOLORBTN` fell
  through to the default `COLOR_BTNFACE`. `DrawNativeButton` now fills the whole button rectangle
  with the surface colour, and both window procedures answer `WM_CTLCOLORBTN`.
- The exit dialog flashed a black frame on each countdown tick. The whole surface is now rendered
  into an `ExitBackBuffer` and handed to the window in a single `BitBlt`, clipped to `ps.rcPaint`.
- The download URL from the manifest is decoded as UTF-8 and its host converted to punycode;
  decoding it with the ANSI code page corrupted the host.
- `CompareVersions` treats a missing component as zero, so `1.0.5` and `1.0.5.0` no longer compare
  as different.
- The updater's version comparison reads the installed program's file version rather than its own
  build-time constant.

### Removed

- The in-app download window, its progress reporting, and the payload/parent-pid helper path.
- The `AutoUpdate` setting and the startup update timer. The new flow requires the user to close the
  main program, so an unattended check cannot work.
- The "使用帮助" page is gone from the sidebar; its slot is taken by "进程注入", which moves up from
  the last position. The tray menu's "帮助" item shows the same short help message the
  broadcast-window menu already used.

## [1.0.4.3] - 2026-11-14

### Added

- Custom process injection: the new "进程注入" page injects the hooks DLL into a user-specified PID
  or process name, for both 32-bit and 64-bit targets.
- The injection page picks targets from a built-in process list showing icon, PID, image path,
  bitness and whether the process is already injected, with type-to-filter and keyboard navigation.
- The list is multi-select: click toggles a row, Shift+click selects a range, Ctrl+A selects
  everything the filter shows. The action button shows the count and reports failures by name.
- System-critical processes can be injected like any other target and are only tagged "系统"; the
  trainer's own process stays non-selectable.
- CLI switches `-inject <PID|name>` and `-uninject <PID|name>` run one injection or unload at
  startup.
- Injected processes are tracked, hidden from the generic host's status window, and unloaded on exit
  through `VUnloadAll()` before `FreeLibrary`.
- `[CustomInject] Mode=minimal` in `DzjsTrainer.ini` skips JiYu-specific display/driver/remote-run
  hooks for custom hosts; default `full` installs the same 34-item hook table as StudentMain.

### Fixed

- Process icons that carry their own near-white plate no longer show as a white square on the
  selection row; every row draws its icon on a shared light rounded tile.

## [1.0.4.2] - 2026-11-14

### Fixed

- The update check no longer offers the same version forever on a build whose fourth version
  component is non-zero. `ReadOwnFileVersion` formatted the version as three components, so a
  `1.0.4.1` executable reported itself as `1.0.4`. It now returns all four, and the UI trims a
  trailing `.0` for display.

## [1.0.4.1] - 2026-11-14

### Fixed

- In-process update checks no longer fail with `SEC_E_SECPKG_NOT_FOUND`. Schannel and Negotiate
  credentials are acquired in `wWinMain`, before the security package cache is first needed.
- The `请求发送失败 err=183` line no longer reports a stale Win32 error; it reuses the status captured
  at the failing call.

### Changed

- The early-warmup result is carried in process globals and logged together with the pre-probe
  retest, because the log file is not open yet when `wWinMain` runs.

## [1.0.4] - 2026-11-14

### Security

- The update channel no longer trusts the manifest on its own word. The server signs a short-lived
  ticket over the manifest body, and the client verifies the signature, the nonce echo, the issue
  window and the SHA-256 of the body. A ticket older than the last accepted one is rejected.
- Only the public key ships in the client. The previous scheme used a shared HMAC secret, which put
  signing authority into every copy of the program.
- Both paths that write a downloaded package to disk refuse to run without a SHA-256 to check
  against, and refuse to apply it if the ticket check failed.
- The plain-text `checkupdate`, `getupdateinfo`, `getnewver` and `getupdate` endpoints are gone.

### Changed

- The public key is injected at build time from `UpdaterTicketKey.h`, generated by
  `tools/make-ticket-key.py`. A clone carries an example key and refuses tickets signed by the real
  server.
- The executables are packed with UPX.

### Fixed

- The updater reports why a manifest fetch failed instead of the generic "could not get update
  information".

## [1.0.2] - 2026-09-06

### Added

- Added a dedicated homepage/header action for super topmost.
- Added runtime UIAccess relaunch and token verification.

### Changed

- The super-topmost header button now displays `超级置顶：已开启` or `超级置顶：未开启` from the effective UIAccess and topmost window state.
- Updated the application version to `1.0.2` and Windows file/product version to `1.0.2.0`.

### Release Package

- `DzjsTrainer-v1.0.2-win32.zip` contains the Win32 main executable only.
- The package includes the embedded signed primary driver.
- A SHA-256 checksum file is published alongside the archive.

## [1.0.1] - 2026-09-05

### Fixed

- Corrected the confirmation private-desktop cleanup sequence. The application now returns to the previous desktop before closing the confirmation desktop, and retains that desktop when restoration does not succeed.
- Removed an unnecessary foreground-window transition across desktops and guarded edit-control subclass cleanup against an already-destroyed window.
- Added timeout and exception handling around background repository cleanup so an unresponsive cleanup operation cannot block the application indefinitely.
- Changed a transient driver heartbeat communication failure from a process-termination path to a logged degraded state; driver-backed operations can retry on their next use.
- Hardened application-boundary exception handling and crash-report path construction.

### Changed

- Updated the application version to `1.0.1` and Windows file/product version to `1.0.1.0`.

### Release Package

- `DzjsTrainer-v1.0.1-win32.zip` contains the Win32 main executable only.
- The existing signed embedded primary driver is unchanged from the previous release.
- A SHA-256 checksum file is published alongside the archive.

## [1.0.0] - 2026-08-30

- Initial public release of Dzjs Trainer.
