# IR Pass Lesson（compiler_lab）

> 目标：解释当前 `ir_pass` 模块已经做了什么优化、接口怎么用、默认管线怎么配、边界在哪里。

## 导学

这份讲义建议按“接口 -> 文件分工 -> 默认 pipeline -> 各 pass -> 测试”的顺序读：

1. 先看 `include/ir_pass.h`
2. 再看 `src/ir_pass/` 里的分片实现
3. 最后对照 `tests/ir/ir_pass_test.c`

当前这套 pass 还不大，但它已经从“只有一个 fold”长成了一条真的小型优化管线。

---

## 1. 文件定位

- pass 接口：`include/ir_pass.h`
- pass 聚合入口：`src/ir_pass/ir_pass.c`
- pass 核心工具：`src/ir_pass/ir_pass_core.inc`
- temp/use 分析 helper：`src/ir_pass/ir_pass_temp_analysis.inc`
- CFG 分析 helper：`src/ir_pass/ir_pass_cfg_analysis.inc`
- immediate folding：`src/ir_pass/ir_pass_fold.inc`
- temp-constant propagation：`src/ir_pass/ir_pass_const.inc`
- temp copy propagation：`src/ir_pass/ir_pass_copy.inc`
- CFG simplification：`src/ir_pass/ir_pass_cfg.inc`
- dead temp elimination：`src/ir_pass/ir_pass_dce.inc`
- pipeline 调度：`src/ir_pass/ir_pass_pipeline.inc`
- pass 测试聚合入口：`tests/ir/ir_pass_test.c`
- direct-pass 测试分片：`tests/ir/ir_pass_test_direct.inc`
- default-pipeline 测试分片：`tests/ir/ir_pass_test_pipeline.inc`
- Makefile 入口：`make test-ir-pass`

当前关系更像：

$$
\text{AST} \rightarrow \text{IR lowering} \rightarrow \text{verify} \rightarrow \text{optional IR passes}
$$

也就是说，`ir_pass` 不是 lowering 的一部分，而是对“已经构好的 IR”再做额外改写。

这次拆分之后，`src/ir_pass/ir_pass.c` 更像聚合入口：

- 放共享前置声明
- 定义 `IR_PASS_SPLIT_AGGREGATOR`
- 再把各个 `.inc` 分片拼起来

所以现在的结构已经不是“一个大文件包办所有 pass 逻辑”，而是：

`core + temp-analysis + cfg-analysis + fold + const + copy + cfg + dce + pipeline`

其中 `core` 现在除了 error / immediate helper 之外，还放了共享的 `IrValueRef` equality 语义。  
像 fold 里的“`lhs` 和 `rhs` 是不是同一个 IR 值”，以及 CFG pass 里的“两个 `ret` 返回的是不是同一个值”，现在都复用这条公共 helper，而不是各写一套局部比较逻辑。

这次拆分后，`analysis` 也明确分成了两层：

- `ir_pass_temp_analysis.inc`
  负责 temp 相关事实，比如 `use_counts`、唯一 definition/use-site、copy facts、known constant facts
- `ir_pass_cfg_analysis.inc`
  负责 CFG 相关事实，比如 `reachable` 和 `predecessor_counts`

这样 `ir_pass_cfg.inc` 现在更专注于“改图”，而不再把“先算 reachable / predecessor”也塞在同一个文件里。

测试这边也做了同样风格的拆分：

- `tests/ir/ir_pass_test.c`
  现在更像测试聚合入口和共享 helper 容器
- `tests/ir/ir_pass_test_direct.inc`
  主要放“直接调用某个 pass”的回归
- `tests/ir/ir_pass_test_pipeline.inc`
  主要放“跑 default pipeline 整体效果”的回归

所以现在 `ir_pass` 这一块已经是“实现按 pass/analysis 拆分，测试也按 direct-vs-pipeline 拆分”的对称结构。

---

## 2. 对外接口

`include/ir_pass.h` 现在公开了 7 个入口：

