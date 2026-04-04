# IR Lesson（compiler_lab）

> 目标：解释当前 IR 模块到底实现了什么、边界在哪里，以及它如何把“已经通过 semantic 的 AST”降成当前这版最小可用 IR。

## 导学

这份讲义建议按“契约 -> lowering -> dump -> 测试”的顺序阅读：

1. 先看 `include/ir.h`，建立 IR 数据模型与 API 契约。
2. 再看 `src/ir/ir.c` 和各个 `.inc` 分片，理解 AST 到 IR 的 lowering 主路径。
3. 最后对照 `tests/ir/ir_regression_test.c`，把输出格式和行为锁定下来。

学完你应该能：

1. 能解释当前 IR v1 支持哪些 AST 子集。
2. 能说明局部变量/参数/临时值在 IR 里怎么编号与命名。
3. 能为新的 lowering 能力补一条对应的 IR regression。

---

## 1. 文件定位

- 接口：`include/ir.h`
- 聚合入口：`src/ir/ir.c`
- 核心/生命周期：`src/ir/ir_core.inc`
- lowering 作用域：`src/ir/ir_lower_scope.inc`
- 表达式 lowering：`src/ir/ir_lower_expr.inc`
- 语句 lowering：`src/ir/ir_lower_stmt.inc`
- global initializer 依赖收集：`src/ir/ir_global_dep.inc`
- global initializer / function / program lowering：`src/ir/ir_global_init.inc`
- verifier：`src/ir/ir_verify.inc`
- dump 与字符串 builder：`src/ir/ir_dump.inc`
- 回归：`tests/ir/ir_regression_test.c`
- verifier 测试：`tests/ir/ir_verifier_test.c`

当前 IR 也已经和 parser/semantic 一样，采用“聚合入口 + 分片 include”的组织方式：

```c
#include "ir_core.inc"
#include "ir_lower_scope.inc"
#include "ir_lower_expr.inc"
#include "ir_lower_stmt.inc"
#include "ir_global_dep.inc"
#include "ir_global_init.inc"
#include "ir_verify.inc"
#include "ir_dump.inc"
```

`src/ir/ir.c` 现在主要负责：

1. 定义共享内部结构体
2. 声明跨分片 helper 原型
3. 放置通用小工具（如 `ir_set_error`、`ir_next_growth_capacity`、`ir_value_*`、`ir_lower_current_block`）
4. 聚合 8 个 `.inc` 功能分片

共享内部结构体里现在除了 `IrLowerScope*` 之外，还包括：

- `IrLoopTarget`
- `IrLoopTargetStack`
- 带 `loop_targets` 的 `IrLowerContext`

这也是 `break/continue` lowering 现在能工作的基础。

另外，`src/ir/ir.c` 里现在也声明并聚合了 verifier 相关 helper；`ir_lower_program(...)` 在成功构好整份 IR 后，会再调用一次 `ir_verify_program(...)`，确保导出的 IR 先满足结构不变量。

最近这次拆分最重要的变化是：

- `ir_lower_stmt.inc` 重新收窄成“纯语句 CFG lowering”
- `ir_global_dep.inc` 单独承接 runtime global initializer 的依赖收集
- `ir_global_init.inc` 单独承接 global initializer 求值/回退、函数签名整理、函数 lowering、program 级 runtime-init 收尾

所以如果你脑子里还停留在“`ir_lower_stmt.inc` 什么都管”的旧印象，现在要把它更新成：

`statement CFG lowering` 和 `top-level/global-init orchestration` 已经被拆开了

这条链路当前的真实前提是：

$$
\text{Source}
\xrightarrow{\text{lexer}}
\text{Tokens}
\xrightarrow{\text{parser}}
\text{AST}
\xrightarrow{\text{semantic}}
\text{Semantically Valid AST}
\xrightarrow{\text{IR Lowering}}
\text{IR Program}
$$

也就是说，IR 阶段不是重新做语义分析；它假定输入 AST 已经过了 semantic。

---

## 2. `include/ir.h`：当前 IR 数据模型

### 2.1 错误出口 `IrError`

```c
typedef struct {
    int line;
    int column;
    char message[512];
} IrError;
```

它和 parser/semantic 的错误风格一致：统一返回 `(line, column, message)`。

抽象写法：

$$
\mathrm{IrError}=(\text{line},\text{column},\text{message})
$$

---

### 2.2 值引用 `IrValueRef`

当前 IR 里的“值”现在有 4 类：

- `IR_VALUE_IMMEDIATE`：立即数
- `IR_VALUE_LOCAL`：局部槽位（含参数）
- `IR_VALUE_GLOBAL`：顶层全局对象槽位
- `IR_VALUE_TEMP`： lowering 过程中分配的临时值

可写成：

$$
V = Imm(c) \mid Local(i) \mid Global(g) \mid Temp(t)
$$

其中：

- `immediate` 只在 `Imm` 有意义
- `id` 用于 `Local(i)`、`Global(g)` 和 `Temp(t)`

---

### 2.3 指令与终结符

当前指令种类仍然比较小：

- `IR_INSTR_MOV`
- `IR_INSTR_BINARY`
- `IR_INSTR_CALL`

当前二元算子现在分成 4 组：

- `IR_BINARY_ADD`
- `IR_BINARY_SUB`
- `IR_BINARY_MUL`
- `IR_BINARY_DIV`
- `IR_BINARY_MOD`
- `IR_BINARY_BIT_AND`
- `IR_BINARY_BIT_XOR`
- `IR_BINARY_BIT_OR`
- `IR_BINARY_SHIFT_LEFT`
- `IR_BINARY_SHIFT_RIGHT`
- `IR_BINARY_EQ`
- `IR_BINARY_NE`
- `IR_BINARY_LT`
- `IR_BINARY_LE`
- `IR_BINARY_GT`
- `IR_BINARY_GE`

当前 terminator 现在有 3 种：

- `IR_TERM_RETURN`
- `IR_TERM_JUMP`
- `IR_TERM_BRANCH`

所以当前 IR v1 的控制流能力已经可以概括成：

$$
\text{basic blocks} + \text{straight-line instructions} + \text{branch/jump/return}
$$

其中：

- `return` 结束当前路径
- `jmp` 做无条件跳转
- `br cond, then, else` 做条件分叉

还没有 phi、memory load/store、call terminator 之类结构。

---

### 2.4 容器层级

当前 IR 容器现在不只是“函数 -> block”这一条线了，而是：

1. `IrProgram`
2. `IrGlobal`
3. `IrFunction`
4. `IrBasicBlock`

关系可写成：

$$
Program = (Global^*,\ Function^*)
$$

$$
Global = (name,\ hasInitializer,\ initializerValue)
$$

$$
Function = (locals,\ blocks,\ nextTempId)
$$

$$
BasicBlock = (instructions,\ terminator?)
$$

`IrFunction` 这一层最近多了一个很重要的元数据：`has_body`。

它把“只有签名的声明项”和“已经 lower 出 CFG 的定义项”区分开来：

- `has_body = 0`：当前只记录了函数签名，没有 basic blocks
- `has_body = 1`：当前函数已经 lower 出真实 CFG

所以现在更准确的抽象是：

$$
Function = (name,\ hasBody,\ parameters,\ locals,\ blocks,\ nextTempId)
$$

当前每个“有 body 的函数”仍然从入口块 `bb.0` 开始 lowering；若出现 `if/while/for/ternary/logical short-circuit`，会继续生成真实的后继 basic block。

---

### 2.5 对外 API

公开 API 现在有 5 个：

- `ir_program_init`
- `ir_program_free`
- `ir_lower_program`
- `ir_verify_program`
- `ir_dump_program`

其中 lowering 契约是：

```c
int ir_lower_program(const AstProgram *ast_program,
    IrProgram *out_program,
    IrError *error);
```

头文件注释里已经写清楚：`out_program` 应该先 `ir_program_init(...)`，或者至少是零初始化状态。

---

## 3. 当前 IR v1 支持什么，不支持什么

这部分最重要，因为当前 IR 模块是“明确的可运行子集”，不是“全 AST 覆盖”。

### 3.1 当前支持的形态

顶层层面：

