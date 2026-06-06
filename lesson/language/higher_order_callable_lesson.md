# Higher-Order Callable Lesson（compiler_lab）

> 目标：专门讲当前 function 主线里最容易被低估的一段：function-valued parameter 不只是能传普通函数了，而是开始能传“接收 function-valued parameter 的函数”、多层 passthrough wrapper、returned dynamic higher-order callee，以及 mixed closure actual arguments。

## 一句话定位

这条线回答的是：

`当前仓库已经能跑一批 second/third/fourth/fifth-order higher-order function-value 代码，但它靠的是 finite shape + specialization/callable-object collapse，不是 auto/generic lambda 那种完整高阶类型系统。`

## 1. 为什么这篇最需要补

前面的 function lessons 已经讲了：

- `function_value_callee_lowering_lesson.md`
  - `apply(add1, 41)` 这种第一层 function-valued parameter call
- `returned_callable_lesson.md`
  - `make(3)` / `pick(...)` 跨 return boundary 后怎么 rebuild
- `function_family_table_lesson.md`
  - 当前 function family 总表

但最近 `NEXT_STEPS` 里真正密集推进的是另一层：

```text
function parameter itself accepts a function parameter
```

也就是：

```c
int apply(int f(int), int x){ return f(x); }
int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }
int relay(int q(int h(int f(int), int x), int f(int), int x),
          int h(int f(int), int x),
          int f(int),
          int x){
  return q(h, f, x);
}
```

这类 shape 不能只用“函数值能传参了”概括。

它真正逼出来的是：

```text
nested function signature
  -> higher-order compatibility
  -> passthrough alias recovery
  -> recursive specialization/collapse
  -> callable-object leaf dispatch
```

## 2. 最小 second-order 模型

最小二阶例子：

```c
int add1(int x){ return x + 1; }
int apply(int f(int), int x){ return f(x); }
int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }
int main(){ return pass(apply, add1, 41); }
```

source 上看：

```text
pass receives:
    h = apply
    f = add1
    x = 41

pass body:
    return h(f, x)
```

当前 lowering 的心智模型：

```text
pass(apply, add1, 41)
    -> pass__fv_0_apply_1_add1(41)
    -> apply__fv_0_add1(41)
    -> fn_make __fnwrap_add1 + call_indirect
```

所以二阶线不是“直接把 apply 当普通 int 参数传过去”。

它是在做：

```text
outer callable binding
  -> inner callable binding
  -> leaf callable-object dispatch
```

## 3. 为什么 parser / semantic 也必须跟着长

二阶开始后，签名不再只是：

```text
int f(int)
```

而是：

```text
int h(int f(int), int x)
```

再往上就是：

```text
int q(int h(int f(int), int x), int f(int), int x)
```

所以前门必须能保留 nested function type metadata。

lesson 级伪代码：

```text
parse_function_parameter_type():
    if parameter is scalar:
        return scalar descriptor

    if parameter is function-valued:
        return function_shape {
            return kind,
            parameter descriptors,
            nested function parameter shapes
        }
```

semantic 再做：

```text
check_higher_order_compat(expected_shape, actual_shape):
    compare return kind
    compare parameter count
    for each parameter:
        if scalar:
            compare scalar kind
        if function-valued:
            recursively compare nested function shape
```

这也是为什么最近 higher-order 线的提交会同时碰：

1. `src/parser/parser_stmt_decl_tu.inc`
2. `src/semantic/semantic_entry.inc`
3. `src/semantic/semantic_scope_rules.inc`
4. `src/ir/ir_lower_expr.inc`

它不是纯 lowering 问题。

## 4. third/fourth/fifth-order 到底是什么意思

这里的 “order” 不需要想得太玄。

可以先用这张小表理解：

| order | 典型 source | lesson 级意义 |
| --- | --- | --- |
| first-order function value | `apply(add1, 41)` | 函数作为普通参数 |
| second-order | `pass(apply, add1, 41)` | 函数参数本身接收函数参数 |
| third-order | `relay(pass, apply, add1, 41)` | higher-order wrapper 再被传给 higher-order wrapper |
| fourth-order | `meta(relay, pass, apply, add1, 41)` | 再多一层 nested signature / passthrough |
| fifth-order | `ultra(meta, relay, pass, apply, add1, 41)` | 当前 conservative depth 的继续拓宽 |

当前能写通这些，不代表：

```text
arbitrary recursive polymorphic callable
```

已经存在。

更准确地说，它代表：

