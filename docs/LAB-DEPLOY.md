# 机房部署说明

面向学校机房（多台 Windows 桌面，常见还原卡、无企业证书）。个人机器同样适用。

## 1. 两种部署形态

| 形态 | 需要什么 | 具备的功能 | 适用 |
| --- | --- | --- | --- |
| **无驱动（推荐先上）** | 无 | 窗口化、注入 Hook、涂黑/替换画面、键鼠解锁、诊断、日志 | 所有机器，含还原卡、Win7 |
| 带驱动 | 测试模式（部分机器还要关 Secure Boot） | 上面全部 + 内核杀进程、进程自保护、DriverGuard、AV 特征扫描 | 自己可控、能改 BIOS 的机器 |

主程序在启动时尝试加载内嵌驱动，失败不会中断运行，只是在日志里记一条失败并继续。想彻底不尝试，就在 `DzjsTrainer.ini` 里写：

```ini
[JTSettings]
DisableDriver=TRUE
```

## 2. 无驱动模式部署（最快）

在任意一台机器上：

```powershell
# 1) 取 Release 里的 zip，解开
Expand-Archive .\bundle-fix.zip -DestinationPath D:\Deploy

# 2) 写到目标目录，顺便关闭驱动加载、建桌面快捷方式
powershell -ExecutionPolicy Bypass -File scripts\Deploy-Lab.ps1 `
    -SourceDir D:\Deploy `
    -TargetDir "C:\DzjsTrainer" `
    -DisableDriver `
    -CreateShortcut
```

脚本做的事：复制文件、写 ini（`DisableDriver=TRUE`）、建快捷方式、打印核对清单。

然后按机房常规方式（还原卡母盘 / 域下发 / 复制到每台）分发 `C:\DzjsTrainer` 目录。

## 3. 需要驱动时的条件

1. 驱动是自签证书，内核要求 **测试模式**；
2. 开启测试模式：
   ```powershell
   bcdedit /set testsigning on
   ```
   需要重启，桌面右下角会出现"测试模式"水印；
3. **Secure Boot 机器**：`bcdedit` 会报 `protected by Secure Boot policy`，必须先进 BIOS 关闭 Secure Boot；
4. **还原卡**：`testsigning` 写在 BCD（系统盘），还原会把它抹掉。正确做法是
   **在母盘上开启测试模式 → 再做还原点/下发镜像**，而不是每台机器手动开；
5. Win7 x64 没有 Secure Boot，`bcdedit` 一条即可；Win7 x86 没有可用驱动（驱动仅 x64）。

`Deploy-Lab.ps1 -EnableTestSigning` 会替你执行第 2 步并提示重启，不会碰 BIOS。

## 4. 验证清单

每台机器部署后确认：

- [ ] 双击桌面快捷方式，UAC 提权后能进主界面
- [ ] 日志（`DzjsTrainer.log`）里没有"驱动加载成功"也没关系，但不应出现反复重启/崩溃
- [ ] 极域广播开始后，窗口从全屏变成可移动窗口
- [ ] 教师端"打开看"时画面按设置被涂黑/替换（无驱动也生效）
- [ ] 退出确认能正常退出（托盘右键 → 退出）

## 5. 升级

替换 `DzjsTrainer.exe` 与 `TeacherEndpoint\` 即可，ini 保留不变。驱动文件是内嵌的，不需要单独更新（除非要换签名）。

## 6. 已知限制

- 无驱动时教师端若用内核手段强制结束进程，只能靠用户态重启，防不住；
- 高 DPI 目前主界面已清晰，选图/日志导出/退出确认三个独立对话框仍是 96 DPI；
- 教师端（`TeacherEndpoint\JiYuTeacherGui.exe`）是 Python 控制台程序，独立窗口运行，不嵌入主界面；只有私有的 FLTK 版才能嵌入。