- 顶层函数声明会先被登记成“只有签名的 `IrFunction`”
- 顶层函数定义会在同名 `IrFunction` 上继续补 body 与 basic blocks
- 顶层变量声明会 lower 成 `IrGlobal`
- 可常量求值的顶层 initializer 会在 lowering 期直接写进 `IrGlobal.initializer_value`
- 不能常量求值但仍可 runtime lower 的顶层 initializer 会被收进运行时初始化函数 `__global.init()`
- `IrGlobal` 当前会用 `has_initializer` / `has_runtime_initializer` 区分 constant initializer 与 runtime initializer 两种状态

语句层面：

- `AST_STMT_COMPOUND`
- `AST_STMT_DECLARATION`
- `AST_STMT_EXPRESSION`
- `AST_STMT_IF`
- `AST_STMT_WHILE`
- `AST_STMT_FOR`
- `AST_STMT_RETURN`
- `AST_STMT_BREAK`
- `AST_STMT_CONTINUE`

表达式层面：

- 数字字面量
- 局部标识符 / 参数标识符
- 全局标识符
- 括号表达式
- 前缀 `++x` / `--x`
- 一元 `+`
- 一元 `-`
- 一元 `~`
- 一元 `!`
- 二元 `+ - * / %`
- 二元 `& ^ | << >>`
- 二元 `== != < <= > >=`
- 二元 `&& ||`
- 三目表达式 `?:`
- 逗号表达式 `,`
- 赋值 `=`
- 复合赋值 `+= -= *= /= %= &= ^= |= <<= >>=`
- 后缀 `x++` / `x--`
- 直接调用表达式 `f(...)`

### 3.1.1 最近这几轮 IR 主要新增了什么

如果只看最近几轮实现，不看更早的基线，重点其实集中在这 4 组：

1. global-aware IR  
   顶层 declaration 不再被直接跳过，而是会 lower 成 `IrGlobal`；标识符读写也变成“先查 local，再查 global”。
2. runtime global initializer  
   不能静态求值的顶层 initializer 不再直接失败，而是会回退到 `__global.init()`；没有可注入的已 lowered `main()` 定义时，再额外导出 `__program.init()`。
   最近这块又往前走了一步：runtime-init 依赖收集已经不再是最早的“纯语法整棵树扫描”，而是开始利用 `condition truthiness + may_fallthrough` 做有限裁剪。
   同时，`IR-LOWER-022` 现在也不再只是笼统报“依赖顺序错误”，而是会尝试带出 `dependency path` / `dependency cycle` 解释。
3. global update / bitwise-shift family  
   global 现在不只支持普通读写，也支持 compound assignment、`++/--`、`<<=` / `&=` / `^=` / `|=` 这类更新路径。
4. runtime-init verifier 契约  
   verifier 现在不只查普通 CFG/temps，还会单独检查 `__global.init()` / `__program.init()` 的 helper 形状、入口调用和保留名约束。

---

### 3.2 当前明确不支持的形态

下面这些不是“还没在测试里覆盖”，而是当前 lowering 会直接报错：

- callee 不是“直接标识符”的调用表达式
- 没有返回值表达式的 `return`
- 无法在当前 `IrProgram` / scope 里解析的标识符
- runtime path 也无法 lower 的顶层初始化表达式
- runtime global initializer 对未初始化 global 的前向依赖
- 源码侧滥用 `__global.init` / `__program.init` 这类内部保留 helper 名
- 跳过 semantic gate 后仍无法自洽的 call / declaration 形态
- 任何不在上面支持列表里的 AST 节点

也就是说，当前 IR v1 更接近：

$$
\text{locals/globals} + \text{calls} + \text{arithmetic/bitwise/shift/comparison/logical/ternary} + \text{assignment/comma} + \text{if/while/for} + \text{return/break/continue}
$$

而不是完整的 C-like IR。

---

## 4. IR 模块的总体结构

现在更准确的理解方式不是“一个大 `ir.c` 文件”，而是 9 个文件协作：

1. `src/ir/ir.c`：共享 typedef、helper 原型、通用工具、聚合 include
2. `src/ir/ir_core.inc`：Program/Function/Block 生命周期、append helper、`return/jump/branch` terminator helper
3. `src/ir/ir_lower_scope.inc`：lowering 作用域栈
4. `src/ir/ir_lower_expr.inc`：表达式 lowering
5. `src/ir/ir_lower_stmt.inc`：纯语句 CFG lowering（`if/while/for/return/break/continue` 等）
6. `src/ir/ir_global_dep.inc`：runtime global initializer 依赖收集
7. `src/ir/ir_global_init.inc`：global initializer 求值/回退、函数签名/函数 lowering、program 级收尾
8. `src/ir/ir_verify.inc`：IR 结构自检与不变量校验
9. `src/ir/ir_dump.inc`：dump builder 与文本输出

从 Makefile 角度看，这几份源码当前也被明确列进 `IR_SPLIT_INCLUDES`，和 parser/semantic 的拆分方式保持一致。

---

## 5. 动态扩容与内存管理

### 5.1 统一扩容 helper

当前多个动态数组都共用：

```c
static int ir_next_growth_capacity(size_t current,
    size_t initial,
    size_t elem_size,
    size_t *out_next_capacity);
```

策略是：

$$
\mathrm{cap}_{next}=
\begin{cases}
\mathrm{initial}, & current=0 \\
2 \cdot current, & current>0
\end{cases}
$$

但真正实现还会先做两层保护：

1. `current > SIZE_MAX / 2` 时拒绝倍增
2. `next_capacity > SIZE_MAX / elem_size` 时拒绝分配

所以它和最近 lexer/ast 的容量策略一致：都是“倍增 + 溢出保护”。

---

### 5.2 当前默认初始容量

不同对象的初始增长值不一样：

- function 数组：4
- local 数组：4
- block 数组：4
- instruction 数组：8
- dump 字符串 builder：64 字节

这类数字本身不重要，重要的是：IR 这边已经把容量增长逻辑统一成一个 helper，而不是每个数组自己写一份。

---

### 5.3 生命周期

公开生命周期 API：

- `ir_program_init`
- `ir_program_free`

释放路径会递归清掉：

- 每个 `IrGlobal.name`
- `IrFunction.name`
- 每个 `IrLocal.source_name`
- 每个 `IrBasicBlock.instructions`
- `program->globals`
- `program->functions`

不变量可以写成：

$$
\mathrm{free}(program)\Rightarrow
functions=NULL,\ count=0,\ capacity=0
$$

---

## 6. lowering 时的名字与作用域

### 6.0 `IrLowerContext` 现在怎么表示“当前块”

最近这版 IR lowering 上还有一个实现层变化：`IrLowerContext` 不再直接保存
`IrBasicBlock *block`，而是改成了：

- `size_t block_id`
- `int has_block`

并通过 `ir_lower_current_block(ctx)` 现取当前 block。

这样做的好处是：

1. block 数组扩容后，避免长期持有旧指针
2. `has_block=0` 可以显式表达“当前路径已经封口，没有活动块”
3. 需要跨 append-block 保存出口位置时，用 `block_id` 比裸指针更稳

所以现在 IR 里的 fallthrough/封口判断，更准确地说是：

$$
\text{active path} \equiv has\_block = 1
$$

并且只有在 `has_block=1` 时，才继续查看当前 block 的 `has_terminator`。

### 6.1 为什么需要 `IrLowerScopeStack`

IR lowering 不是直接拿 AST 里的标识符字符串就结束了，因为要解决：

- 参数和局部变量编号
- 同名遮蔽
- 内层块引用最近一层绑定

当前实现用：

- `IrLowerScope`
- `IrLowerScopeStack`

来记录“名字 -> local id”的映射。

查找规则是从栈顶往下找：

$$
\mathrm{lookup}(x)=\text{nearest binding of }x
$$

所以像下面这种 shadowing 会被正确区分：

```c
int f(int a){{int a=1; return a;}}
```

最终返回的是内层 `a.1`，不是参数 `a.0`。

---

### 6.2 参数与局部变量的编号

参数和局部都存在同一个 `locals[]` 数组里，只是 `is_parameter` 不同：

