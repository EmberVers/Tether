# tools/ 目录说明

Tether 仓库的离线工具集：manifest 生成、多引擎构建、版本桩生成、环境预检，以及 62 个离线 contract 测试。本页是每个工具的用途与调用速查；工具头部 docstring 是更详细的事实源。

**统一前置**：仓库根目录即工作目录（`python tools/<name>.py`）；涉及 UE 编辑器的工具需要编辑器在线并通过 tether 连接（`tether.py ping` 通过）。

## 核心工具（CLAUDE.md 已引用，日常工作流的一部分）

| 工具 | 用途 | 调用方式 |
|---|---|---|
| `gen_manifest.py` | 重新生成 `tether_manifest.json`（AST preflight 与 kwargs-only wrapper 的唯一事实源）。驱动在线编辑器的反射面，产出 manifest、`Content/Python/tether.py` wrapper，并镜像到工程 `Plugins/Tether/Content/Python/`。**新增/改名 UFUNCTION / USTRUCT / 枚举后必跑**——反射变更不会自动更新 manifest，漏跑会让客户端 preflight 以 "no such function" 拒绝调用 | `python tools/gen_manifest.py`（编辑器在线时）；支持 `--out` / `--wrapper-out` / `--no-wrapper` / `--tether` |
| `build_matrix.py` | 对多个引擎版本跑 `RunUAT.bat BuildPlugin`（读 `tools/engines.local.json`，无则 `engines.json`），汇总为 `build_matrix_report.md` | `python tools/build_matrix.py`；`--only 5.7`、`--skip 5.4`、`--list`、`--verbose` |
| `gen_version_stubs.py` | 为 5.7+ 门控的 UFUNCTION 生成 `<Name>_Stubs.cpp`（UHT 不允许反射声明进预处理块，旧引擎用桩体满足链接器）。`--check` 用于 CI 校验桩文件不缺失/不过期 | `python tools/gen_version_stubs.py` 或 `--check` |
| `preflight.py` | 构建前环境自检：Python / MSVC / Windows SDK / 磁盘 / GPU / `UNREAL_EDITOR_EXE` 或 `UE_ROOT` / 插件文件，提前暴露“MSVC 版本不对 / 磁盘不够”类问题 | `python tools/preflight.py`；`--json`、`--strict` |

## 离线 contract 测试（改 preflight / discovery / 协议脚本后必跑）

```bash
python -m unittest discover -s tools -p "test_*.py"
```

`tools/test_*.py` 共 62 个秒级离线测试（不连编辑器）：preflight 行为、discovery 协议、endpoint 身份、modal/health 客户端契约、插件描述符、可取消工作等。详见各 `test_*.py` 文件头。

## 状态标注工具（架构评审判定：一次性/错位，不再纳入日常工作流）

| 工具 | 状态 | 说明 |
|---|---|---|
| `check_engine_drift.py` | **可用但错位** | 对比 `.uproject` 的 `EngineAssociation` 与在线编辑器版本，报告漂移。功能可用，但属于排障工具而非工作流一环，按需手动运行即可 |
| `gen_changelog.py` | **按需** | diff 两个 git ref 的 `tether_manifest.json` 生成 Markdown changelog（`--from v1.0.0` / `--output` / `--json`）。发版时手动用，平时不跑 |
| `add_categories.py` | **一次性迁移工具（已完成使命）** | 曾用于给反射声明批量补 `Category="Tether|<area>"`（UAT BuildPlugin 的 UHT 强制要求）。头文件已全部带 Category，勿再运行 |

## scratch/ — 免维护的一次性调试区

`tools/scratch/` 存放依赖外部库、与仓库无引用关系的一次性调试脚本，**不参与任何工作流，不做维护承诺**：

- `navmesh_pathfinder_3d.py` — 交互式 3D NavMesh 寻路可视化（pyvista 点选起点/终点，穿三角面质心寻路）。依赖 `numpy` + `pyvista`（`pip install numpy pyvista`），输入为 Navigation 库导出的 OBJ。全仓零引用，纯调试用途。

## 配置文件

- `engines.example.json` — `build_matrix.py` 引擎配置的样例；复制为 `engines.local.json`（本地，不入库）后按机器实际路径修改。
