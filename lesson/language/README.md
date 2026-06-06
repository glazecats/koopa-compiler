# Language Lessons

这条线回答的是：

`当前仓库的 -extension 语言特性到底开到了哪、为什么要单独开一条保守翻译线、以及这些特性怎么继续接回 parser / semantic / IR / backend`

和 `lesson/core/` 的关系可以先压成一句：

- `core`
  - 讲前中端的稳定基础能力
- `language`
  - 讲当前 language-feature round 在这些基础层上新增了什么

当前这条线最值得先记的，不是某一个单独语法点，而是一个统一原则：

- `-extension` 是**功能开关**
- 也是**保守翻译管线开关**

也就是说，很多语言特性现在已经能跑通：

1. `defer` / `fndefer` / `capdefer`
2. `unless`
3. non-capturing function values
4. first closure slice / returned closure follow-ups
5. `pair` / `struct`
6. aggregate parameter / return 扩张方向
7. conservative `float` slice

但这并不等于：

- `-riscv`
- `-perf`

已经默认接受这些语法。

---

## 1. 最推荐的阅读顺序

最推荐按下面顺序：

1. `extension_mode_lesson.md`
2. `defer_family_lesson.md`
3. `function_values_lesson.md`
4. `function_value_callee_lowering_lesson.md`
5. `higher_order_callable_lesson.md`
6. `closure_object_lesson.md`
7. `returned_callable_lesson.md`
8. `returned_closure_transport_lesson.md`
9. `callable_object_ir_lesson.md`
10. `closure_capture_callable_lesson.md`
11. `generic_function_values_lesson.md`
12. `recursive_signature_lesson.md`
13. `function_implementation_map_lesson.md`
14. `function_family_table_lesson.md`
15. `function_supported_code_lesson.md`
16. `function_evaluation_reuse_lesson.md`
17. `function_checkpoints_lesson.md`
18. `function_terminology_lesson.md`
19. `aggregate_lesson.md`
20. `aggregate_boundary_lesson.md`
21. `type_system_lesson.md`
22. `float_lesson.md`
23. `float_followup_lesson.md`

这个顺序的好处是：

- 先把 `-extension` 的统一边界看懂
- 再按“前门 -> binding -> payload -> object -> type -> numeric”顺着看
- 最后看目前扩张得最宽、也最容易误解的 float 线

如果你更想按“怎么实现”来读，而不是按 feature 名来读，当前更推荐这条实现路线图：

1. `extension_mode_lesson.md`
2. `recursive_signature_lesson.md`
3. `function_values_lesson.md`
4. `function_value_callee_lowering_lesson.md`
5. `higher_order_callable_lesson.md`
6. `returned_callable_lesson.md`
7. `returned_closure_transport_lesson.md`
8. `callable_object_ir_lesson.md`
9. `closure_capture_callable_lesson.md`
10. `closure_object_lesson.md`
11. `function_family_table_lesson.md`
12. `function_supported_code_lesson.md`
13. `function_evaluation_reuse_lesson.md`
14. `function_implementation_map_lesson.md`
15. `function_checkpoints_lesson.md`
16. `function_terminology_lesson.md`
17. `generic_function_values_lesson.md`
18. `type_system_lesson.md`
19. `aggregate_lesson.md`
20. `aggregate_boundary_lesson.md`
21. `float_lesson.md`
22. `float_followup_lesson.md`

这一版路线图背后的统一问题其实是：

```text
front door type shape
  -> static/dynamic binding
  -> payload transport
  -> callable object rebuild
  -> exactly-once evaluation safety
  -> shared type compatibility
  -> aggregate / float follow-up families
```

---

## 2. 每篇在讲什么

### `extension_mode_lesson.md`

重点看：

- 为什么仓库要把 language-feature round 放在 `-extension`
- 它和 `-riscv` / `-perf` 的职责边界
- 当前 conservative extension pipeline 大概是什么样

### `defer_family_lesson.md`

重点看：

- `defer`
- `fndefer`
- `capdefer`
- `unless`

这几个控制/清理相关扩展现在各自是什么意思、各自卡在哪一层。

### `function_values_lesson.md`

重点看：

- non-capturing function values
- closure literal 的第一版边界
- returned closure / callable-object IR 的关系

也就是：

`函数何时只是名字，何时开始真的像值 / 对象`

### `function_value_callee_lowering_lesson.md`

重点看：

- function-valued parameter 真正被 call 时怎么 lower
- 为什么第一阶段先走 specialization helper
- 为什么这仍然不是 generic indirect call

也就是：

`higher-order call 第一次真正能跑起来时，仓库到底是怎么保守翻译的`

### `higher_order_callable_lesson.md`

重点看：