- 参数先 append，所以最先占据 `0..parameter_count-1`
- 后续局部声明继续往后编号

dump 时的命名规则：

- 参数/局部：`source_name.id`
- 临时值：`tmp.id`
- 立即数：直接打印数字

所以：

- 第一个参数 `a` 会显示成 `a.0`
- 第一个局部 `a`（若遮蔽参数）会显示成 `a.1`
- 第一个临时值是 `tmp.0`

---

## 7. 表达式 lowering：`ir_lower_expression`

这是 IR v1 的核心入口：

```c
static int ir_lower_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value);
```

它的职责是：

1. 递归 lower AST 表达式
2. 必要时发出指令
3. 返回“这个表达式的结果值引用”

抽象上：

$$
\mathrm{LowerExpr}(E) \Rightarrow (Instr^*, ValueRef)
$$

---

### 7.1 数字、标识符、括号

- 数字：直接变成 `IR_VALUE_IMMEDIATE`
- 标识符：先查当前 scope stack 里的 local，再回退到 `IrProgram.globals`
- 括号：直接透传内部表达式

这里一个关键边界是：

- 局部/参数标识符会 lower 成 `IR_VALUE_LOCAL`
- 已登记到 `IrProgram` 的顶层对象会 lower 成 `IR_VALUE_GLOBAL`
- 如果两边都找不到，才报 `IR-LOWER-002`

### 7.1.1 `global` 和 `local` 现在有什么区别

虽然在表达式里它们都能表现成“一个可读写名字”，但当前实现里来源和处理路径并不一样：

- `local` 属于某个 `IrFunction.locals[]`
- `global` 属于整个 `IrProgram.globals[]`

查找顺序是：

1. 先查当前 lowering scope stack 里的 local
2. 查不到再去 `IrProgram.globals` 里找 global

所以 local 会遮蔽同名 global。例如：

```c
int g=4;
int main(){int g=1; g+=2; return g;}
```

当前会把函数体里的 `g` 全部解析成 local，而不是顶层 global。

初始化方式也不同：

- `local` initializer 会变成函数 CFG 里的指令
- `global` initializer 则优先尝试在 lowering 期直接求值成常量

也就是说，`int x=1;` 和 `int g=1;` 虽然表面上都写成 `=1`，但在 IR 里：

- local 走 `mov`
- global 直接变成 `global g.0 = 1`

---

### 7.2 一元运算

当前支持四种：

- 一元 `+`：直接透传 operand
- 一元 `-`：lower 成 `0 - operand`
- 一元 `~`：lower 成 `operand xor -1`
- 一元 `!`：走逻辑值 lowering，生成 true/false/join CFG 后产出 `0/1`

例如：

$$
-x \Rightarrow tmp.n = sub\ 0,\ x
$$

其中 `~x` 现在没有专门的 `bitnot` 指令，而是直接复用现有二元 IR：

$$
\sim x \Rightarrow tmp.n = xor\ x,\ -1
$$

这说明当前 IR 的扩展策略更偏向“复用既有指令模型”，而不是为每个语法点都新增一条专用指令。

其它一元运算会报 `IR-LOWER-003`。

---

### 7.3 二元算术、位运算、移位与比较

支持的 token 到 IR opcode 映射：

- `+ -> IR_BINARY_ADD`
- `- -> IR_BINARY_SUB`
- `* -> IR_BINARY_MUL`
- `/ -> IR_BINARY_DIV`
- `% -> IR_BINARY_MOD`
- `& -> IR_BINARY_BIT_AND`
- `^ -> IR_BINARY_BIT_XOR`
- `| -> IR_BINARY_BIT_OR`
- `<< -> IR_BINARY_SHIFT_LEFT`
- `>> -> IR_BINARY_SHIFT_RIGHT`
- `== -> IR_BINARY_EQ`
- `!= -> IR_BINARY_NE`
- `< -> IR_BINARY_LT`
- `<= -> IR_BINARY_LE`
- `> -> IR_BINARY_GT`
- `>= -> IR_BINARY_GE`

lowering 方式是：

1. 先递归 lower 左右操作数
2. 分配一个新的 `tmp.id`
3. 发出一条 `IR_INSTR_BINARY`
4. 返回这个 temp 作为表达式结果

伪代码：

```text
lhs = lower(expr.left)
rhs = lower(expr.right)
t = new_temp()
emit t = op lhs, rhs
return t
```

---

### 7.4 赋值与复合赋值表达式

赋值是一个单独分支：

- 左值必须是“可解析到的 identifier lvalue”
- 右侧先 lower 成一个值
- 然后发出 `mov`
- 整个赋值表达式的结果值就是被写入的 lvalue

也就是说：

$$
x = y + 1
\Rightarrow
tmp.0 = add\ y,\ 1
$$

$$
x.0 = mov\ tmp.0
$$

并且表达式结果仍然是目标 lvalue 本身。

当前 identifier lvalue 既可以是 local，也可以是 global；如果左边不是受支持的 identifier lvalue，当前会报 `IR-LOWER-004`。

---

复合赋值也是单独支持的一支。当前不会引入新的 IR 指令，而是复用：

1. 读出左值 lvalue
2. lower 右值
3. 发一条 `binary`
4. 再 `mov` 回原 lvalue
5. 整个表达式结果就是更新后的那个值

例如：

```c
x += 2;
```

会近似 lower 成：

```text
tmp.0 = add x.0, 2
x.0 = mov tmp.0
```

支持的复合赋值和当前 binary op 对齐：

- `+= -= *= /= %=`
- `&= ^= |=`
- `<<= >>=`

如果复合赋值左边不是受支持的 identifier lvalue，当前会报 `IR-LOWER-015`。

---

### 7.5 前缀与后缀更新表达式

当前 IR 也已经支持：

- 前缀 `++x` / `--x`
- 后缀 `x++` / `x--`

它们都要求操作数能解析成 identifier lvalue；这个检查会先去掉多余括号，所以 `(x)++` 这类 parenthesized identifier 也能按同一套规则处理。当前这既覆盖 local，也覆盖 global。

前缀更新的 lowering 是：

1. 取当前 identifier lvalue 的值
2. 发 `add/sub lvalue, 1`
3. `mov` 回原 lvalue
4. 表达式结果返回“更新后的值”

所以：

```c
++x
```

会近似变成：

```text
tmp.0 = add x.0, 1
x.0 = mov tmp.0
ret tmp.0
```

后缀更新会多一步“保留旧值”：

1. 先 `mov` 一份旧值到 `tmp.old`
2. 再算更新后的值
3. `mov` 回原 lvalue
4. 表达式结果返回旧值

所以：

```c
x++
```

会近似变成：

```text
tmp.0 = mov x.0
tmp.1 = add x.0, 1
x.0 = mov tmp.1
ret tmp.0
```

当前对应错误码是：

- `IR-LOWER-014`：prefix update target 不是受支持的 identifier lvalue
- `IR-LOWER-016`：postfix update target 不是受支持的 identifier lvalue
- `IR-LOWER-017`：postfix 运算符本身不在当前支持集合里

---

### 7.6 三目、逗号表达式与逻辑值表达式

三目表达式现在也已经支持，而且是按真实 CFG lower 的。

值表达式形态 `cond ? a : b` 的做法是：

1. 先为 `then/else/join` 建 block
2. 用 `ir_lower_boolean_branch(...)` lower 条件
3. 在 then / else 两边分别 lower 出值
4. 把两边结果都 `mov` 到同一个 join temp
5. 在 join block 把这个 temp 当作整个三目表达式的值返回

所以它当前走的是 non-SSA 风格的“join temp 汇合”，不是 phi 指令。

逗号表达式 `(e1, e2)` 现在也已经支持，但没有引入新的 `IR_INSTR_COMMA`。

当前做法是：

1. 先顺序 lower 左边 `e1`
2. 保留它可能产生的副作用
3. 丢弃左边最终值
4. 再 lower 右边 `e2`
5. 整个逗号表达式的结果值取右边

所以：

$$
(e_1,\ e_2) \Rightarrow \mathrm{emit}(e_1) + \mathrm{emit}(e_2) + \mathrm{return}\ value(e_2)
$$

