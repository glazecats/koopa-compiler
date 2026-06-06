# Closure Object Lesson（compiler_lab）

> 目标：解释当前仓库的 closure / returned-closure / callable-object IR 这条更细的语言线到底推进到了哪，为什么它已经不只是“function value 的一个补充”，以及它和 generic first-class function values 的距离还在哪。

## 一句话定位

这条线回答的是：

`当前仓库已经不只是在支持“函数像值”，而是在逐步把 callable value 逼成一个真正的 object / IR boundary。`

## 1. 为什么这篇需要单独存在

`function_values_lesson.md` 讲的是大图景：

- non-capturing function values
- local closure slice
- returned closure
- callable-object IR

但现在 `docs/language` 已经把后面三块都写成了单独 authority：

1. `CLOSURE_SLICE_PLAN.md`
2. `RETURNED_CLOSURE_PLAN.md`
3. `CALLABLE_OBJECT_IR_PLAN.md`

这意味着 lesson 也应该把它们拆开讲，否则读者很容易把：

- “closure literal 已 landed”
- “returned closure 还在 conservative boundary”
- “callable-object IR 还在往下沉”

误读成一条已经全开的 feature。

## 2. 当前 closure slice 到底开到了哪

当前 first closure slice 最重要的 lesson 口径是：

- closure literal 已经不是纯设计草图
- 但它还是 deliberately narrow

代表 shape：

```c
int x = 3;
int f(int) = closure [x] int (int y) { return x + y; };
return f(4);
```

要记住的四个边界：

1. closure literal 是 expression-shaped
2. capture list 是 explicit 的
3. phase-1 capture 是 by-value
4. 当前 use-site 仍然很窄

也就是说，这条线当前解决的是：

`本地 closure object 能不能被创建并 call`

不是：

`所有 closure value 都已经能自由流动`

## 3. returned closure：为什么它比 local closure 难很多

当前 returned closure 这条线的意义，不是“再多一条语法”。

它逼仓库回答的其实是三个真正的 object-model 问题：

1. closure environment 能不能跨出创建作用域
2. caller 怎么重新接住这个 closure
3. 后续 call 时怎么恢复 helper target + capture payload

所以 returned closure 的推荐理解是：

`local closure -> cross-function closure transport`

当前 docs 里最值得记住的 conservative 决策是：

- 先不要上 heap
- 先不要上 generic code pointer ABI
- 先把 returned closure 当成一个固定布局的小对象

也就是：

- known helper family identity
- copied scalar capture payload

所以这条线当前仍然明显不是：

`full escaping closure runtime`

## 4. callable-object IR：为什么它已经从 lowering helper 变成 IR 议题

这条线现在最值得强调的是：

`CALLABLE_OBJECT_IR_PLAN.md` 和 `docs/ir/CALLABLE_OBJECT_IR_DESIGN.md` 都已经存在`

这说明仓库现在已经不满足于：

- lowering-side ad hoc helper
- tag + branch-selected direct call
- hidden local naming convention

而是在往更 explicit 的 IR object model 推。

当前最稳的 lesson 口径是：

一个 future callable object 至少会越来越像这三元组：

1. `code`
2. `env`
3. `shape`

以及这一组操作：

- `fn_make`
- `fn_code`
- `fn_env`
- `fn_shape`
- `call_indirect`

所以这一层最重要的 signal 不是“现在已经有最终 ABI/runtime 了”，而是：

`language feature 已经开始逼 canonical IR / lower IR / downstream consumer 重新定义 callable object contract。`

补到当前 live tree 的口径是：

```text
canonical IR callable-object contract 已经有第一版真实实现：
    shape table
    fn_make
    fn_code / fn_env / fn_shape
    call_indirect
    verifier checks
```

但这仍然不是最终 generic function pointer ABI。

更准确地说：

`IR 合约已经显式化；runtime / storage / escape / aggregate transport 还没有 fully generic。`

## 4.1 一个最小 callable-object 实现心智模型

如果把这一层压成实现视角，最稳的记法是：

```text
callable_object =
  { code, env, shape }
```

然后不管 source 上它来自：

- noncapturing returned function value
- closure literal
- returned closure

当前越来越多 family 都会往这个桥上靠：

```text
code  = wrapper/helper/closure body entry
env   = null or capture payload view
shape = static callable signature id
```

## 4.2 伪代码：为什么要有 `fn_make`

这一层最容易被误解成“只是把 helper 名放进某个临时变量”。

更贴近现在仓库方向的伪代码是：

```text
lower_callable_value(source):
    code  = resolve_callable_code(source)
    env   = resolve_callable_env(source)
    shape = resolve_callable_shape(source)
    return fn_make(code, env, shape)
