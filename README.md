# Secure Resource Application Core

一个与芯片型号、操作系统和具体产品解耦的 C99 应用状态机。它通过依赖注入连接安全芯片和平台动作，可与不同国内/国外安全芯片驱动组合。

本仓库有意不包含设备名称、客户流程、物理部件名称、UI 协议、生产参数、密钥或公司工程配置。

## 架构边界

- `secure_chip_ops`：认证、通用会话记录、配额读取和单次消费
- `secure_platform_ops`：时间、资源检测、动作启停和状态通知
- `secure_app`：同步事件状态机，不创建线程，不依赖特定 RTOS

芯片驱动只需实现 `secure_chip_ops` 适配器；平台工程负责事件队列、线程、UI 和硬件控制。

## 状态与事件

状态：`DETACHED`、`READY`、`AUTHORIZED`、`ACTIVE`、`ERROR`。

事件：资源接入/移除、开始/停止、周期 Tick 和错误清除。启动动作前先认证并读取配额；每次成功启动只调用一次 `consume_once()`；超时或停止时写入通用完成记录。

## 构建与测试

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 集成示例

```c
struct secure_chip_ops chip = {
    .init = backend_init,
    .authenticate = backend_authenticate,
    .read_record = backend_read_record,
    .write_record = backend_write_record,
    .read_quota = backend_read_quota,
    .consume_once = backend_consume_once,
    .context = &backend_context,
};
```

密钥和不可逆操作策略必须留在部署方的私有适配层。不要把生产密钥编译进本仓库。

## 文档边界

产品到通用接口的映射、内部架构决策和发布审计保存在本地 `private-docs/`，该目录已被 `.gitignore` 排除，不会上传。

## 许可证

仓库级许可证需由代码权利人确认后另行添加。