例如：

```c
return (a=1, b=2, a+b);
```

会先发出两个 `mov`，再发 `add`，最后 `ret` 右边结果。

逻辑值表达式 `!` / `&&` / `||` 的实现也不是“直接当成普通 binary op”。

当前真实做法是：

1. 先用 `ir_lower_boolean_branch(...)` 做短路分支 CFG
2. 再在 true/false block 里分别写入 `mov 1` / `mov 0`
3. 最后汇合到 join block，把 `tmp.n` 当作逻辑表达式的值返回

所以像：

```c
return a&&b;
```

不会 lower 成单条 `binary and`，而是 lower 成“短路分支 + `0/1` 物化”的小 CFG。

---

### 7.7 调用表达式

当前 IR 已经支持直接调用表达式，会发出 `IR_INSTR_CALL`。

支持边界是：

- callee 必须是“去掉括号后仍是标识符”的直接调用
- 参数会按从左到右顺序先 lower
- 调用结果会写入一个新的 `tmp.id`

所以：

```c
return add(x, x+1);
```

会先 lower 参数 `x` 与 `x+1`，再发出：

```text
tmp.1 = call add(x.0, tmp.0)
```

如果 callee 不是这种直接标识符形态，当前会报 `IR-LOWER-013`。

### 7.7.1 顶层 initializer 为什么有时不直接写成 `global x = ...`

顶层 global initializer 现在分两种路径：

1. 能静态求值  
   例如：
   ```c
   int a=1;
   int b=a+2;
   ```
   会直接 lower 成：
   ```text
   global a.0 = 1
   global b.1 = 3
   ```
2. 不能静态求值  
   例如：
   ```c
   int seed();
   int g=seed();
   ```
   就不会直接写成 `global g.0 = ...`，而是会生成一个内部运行时初始化函数 `__global.init()`，在里面先 `call seed()`，再把结果 `mov` 到 `g.0`。

这个 `__global.init()` 不是一开始就固定存在，而是第一次遇到“必须走 runtime initializer 的 global”时才懒创建；后续其它 runtime global initializer 会继续把指令追加到同一个 helper 里。

这里还有一个最近的重要变化：当前 lowering 会先“尝试静态求值”，但这一步失败并不一定直接报错。像下面这些形态，现在会自动回退到 runtime global initializer 路径：

- `int a=-(~9223372036854775807);`
- `int a=9223372036854775807+1;`

也就是说，静态 evaluator 不愿意在 lowering 时直接给出常量值的 initializer，如今会优先尝试改走 `__global.init()`，而不是立刻失败。

真正会稳定拒绝的是：

- runtime path 也无法 lower 的 initializer
- 依赖顺序不合法的 initializer  
  例如某个 global initializer 依赖了“还没完成 constant initializer、也还没登记 runtime initializer”的更早 global；这类当前会报 `IR-LOWER-022`

这里的“依赖”现在也不再是最早那种“纯语法整棵子树一扫到底”。  
`ir_collect_global_initializer_dependencies(...)` 当前已经是一个保守的、带少量 reachability 信息的依赖收集器，它会：

- 收集 initializer 表达式里直接读到的 global
- 递归跟进 direct call callee 对应的函数定义
- 继续扫描被调函数体里读到的 global
- 通过 `ir_dependency_try_eval_condition_truthiness(...)` 复用一份“identifier-free 常量条件真假”判断 helper
- 在表达式层对 `&&` / `||` 做短路裁剪，对 `?:` 做常量条件分支裁剪
- 在 `compound` 里用 `may_fallthrough` 停掉 `return/break/continue` 之后的后续语句扫描
- 对 `if` 做条件真假裁剪：常量真只扫 then，常量假只扫 else
- 对 `while` / `for` 做有限条件裁剪：常量假时不再继续扫 loop body
- 同时尊重局部作用域遮蔽，所以 local/parameter/for-init declaration 不会被误记成 global 依赖

所以像 `int a=read_b(); int b=seed();` 这种“通过函数间接读未初始化 global”的情况，当前会稳定落到 `IR-LOWER-022`。

但这里也要注意边界：这套东西现在依然是“保守依赖收集”，还不是完整的 reachable-read analysis。  
它已经不再是旧的“完全不看不可达”的实现，但也还不会做更强的路径可达性证明或循环退出证明。  
尤其是 loop 这边目前仍然比较保守：`while(true)` / `for(;;)` 这种情形不会因为 body 不 fallthrough 就被进一步证明成“后续一定不可达”，所以这套分析还没有升级成完整的控制流依赖证明。

不过“完全没有解释”这件事已经不是现状了。当前 `IR-LOWER-022` 会优先尝试格式化两类更具体的诊断：

- `dependency path: a -> b`
- `dependency path via callee body: a -> read_b() -> b`
- `dependency cycle: a -> b -> a`

所以最近这块真正新增的价值，不只是“更容易命中正确拒绝”，还包括“被拒绝时更容易看懂为什么”。

如果程序里有已 lower 的 `main()` 定义，当前会把对 `__global.init()` 的调用前插到 `main` 入口块。  
如果没有可注入的已 lower `main()` 定义，当前会额外导出一个 `__program.init()`，它只负责去调用 `__global.init()`。

---

### 7.8 其它表达式为什么会失败

`ir_lower_expression` 的 default 分支会报：

- `IR-LOWER-005`：二元运算符不在当前支持集合里
- `IR-LOWER-006`：表达式 kind 本身不在 IR v1 支持集合里

这正是当前 IR v1 的边界锁。

---

## 8. 语句 lowering：`ir_lower_statement`

当前语句 lowering 支持 9 类语句。

### 8.1 复合语句 `AST_STMT_COMPOUND`

进入块时：

1. `push` 一个新的 lowering scope frame
2. 依次 lower 每个 child statement
3. 块结束时 `pop`

这保证了块级 shadowing 和名字可见性与 semantic 一致。

另外当前还有一个实现细节：

- 如果当前 basic block 已经有 terminator，后续语句会被直接跳过

这意味着 lowering 只保留“当前仍可到达的 CFG 路径”。一旦当前路径没有活动块，或者当前块已经有 terminator，后续同一路径上的语句就不会再往这个块里继续发指令。

---

### 8.2 声明语句 `AST_STMT_DECLARATION`

每个声明名都会做两件事：

1. append 一个新的 `IrLocal`
2. 把名字绑定进当前 scope frame

如果这个 declarator 带 initializer，并且 `stmt->expressions[i]` 非空：

1. 先 lower initializer 表达式
2. 再发一条 `mov` 写入对应 local

所以：

```c
int a = 1;
```

会 lower 成近似：

```text
a.0 = mov 1
```

这里和 parser 当前的“声明器 initializer 槽位对齐”设计是对上的：IR 直接按 `declaration_names[i] <-> expressions[i]` 去读。

还有一个容易忽略的实现顺序：当前 IR 是“先 append local / 先把名字绑进 scope，再 lower initializer”。  
也就是说，如果某个声明真的走到了 IR，这个 initializer 在名字解析上已经能看见“当前 declarator 自己”。不过当前 semantic 会先把 `int x=x+1;` 这类自引用声明挡掉，所以正常输入里不会把这条差异暴露成用户可见行为。

---

### 8.3 表达式语句 `AST_STMT_EXPRESSION`

表达式语句会逐个 lower 里面的表达式，但丢弃最终结果值：

$$
\mathrm{LowerStmt}(expr;) = \mathrm{emit}(expr) + \text{discard result}
$$

这适合像赋值这种“有副作用但返回值不用”的场景。

---

### 8.4 `return`

当前 `return` lowering 只支持“有表达式的 return”。

流程是：

1. 找到 return 的 primary expression
2. lower 成一个 `IrValueRef`
3. 写入 block terminator：`IR_TERM_RETURN`

如果拿不到返回表达式，报 `IR-LOWER-001`。

---

### 8.5 `if`

这是当前 IR v1 最大的变化点：`if` 已经不再走“拒绝”路径，而是会生成真实 basic block 分叉。

实现入口在 `src/ir/ir_lower_stmt.inc` 的 `ir_lower_if_statement(...)`。它的大致流程是：