```

然后 consumer 再统一吃：

```text
lower_callable_call(obj, args):
    return call_indirect(obj, args)
```

也就是说，`fn_make` 的意义不是“新指令比较酷”，而是：

`它把原来分散在 lowering context / helper naming / hidden locals 里的 callable 构造动作，显式收成一个 IR 事件。`

## 5. generic first-class function values：为什么还不能说已经到了

现在最容易误解的一点就是：

- 既然 non-capturing、closure、returned closure、callable-object 都在推进
- 那是不是 generic first-class function values 其实已经差不多了？

答案仍然是否定的。

当前 generic end-state 仍然是**前瞻 authority**，不是 live-tree 已闭合 checkpoint。

lesson 里最稳妥的分层是：

1. **已 landed / 正在 landed 的 conservative line**
   - non-capturing function values
   - local closure slice
   - returned closure conservative slice
2. **正在形成的 IR boundary**
   - callable-object IR
3. **前瞻 end-state**
   - generic first-class function values everywhere

这三层不能混着讲。

## 6. 当前最重要的统一判断标准

如果你在看 closure/object 这条线，建议一直问：

1. 当前这个 value 是不是 still backed by specialization/helper/tag？
2. capture payload 是不是 still copied scalar slots？
3. 这一步是在扩大 source surface，还是在逼 IR contract 往下长？
4. 这是 landed slice，还是 forward-looking generic authority？

## 6.1 伪代码：什么时候还能 collapse 回 direct call

这层再往下一个非常重要的实现判断是：

```text
if code/env pair becomes fully known:
    later stage may collapse
        call_indirect(fn_make(code, env, shape), args)
    into:
        direct wrapped/helper call
else:
    keep explicit callable-object path
```

这也解释了为什么很多 lesson 里会同时出现两种说法：

- canonical IR 明确锁 `fn_make + call_indirect`
- downstream path 又允许“证明之后再折回 direct call”

因为：

`explicit object path 是当前设计 contract，是否继续保留到更后层不是 contract 本身。`

## 6.2 源码 / 测试地图：closure object 现在怎么回查

这条线最适合分层回查。

第一层是 IR object contract：

- `src/ir/ir.c`
- `src/ir/ir_gen.c`
- `tests/ir/ir_verifier_test.c`
- `tests/ir/ir_regression_test.c`

重点关键词：

```text
fn_make
fn_code
fn_env
fn_shape
call_indirect
__fnwrap_closure_
```

这一层证明的是：

`closure/callable 不再只是 lowering context 里的隐形事实，而是能出现在 canonical IR 的显式 object path 上。`

第二层是 lower-IR bridge：

- `src/lower_ir/lower_ir.c`
- `tests/lower_ir/lower_ir_regression_test.c`

重点看：

```text
call_indirect bridge
call __fnwrap_*
callable-object projection
```

这一层证明的是：

`lower-IR 可以消费 callable-object 形状，并在当前保守阶段把它桥回 wrapper/direct-call 友好的形态。`

第三层是 source/runtime witness：

- `tests/compiler/compiler_driver_test.c`

重点搜索：

```text
closure literal forwarding into function-valued parameter
direct closure literal argument to function-valued parameter
returned closure forwarding into function-valued parameter
dynamic runtime closure local forwarding
multi-capture two-arg closure direct local call
```

这些 witness 要按家族读，不要合并成一句“closure 已经完全泛化”。

更稳的判断是：

```text
local closure
  -> returned closure
  -> closure forwarded into function-valued parameter
  -> dynamic closure family
  -> callable-object IR bridge
```

每跨一层，都可能需要新的 payload / wrapper / shape / exactly-once 求值保护。

## 7. 读完后接哪篇

最自然往下接：

1. `function_values_lesson.md`
2. `returned_closure_transport_lesson.md`
3. `callable_object_ir_lesson.md`
4. `generic_function_values_lesson.md`
5. `lesson/core/ir_lesson.md`
6. `lesson/ssa/value_ssa_lesson.md`

因为这条线最后一定会落到：

- direct callee vs indirect call
- returned closure payload transport
- explicit IR object
- generic function-value end-state
- downstream SSA/backend consumption