- `ir_pass_run_pipeline(...)`
- `ir_pass_run_default_pipeline(...)`
- `ir_pass_fold_immediate_binary(...)`
- `ir_pass_propagate_temp_constants(...)`
- `ir_pass_propagate_temp_copies(...)`
- `ir_pass_simplify_cfg(...)`
- `ir_pass_eliminate_dead_temp_defs(...)`

还有一个 pass 描述结构：

```c
typedef int (*IrPassRunFn)(IrProgram *program, IrError *error);

typedef struct {
    const char *name;
    IrPassRunFn run;
} IrPassSpec;
```

所以当前 pass framework 的模型非常直接：

- 一个 pass 就是 `int run(IrProgram *, IrError *)`
- pipeline 就是 `IrPassSpec[]`
- 逐个执行，遇到失败立即停止

这里还要注意一层边界：

- `ir_pass_temp_analysis.inc` 和 `ir_pass_cfg_analysis.inc` 里已经有一批可复用的 analysis helper
- 但它们现在还只是内部 helper，不是公开的 analysis pass API

所以当前“analysis”主要还是 pass 内部共用工具，还没有独立暴露成 `ir_pass_run_xxx_analysis(...)`。

---

## 3. 当前默认 pipeline 做了什么

`ir_pass_run_default_pipeline(...)` 现在是 8 段：

```c
{"fold-immediate-binary", ir_pass_fold_immediate_binary},
{"propagate-temp-constants", ir_pass_propagate_temp_constants},
{"propagate-temp-copies", ir_pass_propagate_temp_copies},
{"fold-immediate-binary", ir_pass_fold_immediate_binary},
{"propagate-temp-constants", ir_pass_propagate_temp_constants},
{"propagate-temp-copies", ir_pass_propagate_temp_copies},
{"simplify-cfg", ir_pass_simplify_cfg},
{"eliminate-dead-temp-defs", ir_pass_eliminate_dead_temp_defs},
```

也就是：

$$
\text{fold} \rightarrow \text{const} \rightarrow \text{copy} \rightarrow \text{fold} \rightarrow \text{const} \rightarrow \text{copy} \rightarrow \text{simplify-cfg} \rightarrow \text{dead-temp-elim}
$$

这条顺序不是随便排的：

- 第一次 `fold` 会把一部分 `binary immediate` 改成 `mov immediate`
- 第一次 `const` 会把“可证明恒等于常量”的 temp 使用点直接改成立即数
- 第一次 `copy` 会把 `mov` 形成的 temp-copy 往后推
- 第二次 `fold` 会吃到前两步新暴露出来的 immediate `binary`
- 第二次 `const` 会继续把更长的 temp 常量链压成立即数 use
- 第二次 `copy` 会继续消掉新出现的 temp-copy use
- `simplify-cfg` 会把已经变成常量条件的分支收直，并删掉因此不可达的块
- 最后的 `dead-temp-elim` 会把已经没人用的纯 temp 定义删掉

所以默认 pipeline 现在不再是“只有一个非常保守的 fold”，而是一条小型、串联式的清理管线。

---

## 4. Pass 1：Immediate Binary Folding

### 4.1 它在干什么

这条 pass 现在做两类事：

- `immediate/immediate` 的安全常量折叠
- 带单边 immediate 的保守恒等归约

第一类最典型的是：

- `add 1, 2 -> mov 3`
- `eq 1, 2 -> mov 0`

第二类最典型的是：

- `a + 0 -> mov a`
- `a - 0 -> mov a`
- `a * 1 -> mov a`
- `a * 0 -> mov 0`
- `a / 1 -> mov a`
- `a % 1 -> mov 0`
- `a & 0 -> mov 0`
- `a & -1 -> mov a`
- `a | 0 -> mov a`
- `a ^ 0 -> mov a`
- `a << 0 -> mov a`
- `a >> 0 -> mov a`

也就是把：

```text
tmp.0 = eq 1, 2
```

改成：

```text
tmp.0 = mov 0
```

### 4.2 当前会折叠哪些运算