1. 先 lower condition，得到一个 `IrValueRef`
2. 先创建 `then_block`
3. 如果有 `else`：
   - 再创建 `else_block`
   - 在入口块写入 `IR_TERM_BRANCH(condition, then, else)`
4. 如果没有 `else`：
   - 先创建一个“假分支直接落去的 continue block”
   - 在入口块写入 `IR_TERM_BRANCH(condition, then, continue)`
5. 分别 lower then/else 子语句
6. 看 then/else 最后的 block 是否已经有 terminator
7. 只有在某一侧还能 fallthrough 时，才创建 continuation block，并给这些 fallthrough 路径补 `jmp`

也就是说，判断“要不要 join block”的关键不是语法上有没有 `else`，而是：

$$
\text{need join block} \iff \text{there exists a branch arm that still falls through}
$$

这就解释了你说的那条行为：

- 两边都 `return`：两边都已经终结，不需要 continuation block
- 只有一边 `return`：另一边还会落下去，需要 continuation block
- 两边都只是赋值后继续：两边都 fallthrough，需要 continuation block

---

### 8.6 `if` 为什么只在需要汇合时才建 continuation block

核心就两步：

1. lowering 完 then arm 后，记录 `then_fallthrough = ctx->has_block && !current_block->has_terminator`
2. lowering 完 else arm 后，记录 `else_fallthrough = ctx->has_block && !current_block->has_terminator`

然后分情况：

- `then_fallthrough || else_fallthrough`
  - 创建 continuation block
  - 给所有 still-fallthrough 的出口块补 `jmp bb.cont`
  - `ctx->block_id = continue_block_id; ctx->has_block = 1`
- 否则
  - 不创建 continuation block
  - 直接把 `ctx->has_block = 0`

`ctx->has_block = 0` 这一点很关键，它表示“当前这条后续直线 lowering 已经没有活动块了”。外层 `compound` 在继续扫后续语句时，看到 `!ctx->has_block` 就会停下来。

所以“两个分支都 return 时不多造 join block”不是 dump 层省略，而是 lowering 阶段根本就没有创建那个 block。

---

### 8.7 `if` lowering 例子

例 1：只有一侧继续落下去

```c
int f(int a){int x=0; if(a) x=1; return x;}
```

会变成：

```text
func f(a.0) {
  bb.0:
    x.1 = mov 0
    br a.0, bb.1, bb.2
  bb.1:
    x.1 = mov 1
    jmp bb.2
  bb.2:
    ret x.1
}
```

这里 `bb.2` 既是“if(false) 路径的直接去向”，也是 then arm 的 continuation block。

例 2：两边都继续落下去

```c
int f(int a){int x=0; if(a) x=1; else x=2; return x;}
```

会变成：

```text
func f(a.0) {
  bb.0:
    x.1 = mov 0
    br a.0, bb.1, bb.2
  bb.1:
    x.1 = mov 1
    jmp bb.3
  bb.2:
    x.1 = mov 2
    jmp bb.3
  bb.3:
    ret x.1
}
```

这里 then/else 两边都会 fallthrough，所以必须显式建 `bb.3`。

例 3：两边都终结

```c
int f(int a){if(a) return 1; else return 2;}
```

会变成：

```text
func f(a.0) {
  bb.0:
    br a.0, bb.1, bb.2
  bb.1:
    ret 1
  bb.2:
    ret 2
}
```

这里没有 `bb.3`，因为 then/else 两边都没有 fallthrough。

---

### 8.8 `while`

`while` 现在也已经 lower 成真实 loop CFG 了，入口在 `ir_lower_while_statement(...)`。

它的骨架是：

1. 记住当前入口块 `entry_block_id`
2. 新建 `header_block`、`body_block`、`exit_block`
3. 从入口块发一条 `jmp header`
4. 在 header 里 lower condition，再发 `br cond, body, exit`
5. lower body
6. 如果 body 结束后仍然 fallthrough，就补一条 `jmp header`
7. 最后把当前活动块切到 `exit_block`

所以它构出来的是标准的“前测循环”形状：

```text
entry -> header
header --true--> body
header --false-> exit
body --fallthrough--> header
```

回归里的典型例子：

```c
int f(int a){while(a){a=a-1;} return a;}
```

会 lower 成：

```text
func f(a.0) {
  bb.0:
    jmp bb.1
  bb.1:
    br a.0, bb.2, bb.3
  bb.2:
    tmp.0 = sub a.0, 1
    a.0 = mov tmp.0
    jmp bb.1
  bb.3:
    ret a.0
}
```

---

### 8.9 `for`

`for` lowering 也已经具备了，入口在 `ir_lower_for_statement(...)`。

它比 `while` 多两件事：

1. 可能有 for-init 声明，需要单独的 loop scope
2. 可能有 step 表达式，需要显式的 `step_block`

当前实现会先：

1. `push` 一个新的 lowering scope frame（只给这次 `for` 用）
2. 处理 init
   - 如果是 declaration-init，就走 `ir_lower_declaration_names`
   - 如果是普通 init 表达式，就先 lower 这个表达式
3. 再创建 `header/body/step/exit` 四个块

CFG 形状是：

```text
entry -> header
header --true--> body
header --false-> exit
body --fallthrough--> step
step -> header
```

如果 condition 缺失，header 会直接 `jmp body`，相当于 `for(;;)` 的无条件进入。

回归里的典型例子：

```c
int f(){for(int a=3;a;a=a-1){} return 0;}
```

会 lower 成：

```text
func f() {
  bb.0:
    a.0 = mov 3
    jmp bb.1
  bb.1:
    br a.0, bb.2, bb.4
  bb.2:
    jmp bb.3
  bb.3:
    tmp.0 = sub a.0, 1
    a.0 = mov tmp.0
    jmp bb.1
  bb.4:
    ret 0
}
```

这里 `bb.2` 是 loop body，空 body 所以直接落到 `bb.3`。

---

### 8.10 `break` / `continue`

现在 `break` 和 `continue` 都已经 lower 成普通 `jmp` terminator 了。

实现方式不是给 IR 再发明一套新指令，也不是让 `break` 节点自己回头在 AST 里找“父循环”。

当前真实做法是：在 lowering context 里把“当前最近一层循环该跳到哪里”一层层往下传。

具体来说，`IrLowerContext` 里带着一个 `IrLoopTargetStack`，而每个 loop frame 只记录两件事：

- `break_target`
- `continue_target`

也就是说，传下去的不是 AST 位置本身，而是“目标 basic block 的 id”。

进入循环时：

- `while` 会 push `(break_target = exit_block, continue_target = header_block)`
- `for` 会 push `(break_target = exit_block, continue_target = step_block)`

离开循环体后再 pop。

因此，循环体里更深层的语句在递归 lowering 时，天然就能从当前 context 读到“最近一层 loop target”。

实现方式不是给 IR 再发明一套新指令，而是在 lowering context 里维护一个 loop-target stack：

- `break` 取当前 loop frame 的 `break_target`
- `continue` 取当前 loop frame 的 `continue_target`

因此：

- `while` 会把 `continue` 指到 header block
- `for` 会把 `continue` 指到 step block
- 嵌套循环天然遵守“就近匹配”的语义，因为总是读取栈顶 loop target

所以这套机制的本质可以写成：

$$
\text{break/continue lowering} =
\text{read current loop target from context}
+\ 
\text{emit jmp target\_block\_id}
$$

若当前不在任何循环里，`break` / `continue` 会分别报：

- `IR-LOWER-011`：break statement without active loop target
- `IR-LOWER-012`：continue statement without active loop target

### 8.11 其它语句

默认分支会报 `IR-LOWER-008`。

---

## 9. 函数 lowering：`ir_lower_function`

这一层现在已经不在 `src/ir/ir_lower_stmt.inc` 里了，而是跟 global initializer / program 收尾一起挪到了 `src/ir/ir_global_init.inc`。

单个函数定义的 lowering 主线是：

1. 先通过 `ir_ensure_function_signature(...)` 确保程序里已经有对应的 `IrFunction`
2. 创建入口块 `bb.0`
3. 初始化 scope stack
4. 先把参数逐个注册成 local
5. 再 lower `function_body`
6. 把 `has_body` 置为 `1`
7. 要求入口块最终拥有 terminator

