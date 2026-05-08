# Machine Select Lesson（compiler_lab）

> 目标：解释 `machine_select` 为什么要从 `machine_ir` 里单独拆出来，它当前到底在“选”什么，`src/machine/lowering/machine_select/` 里各个分片在做什么，以及这层源码里真实存在的 lowering / cleanup 行为是什么。

## 一句话定位

`machine_select` 是 backend 真正开始进入 instruction-selection / target-lowering 语境的第一层。

## 常见误解

- 误解一：select 就只是把 op 名字改细一点。
  - 当前这层已经在决定 selected op family、selected terminator family、call/result/return shape 以及局部 cleanup。
- 误解二：有了 `machine_ir` 就可以直接去 layout/emit。
  - 没有 `machine_select`，后面很多 selected-specific layout、encode、bytes 语义都还没有稳定入口。

## 导学

如果说：

- `machine_ir` 回答的是“值已经住到哪里了”

那么：

- `machine_select` 回答的是“这些 machine-ir 操作，具体选成哪类更低层 op”

所以这层是整条 machine pipeline 里第一个真正的：

`instruction-selection / target-lowering consumer`

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_ir` 还不够，必须再落一层 `machine_select`
2. 再看 `include/machine/select.h`，建立 selected op / terminator family 的整体印象
3. 再看 `src/machine/lowering/machine_select/` 的文件分工，理解 lowering / cleanup / query / report 是怎么拆的
4. 最后带着最小例子和伪代码去看“它到底怎么选、怎么清”

学完你应该能：

1. 解释 `machine_select` 和 `machine_ir` / `machine_layout` 的职责边界
2. 说明 `machine_select` 当前到底新增了哪些 selected operation families
3. 能看懂 `machine_ir -> machine_select` 的代表性翻译例子
4. 能说明这层已经有什么 cleanup/canonicalization，而不是只会“改名字”

---

## 前置阅读

最推荐先读：

1. `lesson/machine/lowering/machine_ir_lesson.md`

如果你还没先理解：

- `machine_ir` 里值住在 `reg/spill/imm` 哪
- `machine_ir` 为什么还是 generic machine-facing op

那 `machine_select` 到底在“选”什么会很容易看成只是枚举改名。

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/lowering/machine_layout_lesson.md`
2. `lesson/machine/lowering/machine_emit_lesson.md`

因为 select 之后最自然的问题就是：

- block 最后怎么排
- label / branch target 怎么继续显式化

---

## 最近同步

最近 `machine_select` 最值得补进 lesson 的，不是“又多了几条 selected op”，而是它开始有了更明确的 **consumer-facing compatibility surface**。

你现在最好把这层再多记三件事：

1. **program/report policy summary 已经成形**
   现在 selected artifact 不只是给你看 raw program，还会显式告诉你当前 preview lane 依赖哪些事实，比如：
   - preview register cap
   - immediate legalization
   - compare-branch fusion
   - spill preservation
   - global-slot preservation

2. **`riscv32-preview` verifier entrypoint 已经有了**
   这意味着一些“反正后面 bytes 层会炸”的问题，现在可以更早在 selected 层失败。

3. **它已经不只是 instruction selection 层，还是 preview-lane honesty 的第一道闸门**

所以 lesson 口径上，这层除了：

- generic machine-facing op -> selected op family

还要再加一句：

- `machine_select` 现在也是当前 `riscv32-preview` compatibility contract 的最早显式边界

最近还要再补一个更新：这层的 cleanup line 现在已经不只是“保守 successor peeking fallback”，而是更明确地走向 **CFG live-out-aware selected cleanup**。

当前最好再多记四件事：

1. **block-entry must-agree alias propagation**
   - `copy` / `materialize_imm` 这类事实现在已经能跨 CFG 边传播

2. **`live_out` 决定 cross-block trivial-def / dead-def removal**
   - 不再只是“看下一个块像不像会用”

3. **`call` barrier 边界现在更明确**
   - caller-clobbered register alias 会被杀
   - spill-slot alias 则能继续活下去

4. **这条线的 focused regression 已经锁到 mixed resource / mixed path family**
   - 不是只有“寄存器会不会被删掉”
   - 还包括 register 和 spill 在同一 successor region 下不同命运的情况