现在实际会 fold 的已经不只“纯 immediate/immediate”，而是这 4 组“安全 binary 简化”：

- arithmetic
  - `add`
  - `sub`
  - `mul`
  - `div`
  - `mod`
- bitwise
  - `and`
  - `xor`
  - `or`
- shift
  - `shl`
  - `shr`
- comparison
  - `eq`
  - `ne`
  - `lt`
  - `le`
  - `gt`
  - `ge`

其中：

- 如果 `lhs/rhs` 都是 immediate，就尝试真正算出结果
- 如果只有一边是某些特殊 immediate，就尝试做 identity simplification

### 4.3 当前不会折叠哪些运算

虽然这条 pass 的支持面已经扩大了，但它仍然刻意避开一些会改变语义边界、尤其是会抹掉 UB 检查的形状。

首先，左右操作数都是 immediate 时，下面这些“语义上看起来能算”的形状当前仍然可能不会 fold：

- 有未定义/不安全风险的情况
  - `div` / `mod` 的除数为 `0`
  - `LLONG_MIN / -1`
- 会溢出的算术
  - `add/sub/mul` 溢出
- 不安全的移位
  - 负 shift count
  - shift count 超过位宽
  - 当前实现里对移位还要求 `lhs >= 0`
  - `shl` 还会拒绝会溢出的左移

其次，即使某个代数恒等式在数学上成立，当前也不会一概照做。最重要的例子是：

- 不会把 `0 / x` 折成 `0`
- 不会把 `0 % x` 折成 `0`
- 不会把 `0 << x` 折成 `0`

原因不是“结果通常不对”，而是这些改写会把：

- 除零
- 非法 shift count
- 以及类似 UB / trap 边界

直接抹掉。当前这条 pass 是故意保守的。

所以现在更准确的口径是：

`fold pass 已经覆盖 arithmetic/bitwise/shift/comparison，并且会做一小组保守的 identity simplification；但它会刻意避开可能抹掉 UB/错误边界的改写。`

### 4.4 一张“会折 / 不会折”的小表

当前这条 pass 可以粗略记成下面两组。

会折成“返回原值”的形状：

- `a + 0 -> mov a`
- `a - 0 -> mov a`
- `a * 1 -> mov a`
- `a / 1 -> mov a`
- `a & -1 -> mov a`
- `a | 0 -> mov a`
- `a ^ 0 -> mov a`
- `a << 0 -> mov a`
- `a >> 0 -> mov a`

会折成“直接常量”的形状：

- `a * 0 -> mov 0`
- `a % 1 -> mov 0`
- `a & 0 -> mov 0`

故意不折的代表形状：

- `0 / x`
- `0 % x`
- `0 << x`

这三类最值得记，因为它们很能体现当前 pass 的安全边界：

- 不是“数学上像是能算就都算掉”
- 而是“只有在不会抹掉原本的错误/UB 检查时才归约”

---

## 5. Pass 2：Temp Constant Propagation

### 5.1 它在干什么

这条 pass 会把“已经能证明恒等于某个立即数”的 temp，在使用点直接改成立即数。

最典型的形状是：

```text
tmp.0 = add 1, 2
tmp.1 = add tmp.0, 4
```

如果共享分析能证明 `tmp.0 == 3`，它就会把 use 点改成：

```text
tmp.1 = add 3, 4
```

这样后面的 `fold` 和 `dce` 就能自然继续吃。

### 5.2 它和原来的 copy pass 有什么区别

原来的 copy pass 更擅长：

```text
tmp.2 = mov tmp.1
tmp.1 = mov tmp.0
tmp.0 = mov 3
```

这种 `temp -> temp -> temp -> immediate` 的 copy 链。

新的 constant pass 更强在于它还能处理：

```text
tmp.0 = add 1, 2
tmp.1 = add tmp.0, 4
tmp.2 = add tmp.1, 5
```

这种“binary/mov 混合形成的常量链”。  
也就是说，它不是只追 `mov` alias，而是会追“最终是否可解成常量值”。