伪代码：

```text
ensure function signature(name)
append entry block
push function scope
for each parameter:
    ensure local(parameter)
    bind name -> local id
lower(function_body)
mark has_body = 1
require entry block has terminator
```

这里的“入口块拥有 terminator”现在不再意味着“函数只有一个块”，而是意味着 CFG 至少从 `bb.0` 被正确封口成：

- `ret`
- `jmp`
- `br`

当前若入口块最终没有形成受支持的 terminator，会报 `IR-LOWER-009`。

`ir_ensure_function_signature(...)` 这一步现在还有两个具体契约：

- 如果之前只有 prototype，没有 body，后续 definition 会复用同一个 `IrFunction`，而不是新建一份重名函数
- unnamed parameter 不会缺槽位，而是先落成后备名 `param0`、`param1` 这种 local；如果后面 definition 给出了真实参数名，再把这些参数 local 重命名过去
- 如果 declaration arity 在 IR lowering 自己这层就已经对不上，会报 `IR-LOWER-020`

因此 declaration-aware IR 不只是“保留 signature”，还包括“后续 definition 会在同一个函数项上继续补 body，并顺手修正参数名”。

这和 semantic 的关系是：

- semantic 保证“所有路径返回”这类语义条件
- IR v1 还额外要求“返回路径必须能 lower 成当前支持的 IR 子集”

---

## 10. `ir_lower_program`：程序级入口

这部分当前也在 `src/ir/ir_global_init.inc`，不再和纯 statement CFG lowering 混在 `src/ir/ir_lower_stmt.inc` 里。

程序级 lowering 现在比最初版本多了两步：global lowering 和函数签名登记。

- 遍历 `AstProgram.externals`
- 遇到 `AST_EXTERNAL_DECLARATION` 时，先 lower 成 `IrGlobal`
- 遇到函数声明时，先用 `ir_ensure_function_signature(...)` 记录函数名与参数 locals
- 遇到函数定义时，在同名 `IrFunction` 上继续补 body / blocks / temps
- 第一次遇到 runtime global initializer 时，才懒创建内部 helper `__global.init()`
- 若存在 runtime global initializer，则在最后给 `__global.init()` 的末尾补 `ret 0`
- 如果存在已 lowered 的 `main()` 定义，就把 `call __global.init()` 前插到 `main` 的 entry block
- 如果没有可注入的已 lowered `main()` 定义，就额外导出 `__program.init()`，让它只做一条 `call __global.init()` 然后 `ret 0`
- 全部结束后，再统一跑一次 `ir_verify_program(...)`

除了“正常路径”，program lowering 这一层最近也补了几条更明确的 lowering-side gate：

- `IR-LOWER-021`：同名函数 body 被重复 lowering
- `IR-LOWER-024`：`__global.init` / `__program.init` 这类内部 helper 保留名不能被用户源码拿来伪造

而 call lowering 这边，即使跳过 semantic gate，IR 自己也会守住两条底线：

- `IR-LOWER-025`：call target 在当前 IR lowering 上下文里根本没声明
- `IR-LOWER-026`：call 实参数量和已知 callee signature 对不上

这组 `IR-LOWER-020/021/024/025/026` 的定位最好一起记：

- 它们不是“新语法能力”
- 也不是主要面向最终用户的 semantic 诊断
- 而是 IR lowering 自己的 self-check / last-line guard

也就是说，就算前面的 semantic gate 被绕开，IR 这一层仍会拒绝“不自洽的声明/定义/调用状态”，防止坏 AST 或坏签名关系继续流进 IR。

所以当前顶层行为更准确地写成：

$$
\mathrm{IRProgram}
=
\mathrm{globals} + \mathrm{signatures} + \mathrm{defined\ bodies}
$$

这也是为什么现在：

- 顶层 declaration 也不再完全消失
- 纯 prototype 不会再完全消失
- declaration-only callee 的参数个数也能被 verifier 拿来检查
- dump 输出会区分 `global g.0 = 1`、`declare foo(...)` 和 `func foo(...) { ... }`
- 运行时 global initializer 还会额外导出 `__global.init()`，必要时再导出 `__program.init()`

---

## 11. `ir_verify_program`：为什么还要再做一层结构校验

`ir_verify_program(...)` 不是做源码语义分析，而是检查“已经构好的 IR”有没有被构坏。

它更像开发期和回归里的 invariant checker / sanity check。

当前 verifier 会检查至少这些结构条件：

1. `IrValueRef` 是否引用了存在的 local / global / temp
2. 每个 block 是否真的有 terminator
3. `jmp` / `br` 的目标 block 是否存在
4. 指令 kind / binary op 是否在支持集合里
5. `call` 的 callee 名是否为空、是否能在当前 `IrProgram` 里找到
6. 已知 callee 的 `arg_count` 是否和函数签名的 `parameter_count` 一致
7. `call` 在 `arg_count > 0` 时是否真的带了 `args` payload
8. block 是否从 `bb.0` 可达
9. local / global / block 的 `id` 和它们所在槽位是否一致
10. `temp.n` 是否至少有定义一次，且不会在所有前驱都定义前就被使用
11. declaration-only function 是否保持“只有签名、没有 body”的约束
12. 同一个 basic block 内是否把同一个 `temp.n` 重复定义多次
13. 整个 `IrProgram` 里是否出现重复函数入口名
14. program / function / block 的 count-capacity 关系，以及表指针是否为 NULL
15. `global` 是否错误同时带了 constant initializer 与 runtime initializer 标记
16. `__global.init()` / `__program.init()` 是否满足内部 helper 形状契约
17. 没有 runtime global initializer 时，保留名 `__global.init()` / `__program.init()` 是否被滥用

其中最近新增的 runtime-init 约束，最好直接按错误码记：

- `IR-VERIFY-048`：某个 `global` 不能同时带 constant/runtime initializer 两套标记
- `IR-VERIFY-049`：只要存在 runtime global initializer，就必须真的 lower 出 `__global.init()`
- `IR-VERIFY-050`：runtime global initializer 必须从 `main()` 或 `__program.init()` 入口被调用
- `IR-VERIFY-051/052/053`：`__global.init()` / `__program.init()` 必须满足零参数、零 locals、单 entry block、`ret 0`
- `IR-VERIFY-054/055`：`__program.init()` 必须以唯一一条零参 `call __global.init()` 开头
- `IR-VERIFY-056`：`__global.init` / `__program.init` 这两个 helper 名是保留名，只能用于 runtime global initializer 基础设施
- `IR-VERIFY-059`：普通函数体不能从非 startup 位置调用保留的 internal init helper
- `IR-VERIFY-060`：普通函数体不能调用 `__program.init()` 这种 startup helper
- `IR-VERIFY-062`：program 里的 global 名不能重复
- `IR-VERIFY-063`：同一个 symbol 名不能同时被 function entry 和 global entry 复用

对应测试也已经独立成了 `tests/ir/ir_verifier_test.c`。

当前 `ir_lower_program(...)` 在 lowering 成功后还会立即调用 verifier；所以对外返回成功，意味着：

$$
\text{lowered successfully} \land \text{structurally verified}
$$

### 11.1 verifier 的真实执行顺序

如果按实现顺序看，`ir_verify_program(...)` 不是把所有检查混在一起，而是分层往下走：

1. 先在 program 层检查函数入口名是否重复
2. 再逐个函数跑 `ir_verify_function(...)`
3. 在函数内部，先检查函数壳子：
   - declaration-only function 的 `has_body/block_count/next_temp_id/local_count` 是否自洽
   - 有 body 的函数是否至少有一个 block
4. 再检查 locals / blocks 的 metadata：
   - `local.id == slot index`
   - 参数 locals 必须占前缀
   - `block.id == slot index`
5. 再跑 `ir_verify_temp_definitions(...)`
6. 再逐条检查 instruction 与 terminator
7. 再跑 `ir_verify_temp_availability(...)`
8. 最后跑 `ir_verify_reachability(...)`

所以它整体更像：

