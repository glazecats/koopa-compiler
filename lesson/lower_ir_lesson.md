# Lower IR Lesson（compiler_lab）

> 目标：解释当前 `lower_ir` 模块为什么存在、它和 canonical IR 的边界差异是什么，以及这版 lower IR 现在已经稳定实现了哪些契约。

## 导学

这份讲义建议按“设计边界 -> 数据模型 -> lowering -> dump/verifier -> 测试”的顺序阅读：

1. 先看 `docs/LOWER_IR_DESIGN.md`，理解为什么 lower IR 是一层新的 downstream IR。
2. 再看 `include/lower_ir.h`，建立 lower IR 的数据模型与 builder API。
3. 再看 `lower_ir_lower_from_ir(...)`，理解 canonical IR 是怎么 serial lowering 到 lower IR 的。
4. 最后对照 `src/lower_ir/` 和 `tests/lower_ir/`，把 dump/verifier 契约锁定下来。

学完你应该能：

1. 能说明 lower IR 和 canonical IR 的职责边界。
2. 能解释为什么 lower IR 要把 value ref 和 slot ref 明确拆开。
3. 能解释 canonical IR 到 lower IR 的最小 lowering 策略。
4. 能看懂它的 dump 和 verifier 规则。

---

## 1. 为什么需要 `lower_ir`

当前编译链的推荐方向已经明确成：

$$
\text{AST} \rightarrow \text{semantic} \rightarrow \text{canonical IR} \rightarrow \text{lower IR} \rightarrow \text{later backend}
$$

这里的 lower IR 不是“把 canonical IR 原地多加几个 opcode”，而是一个新的表示层。

它的核心目标是：

1. 把“值”与“存储位置”拆开。
2. 让 local/global 的读写变成显式 `load_*` / `store_*`。
3. 给后续更低层的 SSA / backend / codegen 预留一个更清楚的边界。

所以它和 canonical IR 的区别不是“更复杂”，而是“更显式”：

- canonical IR 里，`local/global` 还可以直接作为值参与运算
- lower IR 里，算术/比较/call/ret/br 只吃 value
- local/global 只能通过 slot + `load_*` / `store_*` 被读写

这也是 `docs/ir-conventions.md` 里那句 working-memory 约束的落地：

`lower IR splits value refs from slot refs on purpose`

---

## 2. 文件定位

- 接口：`include/lower_ir.h`
- 聚合入口：`src/lower_ir/lower_ir.c`
- serial lowering：`src/lower_ir/lower_from_ir.inc`
- verifier：`src/lower_ir/lower_ir_verify.inc`
- dump：`src/lower_ir/lower_ir_dump.inc`
- regression：`tests/lower_ir/lower_ir_regression_test.c`
- verifier 测试：`tests/lower_ir/lower_ir_verifier_test.c`

当前 `src/lower_ir/lower_ir.c` 还是一个小型聚合入口：

```c
#include "lower_from_ir.inc"
#include "lower_ir_verify.inc"
#include "lower_ir_dump.inc"
```

它本身主要负责：

1. 生命周期与 builder helper
2. 容量增长与字符串 builder 小工具
3. canonical IR -> lower IR 的 serial lowering
4. 指令复制/释放
5. 聚合 verifier 和 dump 分片

所以 lower IR 当前已经不只是 phase-1 skeleton，而是进入了 phase 2 的最小闭环：

- 数据模型已落地
- serial lowering 已落地
- dump/verifier 已落地
- source-lowered regression 已开始覆盖代表性 shape
- lowering 成功前还会自跑一遍 lower-IR verifier

不过这条闭环当前仍然保持得很克制：

- lower IR 只做 serial lowering，不重建 CFG
- 只把 value/storage 边界显式化
- 还没有 lower-IR pass pipeline，也还没进入 SSA / backend

---

## 3. `include/lower_ir.h`：当前数据模型

### 3.1 错误出口 `LowerIrError`

```c
typedef struct {
    int line;
    int column;
    char message[512];
} LowerIrError;
```

风格和 parser / semantic / canonical IR 一致：统一给出位置和错误消息。

---

### 3.2 `LowerIrValueRef` 与 `LowerIrSlotRef`

lower IR 最关键的设计点就是把“值”和“槽位”拆成两套引用。

值引用 `LowerIrValueRef` 只有两类：