### 5.3 它怎么做

共享分析现在在 `ir_pass_temp_analysis.inc` 里会记录：

- `use_counts`
- `temp_copy_values`
- `temp_defs`
- `temp_constant_values`

然后递归解：

- temp 的定义是不是 `mov`
- 或者是不是 `binary`
- 它依赖的输入值能不能继续解成常量

只要一路能证明成立，就会把这个 temp 标成 known constant。

真正做 use-site 改写的是 `ir_pass_const.inc`：

- `mov` 的源值
- `binary` 的 `lhs/rhs`
- `call` 的参数
- `ret` 的返回值
- `br` 的条件

### 5.4 当前边界

这条 pass 的定位仍然很窄：

- 只把 use 点改成立即数
- 不直接删除定义点
- 不自己做 CFG 级数据流传播
- 主要依赖 temp-analysis 里能递归证明出的局部闭包

---

## 6. Pass 3：Temp Copy Propagation

### 5.1 它在干什么

这个 pass 会把“由 `mov` 定义出来的 temp”当成 value alias，再把 use 点改成原值。

最典型的形状是：

```text
tmp.0 = mov x.0
tmp.1 = add tmp.0, 1
ret tmp.1
```

propagation 之后更像：

```text
tmp.0 = mov x.0
tmp.1 = add x.0, 1
ret tmp.1
```

这个 pass 自己不删定义；它只是先把 use 改掉，方便后面的 fold 和 dead-temp-elim 继续收缩。

### 5.2 它怎么做

它的实现分两步：

1. `ir_pass_collect_temp_copy_defs(...)`
   只收集这种定义：
   - `result.kind == IR_VALUE_TEMP`
   - `instruction->kind == IR_INSTR_MOV`

2. `ir_pass_apply_temp_copy_propagation(...)`
   把这些 temp-copy 用到的地方改写成真正值源

它会改写这些 use 点：

- `mov` 的源值
- `binary` 的 `lhs/rhs`
- `call` 的参数
- `ret` 的返回值
- `br` 的条件

### 5.3 它支持链式解析

当前实现不是只看一跳 copy，而是会沿着链一直解：

```text
tmp.0 = mov a.0
tmp.1 = mov tmp.0
tmp.2 = mov tmp.1
```

最终 `tmp.2` 可以被解析成 `a.0`。

这部分核心逻辑在 `ir_pass_resolve_temp_copy_value(...)`。  
它是“反复沿 temp replacement 往前找”，不是建一张完整 def-use 图。

### 5.4 当前边界

这条 pass 现在的定位是“temp-copy propagation”，不是完整 copy propagation：

- 只传播由 `mov` 定义出来的 temp
- 不会删除定义点
- 不会做跨语义的重写
- 不会做更强的常量传播或全函数数据流求解

---

## 7. Pass 4：CFG Simplification

### 7.1 它在干什么

这条 pass 优化的不是单条表达式值，而是函数的控制流图（CFG）形状。

它当前已经覆盖 4 类很窄、很稳的 CFG 清理：

- 把常量条件分支改成直接跳转
- 线程化空 `jmp` trampoline block
- 把 `jmp -> empty ret block` 直接折成当前块自己的 `ret`
- 把 `same-return diamond` 直接折成当前块自己的 `ret`
- 合并“单前驱 jump-block”

最典型的形状是：

```text
br 1, bb.1, bb.2
```

会被改成：

```text
jmp bb.1
```

如果一个 branch 的 `then_target` 和 `else_target` 本来就相同，它也会直接压成 `jmp`。

### 7.2 它怎么做

实现放在 `ir_pass_cfg.inc`，大致分两步：