$$
\text{program uniqueness}
\rightarrow
\text{function shell}
\rightarrow
\text{temp definitions}
\rightarrow
\text{instruction/terminator well-formedness}
\rightarrow
\text{temp dataflow availability}
\rightarrow
\text{reachability}
$$

### 11.2 `temp availability` 是怎么做的

`ir_verify_temp_definitions(...)` 只能回答：

- 某个 `tmp.n` 有没有被定义过
- 它有没有在单个 block 里被重复定义坏掉

但它回答不了这件事：

`当某条指令读取 tmp.n 时，沿所有前驱路径走到这里，它都已经可用了吗？`

这就是 `ir_verify_temp_availability(...)` 单独存在的原因。它的大致做法是：

1. 先扫描每个 block，统计 `block_defs[bb][tmp]`
2. 再根据 `jmp/br` 建 CFG 边表 `edges[pred][succ]`
3. 迭代求每个 block 的：
   - `in_sets[bb][tmp]`
   - `out_sets[bb][tmp]`
4. 之后按 block 内指令顺序再走一遍，用当前 `available[]` 检查每次读 temp 时它是否已经定义好

这里 join 用的是“所有前驱都必须成立”的规则：

$$
in[bb][t] = \bigwedge_{pred \to bb} out[pred][t]
$$

所以它抓的是这种错误：

- 某些前驱路径里 `tmp.0` 根本没定义
- 但 join 后你就在 `binary` / `call` / `branch` / `return` 里读了 `tmp.0`

这时就会报 `IR-VERIFY-024`。

这里要特别注意一个最近修正过的边界：

- verifier 现在不要求“同一个 temp 在整个函数里只能定义一次”
- 当前只禁止“同一个 temp 在同一个 basic block 里重复定义”

这是为了和当前 non-SSA lowering 设计保持一致。像逻辑值 CFG、以后若加入 ternary/join 这类“多条分支都往同一个 join temp 写值、再在汇合后读取”的形态，在当前模型下是合法的；真正需要拒绝的是“单个 block 内把同一 temp 重复覆写”，因为这通常意味着 lowering 自己把 temp 管理写坏了。

---

## 12. `ir_dump_program`：文本格式怎么来的

当前 dump 是稳定文本协议，测试就是靠它锁行为。

格式大致是：

```text
global g.0 = 1

func name(param.0, param.1) {
  bb.0:
    br param.0, bb.1, bb.2
  bb.1:
    x.1 = mov 1
    jmp bb.3
  bb.2:
    x.1 = mov 2
    jmp bb.3
  bb.3:
    ret x.1
}
```

如果一个函数只有签名、没有 body，当前 dump 会输出：

```text
declare add(a.0, b.1)
```

而不是空花括号函数体。

如果存在运行时 global initializer，当前 dump 还可能出现：

```text
func __global.init() { ... }
```

以及在没有 `main()` 时出现：

```text
func __program.init() { ... }
```

几个关键规则：

1. globals 先于 functions 输出
2. global 行格式是 `global name.id`，若有 initializer 则追加 ` = value`
3. 函数之间用一个空行分隔
4. 有 body 的函数打印成 `func name(...) { ... }`
5. 只有声明的函数打印成 `declare name(...)`
6. basic block 名固定为 `bb.%zu`
7. `mov` 格式是 `dst = mov value`
8. binary 格式是 `dst = add|sub|mul|div|mod|and|xor|or|shl|shr|eq|ne|lt|le|gt|ge lhs, rhs`
9. call 格式是 `dst = call callee(arg0, arg1, ...)`
10. jump 格式是 `jmp bb.%zu`
11. branch 格式是 `br cond, bb.then, bb.else`
12. return 格式是 `ret value`

当前 dump 不是“调试随便打印”，而是 regression contract 的一部分。

---

## 13. `tests/ir/ir_regression_test.c`：怎么给 IR 写回归

当前 IR 回归 harness 很直接：

1. `lexer_tokenize`
2. `parser_parse_translation_unit_ast`
3. `semantic_analyze_program`
4. `ir_lower_program`
5. `ir_dump_program`
6. `strcmp(actual, expected)`

也就是说，它锁的是端到端结果，而不是只测某个 helper。

---

### 13.1 当前已锁住的一组 lowering 行为

1. `IR-RET-LIT`：`return 0`
2. `IR-RET-PARAM`：参数直接返回
3. `IR-ARITH`：算术优先级 lowering
4. `IR-BITWISE-FAMILY`：`& ^ | << >>` 能 lower 成 bitwise/shift binary op
5. `IR-TILDE`：`~x` lower 成 `xor x, -1`
6. `IR-COMMA`：逗号表达式保留左侧副作用并返回右侧值
7. `IR-CALL-DIRECT`：直接调用会 lower 成 `IR_INSTR_CALL`
8. `IR-CALL-ARGS`：调用参数会先逐个 lower，再形成 call 指令
9. `IR-COMPOUND-ASSIGN`：`+=` / `<<=` 这类会 lower 成 `binary + mov`
10. `IR-PREFIX-INC`：前缀 `++x` 返回更新后的值
11. `IR-POSTFIX-INC`：后缀 `x++` 返回更新前的旧值
12. `IR-LOGICAL-NOT`：`!x` 走逻辑值 CFG lowering
13. `IR-CMP-FAMILY`：`== != < <= > >=` 都能 lower 成 comparison binary op
14. `IR-LOGICAL-AND`：`a&&b` 会短路并物化成 `0/1`
15. `IR-LOGICAL-OR`：`a||b` 会短路并物化成 `0/1`
16. `IR-SHADOW-LOCAL`：局部遮蔽参数
17. `IR-IF-JOIN`：无 `else` 的 `if` 会生成 then block + continuation block
18. `IR-IF-ELSE-JOIN`：两边都会落下去时，生成共享 join block
19. `IR-IF-ELSE-RET`：两边都 `return` 时，不生成多余 join block
20. `IR-IF-CMP`：comparison 表达式可直接作为 `if` 条件
21. `IR-IF-LOGICAL-COND`：逻辑短路条件可直接驱动 `if` 分支
22. `IR-WHILE-BACKEDGE`：`while` 生成 header/body/exit 和 backedge
23. `IR-WHILE-BREAK`：`while` 里的 `break` 会跳到 loop exit
24. `IR-FOR-INIT-STEP`：`for` 生成 init/header/body/step/exit，并保留 for-init lowering
25. `IR-NESTED-LOOP-CONTROL`：嵌套循环里的 `break/continue` 使用就近 loop target
26. `IR-GLOBAL-COMPOUND-ASSIGN`：global 也能作为 compound assignment 目标
27. `IR-GLOBAL-BITWISE-SHIFT-COMPOUND`：global 也能作为 `<<=` / `&=` / `^=` / `|=` 这类复合赋值目标
28. `IR-GLOBAL-INCDEC`：global 也能作为 prefix/postfix update 目标
29. `IR-PREFIX-DEC`：prefix `--x` 返回更新后的值
30. `IR-POSTFIX-DEC`：postfix `x--` 先保留旧值再更新
31. `IR-GLOBAL-INIT-DEP`：顶层 initializer 可依赖之前已知 initializer 的 global
32. `IR-GLOBAL-SHADOW-LOCAL`：local 同名声明会遮蔽 global
33. `IR-GLOBAL-RUNTIME-INIT`：不能静态求值但仍可 runtime lower 的 global initializer 会进入 `__global.init()`
34. `IR-GLOBAL-RUNTIME-INIT-NO-MAIN`：没有 `main()` 时会额外导出 `__program.init()`
35. `IR-GLOBAL-RUNTIME-INIT-DEP-ORDER`：runtime global initializer 若依赖未初始化 global，应报 `IR-LOWER-022`
36. `IR-GLOBAL-RUNTIME-INIT-INDIRECT-DEP-ORDER`：间接读取未初始化 global 的 runtime initializer 也应报 `IR-LOWER-022`
37. `IR-GLOBAL-RUNTIME-INIT-DEP-CYCLE`：runtime global initializer 命中环路时，`IR-LOWER-022` 会带 `dependency cycle: ...`
38. `IR-GLOBAL-RUNTIME-INIT-DEP-PATH-VIA-CALLEE`：经由 callee body 命中的依赖拒绝会带 `dependency path via callee body: ...`
39. `IR-GLOBAL-INIT-NEG-LLONG-MIN`：静态 evaluator 不安全时会回退到 runtime init，而不是在 lowering 期制造 host UB
40. `IR-GLOBAL-INIT-ADD-OVERFLOW`：静态 evaluator 不愿直接给常量值时也会回退到 runtime init
41. `IR-CALL-UNKNOWN-CALLEE-LOWER-GATE`：跳过 semantic gate 时，IR lowering 自己仍会拒绝未声明 callee（`IR-LOWER-025`）
42. `IR-CALL-ARITY-MISMATCH-LOWER-GATE`：跳过 semantic gate 时，IR lowering 自己仍会拒绝 call arity mismatch（`IR-LOWER-026`）
43. `IR-DECL-ARITY-MISMATCH-LOWER-GATE`：重复 declaration 的参数数不一致时，IR lowering 自己也会报 `IR-LOWER-020`
44. `IR-DUPLICATE-DEF-LOWER-GATE`：重复 function body lowering 会报 `IR-LOWER-021`
45. `IR-DECL-TERNARY-VALUE`：声明函数调用可出现在 ternary value expression 里
46. `IR-DECL-TERNARY-COND`：声明函数调用可出现在 ternary condition path 里

