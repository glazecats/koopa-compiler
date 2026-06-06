# Function Family Table Lesson（compiler_lab）

> 目标：把当前 function-value / closure / returned-callable 主线按 source family 整理成一张能回源码、回测试、回边界的总表。它不是替代 `function_checkpoints_lesson.md`，而是把最近大量扩张过的 family 摆到一起，方便判断“这个形状现在大概走哪条实现路”。

## 一句话定位

这条线回答的是：

`我们现在能写通哪些 function/closure/higher-order 代码，它们分别靠 specialization、payload/rebuild、还是 callable-object consumer 跑起来。`

## 1. 为什么要单独做 family table

最近 function 主线推进得很快，单独看提交名会感觉像一串相似的修复：

```text
returned closure
dynamic returned callable
mixed producer
multiple dynamic arguments
higher-order shell
side-effect argument repair
```

但这些其实不是同一个层级。

更适合 lesson 的组织方式是：

```text
source family
  -> semantic gate
  -> lowering route
  -> expected IR shape
  -> regression surface
  -> kept boundary
```

这样看，当前主线不是“乱补 witness”，而是在把越来越多 source shape 归一到几个实现路线。

## 2. 总表：当前 function family 怎么分

| family | 代表 source | 主要 lowering 路线 | 当前最该记的边界 |
| --- | --- | --- | --- |
| static noncapturing parameter | `apply(add1, 41)` | specialized helper | 不是 generic function pointer ABI |
| local noncapturing alias | `int f(int)=add1; f(41)` | local binding -> direct/specialized call | 只接受兼容 function shape |
| dynamic local noncapturing | `if(c) f=add2; f(40)` | target-set + tag dispatch | branch merge 必须保留 target family |
| returned noncapturing | `pick()(41)` | returned tag/payload -> rebuild/call | caller 侧必须 materialize returned family |
| returned closure | `make(3)(4)` | returned tag + capture payload -> `fn_make`/wrapper | payload 不能重复 materialize producer call |
| closure literal actual arg | `apply(closure [] int(int z){...}, x)` | closure object view -> function-valued parameter consumer | 普通实参有 side effect 时不能走会重复求值的 shortcut |
| closure captures callable | `closure [f] int(int y){ return f(y); }` | captured callable tag/payload -> helper body rebuild | 仍是 conservative capture payload broadening |
| returned closure captures callable | `make(add1)(4)` where returned closure captures `f` | returned closure payload also carries captured callable | returned 和 capture 两条线在这里合流 |
| higher-order passthrough | `relay(pass, apply, f, x)` | recursive specialization / wrapper shell collapse | `auto f = [=](auto f, ...)` 这类自类型递归不在当前目标内 |
| multiple dynamic callable args | `combine(pick(...), make(...), x)` | payload views -> cross-product dispatch | 不是 runtime callable array loop |
| mixed dynamic/static producer | one arg dynamic returned, one arg static returned closure | normalize each arg into callable payload view | producer kind 先归一，之后再组合 dispatch |

这张表里最重要的不是每行都背下来，而是看出两个趋势：

1. 静态/局部 family 主要靠 specialization 和 direct wrapper。
2. returned/closure/mixed family 越来越靠 payload view、rebuild、callable-object consumer。

## 3. 最小 source family 例子

### 3.1 static noncapturing parameter

```c
int add1(int x){ return x + 1; }
int apply(int f(int), int x){ return f(x); }
int main(){ return apply(add1, 41); }
```

实现心智模型：

```text
semantic:
    check add1 shape matches int(int)

lowering:
    synthesize or select apply__fv_0_add1
    lower call as apply__fv_0_add1(41)
```

这是最早、也最稳定的 specialization family。

### 3.2 dynamic local noncapturing

```c
int add1(int x){ return x + 1; }
int add2(int x){ return x + 2; }
int main(){
  int f(int)=add1;
  if(getint()) f=add2;
  return f(40);
}
```

实现心智模型：