所以现在这层除了“selected lowering”，还越来越像：

- selected-side dataflow cleanup 的第一版正式实现

而不是：

- 一层只会做局部 peephole 的轻量清理

结合最近最新那次提交，现在这条 cleanup 线还可以再更明确地理解成：

- 当前已经是 **focused regressions 锁住、接近 checkpoint 的 selected-side cleanup 主线**

也就是说，lesson 口径上最好不要再把它写成“还只是一个保守 fallback”，而是：

- 一条已经有明确 dataflow、明确 call barrier、明确 mixed resource/path coverage 的 cleanup line

这轮未提交改动里，这层还要再同步两个容易漏掉的点：

1. **void return 现在真的会落成 bare `ret`**
   - 不再把它统一伪装成 `reti 0`
2. **cleanup 会刻意保留某些 block 尾部 edge-copy**
   - 如果这些 trailing `copy` 是为了喂给多前驱 successor 的边界值，cleanup 不会再把它们当普通 trivial copy 一把删掉

所以现在这层除了“选 op”，还在更认真地守两种真实性：

- return-shape honesty
- edge-copy / successor-boundary honesty

如果按最新 `lv9` 这轮往前推，这层还要再补一个新的主线切口：

3. **selected memory family 不再只盯着 local/global slot**
   - 现在已经开始有
     - `addr_local`
     - `addr_global`
     - `load_indirect`
     - `store_indirect`
   这组 selected op
4. **只要程序用了 indirect memory，这层当前会主动跳过旧 cleanup 主线**
   - 也就是说，当前 cleanup 还是标量 slot 模型更成熟
   - `lv9` 这轮是先让 indirect-memory path honest 地通过，而不是假装 cleanup 已经完全懂它

而这轮未提交代码里，又把 indirect-memory 路线再往前推了一小步：

5. **indirect-memory path 现在也开始有 very targeted cleanup**
   - 比如重复 `addr_local` root 复用
   - `copy self` noop 删除
   - 某些 spill-backed pure expression 复用
   - 重复 pure internal call 在 indirect block 下的复用

所以 lesson 口径上要再精确一点：

- 不是“indirect memory 完全没有 cleanup”
- 而是“不会再跑旧的 full cleanup 主线，但已经开始有一组窄而诚实的 targeted cleanup”

所以这层现在最准确的 lesson 口径变成：

- selected op family 已经开始吃第一批 indirect-memory 形状
- 但 selected cleanup 目前仍然是“标量 slot 路线最成熟、indirect 路线先 targeted cleanup 再保守直通”

---

## 1. 为什么需要 `machine_select`

`machine_ir` 已经把这些问题变成显式信息：

- 值住在 register、spill 还是 immediate
- CFG / branch / call / slot 结构还长什么样

但 `machine_ir` 仍然保留的是比较 generic 的 machine-facing operation family：

- `mov`
- `binary`
- `call`
- `load_*`
- `store_*`
- `ret`
- `jmp`
- `br`

这还不等于真正的 instruction selection。

因为后端继续往下走时，真正想问的是：

- 一个 generic `binary` 最后应该落成 `ALU`、`ALU_IMM`、`CMP` 还是 `CMP_IMM`？
- 一个带 immediate 参数的 call，和普通 call，要不要进入不同 selected family？
- 一个 `br`，如果条件其实来自 compare，能不能直接变成 compare-branch terminator？

也就是说，`machine_select` 引入的真正边界是：

`从 generic machine-facing op，进入 selected low-level op family`

所以它的准确身份是：

`machine_ir 的下游消费者`

不是 `machine_ir` 的更深内部阶段。

---

## 2. 为什么不能把这层继续塞进 `machine_ir`

`machine_ir` 负责的是：

- `reg/spill/imm` operand
- CFG
- phi-elimination 之后的 generic machine-facing cleanup
- 一个稳定的 machine-side IR artifact

但它不应该再继续负责：

- 一个 generic binary 最后选成什么 selected op
- call 参数/结果用哪种 selected call shape 表示
- branch / compare / constant-condition 最后进入哪种 terminator family

不然 `machine_ir` 会变成：

`既是 stable IR，又是 instruction selector`

这样会把边界揉坏。

项目现在的拆法非常明确：

- `machine_ir`
  - 回答“放置结果 + generic machine op artifact”