- second / third / fourth / fifth-order function-valued parameter forwarding
- passthrough wrapper / local alias / helper-body evaluator 怎么把 shell collapse 到 leaf callable-object call
- returned dynamic higher-order 和 mixed closure actual arguments 当前收敛到了哪、哪里仍然不是 generic lambda

也就是：

`如果你想理解 pass/apply/relay/idh/choose 这些最近密集推进的 higher-order family，这篇是主入口`

### `closure_object_lesson.md`

重点看：

- first closure slice
- returned closure
- callable-object IR
- generic first-class function values 还没到哪里

也就是：

`callable value 何时开始逼 IR/object model 真正往下长`

### `returned_callable_lesson.md`

重点看：

- returned noncapturing / returned closure 现在怎么跨函数边界
- caller-side rebuild 为什么越来越像一条 explicit callable-object spine
- ternary / passthrough / mixed producer family 为什么会把这条线推得很深

也就是：

`callable 一旦先 return 再消费，仓库到底怎么把它重新接住`

### `returned_closure_transport_lesson.md`

重点看：

- `__retclosure_declslot / immediate / argslot / argcap / specarg` 这些 payload view 到底是什么意思
- producer-side ternary merge、local bounce、passthrough、reassign、zero-arg/void sibling 怎么保持 payload transport
- 为什么同一个 returned call expression 的 materialization 要复用，不能让 `make(3)` 因不同 consumer view 被重复求值

也就是：

`如果你想理解 returned closure 怎么从 make/pick 一路穿过绑定、合并、转发、实参、再次 return，先读这篇`

### `callable_object_ir_lesson.md`

重点看：

- canonical IR 里的 `shape + fn_make + call_indirect` 当前到底是什么合约
- `fn_code / fn_env / fn_shape` projection 和 verifier 的 `IR-VERIFY-074..090` 在守什么
- 为什么 lower/backend 看到 direct `__fnwrap_*` call 不等于 canonical callable-object path 没走

也就是：

`如果你想把 function lessons 里的 payload/rebuild 真接到 IR verifier contract，这篇是主桥`

### `closure_capture_callable_lesson.md`

重点看：

- closure 开始 capture function value / function-valued parameter 之后发生了什么
- returned parameter-capture family 为什么会把 closure 和 returned-callable 两条线接起来
- 为什么这仍然是 conservative callable-payload broadening，不是 fully generic closure runtime

也就是：

`closure payload 一旦不再只是标量，而是 callable，本地 helper / returned payload / rebuild spine 怎么一起工作`

### `generic_function_values_lesson.md`

重点看：

- generic first-class function values 的 end-state 长什么样
- `code + env + shape` 为什么是核心三元组
- 为什么这篇是 architecture authority，不是“已经全做完了”的宣告

也就是：

`现在这些 conservative slice 最终想收敛成什么统一模型`

### `recursive_signature_lesson.md`

重点看：

- function-type front door 为什么不再停在浅层 declarator
- parser / AST bridge 怎么重建更深的 recursive signature
- 为什么这条线其实是在给 higher-order function-value world 开前门

也就是：

`更深的 function-type 语法和 shape 到底是怎么先被放进系统里的`

### `function_implementation_map_lesson.md`

重点看：

- function 主线现在在源码里主要落在哪几层
- `semantic -> ir_lower_stmt -> ir_verify` 这三段各自守什么
- returned payload / descriptor / `fn_make + call_indirect` 真正该去哪里看

也就是：

`如果你想从 lesson 真回到代码里，function 这条线现在最该先翻哪几个文件`

### `function_family_table_lesson.md`

重点看：

- static / dynamic / returned / closure / higher-order function family 的总表
- 每类 source shape 大概走 specialization、payload/rebuild、还是 callable-object consumer
- 最近多 dynamic callable argument / mixed producer / higher-order shell 怎么放进同一张地图
- 怎么按 `SEMANTIC-*` / `IR-*` / `LOWER-IR-*` / `COMPILER-*` filter 分层验证 family 状态

也就是：

`如果你想知道“现在能写通哪些 function 代码、分别靠什么跑起来”，这篇是最快入口`

### `function_supported_code_lesson.md`

重点看：

- 当前 `-extension` 下 function value / closure / returned closure / higher-order 实际能写哪些 source family
- 哪些 runtime witness 已经比较稳，例如 local alias、dynamic rebinding、returned closure、closure captures callable、higher-order passthrough
- 怎么按 semantic / IR / lower-IR / compiler-driver 测试 filter 反查某个 source family 是否真的闭合
- 哪些看起来相近但还不能期待，例如 `auto` generic lambda、self-recursive callable、arbitrary aggregate callable transport

也就是：

`如果你想马上写例子或者判断某段代码是不是当前仓库该支持的，这篇是使用者视角的速查表`

