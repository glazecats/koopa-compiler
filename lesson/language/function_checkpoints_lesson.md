# Function Checkpoints Lesson（compiler_lab）

> 目标：把当前 function 主线按 checkpoint surface 写成一张总表。它不再主要讲“怎么设计”，而是讲“哪类 function-value/closure/returned shape 现在在 parser、semantic、IR、lower-IR、compiler-driver、extension-runtime 上到底锁到了哪里”。

## 一句话定位

这条线回答的是：

`当前 function 主线已经很宽，但到底哪些 family 在哪一层是真闭合、哪些还只是前半段过了，这件事现在怎么系统看。`

## 1. 为什么这篇值得单独存在

现在 function 这条线最大的问题已经不是“没有内容”，而是：

- 内容太多
- family 太多
- 容易把“semantic 过了”误讲成“整条线闭合了”

所以这篇更像一个 checkpoint 地图：

```text
source family
  -> parser
  -> semantic
  -> canonical IR
  -> lower IR
  -> compiler-driver
  -> extension-runtime
```

## 2. 最稳的 surface 分层

当前最推荐按下面六层看：

1. `parser`
2. `semantic`
3. `canonical IR`
4. `lower IR`
5. `compiler-driver`
6. `extension-runtime`

有些 family 还会多看：

7. `ValueSSA`
8. `machine_ir / machine_select` report

但 function 主线里最常用的闭合口径，当前还是前六层。

## 3. 当前最主要的 family 组

可以先把 function 主线的 family 压成六组：

1. **static noncapturing**
   - function-valued parameter
   - local alias
   - direct callee
2. **dynamic noncapturing**
   - rebinding
   - returned noncapturing
   - higher-order passthrough
3. **closure local**
   - local closure
   - closure reassignment
   - declaration-only placeholder from closure
4. **returned closure**
   - bind-and-call
   - direct returned-call
   - forwarding into function-valued parameter
5. **closure captures callable**
   - static local callable capture
   - function-parameter capture
   - returned parameter capture
6. **mixed producer / mixed consumer**
   - ternary returned-call consumer
   - multi-dynamic callable arguments
   - mixed dynamic/static producer composition

## 4. 一个最小 checkpoint 表怎么读

最实用的阅读方式是先问：

```text
这个 family 现在只是 source admitted，
还是 canonical IR / lower IR / compiler runtime 都已经锁住？
```

所以你可以把当前 function lesson 的很多句子，翻译成下面这种表意：

```text
semantic green
!=
whole feature closed
```

## 5. 现在已经比较“真闭合”的几组

### 5.1 static noncapturing direct-callee

最典型：

```c
int apply(int f(int), int x) { return f(x); }
return apply(add1, 41);
```

当前最稳定的闭合口径是：

- parser
  - 绿
- semantic
  - 绿
- canonical IR
  - specialized helper shape 绿
- lower IR
  - 绿
- compiler-driver
  - 绿
- extension-runtime
  - 绿

当前如果你想在真实测试里先找到这一组，最值得直接搜的 surface 是：

- `tests/semantic/semantic_regression_test.c`
- `tests/compiler/compiler_driver_test.c`

最常见的名字模式一般会带：

- `function_value`
- `direct_callee`
- `under_extension`

### 5.2 returned noncapturing bind-and-call / immediate-call

最典型：

```c
int f(int)=pick();
return f(41);
```

以及：

```c
return pick()(41);
```

当前 lesson 级最稳口径是：

- parser / semantic
  - 绿
- canonical IR
  - tag family 或显式 `shape + fn_make + call_indirect` 已锁
- lower IR
  - bridged call shape 绿
- compiler-driver
  - 绿
- extension-runtime
  - 对对应 family 已有 focused 绿面

这类 family 现在最值得直接搜的测试 surface 通常是：

- `tests/semantic/semantic_regression_test.c`
  - `returned_function_value_parameter_*`
- `tests/lower_ir/lower_ir_regression_test.c`
  - returned bind-and-call / immediate-call family
- `tests/compiler/compiler_driver_test.c`
  - returned noncapturing execution witness

### 5.3 returned closure bind-and-call / forwarding

最典型：

```c
int f(int)=make(3);
return f(4);
```

和：

```c
return apply(f, 4);
```

当前最稳口径是：

- canonical IR
  - `shape + fn_make make__retclosure_* + call_indirect`
- lower IR
  - 允许 bridged direct wrapper call
- compiler-driver
  - 绿