- `machine_select`
  - 回答“这些 generic op 选成哪类 low-level op”
- `machine_layout`
  - 回答“块怎么线性排、哪条边是 fallthrough”

---

## 3. 文件定位

- 接口：`include/machine/select.h`
- 实现：`src/machine/lowering/machine_select/`
- 测试：`tests/machine/lowering/machine_select/`
- 规划文档：`docs/backend/MACHINE_SELECT_PLAN.md`

当前目录已经不是一个大文件，而是按职责拆开：

- `machine_select.c`
- `machine_select_core.inc`
- `machine_select_verify.inc`
- `machine_select_dump.inc`
- `machine_select_query.inc`
- `machine_select_report.inc`
- `machine_select_cleanup.inc`
- `machine_select_lower.inc`
- `machine_select_lower_value.inc`
- `machine_select_lower_memory.inc`
- `machine_select_lower_control.inc`
- `machine_select_lower_call.inc`

所以这层现在已经不是“先写个 `machine_select.c` 占坑”，而是有真实 module boundary 的实现层。

---

## 4. `src/machine/lowering/machine_select/` 目录怎么读

最简单的方式不是按文件名硬背，而是按四组职责去记。

### 4.1 core / artifact 基础层

- `machine_select_core.inc`
- `machine_select_verify.inc`
- `machine_select_dump.inc`
- `machine_select_query.inc`
- `machine_select_report.inc`

这几块负责的是：

- 生命周期
- verifier
- dump
- query
- summary/report artifact

它们在回答：

`selected program 怎么被建、被验、被看、被汇总`

这里有一个很值得 lesson 单独强调的点：

`machine_select` 现在已经不只是“lower 完 dump 一下”。

它已经有：

- raw selected program query
- function-level summary
- block-level summary
- terminator summary
- report artifact build / refresh / dump

所以这层已经明显是一个：

`可被后续 consumer 直接导航和消费的 artifact 层`

而不是一次性文本导出器。

### 4.2 lowering 主线

- `machine_select_lower.inc`
- `machine_select_lower_value.inc`
- `machine_select_lower_memory.inc`
- `machine_select_lower_control.inc`
- `machine_select_lower_call.inc`

这几块合起来才是这层最核心的实现。

它们在回答：

- `mov/binary` 选成什么
- `load/store` 选成什么
- `branch/compare/constant-condition` 选成什么
- `call` 选成什么

也就是说，真正“instruction selection”的主体就在这几块里。

### 4.3 cleanup / canonicalization 层

- `machine_select_cleanup.inc`

这一块不是大而全优化器，但它也绝对不只是 trivial beautify。

它会做：

- op shape canonicalize
- terminator shape canonicalize
- trivial def forwarding
- pure dead-def removal

所以它更准确的身份是：

`selected-side local canonicalization pipeline`

### 4.4 聚合入口

- `machine_select.c`

这块主要负责把 split includes 聚起来，以及把公共 API 接口收口到一个编译单元里。

---

## 5. `include/machine/select.h`：当前数据模型

### 5.1 operand / slot

当前 operand 仍然和 `machine_ir` 很接近：

- immediate
- register
- spill slot

slot 也仍然保留：

- local
- global

所以这层并没有改动“值住在哪里”的事实，它改动的是：

`操作族`

### 5.2 selected op family

从 header 看，当前 selected op family 已经很明确：

- `COPY`
- `MATERIALIZE_IMM`
- `ALU`
- `ALU_IMM`
- `CMP`
- `CMP_IMM`
- `CALL`
- `CALL_IMM`
- `CALL_SPILL`
- `CALL_IMM_SPILL`
- `CALL_VOID`
- `CALL_VOID_IMM`
- `LOAD_LOCAL`
- `STORE_LOCAL`
- `STORE_LOCAL_IMM`
- `LOAD_GLOBAL`
- `STORE_GLOBAL`
- `STORE_GLOBAL_IMM`

这说明当前层已经开始回答：

- 这条 op 是 plain value op 还是 immediate-specialized op？
- 这个 call 有没有 immediate arg？
- 这个 call 的结果是不是落在 spill slot？

### 5.3 selected terminator family

terminator 也已经细化成：