- `LOWER_IR_VALUE_IMMEDIATE`
- `LOWER_IR_VALUE_TEMP`

槽位引用 `LowerIrSlotRef` 只有两类：

- `LOWER_IR_SLOT_LOCAL`
- `LOWER_IR_SLOT_GLOBAL`

可以写成：

$$
Value = Imm(c) \mid Temp(t)
$$

$$
Slot = Local(i) \mid Global(g)
$$

这意味着：

- `binary` / `mov` / `call` / `ret` / `br` 只能吃 value
- local/global 不再直接出现在值运算里
- local/global 必须先 `load_*` 成 temp，或通过 `store_*` 接收一个 value

这就是 lower IR 和 canonical IR 最大的边界差异。

---

### 3.3 指令种类

当前 lower IR 指令种类共有 7 个：

- `LOWER_IR_INSTR_MOV`
- `LOWER_IR_INSTR_BINARY`
- `LOWER_IR_INSTR_CALL`
- `LOWER_IR_INSTR_LOAD_LOCAL`
- `LOWER_IR_INSTR_STORE_LOCAL`
- `LOWER_IR_INSTR_LOAD_GLOBAL`
- `LOWER_IR_INSTR_STORE_GLOBAL`

其中：

- `mov` / `binary` / `call` 都是 value-level 指令
- `load_*` / `store_*` 是 slot 边界指令

当前二元算子覆盖：

- arithmetic：`add/sub/mul/div/mod`
- bitwise：`and/xor/or`
- shift：`shl/shr`
- comparison：`eq/ne/lt/le/gt/ge`

terminator 仍然保持 block-based CFG 结构：

- `LOWER_IR_TERM_RETURN`
- `LOWER_IR_TERM_JUMP`
- `LOWER_IR_TERM_BRANCH`

也就是说，当前 lower IR 仍然刻意只改变“value/storage boundary”，不改变 CFG 形状。

---

### 3.4 容器层级

当前 lower IR 容器和 canonical IR 很接近：

$$
Program = (Global^*, Function^*)
$$

$$
Function = (locals, blocks, nextTempId)
$$

$$
BasicBlock = (instructions, terminator?)
$$

其中：

- `LowerIrProgram` 持有 `globals[]` 和 `functions[]`
- `LowerIrGlobal` 现在除了 `name/id` 之外，也带全局初始化元数据
- `LowerIrFunction` 持有 `locals[]`、`blocks[]` 和 `next_temp_id`
- `LowerIrFunction.has_body` 用来区分 `func` 和 `declare`
- `LowerIrBasicBlock` 仍然是“若干普通指令 + 一个 terminator”

所以你可以把 lower IR 看成：

`canonical IR 的 CFG 容器骨架 + 更低层的 value/slot 边界`

---

## 4. Serial Lowering：canonical IR 怎么变成 lower IR

现在 lower IR 已经有正式入口：

- `lower_ir_lower_from_ir(...)`

它的目标不是“重新设计一份更漂亮的 CFG”，而是：

`保留 canonical IR 的 function / block / terminator 形状，只把 slot-valued 用法显式 lower 成 load/store`

当前策略可以概括成两条。

### 4.1 读 slot 时：先插 `load_*`

如果 canonical IR 的这些位置里出现了 `IR_VALUE_LOCAL` / `IR_VALUE_GLOBAL`：

- `binary` 的 `lhs/rhs`
- `call` 的参数
- `ret` 的返回值
- `br` 的条件

那么 lowering 会先在**同一 basic block** 里插入：

- `load_local`
- 或 `load_global`

把 slot 读成一个 temp，然后后续 lower IR 指令只继续消费这个 temp。

### 4.2 写 slot 时：把 `mov` 改成 `store_*`

如果 canonical IR 里有一条：

```text
local/global = mov value
```

那么 lower IR 不会继续保留“result 是 slot”的 `mov`，而是直接改成：

- `store_local slot, lowered_value`
- 或 `store_global slot, lowered_value`

所以像：

```text
a = b + 1;
return a;
```

lower IR 当前会更接近：

```text
tmp.0 = load_local b.1
tmp.1 = add tmp.0, 1
store_local a.0, tmp.1
tmp.2 = load_local a.0
ret tmp.2
```

这就是 phase 2 当前的真实边界：

