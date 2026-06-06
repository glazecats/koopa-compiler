# Function-Value Callee Lowering Lesson（compiler_lab）

> 目标：专门讲 non-capturing function value 这条线里最早的“真正能调用”阶段，也就是 function-valued parameter / local alias 一旦出现在 callee 位置，仓库当前是怎么保守 lower 的。

## 一句话定位

这条线回答的是：

`函数值第一次不是“只是能传过去”，而是真的在函数体里被 call 时，仓库到底靠什么把它翻译成当前后端还能接受的形状。`

## 1. 为什么这篇值得单独存在

`function_values_lesson.md` 讲的是大图景：

- function-valued parameter
- local alias
- returned function value
- dynamic family

但这里面其实有一个非常关键的分水岭：

- “能声明 / 能传参”
- 和
- “callee 里真的写 `f(x)` 还能跑”

这两件事不是一个难度。

当前仓库第一次把 higher-order call 真正打通，靠的不是 generic indirect call ABI，而是：

`specialization-backed callee lowering`

## 2. 最小 mental model

最小例子是：

```c
int add1(int x) { return x + 1; }

int apply(int f(int), int x) {
  return f(x);
}

int main() {
  return apply(add1, 41);
}
```

当前最稳的 lesson 记法不是“编译器 magically 支持了 `f(x)`”，而是：

```text
apply(add1, 41)
  ~=>
apply__fv_0_add1(41)

int apply__fv_0_add1(int x) {
  return add1(x);
}
```

也就是：

- source 上像是在调 `f(x)`
- lowering 上先把 `f` 绑定到已知目标
- 最后仍然尽量保持 direct call

## 3. 为什么第一步先不用 generic indirect call

这条线当前最重要的工程判断是：

`先把 higher-order source shape 打通，但不假装 canonical IR / lower IR / backend 已经准备好了承担通用函数对象 ABI。`

所以 first landed strategy 不是：

- 把函数值当整数
- 直接开放任意 `call *ptr`
- 提前发明一整套 generic runtime function object

而是：

- 对已知 top-level / builtin target 做 specialization
- 对 callee body 里的 direct-callee use 做定向重写
- 让 IR 仍然尽量保留 direct-call 形状

## 4. 当前 landed 的典型 family

这条线现在已经不只是一条最小 witness。

当前最值得一起记住的 landed family 有：

1. 单个 function-valued parameter 的 direct callee
2. forwarding 到另一个 compatible function-valued parameter
3. 多个同时绑定的 function-valued parameter
4. `void`-return sibling
5. zero-arg sibling
6. zero-arg `void` sibling
7. local alias 再 forwarding 进 function-valued parameter call

所以 lesson 口径应该是：

`callee lowering 这条线已经从“能 call 一次”长成一套保守 helper-specialization 合同。`

## 4.1 一个最小实现心智模型

如果把这条线压成实现视角，最稳的记法是：

```text
call site sees:
  apply(add1, 41)

lowering does:
  1. 识别第 0 个参数是 function-valued
  2. 识别实际绑定目标是已知 top-level/builtin `add1`
  3. 查/建 specialized helper:
       apply__fv_0_add1
  4. 重写原调用：
       call apply__fv_0_add1(41)
```

也就是说，当前不是“在 callee 里动态查 `f`”，而是：

`先把 callee body 重新解释成“f 已经绑定到 add1 的版本”。`

## 4.2 伪代码：specialization helper 怎么来

一个 lesson 级伪代码可以写成：

```text
lower_call(call):
    if not has_function_valued_actual(call):
        return lower_as_normal_call(call)

    binding = resolve_static_function_binding(call.actuals)
    if binding is not fully-known:
        return fallback_to_later_dynamic_path(call)

    helper_name = mangle_specialized_callee(call.callee, binding)

    if helper_name not built yet:
        build_specialized_helper(call.callee, binding, helper_name)

    lowered_actuals = drop_bound_function_actuals(call.actuals)
    return emit_direct_call(helper_name, lowered_actuals)
```

其中 `build_specialized_helper(...)` 的核心不是复制整份编译器逻辑，而是：

```text
build_specialized_helper(callee, binding):
    enter lowering context with:
        function_value_param[slot_i] = concrete_target

    relower callee body
        when seeing direct call through slot_i:
            rewrite to concrete_target(...)
```

## 4.3 为什么这里一定要有 helper cache

这条线的实现里，一个非常重要但 lesson 容易漏掉的点是：

`specialized helper 必须可复用`

否则：

```c
apply(add1, 1);
apply(add1, 2);
apply(add1, 3);
```

会重复生很多份本质一样的 helper。

所以实现视角里，最自然要有一张表：

```text
specialization_key =
  (callee_name, bound_target_set, visible_signature)

specialization_cache[key] -> helper_symbol
```

这也是为什么最近很多 language 扩张虽然看起来是“source shape 更多了”，但底下其实一直在逼：

- helper naming
- helper dedup
- binding-key construction

变成更稳定的 lowering contract。

## 5. 当前最关键的 kept boundary

