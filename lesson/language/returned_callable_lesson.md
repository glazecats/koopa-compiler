# Returned Callable Lesson（compiler_lab）

> 目标：专门讲当前 language round 里最复杂、也最容易被误讲的一条线，也就是 returned noncapturing function value、returned closure、mixed returned producers、caller-side callable-object rebuild 现在到底推进到了哪。

## 一句话定位

这条线回答的是：

`一个 callable 一旦不是“当场 call 掉”，而是要先 return、再在 caller 那边继续流动和调用，仓库当前到底靠什么把它保守地接住。`

## 1. 为什么这篇需要单独存在

`function_values_lesson.md` 和 `closure_object_lesson.md` 已经讲了两件大事：

- 函数开始像值
- closure / callable object 开始逼 IR 往下长

但最近这条线真正爆炸式扩张的地方，其实是：

- returned noncapturing function values
- returned closures
- mixed returned producers
- returned actual-argument consumers
- ternary / passthrough / higher-order wrapper 里的 rebuild

也就是说，当前最难的已经不是：

`callable 能不能存在`

而是：

`callable 离开 producer 之后，还能不能被 caller 正确重建并继续用`

## 2. 最小 mental model：先把“返回的不是普通 int”想清楚

最简单的 returned noncapturing 形状是：

```c
int pick()(int) {
  return add1;
}

int main() {
  int f(int) = pick();
  return f(41);
}
```

第一层 lesson 口径可以先压成：

```text
callee:
  produce "which callable family is this?"

caller:
  receive returned callable payload
  rebuild one local callable view
  then call through that rebuilt view
```

所以 returned callable 的关键不是“return 语法能不能 parse”，而是：

1. producer 返回什么隐藏信息
2. caller 怎么 materialize 本地 callable
3. 后续 direct call / forwarding / actual-argument use 怎么接着走

## 3. returned noncapturing：先返回 family，不先返回 generic object

当前最早 landed 的 returned noncapturing line，并不是 generic function object。

更准确地说，它更像：

```text
pick(...)
  returns one conservative callable payload
    = known family tag / known target-set info

caller:
  bind local function-typed slot
  if needed, branch to the right specialized helper
```

这就是为什么当前 returned noncapturing 线能支持很多 shape：

1. `int f(int)=pick(); return f(41);`
2. `return pick()(41);`
3. `int g(int)=pick(1); return g(40);`
4. `return pick(1)(40);`

但 lesson 上仍然不能把它讲成：

`已经有一个 fully generic runtime code pointer ABI`

## 4. returned closure：payload 不只是“哪个 helper”，还带 capture

一旦进入 returned closure，问题就从：

- 返回哪个 callable family

变成：

- 返回哪个 family
- 还要返回哪些 capture payload

最小 shape 大概像：

```c
int make(int x)(int) {
  return closure [x] int (int y) { return x + y; };
}

int main() {
  int f(int) = make(3);
  return f(4);
}
```

lesson 里最稳的记法是：

```text
returned payload =
  family identity
  + copied capture slots
```

然后 caller 再做：

```text
local callable = rebuild(family identity, copied capture slots)
```

这也是为什么 returned closure 一打开，仓库就必须更认真面对：

- env/payload layout
- hidden return slots
- caller-side rebuild path

## 5. 为什么最近会一直提到 `fn_make + call_indirect`

当前 returned callable 线最近最大的变化，不只是“又多支持几个 witness”。

真正更重要的是：

`caller-side rebuild 开始更明确地收敛到 explicit callable-object spine`

也就是：

- `shape`
- `fn_make`
- `call_indirect`

一个很稳的 lesson 级伪代码可以写成：

```text
payload = call producer(...)

obj = fn_make(
  resolved_code_or_wrapper,
  rebuilt_env_or_capture_slots,
  callable_shape
)

result = call_indirect(obj, visible_args...)
```

当然，当前 live tree 还允许某些 downstream surface 在证明 env/target pair 之后再折回 direct call。

但 lesson 里最重要的是先记住：