- 它追求“显式、保序”
- 不追求“最少 load/store”
- 更不会直接做 mem2reg/SSA

另外，`lower_ir_lower_from_ir(...)` 现在在返回成功前会先自跑一遍 `lower_ir_verify_program(...)`。

所以这条 lowering 入口当前不只是“生成 lower IR”，还承担一层生产边界保护：

`如果 lower 出来的结果自己都过不了 lower-IR verifier，就直接在 lowering 边界失败`

### 4.3 runtime-init 元数据也会被带到 lower IR

最近 `LowerIrGlobal` 已经不只是 `name/id`，还会把 canonical IR 里的 global initializer 状态一起带下来：

- `has_initializer`
- `initializer_value`
- `has_runtime_initializer`

所以 lower IR 现在不只是普通函数/块/指令模型，也已经能承接 runtime global initializer 这条线的下游表示元数据。

### 4.4 当前不改什么

这条 serial lowering 当前明确不改：

- CFG 结构
- block 数量
- terminator 连接关系
- canonical IR 的 temp 编号语义

所以更准确地说，它不是“优化式 lowering”，而是：

`value/storage boundary lowering`

---

### 4.5 `ir` 输出和 `lower_ir` 输出具体差在哪

最容易理解的方式，是直接对比同一段程序在两层 IR 里的文本形状。

#### 例 1：`return a;`

canonical IR 更接近：

```text
func f(a.0) {
  bb.0:
    ret a.0
}
```

lower IR 会变成：

```text
func f(a.0) {
  bb.0:
    tmp.0 = load_local a.0
    ret tmp.0
}
```

差异点是：

- canonical IR 允许 `ret` 直接吃 `local`
- lower IR 里的 `ret` 只能吃 value，所以必须先 `load_local`

#### 例 2：`a = b + 1; return a;`

canonical IR 更接近：

```text
func f(a.0, b.1) {
  bb.0:
    tmp.0 = add b.1, 1
    a.0 = mov tmp.0
    ret a.0
}
```

lower IR 会更接近：

```text
func f(a.0, b.1) {
  bb.0:
    tmp.0 = load_local b.1
    tmp.1 = add tmp.0, 1
    store_local a.0, tmp.1
    tmp.2 = load_local a.0
    ret tmp.2
}
```

差异点是：

- canonical IR 里 `b.1` 可以直接参与 `add`
- canonical IR 里 `a.0 = mov ...` 直接表达“写 local”
- lower IR 会把“读 `b`”和“写 `a`”都显式展开成 `load/store`

#### 例 3：`g = a; return g;`

canonical IR 更接近：

```text
func f(a.0) {
  bb.0:
    g.0 = mov a.0
    ret g.0
}
```

lower IR 会更接近：

```text
func f(a.0) {
  bb.0:
    tmp.0 = load_local a.0
    store_global g.0, tmp.0
    tmp.1 = load_global g.0
    ret tmp.1
}
```

这里最能看出 lower IR 的风格：

- `local/global` 在输出里不再是普通 value 操作数
- 它们只会出现在 `load_*` / `store_*` 的 slot 位置

#### 例 4：控制流形状基本不变，但块内会多出显式 load/store

比如 `if (a) x = 1; return x;` 这类程序：

- canonical IR 的重点是 branch / join / `ret`
- lower IR 仍然保留这些 block / terminator 结构
- 但条件里的 `a`、分支里的 `x` 读写，会在各自 block 里 materialize 成 `load_local` / `store_local`

#### 例 5：runtime-init helper 也开始有 lower-IR 自己的终形 authority

现在 runtime global initializer 这条线不再只是 verifier 会抓错，代表性的最终 dump 形状也开始被 lower-IR regression 锁住。

这意味着：

- `__global.init` 会显式用 `store_global` 写 runtime global
- `main` 会通过显式 `load_global` 把结果读回
- 没有 `main` 时，`__program.init` 会继续保持成单次 `call __global.init()` 的 wrapper

所以一句话总结输出差异：

- `ir` 输出更短、更接近源码值流，也更适合人读
- `lower_ir` 输出更长，因为它把 slot 读写时序显式展开了
- 在 startup helper 这条线上，`lower_ir` 也已经开始对最终 dump 终形承担 authority

---

## 5. Builder API：怎么手工构一份 lower IR

当前公开 builder API 主要有这几组：