- extension-runtime
  - 对 focused family 绿

这类 family 现在最值得直接搜的测试 surface 通常是：

- `tests/semantic/semantic_regression_test.c`
  - `returned_closure_*`
- `tests/lower_ir/lower_ir_regression_test.c`
  - returned closure bind/forward family
- `tests/compiler/compiler_driver_test.c`
  - returned closure driver witness

## 6. 当前最能代表“复杂但已 landed”的几组

### 6.1 closure captures function-valued parameter

最典型：

```c
int wrap(int f(int), int x){
  int g(int)=closure [f] int (int y){ return f(y); };
  return g(x);
}
```

当前最实用的 checkpoint 口径是：

- semantic
  - 绿
- canonical IR
  - specialization + callable-object path 绿
- lower IR
  - 绿
- compiler-driver
  - 绿

这里如果想在真实测试里快速定位，一个非常值的名字就是：

- `test_semantic_accepts_static_function_value_capture_inside_closure_under_extension`

### 6.2 returned function-parameter capture

最典型：

```c
int make(int f(int))(int){ return closure [f] int (int y){ return f(y); }; }
```

当前口径是：

- parser / semantic
  - 绿
- canonical IR
  - returned payload + `fn_make __fnwrap_closure_make__retclosure_*` + `call_indirect`
- lower IR
  - 绿
- compiler-driver
  - 绿

这组现在很值得直接点名的测试名是：

- `test_lower_ir_accepts_returned_function_parameter_capture_inside_closure_under_extension`
- `test_compiler_accepts_returned_function_parameter_capture_inside_closure_under_extension`

### 6.3 ternary returned-call actual-argument consumer

最典型：

```c
return apply(c ? pick(1, add1) : pick(0, add1), 4);
```

这组现在最重要的 lesson 判断是：

- semantic 不是唯一难点
- canonical IR / lower IR 上 caller-side payload materialization 也必须绿
- compiler-driver 也要绿，才能说 family 真闭合

这类 family 现在在 driver / lower-IR 测试里的典型关键词通常是：

- `ternary`
- `returned`
- `actual_argument`
- `under_extension`

### 6.4 higher-order explicit signature family

最典型：

```c
int apply(int f(int), int x){ return f(x); }
int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }
return pass(apply, add1, 41);
```

当前 checkpoint 口径是：

- parser / semantic
  - nested function signature metadata 必须保留
- canonical IR
  - specialization ladder 或 callable-object leaf 必须出现
- lower IR / compiler-driver
  - second-order 到 several finite higher-order families 都有 focused coverage

最值得直接搜的测试关键词：

```text
SECOND-ORDER-FNVAL-FORWARD
SECOND-ORDER-CLOSURE-FNVAL-FORWARD
SECOND-ORDER-ZERO-FNVAL-FORWARD
SECOND-ORDER-ZERO-VOID-FNVAL-FORWARD
THIRD-ORDER-FNVAL-FORWARD
FOURTH-ORDER-FNVAL-FORWARD
FIFTH-ORDER-FNVAL-FORWARD
```

这里的闭合标准不是“任意高阶类型都支持”，而是：

`明确写出的 finite nested signature 可以跨 semantic / IR / lower-IR / compiler-driver 站住。`

### 6.5 returned closure producer-side transport family

最典型：

```c
int wrap(int c, int d)(int){
  int h(int)=pick(5,c);
  int k(int)=pick(7,c);
  int m(int)=d ? h : k;
  return m;
}
```

当前 checkpoint 口径是：

- canonical IR / lower IR
  - 应该能看到两个 producer call 和 merged payload transport
- compiler-driver
  - immediate / actual / bind-return 等 focused siblings 已有覆盖
- exactly-once
  - 同一个 returned call expression 不能因为 `__retclosure_*` view 不同被重复 materialize

最值得直接搜的测试关键词：

```text
DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE
DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE
DYNAMIC-RETURNED-CLOSURE-LOCAL-BOUNCE
DYNAMIC-RETURNED-CLOSURE-PASSTHROUGH
DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE
DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE
```

这组最容易误判：

```text
two different source producer calls
  -> should appear twice

same AST call expression consumed through multiple views
  -> should materialize once then reuse/copy payload
```

### 6.6 callable-object verifier contract

如果某条 family 声称已经进入 explicit callable-object IR，至少应该能解释：

```text
shape shape.N(...)
fn_make
call_indirect
```

并且 verifier surface 里有对应 reject/accept contract。

最值得直接搜：