- `RETURN`
- `RETURN_IMM`
- `RETURN_SPILL`
- `JUMP`
- `BRANCH`
- `COMPARE_BRANCH`
- `COMPARE_BRANCH_IMM`

这说明当前层已经开始把：

- plain branch
- materialized-boolean branch
- compare-produced branch
- constant-condition branch

区分开来。

---

## 6. 这层现在是什么，不是什么

### 当前它是什么

当前 `machine_select` 已经负责：

1. 从 `MachineIrProgram` lower 到 `MachineSelectProgram`
2. 把 generic machine-ir instruction 转成 selected op family
3. 把若干 branch family 收成更精细的 selected terminator kind
4. 保留 function / block CFG 结构
5. 提供 verifier / dump / query / report
6. 做第一批 selected-side cleanup / canonicalization

### 当前它不是什么

它现在还不是：

1. block layout
2. label assignment
3. 真正编码
4. object / relocation / ELF

所以这层仍然是：

`selected CFG IR`

不是最终汇编文本。

---

## 7. 一个很重要的输入契约：它默认更喜欢吃 canonicalized machine-ir

这一点在 `docs/backend/MACHINE_SELECT_PLAN.md` 里其实说得很明确，但很容易在看 lesson 时被忽略。

当前最推荐的主线不是：

`raw machine_ir -> machine_select`

而是：

`machine_ir canonicalize -> machine_select`

也就是说，这层默认更喜欢吃：

- phi-free
- cleanup 过
- generic machine-facing shape 已经比较规整

的 `machine_ir` 输入。

这也是为什么 lesson 里不能把 `machine_select` 讲成：

- “所有复杂 shape 都留给它解决”

更准确的口径是：

- `machine_ir`
  - 先负责 generic machine-facing IR 的规整化
- `machine_select`
  - 再在规整后的输入上做 selected family lowering

所以这层 current input contract 其实是：

`prefer cleaned machine_ir, not arbitrary raw machine_ir forever`

---

## 8. Lowering 总体策略：它不是“全局重写”，而是按指令族分发

如果把 `machine_select_lower.inc` 的精神压成一段 lesson 级伪代码，大致就是：

```text
for each function:
  copy register bank / globals / locals / blocks metadata

  for each block:
    for each machine_ir instruction:
      try lower as value instruction
      else try lower as memory instruction
      else if call:
        lower as call instruction
      else:
        error

    lower terminator

after whole program is lowered:
  run selected-side cleanup
```

所以当前 `machine_select` 的形态很清楚：

- lowering 阶段先把机器侧 generic op 分流到 selected family
- cleanup 阶段再做一轮局部 canonicalization

---

## 9. value lowering：`mov/binary` 到底怎么选

`machine_select_lower_value.inc` 当前最值得抓的点有三个。

### 9.1 `mov`

如果 `mov` 的源是 immediate，那么不会只是 `COPY`，而会直接选成：

- `MATERIALIZE_IMM`

如果源不是 immediate，则选成：

- `COPY`

也就是说，当前层已经开始把：

- “复制现有机器值”
- “物化一个立即数”

分成两种 selected family。

### 9.2 `binary`

当前会先看：

- 两边是不是 immediate

如果是，就先尝试常量求值，直接收成：

- `MATERIALIZE_IMM`

否则再看：

- 这是 compare 还是普通算术/位运算
- 两边是否有 immediate

然后进入：

- `ALU`
- `ALU_IMM`
- `CMP`
- `CMP_IMM`

所以这层不是“binary 统一留下来再说”，而是已经按 family 真正拆开了。

### 9.3 一个最小 value 例子

假设 `machine_ir` 里有：

```text
bb.0:
  reg.1 = mov 7
  reg.2 = binary add reg.1, 1
  reg.3 = binary eq reg.2, 0
```

到了 `machine_select`，概念上更接近：

```text
bb.0:
  reg.1 = materialize_imm 7
  reg.2 = alu_imm add reg.1, 1
  reg.3 = cmp_imm eq reg.2, 0
```

这里真正“新增”的不是 CFG 信息，而是：

- `mov` 被拆成 copy vs immediate-materialization
- `binary` 被拆成 alu vs cmp，再细分是否 immediate-specialized

---

## 10. memory lowering：`load/store` 到底怎么选