### 5.1 program / function / block / local

- `lower_ir_program_init(...)`
- `lower_ir_program_free(...)`
- `lower_ir_program_append_global(...)`
- `lower_ir_program_append_function(...)`
- `lower_ir_function_append_local(...)`
- `lower_ir_function_append_block(...)`
- `lower_ir_function_allocate_temp(...)`

它们负责把 program/function/block/local/temp 这些容器和编号建起来。

### 5.2 value / slot helper

- `lower_ir_value_immediate(...)`
- `lower_ir_value_temp(...)`
- `lower_ir_slot_local(...)`
- `lower_ir_slot_global(...)`

这些 helper 的作用是：

- 明确构造 value ref
- 明确构造 slot ref

而不是让调用方直接手写结构体字段。

### 5.3 指令与 terminator

- `lower_ir_block_append_instruction(...)`
- `lower_ir_block_set_return(...)`
- `lower_ir_block_set_jump(...)`
- `lower_ir_block_set_branch(...)`

也就是说，当前 phase 1 使用方式更像：

1. 先手工建 program / function / block
2. 再手工 append 指令
3. 最后手工设 terminator

这正是 `tests/lower_ir/*.c` 当前在做的事。

---

## 6. 当前 dump 长什么样

`lower_ir_dump_program(...)` 的重点是把“显式 memory flow”打印出来。

当前 regression 里的代表形状是：

```text
global g.0

func main(a.0) {
  bb.0:
    tmp.0 = load_local a.0
    tmp.1 = add tmp.0, 1
    store_global g.0, tmp.1
    ret tmp.1
}
```

这段输出很能说明 lower IR 的边界：

1. `a.0` 作为 local slot，先被 `load_local` 读成 `tmp.0`
2. `add` 只在 temp/immediate 上计算
3. `g.0` 作为 global slot，通过 `store_global` 接收结果
4. `ret` 返回的也是 value，不是 slot

如果把它和 canonical IR 对比：

```text
tmp.0 = add a.0, 1
ret tmp.0
```

你就能看到 lower IR 的本质变化：

`slot 不能再伪装成 value`

---

## 7. verifier 在检查什么

`lower_ir_verify_program(...)` 是 lower IR 当前最重要的稳定契约之一。

它主要分三层检查。

### 7.1 value / slot ref 合法性

- temp 必须在 `0 .. next_temp_id-1`
- local slot 必须落在当前函数 `locals[]`
- global slot 必须落在当前程序 `globals[]`
- value kind / slot kind 不能是未知枚举值

### 7.2 指令形状契约

它会逐种指令检查 opcode 形状是否和 lower IR 设计一致：

- `mov` 必须产出 temp
- `binary` 必须产出 temp，且 lhs/rhs 都是合法 value
- `call` 必须产出 temp，callee 不能为空，args payload 要自洽
- `call` 目标必须能在当前 lower-IR 程序里解析到
- `call` 参数个数必须和已知 lower-IR 函数签名一致
- `load_local` 必须产出 temp，且 slot 必须真的是 local slot
- `store_local` 不能产出结果，且 slot 必须真的是 local slot
- `load_global` 必须产出 temp，且 slot 必须真的是 global slot
- `store_global` 不能产出结果，且 slot 必须真的是 global slot

所以 verifier 不只是检查“有没有越界”，还在强制 lower IR 的 value/slot 分离设计。

### 7.3 CFG / 容器不变量

它还会检查：

- 每个 block 都必须有 terminator
- `jmp` / `br` 目标 block 必须存在
- `func` 和 `declare` 的 `has_body` / `block_count` 关系要自洽
- global/local/block 表要保持 dense id
- 带 body 的函数至少要有一个 block
- function 名不能重复
- global 名不能重复
- function 名和 global 名也不能碰撞
- `globals/functions/locals/blocks` 的 count 非零时，对应表指针不能被破坏成 `NULL`

所以这版 lower IR verifier 的定位和 canonical IR verifier 类似：

`它不是语义分析器，而是 lower-IR 结构契约守门员`

---

### 7.4 runtime-init / startup helper 契约

最近 lower IR verifier 又补齐了一条更具体的结构契约：

- `__global.init`
- `__program.init`

这两个 runtime-init helper 不再只是“名字被带下来”，而是 lower IR 自己会检查：