```text
before branch:
    f target-set = { add1 }

then branch:
    f target-set = { add2 }

after merge:
    f target-set = { add1, add2 }
    f tag local stores runtime choice

call:
    branch on f tag
    leaf call add1/add2 wrapper
```

这解释了为什么最近会修 target-set branch merge：

`如果 merge 后只记住最后一个 branch 的 target，就会把动态语义错误地静态化。`

### 3.3 returned closure

```c
int make(int x)(int){
  return closure [x] int (int y){ return x + y; };
}

int main(){
  int f(int)=make(3);
  return f(4);
}
```

实现心智模型：

```text
callee make:
    write returned payload:
        closure family tag
        captured x slot

caller main:
    materialize payload locals from call make(3)
    rebuild env/callable view
    call through wrapper or call_indirect path
```

这里的核心已经不是“closure 语法能不能 parse”，而是：

`returned payload 的产生、保存、复用、重建必须一致。`

### 3.4 higher-order passthrough

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

当前已支持的是这种“显式 higher-order signature + finite specialization”路线。

实现心智模型：

```text
outer callable argument q
    -> resolve/pass through higher-order wrapper

inner callable argument h
    -> keep function-valued parameter metadata

leaf callable argument f
    -> static or dynamic target family

lowering:
    recursively collapse wrapper shell when source shape is recognized
    emit concrete specialized leaf calls
```

这和下面这种目标不是一回事：

```c
auto f = [=](auto f, int x) { return f(f, x); };
```

后者需要真正的 generic self type / polymorphic callable object / recursive type reasoning，当前仓库的 conservative function line 不追这个。

## 4. 多 dynamic callable argument 为什么会难

一个 caller 里如果有多个 function-valued argument，不能只分发一个 tag。

lesson 级伪代码：

```text
lower_call_with_function_args(call):
    callable_payloads = []

    for arg in call.args:
        if arg parameter kind is function:
            callable_payloads.append(normalize_to_payload_view(arg))
        else:
            scalar_args.append(lower_scalar_once(arg))

    combos = cartesian_product(callable_payloads.target_sets)

    emit dispatch tree:
        for each runtime tag combination:
            call concrete specialized leaf helper
```

这解释了最近几类 checkpoint：

1. multiple dynamic returned closure function arguments
2. mixed dynamic and static returned closure function arguments
3. passthrough returned dynamic higher-order mixed closure function arguments
4. three-target returned dynamic higher-order mixed closure function arguments

它们都在逼同一个实现方向：

`每个 callable argument 先归一成 payload/view，再统一组合 dispatch。`

## 5. 最近 side-effect argument repair 放在哪一格

最近的 `Fix function-value side-effect argument evaluation` 不应该理解成“又开了一个新语法”。

它修的是这条规则：

```text
function-valued-parameter dispatch probe
    可以提前分析 callable 参数
    但不能为了 shortcut 重复 lower 普通 side-effect 参数
```

典型 witness：

```c
int main(){
  int f(int)=add1;
  if(getint()) f=add2;
  return relay(pass, apply, f, getint());
}
```

如果 helper/body-eval shortcut 提前求了最后那个 `getint()`，最终真实 call 又求一次，就会从输入里读两次。

所以当前规则是：

```text
if any non-function actual arg has side effects:
    decline helper/body-eval shortcut
    fall back to normal call lowering path
else:
    shortcut may precompute scalar bindings
```

这个规则同样保护：

```c
apply(closure [] int (int z){ return z+1; }, getint())
```

因为 closure actual argument 可以被探测成 callable view，但 `getint()` 仍然只能执行一次。

## 6. 当前最实用的源码地图

如果从这张 family table 回源码，当前最值得先看：

1. `src/semantic/semantic_entry.inc`
   - function shape / initializer / local binding gate
2. `src/ir/ir.c`
   - lowering context、target-set、returned materialization bookkeeping
3. `src/ir/ir_lower_expr.inc`
   - call expression、actual argument、higher-order wrapper、multi dynamic dispatch