`machine_select_lower_memory.inc` 当前策略很直白，但也很重要。

### 10.1 `load_*`

- `load_local` -> `LOAD_LOCAL`
- `load_global` -> `LOAD_GLOBAL`

### 10.2 `store_*`

如果写入值是 immediate，则会进入：

- `STORE_LOCAL_IMM`
- `STORE_GLOBAL_IMM`

否则进入：

- `STORE_LOCAL`
- `STORE_GLOBAL`

所以即使是 memory family，这层也在做一件 very real 的 specialization：

`store slot, imm`

和

`store slot, register/spill`

已经不是一个 selected op 了。

### 10.3 一个最小 memory 例子

假设 `machine_ir` 里有：

```text
bb.0:
  reg.0 = load_local a.0
  store_global g.0, 42
  store_local b.0, reg.0
```

到了 `machine_select`，概念上会更接近：

```text
bb.0:
  reg.0 = load_local a.0
  store_global_imm g.0, 42
  store_local b.0, reg.0
```

所以这层在 memory 这边做的，不是“更底层的地址计算”，而是：

`slot memory op family specialization`

---

## 11. call lowering：为什么 call family 这么多

`machine_select_lower_call.inc` 当前最值得讲的，就是它为什么已经拆出这么多 call family。

当前逻辑主要看两个维度：

1. 参数里是否有 immediate
2. 结果是否落在 spill slot

于是会得到：

- `CALL`
- `CALL_IMM`
- `CALL_SPILL`
- `CALL_IMM_SPILL`
- `CALL_VOID`
- `CALL_VOID_IMM`

所以这一层已经开始把 calling shape 变成 selected op identity 的一部分。

### 一个最小 call 例子

假设 `machine_ir` 里有：

```text
spill.0 = call foo(reg.1, 7)
call bar(reg.2)
```

到了 `machine_select`，概念上会更接近：

```text
spill.0 = call_imm_spill foo(reg.1, 7)
call_void bar(reg.2)
```

这里新增的信息是：

- “有没有 immediate arg”
- “结果是不是 spill”

已经不再只是 op payload，而是 selected family 本身。

---

## 12. control lowering：这一块其实最有意思

`machine_select_lower_control.inc` 是当前最值得读的一块，因为它不只是把 terminator 名字换一下。

它至少会识别这几种情况。

### 12.1 constant branch

如果 `MachineIrTerminator` 的 branch 条件本身就是 immediate：

```text
br 1, bb.1, bb.2
```

那它会直接 lower 成：

```text
jmp bb.1
```

这说明 `machine_select` 在 lowering 阶段就已经带一点 control simplification。

### 12.2 materialized-boolean branch

如果 block 末尾是：

```text
reg.3 = mov 0
br reg.3, bb.1, bb.2
```

那么当前层会识别出：

- branch 条件其实只是刚被 materialize 的布尔 immediate

于是会把这条尾部 `mov` 删除，再直接 lower 成 jump。

所以这已经不是“选 terminator family”这么简单，而是：

`边看尾部 op，边重写 terminator`

### 12.3 compare-produced branch

如果 block 末尾是：

```text
reg.3 = binary eq reg.1, 0
br reg.3, bb.1, bb.2
```

那么当前层会：

1. 识别出 `br` 的条件正是上一条 compare 的结果
2. 删除那条尾部 compare-producing op
3. 直接把 terminator lower 成：
   - `COMPARE_BRANCH`
   - 或 `COMPARE_BRANCH_IMM`

概念上就像：

```text
cmpbr_imm eq reg.1, 0, bb.1, bb.2
```

这一点很关键，因为这说明 `machine_select` 已经不仅仅是把 instruction 分成 selected op family，它也在做：

`compare + branch fusion-like shaping`

### 12.4 一个最小 control 例子

假设 `machine_ir` 里是：

```text
bb.0:
  reg.2 = binary ne reg.1, 0
  br reg.2, bb.1, bb.2
```

到了 `machine_select`，会更接近：

```text
bb.0:
  cmpbr_imm ne reg.1, 0, bb.1, bb.2
```

也就是说：

- 原来的 compare-producing instruction 消失了
- 语义被吸收到 selected terminator 里

这正是当前 control lowering 最值得抓的一点。

---

## 13. cleanup：它确实也有一些“优化样式”