1. helper 形状是否符合固定启动用途
2. 保留名是否只在真正存在 runtime global initializer 时才合法
3. `__global.init` 是否只从合法启动位被调用
4. `__program.init` 不能被当普通 helper 任意调用

当前允许的启动调用链可以概括成：

- `main` 入口块调用 `__global.init`
- 或 `__program.init` 入口块调用 `__global.init`

也就是说，lower IR 现在已经把：

`runtime global init helper 不是普通函数`

这件事正式变成了 verifier 契约。

---

## 8. 当前测试在锁什么

### 8.1 `tests/lower_ir/lower_ir_regression_test.c`

当前 regression 测试主要锁：

- source-to-lower-IR helper
- `return a;`
- `a = b + 1; return a;`
- `g = a; return g;`
- `if(a) ...`
- `id(a)`
- startup helper exact dump
- 单臂 `if` join 的 `store_local + join-block load_local`
- 双臂 `if/else` join 的 local write/read
- short-circuit 条件在多块里各自 materialize `load_local`
- `while` 回边里保留 canonical temp，同时把 slot 读写显式 lower 成 `load_* / store_*`
- `while` + `break`
- `for` 的 init/cond/step CFG
- nested loop 的 `break/continue`
- 控制流里的 global write/read join
- `declare` 函数在 `for` 的 init/cond/step 里被 lower 成合法 call
- call-heavy CFG slice：`for` 条件/step 与 short-circuit `&&` 条件里的 call 参数 materialize
- value-materialization join：局部 `&&` 值、ternary 值表达式、declared-call logical value、nested logical mix

也就是说，现在 regression 已经不只是“手工搭一份 lower IR 再 dump”，而是开始锁：

`canonical IR -> lower IR` 之后的代表性 shape

最近还多了一条很重要的 authority：

- runtime-init 不再只是 verifier 会抓错
- `__global.init` / `__program.init` 的正常 dump 终形也开始被 exact-dump 回归锁住

这意味着 lower IR 这层现在不只是“结构合法”，还开始对一部分代表性 startup shape 承担稳定输出契约。

也就是说，它当前更像：

`dump contract regression`

### 8.2 `tests/lower_ir/lower_ir_verifier_test.c`

当前 verifier 测试主要锁：

- 合法程序能过 verifier
- 非法 local slot 会被拒绝
- `store_global` 误带结果会被拒绝
- `binary` 结果如果不是 temp 会被拒绝
- 缺 terminator 会被拒绝
- runtime-global 正常流会通过
- 缺 helper / 缺 startup call 会被拒绝
- 保留名误用会被拒绝
- 在非启动位调用保留 helper 会被拒绝
- unknown callee / call arg mismatch 会被拒绝
- duplicate function name / duplicate global name / function-global name collision 会被拒绝
- `global/function/local/block` 表指针被破坏成 `NULL` 会被拒绝
- `count > capacity` 这类表容量契约破坏也会被拒绝
- temp definitions / temp availability / reachability 这些 lower-IR 级不变量也会被检查

这组测试的价值在于：它已经把 lower IR 的几条核心结构契约单独锁住了，而不是只通过 dump 间接验证。

---

## 9. 当前 lower IR 还没做什么

现在这层还没有：

1. 大覆盖面的 source-lowered regression 矩阵
2. lower-IR pass pipeline
3. SSA
4. register allocation
5. machine/codegen 相关建模

所以当前更准确的状态是：

`data model + serial lowering + dump + verifier + isolated/source-lowered tests 已落地`

如果按当前路线图口径再收紧一点，现在这轮 phase 2 更接近：

`代表性 shape 已经成组覆盖，checkpoint-complete；后续扩张应更偏 need-driven，而不是继续无边界铺 case`

而不是：

`已经进入后端`

---

## 10. 一句话总结

当前 `lower_ir` 的真正意义不是“再造一份 IR”，而是把 canonical IR 里还比较抽象的 local/global 值使用，收紧成一条显式的 memory boundary：

$$
\text{slot} \xrightarrow{\text{load}} \text{temp/value computation} \xrightarrow{\text{store}} \text{slot}
$$

而最近这一步又把 runtime-init helper 也纳入了 lower IR 自己的 verifier 契约里，所以这层现在已经不只是“值/槽位边界”，也开始明确承接下游启动 helper 的结构合法性。