例如这条：

```c
int f(int a){return a+2*3;}
```

预期 dump 是：

```text
func f(a.0) {
  bb.0:
    tmp.0 = mul 2, 3
    tmp.1 = add a.0, tmp.0
    ret tmp.1
}
```

这里同时锁了两件事：

1. parser/semantic 产出的 AST 结合顺序正确
2. IR lowering 的临时值生成顺序稳定

---

### 13.2 `tests/ir/ir_verifier_test.c` 当前锁住的结构校验

除了 lowering regression，当前还单独锁了 verifier 行为：

1. 正常 lowering 出来的 IR 应继续通过 verifier
2. 带 declaration-only signature 的 lowered program 也应通过 verifier
3. 去掉 block terminator 应报 `IR-VERIFY-005`
4. 人工插入不可达 block 应报 `IR-VERIFY-010`
5. 篡改成未知 binary op 应报 `IR-VERIFY-020`
6. 把 call 的 callee 清空应报 `IR-VERIFY-021`
7. 把 `next_temp_id` 提高到超过真实定义数，应报 `IR-VERIFY-023`
8. 人工把某条指令改成“先用 tmp.0 再定义”，应报 `IR-VERIFY-024`
9. 人工制造“同一 block 内重复 temp 定义”，应报 `IR-VERIFY-031`
10. 把 call 的 `arg_count` 改坏，不论目标是定义函数还是纯声明函数，都应报 `IR-VERIFY-026`
11. 把 call 目标名改成 program 里不存在的函数，应报 `IR-VERIFY-032`
12. 让 `arg_count > 0` 但 `call.args == NULL`，应报 `IR-VERIFY-033`
13. 让 `arg_count == 0` 但 `call.args != NULL`，应报 `IR-VERIFY-037`
14. 给 declaration-only function 塞进 blocks，应报 `IR-VERIFY-028`
15. 把 function/local/block/instruction/program 的 count-capacity 关系改坏，或把对应表指针置空，应报 `IR-VERIFY-034` 到 `IR-VERIFY-045` 这组结构错误
16. 把 global 引用/id/name 改坏，应报 `IR-VERIFY-043`、`IR-VERIFY-046`、`IR-VERIFY-047`
17. 把 global 同时标成 constant+runtime initializer，应报 `IR-VERIFY-048`
18. 破坏 `__global.init()` / `__program.init()` 的 helper 契约，应报 `IR-VERIFY-049` 到 `IR-VERIFY-056`
19. 在普通函数体里非法调用保留 startup/init helper，应报 `IR-VERIFY-059`、`IR-VERIFY-060`
20. 人工制造 duplicate global name 或 function/global 同名碰撞，应报 `IR-VERIFY-062`、`IR-VERIFY-063`

这组测试的目标不是锁 dump，而是锁“IR 结构体本身的自洽性”。

---

### 13.3 继续扩展 IR 时建议补哪些 case

如果后面继续做 IR，建议每扩一个能力，就补“一正一反”两类 case：

1. global initializer lowering：`int a=1; int b=a+2;`
2. assignment expression lowering：`x = x + 1;`
3. prefix/postfix decrement：`--x`、`x--`
4. compound assignment family 余项：`&=`、`|=`、`>>=` 等
5. runtime global initializer 顺序：多个 `g=seed()` 是否按源码顺序写进 `__global.init()`
6. `while` with empty body / non-empty body / immediate return body
7. `for` with expression-init / declaration-init / missing-condition / missing-step
8. non-identifier call callee：当前应稳定失败
9. global/local shadowing 与 global assignment/update 组合
10. verifier negative cases：新增 instruction/terminator kind 或 program/global metadata 时，同步补结构破坏测试
11. 继续提升 runtime global initializer 依赖精度：虽然 `IR-LOWER-022` 已经会给出 dependency path / cycle，但 loop 与更复杂可达性上的 false positive 仍然值得继续压低

推荐思路：

$$
\mathrm{New\ IR\ Feature}
\Rightarrow
\mathrm{accept\ dump\ lock}
+
\mathrm{reject\ boundary\ lock}
$$

---

## 14. 当前 IR v1 的定位

一句话总结：

当前 IR 不是“编译后端已完成”，而是“把 semantic 通过的一个局部变量/调用/表达式/控制流子集稳定降到文本 IR，并在导出前做结构自检”的第一版骨架。

它已经具备：

- 稳定的数据结构
- 可释放的生命周期
- 局部作用域与 shadowing
- 临时值分配
- 调用与参数求值顺序
- 位运算 / 移位 / 逗号表达式
- 逻辑短路值 lowering 与逻辑条件 lowering
- 基本块分叉与按需 continuation/join block
- `while/for` 的 header/body/step/exit CFG 骨架
- `break/continue` 的 loop-target jump lowering
- declaration-only function signature entries
- global object entries 与 runtime global initializer hook
- verifier 结构校验
- 稳定 dump 文本协议
- 端到端 regression

但它还没有：

- 显式内存模型（`load/store`、地址、栈槽、全局对象引用）
- 更一般的内存式左值与非 identifier 左值
- 寄存器/机器级后端那一层能力

这里还有一个容易说大的点：当前 IR 只支持“直接标识符调用”，但这并不算眼下最现实的缺口。因为当前语言本身还没有：

- 函数指针
- 可调用值
- 返回 callable 的表达式语义

所以像 `f()()`、`(f?g:h)()` 这类 non-identifier callee 形态，现在更准确地说是“语言层面本来就还没有对应模型”，而不只是“IR 还没 lower”。

如果按“现在最现实还缺什么”来排，当前更像是差这三类：

1. data / global 这一层  
   也就是顶层声明不再只是保留在 AST/semantic，而是能进入 IR，形成真正的数据项或对象项。
2. memory/object model 这一层  
   现在 IR 里的 `local` 还是抽象槽位，不是显式地址；所以目前也还没有 `load/store`。这不是因为 IR 已经有寄存器而缺少访存指令，恰恰相反，是因为当前这层 IR 还没有走到“显式建模内存位置”的阶段。
3. 更低层 backend 这一层  
   包括寄存器、指令选择、spill/reload、codegen。当前 IR 明确还高于这一层，所以 `temp` 只是中间结果名，不是物理寄存器。

所以最准确的定位是：

$$
\text{IR v1} = \text{basic-block IR for locals/calls/expressions + function signatures + if/while/for CFG + structural verification}
$$

---

## 可选思考题

1. `if`、`while`、`for`、`break`、`continue` 现在都已经能 lower；如果后面继续做更复杂的 loop control，你会把哪些额外状态放进 lowering context？
2. `block_id + has_block` 这种编码方式，相比直接保存 block 指针，最关键的收益是什么？
3. `assignment` 现在把结果值设成目标 local；如果以后支持更复杂的左值，这个设计要不要改？