4. `src/ir/ir_lower_stmt.inc`
   - returned payload、local binding、function object descriptor finalize
5. `src/ir/ir_verify.inc`
   - explicit callable-object IR contract

最近 side-effect 修复最直接的源码入口是：

```text
ir_call_has_side_effectful_non_function_args(...)
ir_try_build_function_body_eval_bindings_from_callable_views(...)
ir_lower_expression(...)
```

最近 returned-call materialization 复用最直接的源码入口是：

```text
ir_lower_find_returned_call_materialization(...)
ir_lower_find_returned_call_materialization_any_prefix(...)
ir_lower_record_returned_call_materialization(...)
```

## 7. 当前最实用的测试 filter 地图

这张 family table 如果要和真实测试对上，最稳的方式不是背某一个完整测试函数名，而是按 filter 前缀分层搜。

front-door / semantic 层：

```text
SEMANTIC-SECOND-ORDER-*
SEMANTIC-THIRD-ORDER-*
SEMANTIC-*-FNVAL-*
SEMANTIC-*-CLOSURE-*
```

IR shape 层：

```text
IR-DYNAMIC-FNVAL-*
IR-DYNAMIC-RETURNED-CLOSURE-*
IR-CLOSURE-CAPTURE-*
IR-MULTI-DYNAMIC-RETURNED-CLOSURE-FNARGS
IR-PASSTHROUGH-RETURNED-DYNAMIC-HO-*
```

lower-IR shape 层：

```text
LOWER-IR-DYNAMIC-FNVAL-*
LOWER-IR-DYNAMIC-RETURNED-CLOSURE-*
LOWER-IR-CLOSURE-CAPTURE-*
LOWER-IR-MULTI-DYNAMIC-RETURNED-CLOSURE-FNARGS
LOWER-IR-SECOND-ORDER-*
```

compiler-driver 层：

```text
COMPILER-DYNAMIC-FNVAL-*
COMPILER-SECOND-ORDER-*
COMPILER-CLOSURE-CAPTURE-*
COMPILER-DYNAMIC-RETURNED-CLOSURE-*
COMPILER-MULTI-DYNAMIC-RETURNED-CLOSURE-FNARGS
COMPILER-PASSTHROUGH-RETURNED-DYNAMIC-HO-*
```

这四层的含义不同：

```text
SEMANTIC-* 说明 source/type/shape gate
IR-*       说明 canonical lowering route
LOWER-IR-* 说明 downstream storage/value bridge
COMPILER-* 说明当前 extension driver 路径能跑通
```

所以 table 里的“能写通”最好最终回到 `COMPILER-*` 或 runtime-facing focused witness；如果只找到 `SEMANTIC-*`，应该说“前门开了”，不要直接说整条 family 已闭合。

## 8. 最该避免的误讲

当前不能把这条线讲成：

1. “已经有完整泛型 lambda / `auto` callable 了”
2. “所有函数值都已经是统一 runtime object 了”
3. “所有相同文本的 producer call 都可以 dedupe”
4. “higher-order 都已经不需要 specialization 了”

更准确的口径是：

`当前已经能写通非常多 higher-order / returned / closure callable 代码，但仍然依赖 source-shape-aware specialization、payload view、callable-object rebuild 和 focused regression。`

## 9. 读完后接哪篇

最自然往下接：

1. `function_supported_code_lesson.md`
2. `function_evaluation_reuse_lesson.md`
3. `function_checkpoints_lesson.md`
4. `function_implementation_map_lesson.md`
5. `returned_callable_lesson.md`
6. `returned_closure_transport_lesson.md`
7. `callable_object_ir_lesson.md`

如果你是从表格里的 family 名字往实现里追，最稳的分流是：

```text
static / direct function-valued parameter
  -> function_value_callee_lowering_lesson

returned / dynamic callable
  -> returned_callable_lesson

returned closure / producer-side merge / __retclosure_*
  -> returned_closure_transport_lesson

shape / fn_make / call_indirect / verifier
  -> callable_object_ir_lesson
```
