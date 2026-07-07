[English](../en/submodule-update.md)

# 子模块更新说明

本仓库要求上游 CANopenNode 以仓库根目录下 `CANopenNode` git 子模块的形式存在。

## 1. 克隆并拉取子模块

新拉取仓库时：

```sh
git clone --recursive <repo-url> canopennode-rtt
cd canopennode-rtt
```

等价的两步形式：

```sh
git clone <repo-url> canopennode-rtt
cd canopennode-rtt
git submodule update --init --recursive
```

## 2. 确认子模块存在

在仓库根目录执行：

```sh
test -d CANopenNode/301 && echo "CANopenNode submodule is present"
```

构建需要以下目录中的源文件：

```text
CANopenNode/301/
CANopenNode/303/
CANopenNode/304/
CANopenNode/305/
CANopenNode/309/
CANopenNode/storage/
CANopenNode/extra/
```

虽然只有启用的 Kconfig 功能组会参与编译，但子模块 checkout 本身应保持完整和一致。

## 3. 更新仓库和子模块

将 wrapper 和子模块更新到父仓库记录的子模块版本：

```sh
git pull
git submodule update --init --recursive
```

如果父仓库更新了记录的 CANopenNode submodule commit，该命令会将 `CANopenNode/` 移动到对应 commit。

## 4. 有意更新 CANopenNode

如果需要将子模块移动到新的上游版本，应作为明确源码变更处理：

```sh
cd CANopenNode
git fetch origin
git checkout <target-commit-or-branch>
cd ..
git status
```

随后应构建和测试受影响的 Kconfig 组合，再提交新的 submodule pointer。

更新 CANopenNode 后建议验证：

1. 默认软件包构建。
2. SDO server + segmented transfer。
3. PDO/SYNC 开启组合。
4. 产品使用 commissioning 时的 LSS slave。
5. 产品使用的 storage backend。
6. `RT_USING_ULOG` 开启时的 debug logging 配置。
7. 产品自定义 OD 构建。

## 5. 锁定策略

产品固件建议使用固定 submodule commit，而不是跟踪移动分支。固定 commit 有利于可复现构建，也便于审查 Kconfig 与源码兼容性。

分支 checkout 适合开发阶段；发布前应转换为经过审查的 commit pointer。

## 6. 常见错误

### `CANopenNode submodule is missing`

原因：`CANopenNode/301` 不存在。

修复：

```sh
git submodule update --init --recursive
```

该命令应在仓库根目录执行。

### CANopenNode 源文件缺失

原因：当前 Kconfig 功能启用了某个源文件，但当前子模块 checkout 中没有该文件。

修复：

1. 确认子模块已经初始化。
2. 确认子模块 commit 与 wrapper 版本匹配。
3. 关闭相关 Kconfig 选项，或同时更新 wrapper 和 submodule。

### trace 构建路径被请求

trace 选项默认被刻意设为不可用。在 `CO_trace.c/.h` 适配本包当前使用的 SDO server 和 OD APIs 前，不要强行启用 trace。

### `CANopenNode/` 内存在本地修改

更新子模块前先检查本地修改：

```sh
cd CANopenNode
git status
```

除非明确要丢弃或按子模块工作流提交，否则不要覆盖子模块内的本地修改。