如果你问：

- “`machine_select` 里是不是也有一些优化代码？”

答案也是：

- `有`

但当前更准确的叫法仍然是：

- `selected-side canonicalization / cleanup`

不是完整 backend optimizer。

### 13.1 cleanup driver 的味道

把 `machine_select_cleanup.inc` 的主流程压成伪代码，大致就是：

```text
for each block:
  repeat until fixed point:
    canonicalize op shapes
    forward trivial defs in block
    remove dead pure defs in block
    canonicalize terminator shape
```

所以当前 cleanup 线是：

- block-local
- fixed-point
- value/control-aware

而不是纯字符串式规整。

### 13.2 op shape canonicalization

cleanup 会继续根据 operand 形状修正 selected op family。

比如：

- `COPY` 的源如果其实是 immediate，会改成 `MATERIALIZE_IMM`
- `ALU` / `CMP` 如果一边变成 immediate，会切到 `_IMM`
- `STORE_*` 如果写入值变成 immediate，会切到 `_IMM`
- `CALL*` family 也会根据 arg/result 形状重新归类

所以 selected op family 不是只在 lowering 那一刻决定一次，cleanup 还会继续把它收稳。

### 13.3 trivial def forwarding

cleanup 会在 block 内前传两类非常窄但很有用的定义：

- `COPY`
- `MATERIALIZE_IMM`

最小例子：

```text
bb.0:
  reg.1 = copy reg.0
  reg.2 = alu_imm add reg.1, 1
  ret reg.2
```

清理后更接近：

```text
bb.0:
  reg.2 = alu_imm add reg.0, 1
  ret reg.2
```

然后原来的 `reg.1 = copy reg.0` 就能被删掉。

### 13.4 pure dead-def removal

cleanup 还会删 block 内没有可见 use 的 pure value op。

当前被当成 pure value op 的，主要是：

- `COPY`
- `MATERIALIZE_IMM`
- `ALU`
- `ALU_IMM`
- `CMP`
- `CMP_IMM`
- `LOAD_LOCAL`
- `LOAD_GLOBAL`

所以：

- effectful store 不会被这里乱删
- call 也不会被这里当纯值直接抹掉

### 13.5 terminator shape canonicalization

cleanup 不只看 op，也看 terminator：

- `RETURN` / `RETURN_IMM` / `RETURN_SPILL` 会按返回值形状重新归类
- `BRANCH` 若条件变成 immediate，会继续塌成 `JUMP`
- `COMPARE_BRANCH` / `_IMM` 若比较双方都变成立即数，会继续塌成 `JUMP`

所以当前 cleanup 的味道是：

`value shape 改了，selected terminator shape 也要跟着收稳`

---

## 14. 一个最小综合例子：lowering + cleanup 连起来看

假设 `machine_ir` 里有：

```text
bb.0:
  reg.1 = mov 0
  reg.2 = binary add reg.1, 1
  reg.3 = binary ne reg.2, 0
  br reg.3, bb.1, bb.2
```

第一步，value lowering 会先把它翻成更 selected 的形状：

```text
bb.0:
  reg.1 = materialize_imm 0
  reg.2 = alu_imm add reg.1, 1
  reg.3 = cmp_imm ne reg.2, 0
  br reg.3, bb.1, bb.2
```

第二步，control lowering 如果识别出“最后一条 compare 正好给 branch 用”，就会继续收成：

```text
bb.0:
  reg.1 = materialize_imm 0
  reg.2 = alu_imm add reg.1, 1
  cmpbr_imm ne reg.2, 0, bb.1, bb.2
```

第三步，cleanup 还可能继续把 `reg.1` 这类 trivial def 前传掉，于是概念上又会更接近：

```text
bb.0:
  reg.2 = alu_imm add 0, 1
  cmpbr_imm ne reg.2, 0, bb.1, bb.2
```

所以 lesson 口径上，最该抓住的是这句话：

`machine_select = 先把 generic machine-ir op/terminator 选成 family，再把这些 family 收到更稳定的局部形状`

---

## 15. 这层已经不只是 dump：query / report surface 也很重要

很多人第一次读 `machine_select`，会以为它只是：

- lower 完
- dump 一份文本

但现在真实代码已经明显不止这样。

从：