这篇最容易被讲松的地方是：

- 既然 `f(x)` 能跑了
- 那是不是 function values 已经 generic 了？

不是。

这一阶段最关键的 kept boundary 还是：

1. 优先已知 target
2. 优先 direct callee position
3. 优先 specialization helper
4. 不把 plain value-position function object 自动放开

所以它解决的是：

`higher-order callee use`

不是：

`generic first-class function values everywhere`

## 6. local alias / rebinding：为什么下一步会逼出动态线

再往前一点，source 就会变成：

```c
int f(int) = add1;
if (c) f = add2;
return f(40);
```

这时 lesson 上最重要的判断是：

- 这已经不只是静态 specialization
- 因为 callee identity 到 runtime 才确定

所以当前仓库后面才会继续长出：

- caller-side branch selection
- runtime tag family
- 再往后是 explicit callable-object IR

也就是说：

`callee lowering lesson 讲的是第一阶段“先怎么把 call 打通”，不是最后阶段“所有函数值怎么统一表示”。`

## 7. 和 callable-object IR 的关系

这篇最自然的下游问题是：

`如果已知 target specialization 还不够了，那更一般的 call 该怎么办？`

当前仓库给出的方向不是继续无限堆特殊分支，而是逐步转向：

- `shape`
- `fn_make`
- `call_indirect`

所以这篇和 `closure_object_lesson.md` / `callable_object_ir_lesson.md` 的关系可以压成一句：

- 这篇讲：
  - direct-callee higher-order call 第一次怎么 landed
- `closure_object_lesson.md` 讲：
  - 为什么再往前就必须开始长 object / IR boundary
- `callable_object_ir_lesson.md` 讲：
  - 这个 object boundary 现在具体怎样落成 `shape + fn_make + call_indirect` 和 verifier contract

## 7.1 当前实现里的“静态线”和“动态线”怎么分家

这篇最值得补上的一个实现判断是：

```text
if binding is fully-known at lowering time:
    keep direct-call specialization line
else:
    hand off to returned/tag/callable-object line
```

也就是说，`callee lowering` 这条课其实讲的是：

- **静态已知 binding**
  - 继续用 helper specialization 顶住

而不是：

- **跨 CFG merge / returned family / ternary / runtime tag**
  - 这些要走更后面的 callable-object / returned-callable 设计

这能帮助读者把“当前为什么还没一上来做 generic indirect call”看得更具体。

## 7.2 源码 / 测试地图：callee lowering 最该查哪几类证据

这条课如果要回源码确认，优先看三组。

第一组是 semantic/front-door：

- `src/semantic/semantic.c`
- `src/semantic/semantic_entry.inc`
- `tests/semantic/semantic_regression_callable_flow.inc`
- `tests/semantic/semantic_regression_test.c`

它回答的是：

```text
function-valued parameter syntax / shape
  -> 是否允许进入 extension path
  -> signature mismatch 是否尽早拒绝
```

第二组是 compiler-driver source witness：

- `tests/compiler/compiler_driver_test.c`

重点搜索：

```text
direct call of function-valued parameter
function-valued parameter forwarding
multiple function-valued parameters
void function-valued parameter
zero-arg function-valued parameter
local function value forwarding into function-valued parameter
```

这些 witness 证明的是：

`f(x)` 这条线已经不只在 parser/semantic 停住，而是真的能走到 lowering / driver。`

第三组是 IR shape：

- `tests/ir/ir_regression_test.c`
- `tests/lower_ir/lower_ir_regression_test.c`

重点看：

```text
apply__fv_...
__fnwrap_*
fn_make
call_indirect
```

这里要按阶段理解：

```text
早期/静态形状
    -> specialized helper / direct-call leaning

后续/dynamic-object 形状
    -> fn_make + call_indirect / wrapper bridge
```

所以如果某个新 witness 坏了，先问：

```text
它是 fully-known binding 吗？
    yes -> specialization key/helper/cache 方向查
    no  -> returned/tag/callable-object handoff 方向查
```

## 8. 最稳的分层记法

建议把 function-value 这条线的前半段记成三层：

1. **只是声明/传递**
   - function-valued parameter syntax
2. **第一次真的 call**
   - callee specialization lowering
3. **开始变动态**
   - tag / branch / callable-object IR

这样就不容易把：

- `f(x)` 已 landed
- 和
- generic indirect-call ABI 已 landed

讲成一回事。

## 9. 读完后接哪篇

最自然往下接：

1. `function_values_lesson.md`
2. `higher_order_callable_lesson.md`
3. `closure_object_lesson.md`
4. `callable_object_ir_lesson.md`
5. `returned_closure_transport_lesson.md`
6. `generic_function_values_lesson.md`

因为这条线接下去的真正问题就是：

- 还能不能继续靠 specialization 顶住
- function-valued parameter 自己也接收 function-valued parameter 时怎么继续 collapse
- returned/dynamic/closure-backed family 怎么转进 payload + object contract
- 什么时候必须承认“函数值真的成对象了”