```text
有限层 nested function shape
  + source-shape-aware passthrough
  + concrete specialization/callable-object leaf
```

已经能撑住很多真实 higher-order witness。

## 5. passthrough wrapper 为什么是核心

很多 higher-order 代码本质上是 passthrough：

```c
int pass(int h(int f(int), int x), int f(int), int x){
  return h(f, x);
}
```

或者多一个 alias：

```c
int pass(int h(int f(int), int x), int f(int), int x){
  int k(int f(int), int x)=h;
  return k(f, x);
}
```

当前 lowering 必须识别：

```text
k is just h
x maybe copied to y
final return is h(f, y)
```

lesson 级伪代码：

```text
collect_passthrough_aliases(helper_body):
    for each prefix statement:
        if local function alias = parameter or previous alias:
            record callable alias
        if local scalar alias = parameter or previous scalar alias:
            record scalar alias
        if simple scalar update is supported:
            record updated scalar binding

    find final passthrough statement:
        return h(f, x)
        or h(f, x)
```

然后：

```text
emit_passthrough_dispatch(call_site, helper_body):
    remap helper parameters to caller actuals
    recursively dispatch nested callable arguments
    lower leaf call without keeping useless wrapper shell
```

所以“去掉 `pass__fv_*` / `relay__fv_*` 残余 shell”不是纯美化。

它说明 lowering 已经理解：

`这个 helper body 只是把 callable 和 scalar 参数转发到更里面。`

## 6. helper-body evaluator 是什么

有些 higher-order helper 不是完全一行 passthrough。

比如：

```c
int pass(int h(int f(int), int x), int f(int), int x){
  int y=x;
  y=y+1;
  return h(f, y);
}
```

或者：

```c
int pass(int h(int f(int), int x), int f(int), int x){
  if(h(f, x)) return h(f, x) + 1;
  return 0;
}
```

当前并没有开放“任意 helper body 都能解释”。

它开的是一批保守 body-eval slice：

1. declaration-only alias setup
2. scalar alias
3. simple scalar update
4. repeated scalar update
5. unary / compound / comma / ternary expression helper eval
6. tiny `if-return` CFG family

lesson 级伪代码：

```text
try_simple_helper_body_eval(helper_body, bindings):
    if body is supported straight-line prefix + final callable call:
        evaluate supported prefix under current bindings
        emit final callable dispatch
        return success

    if body is supported if-return shape:
        lower condition under current bindings
        lower then/else return expressions under current bindings
        return success

    return no_match
```

这里要记住：

`helper-body evaluator 是保守解释器，不是小型通用编译器。`

它只能吃当前被证明安全的 helper body shape。

## 7. dynamic higher-order：不能静态冻结第一支

更难的例子：

```c
int apply(int f(int), int x){ return f(x); }
int apply_twice(int f(int), int x){ return f(f(x)); }
int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }

int main(){
  int c=getint();
  return pass(c ? apply : apply_twice, add1, 41);
}
```

错误做法：

```text
see ternary first arm apply
freeze h = apply
```

正确心智模型：

```text
h target-set = { apply, apply_twice }

emit branch on runtime tag:
    apply branch:
        call pass__fv_0_apply_1_add1 or collapse to apply leaf
    apply_twice branch:
        call pass__fv_0_apply_twice_1_add1 or collapse to apply_twice leaf
```

所以 dynamic higher-order 的关键是：

`outer higher-order callee 也可以是 runtime-selected target family。`

## 8. returned dynamic higher-order

再往上，higher-order callee 不是 ternary 本地选择，而是 returned producer：

```c
int pick(int c)(int f(int), int x){
  if(c) return apply_twice;
  return apply;
}

int main(){
  return pick(getint())(add1, 41);
}
```

实现心智模型：

```text
payload = materialize returned higher-order family from pick(getint())

branch on payload tag:
    branch apply:
        dispatch apply(add1, 41)
    branch apply_twice:
        dispatch apply_twice(add1, 41)
```

这里会和 `returned_callable_lesson.md` 合流：

```text
returned payload
  -> callable-object view
  -> higher-order dispatch
  -> leaf fn_make/call_indirect
```

如果这个 returned payload 是 closure-backed，实际还会多一层：

```text
returned producer call
  -> __retclosure_* payload view
  -> branch-local callable-object rebuild
  -> helper-body eval / specialization fallback
```

这部分细节不要在 higher-order 课里硬塞，应该去看：

```text
returned_closure_transport_lesson.md
callable_object_ir_lesson.md
```

## 9. mixed closure actual arguments

当前更复杂的一类是：

