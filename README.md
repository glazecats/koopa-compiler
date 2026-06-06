# Compiler Lab

这是一个基于 SysY / 类 C 语言的课程编译器项目。项目的最终输出方向是
RISC-V 汇编，同时保留了中间表示观察、性能优化实验和语言扩展实验三条主线。

项目不是单一路径的“能编译即可”实现，而是把前端、分层 IR、SSA、后端和测试
基础设施拆开维护。这样做的目标是让复杂功能可以被逐步打开，也让每个阶段都能
通过 dump、verifier 和 regression test 被观察和验证。

## 主要模式

编译器当前有三个主要模式：

```bash
build/compiler -riscv input.sy -o output.s
build/compiler -perf input.sy -o output.s
build/compiler -extension input.sy -o output.s
```

- `-riscv`：基础交付模式，面向 SysY / 类 C 程序到 RISC-V 汇编的稳定输出。
- `-perf`：性能优化模式，在正确性闭环下运行 ValueSSA、MemorySSA、后端清理和
  family-aware allocator 等优化实验。
- `-extension`：语言扩展模式，保守开放 `defer`、`pair`、`struct`、函数值、
  closure、callable-object IR 和部分浮点扩展。

此外还有一个可叠加的严格返回路径检查开关：

```bash
build/compiler --enforce-all-paths-return-check -riscv input.sy -o output.s
```

它用于更严格地检查非 `void` 函数是否在所有可达控制流路径上返回。

## 构建

默认构建编译器和工具：

```bash
make
make compiler
make tools
```

`make` 默认只构建编译器，不自动运行全量测试。这是为了兼容课程评测中
“先 build 编译器，再由外部工具运行测试”的习惯。

常用工具链包括：

- C 编译器：`clang`
- RISC-V 汇编 / 链接：`clang -target riscv32-unknown-linux-elf`、`ld.lld`
- RISC-V 运行：`qemu-riscv32-static`
- SysY runtime：通常位于 `/opt/lib/riscv32`

## 快速运行一个程序

使用 Makefile 封装的单文件运行入口：

```bash
make run-one SRC=path/to/input.sy MODE=riscv
make run-one SRC=path/to/input.sy MODE=perf
make run-one SRC=path/to/input.sy MODE=extension
```

如果程序需要输入：

```bash
make run-one SRC=path/to/input.sy MODE=riscv IN=path/to/input.txt
```

如果只想编译、汇编和链接，不运行：

```bash
make compile-one SRC=path/to/input.sy MODE=riscv
```

也可以手动走完整链路：

```bash
build/compiler -riscv input.sy -o output.s
clang output.s -c -o output.o \
  -target riscv32-unknown-linux-elf -march=rv32im -mabi=ilp32
ld.lld output.o -L/opt/lib/riscv32 -lsysy -o output
qemu-riscv32-static ./output
```

## 测试

全量测试：

```bash
make test
make check
```

常用专项测试：

```bash
make test-extension-runtime
make test-asan
make test-fanalyzer
make test-strict-warnings
```

扩展语言功能通常会同时锁 semantic / IR / lower IR / compiler driver /
runtime regression。性能优化相关改动则应优先使用真实 runtime A/B，而不是只看
静态指令数或汇编长度。

## 观察中间阶段

项目提供了一组 dump / profile / diagnostic 工具，用于查看编译管线中的中间产物：

```bash
build/tools/dump_middle_stage --extension ir input.sy
build/tools/dump_middle_stage --extension lower-ir input.sy
build/tools/dump_middle_stage --extension value-ssa-default input.sy

build/tools/dump_machine_stage machine-ir input.sy
build/tools/dump_machine_stage select input.sy
build/tools/dump_machine_stage emit input.sy

build/tools/dump_alloc_stage initial input.sy
build/tools/dump_alloc_stage final input.sy
build/tools/diag_allocator_stages input.sy
```

这些工具是项目质量保障的一部分：很多 bug 不需要等到最终程序运行失败才定位，
可以直接在 IR、SSA、allocator 或 machine artifact 上被发现。

## 目录结构

```text
src/        编译器实现
include/    公共头文件
tests/      分层 regression / verifier / runtime tests
tools/      dump、profile、diagnostic 工具
docs/       工程记忆、设计计划和阶段说明
lesson/     压缩版项目讲义和代码阅读路线
reports/    课程报告 TeX 源文件和 PDF
```

更细的阅读入口：

- `docs/README.md`：工程文档索引。
- `docs/NEXT_STEPS.md`：当前路线、阶段状态和执行记录。
- `docs/ENGINEERING_MEMORY.md`：实现约束、稳定事实和踩坑记录。
- `lesson/README.md`：按 core / language / ssa / machine 组织的精炼阅读路线。
- `reports/course_report.pdf`：课程项目报告。

## 项目特色

- 分层 IR：`canonical IR -> lower IR -> ValueSSA / MemorySSA -> machine backend`。
- Verifier-backed pipeline：多个阶段都有结构契约检查，避免错误一路流到后端。
- Family-aware allocator：在图染色分配基础上记录 move/coalesce family、priority、
  preferred color、targeted eviction 和 spill/rewrite 结果。
- 多模式设计：`-riscv` 稳定输出，`-perf` 做优化实验，`-extension` 保守开放语言特性。
- 可观察工作流：dump、regression、runtime test 和课程报告共同记录项目行为。

## 扩展能力边界

`-extension` 已覆盖不少函数值和 closure 形状，包括 top-level function value、
local alias / reassignment、returned callable、local / returned closure、closure 捕获
callable，以及显式 callable-object IR。

但它不是完整的工业级一等函数 runtime。目前仍然保守拒绝一些形状，例如完整 generic
function pointer ABI、callable 存入 global / array / aggregate field 后自然流动、任意深度
CPS / continuation，以及完整 heap escaping closure lifetime 模型。支持的形状会用测试锁住；
暂未支持的形状尽量在 semantic gate 处明确拒绝。

## 课程报告

课程报告位于：

```text
reports/course_report.tex
reports/course_report.pdf
```

报告更系统地介绍了项目目标、三种模式、IR/SSA/backend 分层、性能优化、语言扩展、
strict all-path return 检查、vibe agent 工作流和质量保障措施。