```text
test_ir_verifier_rejects_fn_make_missing_shape
test_ir_verifier_rejects_call_indirect_arg_mismatch
test_ir_verifier_rejects_call_indirect_non_callable_temp
test_ir_verifier_accepts_call_indirect_with_moved_callable_temp
IR-VERIFY-076
IR-VERIFY-086
IR-VERIFY-088
IR-VERIFY-090
```

这类测试不回答“source 能不能写”，它回答：

`一旦 lowering 产出 callable-object IR，这个 IR 本身是不是合法。`

## 7. 当前最像“测试地图”的伪代码

如果你以后要加新 function family，最稳的 checkpoint 脑图可以直接写成：

```text
for one new function family:
    1. parser admits source
    2. semantic admits or rejects with right boundary
    3. canonical IR shows intended helper/payload/object shape
    4. lower IR preserves that contract honestly
    5. compiler-driver accepts and executes focused witness
    6. extension-runtime stays green if the family is runtime-facing
```

这就是现在 function 主线最常见的“闭合定义”。

更实用一点，可以按 family 选测试层：

```text
syntax/front-door family
    -> parser / semantic regression

payload/rebuild family
    -> IR + lower-IR regression first

runtime callable behavior
    -> compiler-driver / extension-runtime

explicit object contract
    -> ir_verifier_test

downstream translation-only shape
    -> ValueSSA / machine_ir / machine_select focused report tests
```

## 7.1 三层闭合判定：怎么和 supported-code 速查表对齐

`function_supported_code_lesson.md` 是从使用者角度说“能写什么”，这篇则负责给它一个更硬的判定规则。

最稳的三层判断是：

```text
semantic only:
    可以说 front door / type shape 已开
    不要放进“稳定能写通”主列表

IR + lower-IR:
    可以说 lowering contract 有 checkpoint
    还要看 driver/runtime 是否需要额外保证

compiler-driver / extension-runtime:
    才适合说当前 extension 路径下“能写通”
```

也就是说：

```text
SEMANTIC-SECOND-ORDER-*
```

和：

```text
IR-SECOND-ORDER-*
LOWER-IR-SECOND-ORDER-*
COMPILER-SECOND-ORDER-*
```

不是同一句话。

同理：

```text
IR-DYNAMIC-RETURNED-CLOSURE-*
```

说明 payload / callable-object / materialization shape 被锁住；但如果某个 family 还没有 compiler/runtime 面，就应该在 lesson 里说成：

`lowering checkpoint exists`

而不是：

`user-facing runtime family is fully closed`

## 7.2 verifier / report side 也在哪些地方值得看

虽然 function 主线最常用的闭合口径还是前六层，但当前有两组后半段 surface 也很值：

1. verifier
   - `tests/ir/ir_verifier_test.c`
   - 特别值得直接点名：
     - `test_ir_verifier_rejects_fn_make_missing_shape`
     - `test_ir_verifier_rejects_call_indirect_arg_mismatch`
2. machine/report
   - `tests/machine/lowering/machine_ir/machine_ir_test.c`
   - 一个很典型的 focused 入口是：
     - `test_machine_ir_translation_only_preserves_mixed_dynamic_closure_function_argument_consumers`

也就是说，function 线现在已经不只是“前半段 admission + driver 跑通”。

它还开始在：

- explicit callable-object verifier
- translation-only machine/report artifact

上单独锁当前 shape。

## 8. 哪些 family 还更像“进行中”

当前 lesson 里最该保守说的，通常是这些方向：

1. larger target-set dynamic combination
2. deeper higher-order mixed producer trees
3. broader closure-object transport beyond currently locked payload families
4. fully generic ordinary value-position function object world

也就是说，当前不是“function 线没做完”，而是：

`很多高价值 conservative family 已经闭合，但 generic end-state 还在继续逼近。`

## 9. 读完后接哪篇

最自然往下接：

1. `function_implementation_map_lesson.md`
2. `callable_object_ir_lesson.md`
3. `returned_closure_transport_lesson.md`
4. `function_family_table_lesson.md`
5. `function_evaluation_reuse_lesson.md`
6. `returned_callable_lesson.md`
7. `closure_capture_callable_lesson.md`
8. `core/tests_lesson.md`

因为这几篇正好分别回答：

- 代码在哪
- family 怎么分
- exactly-once 求值边界怎么守
- payload/rebuild 怎么做
- capture-callable 怎么做
- 当前到底被哪些 surface 锁住
