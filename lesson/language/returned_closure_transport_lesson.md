# Returned Closure Transport Lesson（compiler_lab）

> 目标：专门讲 returned closure 最深、也最容易误解的一条 transport 主线：dynamic returned closure、producer-side ternary merge、local bounce、passthrough wrapper、reassign、zero-arg/void sibling，以及 `__retclosure_*` payload 视图到底在表达什么。

## 一句话定位

这条线回答的是：

`returned closure 现在不只是 make(3)(4) 能跑，而是越来越像普通 function value 一样，可以被绑定、合并、转发、重返回、作为实参传递，但这一切仍然靠显式 payload/tag/capture transport 保守完成。`

## 1. 为什么这篇必须单独存在

`returned_callable_lesson.md` 已经讲了 returned callable 的大图：

```text
producer writes payload
caller rebuilds callable view
consumer finally calls
```

但 returned closure 现在已经有了更细的内部世界：

```text
tag
capture slots
hidden return slots
local payload slots
actual-argument payload slots
immediate-call payload slots
passthrough/reassign/ternary merge transport
```

如果只说“returned payload + rebuild”，会漏掉现在最重要的工程事实：

`不同 consumer view 可以不同，但它们必须读同一份 returned closure value，而不是重新求值 producer。`

## 2. 最小 returned closure

最小例子：

```c
int make(int x)(int){
  return closure [x] int (int y){ return x + y; };
}

int main(){
  return make(3)(4);
}
```

lesson 级心智模型：

```text
make:
    returns payload:
        closure family tag
        captured x

main:
    materialize returned payload
    rebuild callable object:
        fn_make(wrapper, env, shape)
    call_indirect(obj, 4)
```

这个模型是对的，但已经不够细。

现在还要问：

```text
payload 放在哪个 view 里？
这个 payload 后面是被 local init 读、immediate call 读、还是 actual argument 读？
```

## 3. `__retclosure_*` 视图怎么读

当前测试和 IR dump 里常见这些名字：

| prefix | lesson 级含义 |
| --- | --- |
| `__retclosure_declslot` | returned closure 先 materialize 到 local declaration/binding 用的 payload slots |
| `__retclosure_immediate` | returned closure 被立即调用时使用的 payload view |
| `__retclosure_argslot` | returned closure 作为 function-valued actual argument 时使用的 payload view |
| `__retclosure_argcap` | actual-argument/capture 场景下为了 later rebuild 保留的 capture payload view |
| `__retclosure_specarg` | specialization/returned-function actual family 需要的 payload view |
| `__retclosure_anyview` | 轻量目标/家族探测曾经用到的 broad view，现代路径尽量避免它触发重复 producer materialization |

这些名字不应该被理解成“不同值”。

更准确地说：

```text
same returned closure value
  may be consumed through different views
```

但正确性要求是：

```text
same source evaluation
  -> producer call once
  -> payload locals reused or copied intentionally
```

## 4. tag + capture payload

dynamic returned closure 的 payload 至少包括：

```text
function target tag
copied capture slots
```

测试里经常能看到：

```text
h$ftag
h$closurecap$0
m$ftag
m$closurecap$0
```

lesson 级解释：

```text
local function value h:
    h$ftag          = runtime-selected closure family
    h$closurecap$0  = copied capture slot 0
```

如果是 multi-capture：

```text
h$closurecap$0
h$closurecap$1
...
```

如果是 zero-arg void closure：

```text
shape shape.0() -> void
tag + capture payload still exist
visible return value does not
```

所以 zero-arg / void 不是“没有 payload”。

它只是：

```text
callable shape has no visible args and/or no visible return
```

payload 仍然要 transport。

## 5. bind, re-return, immediate-call, actual-argument

同一个 returned closure producer 可以有几种 consumer：

```c
int f(int)=make(3);
return f(4);
```

```c
return make(3)(4);
```

```c
return apply(make(3), 4);
```

```c
int wrap()(int){
  int f(int)=make(3);
  return f;
}
```

它们的共同点是：

```text
producer payload must be complete
consumer view may differ
final call still rebuilds callable
```

lesson 级伪代码：

```text
materialize_returned_closure(call_expr, view_kind):
    payload = get_or_create_payload_for_same_call_expr(call_expr)

    if view_kind == local_decl:
        copy payload into local tag/capture slots

    if view_kind == immediate_call:
        rebuild object directly from payload

    if view_kind == actual_argument:
        expose payload to function-valued-parameter consumer

    if view_kind == return_again:
        write payload into outgoing hidden return slots
```

## 6. dynamic returned closure local bounce

当前已经支持这种普通 local transport：