`returned callable 这条线现在已经不是“靠局部 helper glue 临时顶住”，而是在逐步形成统一 rebuild spine。`

## 5.1 伪代码：returned payload 和 caller-side rebuild

如果把这条线压成最小实现模型，一个常见的保守形状是：

```text
callee side:
    payload.tag = selected_family
    payload.capture_0 = copied_capture_0
    payload.capture_1 = copied_capture_1
    return payload

caller side:
    payload = call producer(...)
    env = materialize_env_slots(payload.capture_*)
    code = resolve_wrapper_from_tag(payload.tag)
    obj = fn_make(code, env, shape)
    return call_indirect(obj, visible_args...)
```

这里最关键的不是某个字段名字，而是三段式：

1. producer 写 payload
2. caller rebuild callable view
3. final consumer 才真的 call

## 5.2 一个更贴近 live tree 的 payload 心智模型

当前 kept slice 并不总是 generic heap object。

更贴近现在 lesson 的伪代码是：

```text
returned_payload =
    family_tag
    + copied_scalar_capture_slots
    + optional captured-function-value tag slots
```

然后 caller 再决定：

```text
if payload is statically single-family:
    maybe collapse to direct wrapped call later
else:
    rebuild dynamic callable object and branch/select as needed
```

这能解释为什么有些 regression 会锁：

- canonical IR 上一定要看到 `shape + fn_make + call_indirect`

但：

- lower-IR / compiler-driver 之后又允许折回 direct wrapper call

因为“rebuild spine 是 contract”，而“不允许后面再优化”并不是 contract。

## 6. mixed returned producers：为什么最近提交会密集爆这条线

最近几十次语言线提交最密集的一个主题，就是：

`returned callable 不是只来自一种 producer family`

现在已经在往下打通的典型 family 包括：

1. returned noncapturing producer
2. returned closure producer
3. returned function-parameter-capture closure producer
4. ternary over returned producers
5. passthrough / wrapper / higher-order shell over returned producers
6. 同一次 call 里多个 function-valued argument 都来自 returned dynamic family

这意味着 caller-side 不能只会认一种：

```text
if producer == pick then ...
```

而是要越来越会做：

```text
discover actual producer family
reconstruct returned payload
rebuild callable object
dispatch with the right wrapper/specialization
```

## 6.1 伪代码：mixed producer 怎么 dispatch

一旦进入 mixed producer family，caller 侧的实现更像：

```text
lower_callable_actual(expr):
    if expr is direct returned producer:
        return materialize_payload(expr)

    if expr is passthrough(expr1):
        return lower_callable_actual(expr1)

    if expr is ternary(cond, lhs, rhs):
        lhs_payload = lower_callable_actual(lhs)
        rhs_payload = lower_callable_actual(rhs)
        return branch_select_payload(cond, lhs_payload, rhs_payload)

    fail_if_family_is_out_of_scope()
```

然后最终 consumer 再做：

```text
payload = lower_callable_actual(actual_expr)
obj = rebuild_callable_from_payload(payload)
emit_call(obj, visible_args...)
```

这也是为什么最近 `ternary / passthrough / wrapper` 会在 `NEXT_STEPS` 里反复出现：

`真正复杂的不是 call 本身，而是“找到真正 producer 并把 payload 完整带过来”。`

## 6.2 伪代码：多个 function-valued argument 为什么会变成 cross-product

最近这条线真正变复杂的一个原因，是：

```c
compose(pick(getint(), add1), pick(getint(), add1), 4)
```

这种 family 不再只有一个 dynamic callable argument。

更贴近 live tree 的心智模型是：

```text
payload_a = lower_callable_actual(arg0)
payload_b = lower_callable_actual(arg1)

for each tag choice of payload_a:
    for each tag choice of payload_b:
        emit specialized leaf call for (tag_a, tag_b)
```

lesson 级伪代码可以写成：

```text
lower_multi_callable_dispatch(callee, callable_args, scalar_args):
    payloads = map lower_callable_actual over callable_args
    combos = cartesian_product(payloads.tag_families)

    emit nested branches over payload tags
        each leaf -> direct specialized call for one combo
```