1. 先扫描所有 block terminator
   - `br imm, then, else` 会改成 `jmp`
   - `br x, bb.1, bb.1` 也会改成 `jmp bb.1`
   - `jmp bb.t` 如果 `bb.t` 是空 `jmp` trampoline，就把目标线程化到最终 target
   - `jmp bb.t` 如果 `bb.t` 是无指令的 `ret` 块，就直接改成当前块自己的 `ret`
   - `br cond, bb.t, bb.e` 如果两个后继都是无指令 `ret`，且返回同一个非-temp 值，也会直接改成当前块自己的 `ret`
   - `jmp bb.t` 如果 `bb.t` 只有这一个前驱，就把 `bb.t` 的指令和 terminator 直接并回当前块

2. 再从 `bb.0` 出发做一次可达性遍历
   - 只保留还能走到的 block
   - 重建 block id
   - 把 `jmp/br` 的目标 block id 一起 remap

所以这条 pass 的本质是：

`先收直已经恒定或纯中转的控制流边，再把死掉的 CFG 分支块清掉。`

### 7.3 为什么它放在 DCE 前面

因为常量折叠和常量传播先把 branch condition 压成立即数之后，CFG simplification 才真正看得出来：

- 哪条边是恒真
- 哪条边永远走不到

而把不可达块删掉、把单前驱 jump-block 并回去之后，后面的 dead-temp-elim 才更容易把残留的纯 temp 定义继续清掉。

### 7.4 这轮新增的 3 个细化动作

前面最早那版 `simplify-cfg` 主要只会做“常量 branch 收直 + dead-block compaction”。  
现在又往前推了 3 步，但都还停在 verifier 友好的安全边界里。

第一类是空 trampoline threading：

```text
bb.1:
  jmp bb.3

bb.3:
  jmp bb.7
```

会直接把前驱改成：

```text
bb.1:
  jmp bb.7
```

第二类是 `jmp -> ret` folding：

```text
bb.1:
  jmp bb.3

bb.3:
  ret 1
```

如果 `bb.3` 没有任何普通指令，只是一个空 `ret` 块，就会直接压成：

```text
bb.1:
  ret 1
```

第三类是单前驱 jump-block merge：

```text
bb.1:
  jmp bb.3

bb.3:
  tmp.0 = add a.0, 1
  ret tmp.0
```

如果 `bb.3` 只有这一个前驱，那么它会被直接并回：

```text
bb.1:
  tmp.0 = add a.0, 1
  ret tmp.0
```

这里故意限制成“单前驱”很重要，因为这样不需要引入 phi、块参数或更复杂的合流修复；它只是把一个私有 successor block 直接内联回前驱。

第四类是 `same-return diamond` folding：

```text
bb.0:
  br cond, bb.1, bb.2

bb.1:
  ret 7

bb.2:
  ret 7
```

如果两个后继块都：

- 没有普通指令
- terminator 都是 `ret`
- 返回的是同一个非-temp 值

那么当前块会直接改成：

```text
bb.0:
  ret 7
```

这可以看成是把原来的 `jmp -> ret` 又往前推了一步：  
以前只能消“先跳到 ret 块再返回”，现在连“分支两边都返回同一个值”的小 diamond 也能直接塌掉。

这里故意要求“同一个非-temp 值”也很关键：

- immediate / local / global 的同值比较是稳定的
- temp 可能依赖不同路径上的定义来源

所以当前实现宁可保守一些，也不去折 `ret temp.n` 这类 potentially path-sensitive 的情况。

---

## 8. Pass 5：Dead Temp Elimination

### 6.1 它在干什么

`ir_pass_eliminate_dead_temp_defs(...)` 会删除：

- 结果写到 `temp`
- 这个 temp 后面从未被使用
- 并且这条指令是纯的

当前可删除的纯指令只有：

- `IR_INSTR_MOV`
- `IR_INSTR_BINARY`

所以像：

```text
tmp.0 = mov 1
tmp.1 = add 2, 3
ret 0
```

如果 `tmp.0` 和 `tmp.1` 都没人再用，这两条都能被删。

### 6.2 它怎么做

当前实现不是靠完整依赖图或 worklist，而是靠“反复扫 + use-count”：

1. 先统计每个 temp 的 use 次数
2. 删除 `use_count == 0` 的纯 temp 定义
3. 如果这轮删掉了东西，就再来一轮