```c
int pick(int x, int c)(int) {
  int f(int)=closure [x] int (int y){ return x+y; };
  int g(int)=closure [x] int (int y){ return x-y; };
  if(c) f=g;
  return f;
}

int wrap(int c)(int) {
  int h(int)=pick(5,c);
  int k(int)=h;
  return k;
}
```

这里的关键不是 `k` 这个名字。

关键是：

```text
k$ftag = h$ftag
k$closurecap$0 = h$closurecap$0
```

也就是说，ordinary local bounce 对 returned closure 来说必须是 payload copy。

不能只 copy 某个“primary target name”。

## 7. passthrough wrapper

可以再加一层 returned-parameter passthrough：

```c
int id(int f(int))(int){ return f; }

int wrap(int c)(int) {
  int h(int)=id(pick(5,c));
  return h;
}
```

甚至两跳三跳：

```c
int id2(int f(int))(int){ return id(f); }

int id3(int f(int))(int){
  int g(int)=id2(f);
  return g;
}
```

当前 lesson 级心智模型：

```text
resolve_passthrough(expr):
    if expr is exact returned-parameter passthrough:
        follow argument to real producer
    if expr is local bind-return wrapper:
        follow local initializer
    continue until real returned closure producer
```

这条线的重要 regression contract 是：

```text
call pick(...) exactly once
no surviving call id(...)
no surviving call id2(...)
no surviving call id3(...)
final caller still uses fn_make + call_indirect / wrapper call
```

这不是为了漂亮。

它是在证明：

`passthrough wrapper 没有重新求值 producer，也没有把 payload 断掉。`

## 8. producer-side ternary merge

更难的是 producer 里先产生两个 returned closure value，再 merge：

```c
int wrap(int c, int d)(int) {
  int h(int)=pick(5,c);
  int k(int)=pick(7,c);
  int m(int)=d ? h : k;
  return m;
}
```

这时正确的 payload story 是：

```text
h = payload from pick(5,c)
k = payload from pick(7,c)
m = branch-selected payload:
    m$ftag
    m$closurecap$0
return m payload
```

测试里的重要 contract：

```text
call pick(...) appears twice
m$ftag / m$closurecap$0 are visible
final caller dispatches through both possible closure wrapper families
```

这里 `call pick` 出现两次是对的。

因为 source 本来写了两个 producer evaluation：

```c
pick(5,c)
pick(7,c)
```

这和同一个 `pick(...)` 被不同 consumer view 重复 materialize 是两回事。

## 9. merge 后 local bounce / passthrough / reassign

producer-side ternary merge 后，还可以继续 ordinary transport：

```c
int n(int)=m;
return n;
```

或者：

```c
int p(int)=id(n);
return p;
```

或者：

```c
int p(int)=n;
p=m;
return p;
```

这些都在考同一件事：

```text
after merge, payload is still an ordinary local function-value state
```

所以应该看到的是：

```text
n$ftag = m$ftag
n$closurecap$0 = m$closurecap$0

p$ftag = n$ftag
p$closurecap$0 = n$closurecap$0

p$ftag = m$ftag
p$closurecap$0 = m$closurecap$0
```

这说明 returned closure transport 已经不再只能贴着 producer 走。

它能走过：

1. merge
2. local bounce
3. passthrough call
4. statement reassignment
5. returned-call reassignment

## 10. zero-arg / void sibling 为什么重要

zero-arg:

```c
int pick(int x, int c)() {
  int f()=closure [x] int (){ return x; };
  int g()=closure [x] int (){ return x+1; };
  if(c) f=g;
  return f;
}
```

zero-arg void:

```c
void pick(int x, int c)() {
  void f()=closure [x] void (){ putint(x); return; };
  void g()=closure [x] void (){ putint(x+1); return; };
  if(c) f=g;
  return f;
}
```

这两个 sibling 的价值是：

```text
payload transport must be independent of visible arg/return arity
```

`shape shape.0() -> void` 仍然需要：

```text
tag
capture payload
wrapper target
call_indirect arg-count check
```

这也是为什么 `IR-VERIFY-088` 这类 `call_indirect` arg-count/shape mismatch 很值得被单独记住：

`zero-arg void` 很容易暴露“payload slot 数对了，但 callable shape/visible args 没对齐”的 bug。

## 11. returned closure captures returned closure

现在还出现了两跳 closure object chain：

```c
int make(int x)(int){
  return closure [x] int (int y){ return x+y; };
}

int outer(int x)(int){
  int f(int)=make(x);
  return closure [f] int (int y){ return f(y); };
}

int main(){
  return outer(3)(4);
}
```

这条线的意义很大：

```text
returned closure value
  -> captured by another returned closure
  -> caller rebuilds outer closure
  -> outer closure body rebuilds/calls captured closure
```

lesson 级 payload story：

```text
outer returned payload =
    outer closure family tag
    captured callable payload for f

captured callable payload =
    make(x)'s closure family tag
    make(x)'s capture slot x
```