也就是说，当前 kept bridge 仍然不是 generic “runtime callable array + generic loop dispatch”。

更准确地说，它是在做：

`caller-side explicit combination dispatch`

## 6.3 mixed producer family 为什么更难

当 source 变成：

```c
compose(pick(getint(), add1), make(3), 4)
```

难点就不只是“两个 tag family”了，而是：

- 一个 argument 是 returned dynamic family
- 另一个 argument 是 static returned closure producer

当前更贴近 live tree 的伪代码是：

```text
payload_a = dynamic returned payload
payload_b = static returned closure payload

dispatch builder:
    no longer require all callable args come from same producer line
    instead normalize each arg into one payload view
    then reuse same leaf-call combination path
```

所以这条线现在 lesson 上最重要的实现判断是：

`producer kind` 正在先被归一成 payload view，然后 caller 再做统一 rebuild/dispatch。

## 7. ternary / passthrough / wrapper：为什么它们特别重要

如果 returned callable 只能出现在：

```c
int f(int)=make(3);
return f(4);
```

那它还只是比较“直”的 family。

但最近真正让这条线变成熟的，是它开始支持：

```c
return apply(c ? pick(1, add1) : pick(0, add1), 4);
```

或者：

```c
return wrap(idh(pickh(getint())))(add1, 41);
```

这类 family 的 lesson 价值在于，它们逼仓库解决：

1. producer 不是直接裸露在最终 consumer 前面
2. 返回值可能先经过 passthrough shell
3. ternary 两边都各自产生 returned payload
4. caller 需要先 materialize，再统一 rebuild，再最终 call

所以 returned callable 现在已经不是“只会一个 bind-and-call witness”的 feature。

## 8. declaration-only placeholder：为什么这是一个很大的信号

这一条也很值得单独记。

当前已经开的 shape 包括：

```c
int f(int);
f = pick();
return f(41);
```

以及：

```c
int f(int);
f = make(3);
return f(5);
```

这说明 callable value 现在已经不只是：

- 在 initializer 那一刻就固定出生

而开始支持：

- declaration-only placeholder
- first assignment later materializes callable family

lesson 上最重要的意义不是语法，而是：

`普通 local lifecycle 正在开始和 callable-object transport 接轨`

这也是 generic end-state 的一个明显前兆。

## 8.1 伪代码：placeholder local 怎么 materialize

一个最小实现视角可以写成：

```text
declare_local_function_placeholder(f):
    f.tag = empty
    f.payload = empty

assign_function_local(f, rhs):
    payload = lower_callable_value(rhs)
    f.tag = payload.tag
    f.payload = payload.capture_slots

call_function_local(f, args):
    obj = rebuild_callable_from_local(f.tag, f.payload)
    return call_or_dispatch(obj, args)
```

所以 declaration-only placeholder 的本质不是“多了一种声明语法”。

它真正说明的是：

`callable local 已经开始拥有独立 lifecycle，而不只是 initializer 那一瞬间的语法糖。`

## 8.2 为什么 placeholder 会逼出更统一的 transport 模型

当前 declaration-only placeholder 之所以重要，是因为它让：

- static target
- returned noncapturing
- returned closure
- closure-family alias

都开始共享一种更统一的问题：

```text
this local currently holds "some callable payload"
```

lesson 级实现心智模型可以写成：

```text
function_local_state =
  empty
  | static target binding
  | returned payload
  | closure-family payload
```

然后：

```text
assign_function_local(local, rhs):
    local.state = normalize_callable_rhs_to_payload(rhs)
```

这也是为什么 `NEXT_STEPS` 里会把它当成 generic function-object transport 的前兆，而不是一个小语法修补。

## 9. 源码地图：returned payload / rebuild 现在主要看哪

如果你要顺着源码看 returned callable 主线，当前最值得先去的地方是：

- `src/ir/ir_lower_stmt.inc`

重点 helper 现在已经很集中地围绕这些名字展开：

