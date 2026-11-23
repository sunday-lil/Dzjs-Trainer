# 审计发现与已知未修项

2026-11-23 对用户态、UI、Hook、内核驱动、更新链做了一轮静态审计。已修的问题记在
[CHANGELOG](../CHANGELOG.md)。这里记录**确认存在但这一轮没有改**的问题，避免下一个人重新踩。

## 一、更新链的信任边界

1. **清单临时文件中转可被替换**
   `DzjsTrainerUpdater.cpp` 里子进程验签后把清单写成 `%TEMP%\DzjsTrainer-manifest-<PID>.txt`，
   父进程读回时不复验票据/来源。同用户下任意进程可在写入与读取之间替换该文件，等于绕过签名。
   修法：父进程自己验签，或改用命名管道/共享内存传递并带票据校验，不落盘。

2. **提权更新器 exe 落在用户可写目录**
   `%TEMP%\DzjsTrainerUpdater\` 下释放后以管理员启动，存在 TOCTOU 替换风险。
   修法：释放到 `%ProgramData%` 并设 ACL（仅 Administrators 写），或改用已签名的常驻更新服务。

3. **探测超时预算不一致**
   `UpdaterManifest.cpp` 最坏 3 端点 × 2 次重试 × 4 个 6s 阶段 ≈ 144s，而父进程只等 60s。
   修法：把重试/超时收进一个总预算，或让父进程等待与预算一致。

## 二、内核驱动的授权模型

4. **KILL / SUSPEND / RESUME / SHUTDOWN 无调用方身份校验**
   驱动记录了 `g_JdrvHeartbeatProcessId`，但执行类 IOCTL 从不比对调用方。
   修法：执行类 IOCTL 校验调用方 PID == 心跳 PID，失败返回 `STATUS_ACCESS_DENIED`。

5. **AV 驱动的扫描类 IOCTL 未走 `AvControllerIsCurrent()`**
   `SCAN_FILE` / `SCAN_PROCESS` / `SCAN_BUFFER` / `SAMPLE_PROCESS_IMAGE` 任何能打开设备的进程都能调，
   后者还能回吐 64 字节内核可达内存。
   修法：与保护类 IOCTL 一致，先校验控制端身份。

6. **卸载令牌不校验机器绑定 / 有效期 / 防重放**
   `UnloadToken64.c` 只看 size/magic/version/action 与签名，`machineHash`、`notBefore/notAfter`、
   `nonce` 都没用上。合法令牌可跨机、过期后、重复使用。
   修法：实现这三项校验，并在驱动侧记录已用 nonce。

7. **注册表保护不放行 SCM**
   `RegistryProtection64.c` 对删除/改名/写值一律拒绝，`sc delete`、服务键清理、卸载流程都会被自己挡住。
   修法：放行 services.exe（或 PID 4）对本驱动服务键的操作，其余仍拒绝。

8. **卸载与在飞扫描的竞态**
   `JiYuAvKernel` 在 `ARM_UNLOAD` 时只把 `DriverUnload` 设上，没有 `IoReleaseRemoveLockAndWait`；
   扫描线程卡在 2GB 用户空间时 `sc stop` 会卸掉仍在执行的镜像。
   修法：引入 RemoveLock，卸载前等待所有在飞请求。

## 三、其它

9. `JiYuAvKernel/Scanner.c` 附身前不检查目标是否已终止，`KeStackAttachProcess` 对正在退出的进程会挂死。
10. `DzjsTrainerDriver/legacy-x86/` 用 `sizeof(PKAPC_STATE)` 分配 APC 状态（指针大小），若复活该目录必崩；
    它当前不参与构建，仅作历史保留。
11. 预览图链路已加 8192 边长 / 4000 万像素上限，但仍是逐像素 `GetPixel/SetPixel`；更彻底的做法是
    解出 `Gdiplus::Bitmap` 后用 `LockBits` 整块处理。

## 修复优先级建议

- 先修 4、5、6（内核授权，影响面最大）
- 再修 1、2（更新链提权面）
- 7、8、9 需要驱动侧结构调整，建议单独 PR