- `src/machine/lowering/machine_select/machine_select_query.inc`
- `src/machine/lowering/machine_select/machine_select_report.inc`
- `include/machine/select.h`

可以看出它已经有两类很正式的 consumer surface。

### 15.1 raw query

raw query 这边已经能直接做：

- program summary
- function lookup by name
- function artifact / function summary
- block lookup by `function_name + block_id`
- block summary
- terminator summary

也就是说，后面的 consumer 不需要自己重扫整棵 selected program 才知道：

- `main` 有几个 block
- `bb.0` 有几条 op
- 这个 block 的 terminator 是 plain branch 还是 compare branch

### 15.2 report artifact

report 这边更进一步，已经把很多“统计/筛选/导航”固化成 artifact：

- `function_summaries`
- `block_summaries`
- `functions_with_calls`
- `functions_with_spills`
- `functions_with_memory_ops`
- `functions_with_branches`
- block filter by:
  - `CALLS`
  - `MEMORY_OPS`
  - `RETURNS`
  - `JUMPS`
  - `BRANCHES`
  - `COMPARE_BRANCHES`

这说明当前 `machine_select` 已经不是：

`lowering only`

而是：

`selected artifact + query API + report API`

### 15.3 refresh 也已经是正式能力

`machine_select_lower_report_refresh(...)` 这个点特别值得一提。

它说明 report 不是一次性导出的死文本，而是：

- report-owned program 发生局部改动后
- 还能重新刷新 summary / filter / dump

这类设计对后面继续往 `machine_layout`、`machine_emit` 推非常重要，因为它意味着：

`selected artifact 已经开始支持后续本地后处理，而不只是“做完就丢”`

---

## 16. 测试里最值得读的几组 case

如果你要顺着 lesson 真去读代码，我最推荐直接盯：

- `tests/machine/lowering/machine_select/machine_select_test.c`

然后按主题看这几组测试。

### 16.1 最基础 smoke case

- `test_machine_select_lower_machine_ir_smoke`

它锁的是最基础的：

- `load_local`
- `binary add + immediate`
- `store_global`
- `return`

会被翻成：

- `load_local`
- `alui`
- `store_global`
- `ret`

这组测试很适合拿来建立“最普通一条 selected path 长什么样”的第一印象。

### 16.2 control lowering 代表 case

- `test_machine_select_lowers_compare_branch_terminator`
- `test_machine_select_lowers_compare_branch_immediate_terminator`
- `test_machine_select_folds_constant_branches_to_jump`
- `test_machine_select_folds_constant_compare_branches_to_jump`
- `test_machine_select_folds_materialized_boolean_branches_to_jump`

这一组正好对应 lesson 里刚才讲的 control lowering 主线：

- compare-producing branch 被吸进 terminator
- constant branch 直接塌成 jump
- materialized boolean branch 也能塌成 jump

所以如果你想确认“我不是在课堂上口嗨，它代码里真这么干”，这组测试最直接。

### 16.3 call family 代表 case

- `test_machine_select_lowers_value_call_with_immediate_arg_family`
- `test_machine_select_lowers_spill_result_call_families`
- `test_machine_select_distinguishes_void_calls`

这组测试把 call family 为什么要拆这么细讲得很清楚。

例如：

```text
reg.0 = calli f(7)
spill.0 = calli_spill g(9)
call_void sink()
call_voidi sink(5)
ret
```

你从这些 dump 就能直接看出：

- immediate arg
- spill result
- void call
- bare return

都已经被编码进 selected family 了。

### 16.4 report / filter 代表 case

- `test_machine_select_builds_report_artifact`
- `test_machine_select_report_refresh_surface`
- `test_machine_select_report_query_surface`
- `test_machine_select_report_block_filter_surface`
- `test_machine_select_report_dump_surface`

这组测试说明：

- report 不是摆设
- query surface 不是摆设
- filter surface 也不是摆设

它们已经是真正被 regression 锁住的公共能力。

### 16.5 输入契约代表 case

- `test_machine_select_rejects_phi_input`

这个测试很适合拿来提醒自己：

`machine_select` 现在不是吃任意 raw machine-ir 的万能层。

它默认假设输入已经是：

- phi-free
- 经过 canonicalization