1. `ir_try_build_returned_function_value_descriptor(...)`
2. `ir_try_analyze_returned_function_value_family(...)`
3. `ir_prepare_returned_function_value_tag_local_and_targets(...)`
4. `ir_materialize_returned_function_object_descriptor(...)`
5. `ir_finalize_materialized_returned_closure_binding_descriptor(...)`

如果把这些 helper 的职责压成一句：

```text
family analysis
  -> prepare payload/tag locals
  -> materialize returned descriptor/view
  -> finalize binding descriptor for later call/use
```

## 9.1 读源码时最该抓的变量/结构

这条线当前读源码时，最值得盯着的是几类中间物：

1. `IrLowerReturnedFunctionValueFamilyInfo`
   - 代表“这个 returned family 到底属于哪一类”
2. `IrLowerFunctionObjectDescriptor`
   - 代表“当前 lower 到手里的 callable object/binding view”
3. `function_target_tag_local_id`
   - 代表动态 family 的 tag/local transport
4. `capture_local_ids`
   - 代表 closure payload 的 capture slot 位置

如果把 returned callable 的源码心智模型压成伪代码，其实和这些结构是对齐的：

```text
analyze family -> build descriptor -> materialize payload locals -> finalize binding view
```

这能帮助读者从 lesson 里的“payload / rebuild / call_indirect”跳回真实代码，而不是只记概念。

## 9.2 这条线现在最该盯的不是“某个 helper 名”，而是数据流

当前如果真要顺着代码理解 returned callable，最推荐盯的是这个数据流：

```text
Ast call expr
  -> returned family analysis
  -> tag/capture local preparation
  -> returned payload materialization
  -> function object descriptor finalize
  -> later call/use dispatch
```

也就是说，比起死背：

- `__retfn_*`
- `__retclosure_*`

更值得记住的是：

`当前 returned 主线已经有一个相对统一的数据流骨架。`

## 9.3 如果你要从 bug 现象反推源码，最推荐先问什么

returned callable 这条线现在最常见的错位，通常不是“功能完全缺失”，而是这几层里有一层没接上：

1. family analysis 错了
2. payload slot/tag local 放错了
3. caller-side rebuild 选错 branch/object path 了
4. downstream verifier/consumer 以为它还是 simpler family

所以最实用的排查顺序可以直接写成：

```text
family classify
  -> payload materialize
  -> descriptor finalize
  -> final dispatch/rebuild
```

这比一上来盯 helper 名更适合现在的 live tree。

## 10. 当前 still-kept boundary

这篇最重要的一部分，是把还没开的边界守住。

当前仍然不能把 returned callable 讲成：

1. 所有 producer family 都完全统一
2. 所有 closure transport 都已经 generic
3. 所有 returned callable consumer 都不再需要 source-shape-aware repair
4. aggregate field / global / array 里的 callable transport 已经自然工作

更准确的口径仍然是：

`当前是非常宽的 conservative returned-callable mainline，不是 full generic callable runtime。`

## 11. 最稳的分层记法

建议把 returned callable 线记成四层：

1. **returned noncapturing**
   - family/tag 先跨函数边界
2. **returned closure**
   - family + capture payload 一起跨函数边界
3. **caller-side rebuild spine**
   - `shape + fn_make + call_indirect`
4. **mixed producer / mixed consumer expansion**
   - ternary
   - passthrough
   - higher-order shell
   - multiple dynamic returned arguments

这样最不容易把：

- “现在这条线已经很宽了”
- 和
- “已经 fully generic 了”

混成一句话。

## 12. 读完后接哪篇

最自然往下接：

1. `closure_object_lesson.md`
2. `returned_closure_transport_lesson.md`
3. `callable_object_ir_lesson.md`
4. `function_evaluation_reuse_lesson.md`
5. `function_family_table_lesson.md`
6. `generic_function_values_lesson.md`
7. `type_system_lesson.md`

因为 returned callable 真正继续往前走时，最后一定会同时碰到：

- callable-object rebuild contract
- returned-call materialization / exactly-once evaluation
- generic function-object end-state
- function shape / compatibility 的共享类型层