```c
idh(choose(getint()))(pick(getint(), add1), make(3), 4)
```

lesson 上不用先背所有 helper 名。

先看它的 family 组成：

1. outer higher-order callee
   - `choose(getint())`
   - returned dynamic family
2. first inner function-valued actual
   - `pick(getint(), add1)`
   - returned dynamic closure/noncapturing family
3. second inner function-valued actual
   - `make(3)`
   - returned closure family
4. scalar actual
   - `4`

当前实现目标是：

```text
normalize each callable actual into a callable view/payload
branch-local body-eval first
fallback to specialization helper only when collapse cannot apply
leaf calls use wrapper-backed callable-object path
```

这解释了最近两个重要状态：

1. two-target mixed closure family 已经收敛得更好
   - old `compose__fv_*` residual helper shell 消失
2. three-target sibling 已经不再 canonical-IR 崩溃
   - 但还不一定完全达到最终 no-shell end state

所以 lesson 里的保守口径应该是：

`三目标 returned dynamic mixed-closure 已经 unblock，但不是所有 residual shell 都已经消失。`

## 10. 和 `auto f = [=](auto f, ...)` 的区别

用户前面提到过这类形状：

```c
auto f = [=](auto f, int x) { return f(f, x); };
```

这和当前 higher-order line 的差别非常大。

当前仓库支持的是：

```text
source 写出有限层、明确的 function signature
compiler 对这些 finite shapes 做 compatibility + specialization/collapse
```

而 `auto f = ... f(f, x)` 需要：

```text
generic lambda
recursive/self type
polymorphic callable instantiation
possibly unbounded type equations
```

这不只是“再多补一个 helper”。

所以 lesson 必须讲清楚：

`当前 higher-order 能力是 finite explicit signature 的保守主线，不是 C++ 风格 auto/generic lambda。`

## 11. 源码地图

如果你想追 higher-order 主线，当前最值得先看：

1. `src/parser/parser_stmt_decl_tu.inc`
   - nested function signature metadata
2. `src/semantic/semantic_entry.inc`
   - higher-order compatibility / returned parameter passthrough gate
3. `src/semantic/semantic_scope_rules.inc`
   - local/top-level visible function names 恢复 deeper signature metadata
4. `src/ir/ir_lower_expr.inc`
   - passthrough alias recovery、helper-body evaluator、recursive dispatch、multi dynamic callable dispatch

在 `src/ir/ir_lower_expr.inc` 里，读源码时最值得搜这些词：

```text
passthrough
function_body_eval
recursive_passthrough
callable_view
multi_dynamic
```

它们正好对应这篇课的几个实现块。

## 12. 测试地图

当前 higher-order regression 最常见的测试关键词：

```text
SECOND-ORDER
THIRD-ORDER
FOURTH-ORDER
FIFTH-ORDER
PASSTHROUGH
DYNAMIC
MIXED-CLOSURE
HELPER-EVAL
```

最值得先看的测试文件：

1. `tests/semantic/semantic_regression_test.c`
2. `tests/ir/ir_regression_test.c`
3. `tests/lower_ir/lower_ir_regression_test.c`
4. `tests/compiler/compiler_driver_test.c`
5. `tests/value_ssa/value_ssa_regression_test.c`
6. `tests/machine/lowering/machine_ir/machine_ir_test.c`
7. `tests/machine/lowering/machine_select/machine_select_test.c`

后面三类特别说明：

- `ValueSSA`
- `machine_ir`
- `machine_select`

它们通常不是为了证明 parser/semantic 是否接受 source。

它们更像是在锁：

`复杂 higher-order callable 最后是不是还保留/收敛到预期 wrapper-backed shape。`

## 13. 当前 still-kept boundary

当前不能把 higher-order line 讲成：

1. arbitrary-depth function type 都无限支持
2. helper-body evaluator 能解释任何语句
3. all dynamic mixed producer 都已经 no-shell convergence
4. generic lambda / auto callable / self-recursive callable 已经现实可做

更准确的说法是：

`有限层显式 higher-order signature 已经能支撑很多真实 source family；更深的 generic/self-recursive callable 仍然不是当前 live tree 的目标。`

## 14. 读完后接哪篇

最自然往下接：

1. `function_value_callee_lowering_lesson.md`
2. `function_family_table_lesson.md`
3. `function_evaluation_reuse_lesson.md`
4. `returned_callable_lesson.md`
5. `returned_closure_transport_lesson.md`
6. `callable_object_ir_lesson.md`
7. `generic_function_values_lesson.md`