所以这里不是简单的 “closure capture int”。

它是：

`closure capture payload now contains another returned closure payload.`

## 12. returned closure actual argument into closure-returning function

另一个重要形状：

```c
int make(int x)(int){
  return closure [x] int (int y){ return x+y; };
}

int wrap(int f(int))(int){
  return closure [f] int (int y){ return f(y); };
}

int main(){
  return wrap(make(3))(4);
}
```

这里 `make(3)` 是 returned closure actual argument。

`wrap` 又返回一个 closure，并 capture 这个 function-valued parameter。

实现上必须保证：

```text
materialize make(3) payload
pass payload into wrap specialization/capture path
wrap's returned closure payload carries captured callable payload
final caller rebuilds and calls outer closure
```

最近一个关键修复点是：

```text
helper creation / wrapper creation / builtin insertion / specialization detours
may reallocate function table
```

所以 lowering 不能一直拿旧的 `IrFunction *`。

它需要按名字 refresh current owner function，再继续 materialize returned payload locals。

lesson 上可以记成：

`returned closure actual-argument capture 不是只有 payload 问题，也有 current-function ownership stability 问题。`

## 13. mixed capture returned closure immediate-call

混合 capture：

```c
int make(int z, int f(int))(int){
  return closure [z, f] int (int y){ return f(z + y); };
}

int main(){
  return make(3, add1)(4);
}
```

这里 closure capture 同时包含：

1. scalar capture
2. function-value capture

所以 returned payload slot count 必须按 function-object payload rules 算，而不是只数普通 `int` capture。

lesson 级规则：

```text
capture slot count
    = scalar captures
    + callable capture payload slots
```

这就是为什么 mixed-capture family 会暴露：

```text
payload slot count / specialized helper target / owner returned-helper path
```

这些看起来像 lowering 细节、其实是语义正确性的点。

## 14. 当前源码地图

returned closure transport 主要看：

1. `src/ir/ir_lower_stmt.inc`
   - local init / declaration-only / reassignment / return payload placement
   - `__retclosure_declslot`
   - returned descriptor finalize
2. `src/ir/ir_lower_expr.inc`
   - immediate-call / actual-argument / passthrough / dynamic dispatch
   - `__retclosure_immediate`
   - `__retclosure_argslot`
   - `__retclosure_argcap`
   - returned-call materialization reuse
3. `src/ir/ir.c`
   - returned call materialization bookkeeping
   - function target sets / lowering context state
4. `src/ir/ir_verify.inc`
   - callable-object shape / `call_indirect` validation

最值得搜的关键词：

```text
__retclosure_
closurecap
ftag
returned_call_materialization
exact_parameter_passthrough
call_indirect
IR-VERIFY-088
```

## 15. 测试地图

当前最有代表性的测试关键词：

```text
DYNAMIC-RETURNED-CLOSURE
PRODUCER-TERNARY-MERGE
RETURNED-CALL-TERNARY-MERGE
BOUNCE
PASSTHROUGH
REASSIGN
ZERO-ARG-VOID
RETURNED-CLOSURE-CAPTURE-RETURNED-CLOSURE
DIRECT-RETURNED-CLOSURE-ACTUAL-CAPTURE-PARAM-FNVAL
MIXED-CAPTURE-RETURNED-CLOSURE-IMM-CALL
```

最值得先看的测试文件：

1. `tests/ir/ir_regression_test.c`
2. `tests/lower_ir/lower_ir_regression_test.c`
3. `tests/compiler/compiler_driver_test.c`

如果只想抓一个核心 contract，看这些断言：

```text
call pick(...) count
m$ftag / m$closurecap$0
__retclosure_immediate_*
__retclosure_argslot_*
fn_make __fnwrap_closure_*
call_indirect
no call id(...)
no __retclosure_callcap fallback
```

## 16. 当前 still-kept boundary

不能把这条线讲成：

1. arbitrary escaping closure runtime 已完成
2. closure env 已经有统一 heap/lifetime 模型
3. returned closure 可以任意存进 aggregate/array/global 后自然工作
4. 所有 zero-arg/void/multi-target shape 都已经完全无 residual shell
5. 任意同名 `pick(...)` producer 都可以 dedupe

更准确地说：

`returned closure transport 已经能穿过很多普通 function-value value-flow 形状，但仍然是 explicit tag/capture payload + source-shape-aware recovery + callable-object rebuild 的保守模型。`

## 17. 读完后接哪篇

最自然往下接：

1. `returned_callable_lesson.md`
2. `callable_object_ir_lesson.md`
3. `closure_capture_callable_lesson.md`
4. `function_evaluation_reuse_lesson.md`
5. `higher_order_callable_lesson.md`
6. `generic_function_values_lesson.md`