这也和前面 lesson 里的“prefer canonicalized machine-ir”完全对上。

---

## 17. 这层到底“新增 / 改了 / 翻译了”什么

这一节建议你拿来和上游 `machine_ir` 对照着记。

### 17.1 新增了什么

新增的是：

- selected op families
- selected terminator families
- selected-side summary / report / filter surface

也就是它不只是新增了几个 enum，而是新增了一整套“怎么被消费”的 artifact 形状。

### 17.2 改了什么

它把 `machine_ir` 里那种比较 generic 的：

- `mov`
- `binary`
- `call`
- `load/store`
- `ret/jmp/br`

改成了更接近 downstream 消费者想看到的：

- `copy / materialize_imm`
- `alu / alu_imm / cmp / cmp_imm`
- 多种 call family
- `return / jump / branch / compare_branch`

### 17.3 翻译了什么

最值得记的几条翻译表，可以直接压成：

```text
mov imm                  -> materialize_imm
mov non-imm              -> copy
binary compare           -> cmp / cmp_imm
binary non-compare       -> alu / alu_imm
binary const/const       -> materialize_imm
call + imm arg           -> call_imm / call_void_imm
call result in spill     -> call_spill / call_imm_spill
store slot, imm          -> store_*_imm
compare result + branch  -> compare_branch / compare_branch_imm
constant/materialized br -> jump
```

如果把这张表记住，你基本就记住了当前 `machine_select` 的主贡献。

---

## 18. 和上下游的边界

这层最容易和两边混。

### 18.1 相对 `machine_ir`

- `machine_ir`
  - 关心“值放哪儿、CFG 还长什么样、generic machine-facing op 长什么样”
- `machine_select`
  - 关心“这些 generic op 到底选成哪类 selected family”

所以两层的核心差别不是“谁更底层一点点”，而是：

- `machine_ir` 给 stable generic artifact
- `machine_select` 给 stable selected artifact

### 18.2 相对 `machine_layout`

到了 `machine_select` 为止，还没有回答：

- block 线性顺序怎么排
- 哪条 edge 走 fallthrough
- branch lowering 怎么利用物理布局

这些是 `machine_layout` 才开始正式回答的问题。

所以：

- `machine_select`
  - 先把 op / terminator family 选对
- `machine_layout`
  - 再把 block order / fallthrough / branch presentation 选对

### 18.3 和最终 `RISC-V` 方向的关系

这一点也最好明确写出来。

根据：

- `docs/ir-conventions.md`
- `docs/NEXT_STEPS.md`

仓库最终目标方向仍然是：

- `RISC-V`

当前仓库里虽然还能看到：

- `generic-elf32`
- `i386-preview`

这些 staging / preview surface，但 `machine_select` 的工程意义并不是“我们已经决定长期停在这些 preview family 上”，而是：

`先把 generic machine-facing op 收成更稳定的 selected family，为未来真正的 RISC-V lowering 建第一道边界`

所以这层可以理解成：

- 还不是最终 RISC-V 选指
- 但已经是 RISC-V 选指真正开始之前最重要的一层前置整理

---

## 19. 一段伪代码看完整主线

```text
select(machine_ir):
  require input is phi-free and reasonably canonicalized

  for each instruction:
    if mov:
      choose copy or materialize_imm
    if binary:
      maybe constant-fold
      choose alu / alu_imm / cmp / cmp_imm
    if load/store:
      choose local/global + immediate/non-immediate family
    if call:
      choose by {has immediate arg, result in spill?, void?}

  for each terminator:
    if branch on constant:
      jump
    else if branch on materialized boolean:
      delete tail mov and jump
    else if branch on compare-producing result:
      delete tail compare and choose compare_branch
    else:
      plain branch

  cleanup selected shapes to fixed point
  expose program/query/report artifacts
```

---

## 20. 一句话总结

`machine_select` 的核心价值，不是把名字换得更底层一点，而是把 `machine_ir` 从“generic machine-facing IR”推进成“selected family IR”：它已经真实地区分了 `copy/materialize_imm`、`alu/cmp`、多种 call/store family，还会做 compare-branch shaping 和 selected-side cleanup，并且已经长出了 query/report/filter surface，这正是后面继续走向 `machine_layout` 乃至未来 `RISC-V` 选指的第一层真实边界。