也就是：

$$
\text{count uses} \rightarrow \text{delete dead defs} \rightarrow \text{repeat until stable}
$$

这版实现的优点是简单、容易写对；代价是如果函数很大，会比基于 def-use/worklist 的实现更慢。

### 6.3 为什么删完还要做 temp 压缩

删除 dead temp def 后，剩下的 temp 编号可能会变稀疏：

```text
tmp.0 = ...
tmp.2 = ...
```

当前 verifier 对 temp 的假设仍然是“连续编号合同”，所以 pass 末尾会跑一次 temp ID 稠密重映射，把它重新压成：

```text
tmp.0 = ...
tmp.1 = ...
```

对应实现是 `ir_pass_compact_function_temp_ids(...)`。

---

## 9. Pipeline 怎么运行

`ir_pass_run_pipeline(...)` 的行为仍然很简单：

1. 先检查 `program` 是否为空
2. 如果 `pass_count > 0`，就要求 `passes` 非空
3. 按顺序跑每个 `IrPassSpec`
4. `name` 为空、空串、或 `run == NULL` 都会直接失败
5. 任意一个 pass 失败，pipeline 立即停止

所以 pipeline 本身仍然是“线性按顺序执行”，不是：

- worklist
- 自动 fixpoint
- `until-no-change` 总调度器

真正会在内部自己迭代到稳定的，当前只有 `dead-temp-elim` 这个 pass 本身。

---

## 10. 当前错误码说明

### 8.1 Pipeline / fold 基础错误

- `IR-PASS-001`：`program == NULL`
- `IR-PASS-002`：`pass_count > 0` 但 `passes == NULL`
- `IR-PASS-003`：某个 `IrPassSpec` 无效
- `IR-PASS-004`：某个 pass 失败但没自己写 error message
- `IR-PASS-005`：`ir_pass_fold_immediate_binary(...)` 收到空 program
- `IR-PASS-006`：某个函数 `block_count > 0` 但 `blocks == NULL`

### 10.2 Dead-temp elimination / temp remap 相关

- `IR-PASS-007`：temp use id 越界
- `IR-PASS-008`：dead-temp elimination 收到空 program
- `IR-PASS-009`：函数 block table 异常
- `IR-PASS-010`：temp analysis capacity overflow
- `IR-PASS-011`：构建 temp analysis OOM
- `IR-PASS-012`：temp remap id 越界
- `IR-PASS-013`：某个 temp 没有 remapped id
- `IR-PASS-014`：构建 temp remap 表 OOM

### 10.3 Temp-copy propagation / temp-constant propagation 相关

- `IR-PASS-015`：temp copy id 越界
- `IR-PASS-016`：temp copy chain 解析深度异常
- `IR-PASS-017`：temp-use table capacity overflow
- `IR-PASS-018`：temp-copy propagation 收到空 program
- `IR-PASS-019`：函数 block table 异常
- `IR-PASS-020`：temp constant id 越界 / temp-copy table 相关分析失败
- `IR-PASS-021`：temp constant resolution cycle detected
- `IR-PASS-022`：temp constant use id 越界
- `IR-PASS-023`：temp definition 记录越界
- `IR-PASS-024`：temp-constant propagation 收到空 program
- `IR-PASS-025`：temp-constant propagation 看到异常 block table

### 10.4 CFG simplification 相关

- `IR-PASS-026`：构建 CFG reachability 栈 OOM
- `IR-PASS-027`：`jmp` 目标 block 越界
- `IR-PASS-028`：`br` 目标 block 越界
- `IR-PASS-029`：CFG 压缩时分配失败
- `IR-PASS-030`：分配压缩后 block 表失败
- `IR-PASS-031`：`jmp` 目标 remap 失败
- `IR-PASS-032`：`br` 目标 remap 失败
- `IR-PASS-033`：CFG simplification 收到空 program

所以这层已经不只是一个“随便改 IR 的 helper”，而是有自己一套完整 guardrail 的 pass 模块。