### `function_evaluation_reuse_lesson.md`

重点看：

- returned-call payload materialization 为什么必须按同一个 AST call expression 复用
- 为什么不能按源码文本、callee 名、或者“看起来一样”合并两个 producer call
- side-effect 普通实参为什么会让 helper/body-eval shortcut 退出

也就是：

`如果你担心 make(3) 被重复翻译、getint() 被读两次、或者两个闭包实例被错合并，这篇专门讲安全边界`

### `function_checkpoints_lesson.md`

重点看：

- function 主线当前在哪些 surface 算“真闭合”
- 哪些 family 只过了 semantic，哪些已经锁到 IR/lower-IR/compiler/runtime
- 怎么用 semantic-only / IR+lower-IR / compiler-runtime 三层判定对齐 `function_supported_code_lesson.md`
- 如果再加新 family，当前仓库最常用的 checkpoint 闭合标准是什么

也就是：

`function 这条线现在到底哪些东西只是 admitted，哪些已经真的站稳了`

### `function_terminology_lesson.md`

重点看：

- `shape / descriptor / family / target-set / payload / rebuild` 这些词当前各自是什么意思
- `helper-backed` 和 `specialization` 到底差在哪
- 为什么 function 主线现在已经有一套相对稳定的内部词汇

也就是：

`如果你读 function lessons 或源码时老觉得词很像但不完全一样，这篇就是统一词汇表`

### `aggregate_lesson.md`

重点看：

- `pair`
- `struct`
- aggregate parameter / return
- 当前 shared type layer 的方向

也就是：

`仓库现在怎么在不引入完整 ABI/object model 的前提下，保守地支持复合值`

### `aggregate_boundary_lesson.md`

重点看：

- aggregate param / return
- why hidden-slot model still matters
- shared type layer

也就是：

`aggregate 什么时候已经越过 local scope，什么时候还没有变成完整 ABI`

### `type_system_lesson.md`

重点看：

- 为什么 `pair` / `struct` / `float` / function shape 需要共享 descriptor
- 为什么 assignment / init / return / param / member lookup 要共用 compatibility story
- 为什么这条线是 language round 里真正的“共享地基”

也就是：

`仓库现在不是只在加语法点，而是在把类型规则从 ad hoc 往统一层收`

### `float_lesson.md`

重点看：

- conservative float transport
- helper-backed float arithmetic
- explicit conversion
- condition / comparison / recursive pure-float family

也就是：

`float 现在到底开到了哪，哪些是故意没开的`

### `float_followup_lesson.md`

重点看：

- explicit conversion
- ternary / helper-membrane family
- recursive pure-float value / condition family
- helper-backed float follow-up family 的真实边界

也就是：

`float 不是只会 transport 了，但每一步扩张仍然是窄 family`

---

## 3. 和前后目录的关系

这条线最自然的上下游关系是：

- 上游：
  - `lesson/core/parser_lesson.md`
  - `lesson/core/semantic_lesson.md`
  - `lesson/core/ir_lesson.md`
  - `lesson/core/lower_ir_lesson.md`
- 中间：
  - 本目录 `language/*`
- 下游：
  - `lesson/ssa/README.md`
  - `lesson/machine/README.md`

也就是说，`lesson/language/` 最好被理解成：

- **不是**另一套独立编译器教程
- 而是当前仓库在已有 core/SSA/machine 结构上额外打开的语言特性主线

---

## 4. 当前最重要的统一口径

如果你只记一件事，建议记这个：

`-extension` 现在追求的是“正确、保守、可回归锁定的语言能力”，不是“默认走最激进优化后还能凑巧工作”。`

所以读这条线时，最好一直带着三个问题：

1. 这个特性是不是只在 `-extension` 下开放？
2. 当前 lowering 是 structural / helper-backed / specialization-backed，还是已经有了更通用的 IR/object model？
3. 这条能力是“已闭合 checkpoint”，还是“下一阶段设计 authority 已经写好，但实现还故意没开”？

如果你现在读 lesson 更关心“怎么实现”，那这条目录最值得先抓的七个关键词就是：

1. `desugar`
2. `specialization`
3. `payload`
4. `callable-object`
5. `rebuild`
6. `flatten`
7. `helper-backed`

它们分别大致对应：

- `unless`
- function-value callee lowering
- returned callable / closure transport
- `shape + fn_make + call_indirect`
- caller-side returned callable / closure rebuild
- aggregate line
- float line

---

## 5. 读完之后接哪里

最自然的两条下游路线是：

1. 想看这些特性最后怎么进 SSA/backend
   - 去 `lesson/ssa/README.md`
2. 想回头确认前端/IR 基础边界
   - 去 `lesson/core/README.md`