---

## 11. 和 Verifier 的关系

当前 pass 本身不会自动帮你调用 verifier。

但测试里的推荐用法是：

1. 先 lower 出一份合法 IR
2. 跑某个 pass 或默认 pipeline
3. 再手动跑 `ir_verify_program(...)`

这也是 `tests/ir/ir_pass_test.c` 当前在锁的契约：

`pass 输出的 IR 仍然必须通过 verifier`

另外，当前测试已经不只锁“单轮 pass 后 IR 合法”，还开始锁默认 pipeline 的稳定性：

- 对同一份 IR 连跑两次 `ir_pass_run_default_pipeline(...)`
- 第一轮和第二轮 dump 必须完全一致
- 两轮结果都必须继续通过 verifier

这相当于给默认 pipeline 补了一层 fixed-point / idempotence 护栏。  
它不是要求实现里显式做 `until-no-change`，而是要求“按当前排好的默认管线跑完一轮之后，第二轮不应该再冒出新变化”。

---

## 12. 当前测试锁了什么

`tests/ir/ir_pass_test.c` 现在主要锁 6 件事：

1. 默认 pipeline 确实会减少“可 fold 的 immediate binary 指令”数量
2. 默认 pipeline 会跳过当前未进入 fold 集合的 unsafe/unsupported immediate binary
3. `ir_pass_eliminate_dead_temp_defs(...)` 能直接删除 dead temp defs，且输出仍通过 verifier
4. `ir_pass_propagate_temp_constants(...)` 会把可证明恒定的 temp use 压成立即数
5. `ir_pass_propagate_temp_copies(...)` 会在 fold / const 之后继续减少 temp-copy propagation opportunities
6. `ir_pass_simplify_cfg(...)` 会消掉 immediate branch terminator，并减少 block 数
7. `ir_pass_simplify_cfg(...)` 还会线程化空 trampoline、折叠 `jmp -> ret`、折叠 `same-return diamond`，并合并单前驱 jump-block
8. 默认 pipeline 会在常量条件 CFG case 和 same-return diamond case 上自动完成这类 CFG 收直和 block collapse
9. 默认 pipeline 在 dead-result call 周围清纯计算、重访 CFG 时，仍然必须保留 side-effecting `call`
10. 默认 pipeline 在 multi-def constant / copy 路径上必须保留路径区分，不能把 join 后的值误塌成单一路径事实
11. 默认 pipeline 会减少总指令数，并且最后既不剩 dead temp defs，也不剩 temp-copy propagation opportunities
12. 默认 pipeline 对同一份 IR 连跑两轮后，第一轮和第二轮结果必须一致，而且两轮都继续通过 verifier
13. 非法 `IrPassSpec` 和 `passes == NULL` 这类 pipeline 契约错误会正确报错

所以当前的 `test-ir-pass` 已经不只是“证明有个 fold pass”，而是在锁一条 `fold + const + copy + simplify-cfg + dce` 互相喂结果、还能塌缩 diamond / trampoline / jump-return 这类小 CFG 的优化链。

这里还有一个很重要的测试口径变化：  
pipeline 级 multi-def 回归现在锁的是“语义上的路径区分仍然存在”，而不是“join block 原形必须长成某个固定样子”。因为在 default pipeline 下，CFG 可能合法地被继续收直、合并或改写，只要它没有把多路径事实误提升成全路径唯一事实，就应该视为正确。

---

## 13. 当前边界

如果把现在的 `ir_pass` 模块一句话说清楚：

`它已经是一条真的小型 IR 清理管线：先 fold，再传播常量，再传播 copy，最后删掉死 temp 定义。`

当前明确还没有：

- 全函数 def-use 图
- 全 CFG 常量传播
- 通用 DCE
- CFG 简化 / block merge
- unreachable block 删除
- CSE
- SSA / phi
- verifier 内建到 pass pipeline 末尾

所以它已经不是“只有一个玩具 fold”，但离完整优化器也还有明显距离。
