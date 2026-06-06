# Callable Object IR Lesson（compiler_lab）

> 目标：把当前 function-value / closure / returned-callable 主线里最关键的 IR 合约讲清楚：`shape`、`fn_make`、`fn_code`、`fn_env`、`fn_shape`、`call_indirect` 到底在表达什么，为什么它们已经不只是设计稿，而是 canonical IR verifier 会检查的真实 contract。

## 一句话定位

这条线回答的是：

`当前仓库正在把 callable 从 hidden helper / tag branch / payload slot 约定，收敛成 canonical IR 可以看见、dump 可以展示、verifier 可以拒绝的显式 callable-object 合约。`

如果说前面的 function lessons 在讲：

```text
source function value
  -> descriptor / payload / helper / wrapper
```

这篇讲的就是下一层：

```text
descriptor / payload / wrapper
  -> shape + fn_make
  -> call_indirect
  -> verifier contract
```

## 1. 为什么需要 explicit callable-object IR

早期 function-value 支持可以靠 source-shape-specific lowering 先跑起来：

```text
apply(add1, 41)
  -> apply__fv_0_add1(41)
  -> call add1(41)
```

returned closure 也可以先靠 payload slot 约定：

```text
call make(...)
write ftag / closurecap locals
branch on ftag
call closure helper
```

这些 staging 都是合理的。

但当仓库开始同时支持：

1. local function-value binding
2. dynamic rebinding
3. returned noncapturing function value
4. returned closure
5. closure captures callable
6. higher-order passthrough
7. mixed dynamic/static returned producers

继续只靠“每个 witness 自己展开一段 branch/direct-call lowering”会越来越脆。

所以当前主线的方向是：

`把 callable value 的 code/env/shape 显式放进 IR，而不是永远藏在 lowering context 里。`

## 2. 当前最小对象模型

lesson 级心智模型：

```text
callable object =
    code   // wrapper/helper/function entry
    env    // closure capture payload root, noncapturing 时通常是 0
    shape  // visible signature: args + return
```

在 canonical IR dump 里，最常见的是：

```text
shape shape.0(int) -> int

tmp.0 = fn_make __fnwrap_add1, 0, shape.0
tmp.1 = call_indirect tmp.0(41)
ret tmp.1
```

这段不要理解成“真的已经有最终 ABI 级函数指针对象”。

更准确地说：

`canonical IR 现在已经用一个显式 object-like contract 表达 callable dispatch，而 lower/backend 仍然可以把它桥接回 wrapper call。`

## 3. `shape` 是什么

`shape` 是 function callable 的 visible signature。

当前 public IR 里有：

```c
typedef struct {
    size_t id;
    AstFunctionReturnType return_type;
    AstFunctionReturnType *parameter_types;
    size_t parameter_count;
} IrFunctionShape;
```

所以 lesson 里最稳的理解是：

```text
shape id
  -> return type
  -> visible parameter count
  -> visible parameter types
```

它不等于 closure capture 数量。

例如：

```c
int make(int x)(int){
  return closure [x] int (int y){ return x + y; };
}
```

caller 侧最终 callable shape 是：

```text
shape(int) -> int
```

但 env/capture payload 里还有 `x`。

所以：

```text
visible args      -> shape
hidden captures   -> env / payload
```

这两个不能混。

## 4. `fn_make` 是什么

`fn_make` 创建一个 callable temp。

当前 dump 形态通常是：

```text
tmp.0 = fn_make callee_name, env, shape.N
```

比如 noncapturing：

```text
tmp.0 = fn_make __fnwrap_add1, 0, shape.0
```

比如 closure-backed：

```text
tmp.0 = fn_make __fnwrap_closure_main__closure_f_..., f$closurecap$0, shape.0
```

lesson 级伪代码：

```text
emit_callable_object(target, env, shape):
    obj = fn_make target, env, shape
    return obj
```

这里的 `target` 往往不是用户源码里的原始函数名，而是 lowering 生成/选择过的 wrapper：

```text
add1
  -> __fnwrap_add1

closure helper
  -> __fnwrap_closure_*

returned closure helper
  -> __fnwrap_closure_pick__closure_*
```

这正是 `fn_make` 的价值：

`source callable 的真实调用入口和 hidden env 被显式绑成一个 IR object。`

## 5. `call_indirect` 是什么

`call_indirect` 消费 callable object。

dump 形态：

```text
tmp.1 = call_indirect tmp.0(41)
```

lesson 级伪代码：

```text
call_indirect(obj, visible_args):
    code  = fn_code(obj)
    env   = fn_env(obj)
    shape = fn_shape(obj)
    require visible_args.count == shape.parameter_count
    return code(env, visible_args...)
```

当前 lower/backend 可以把它桥接成 wrapper call：

```text
call_indirect(fn_make __fnwrap_add1, 0, shape(int)->int, 41)
  -> call __fnwrap_add1(0, 41)
```

所以你在 lower-IR / compiler text 里看到 direct `call __fnwrap_*`，不代表 canonical callable-object path 没走。

它通常表示：

`canonical IR 已经有 explicit object contract，downstream 选择了更直接的 wrapper-call bridge。`

## 6. `fn_code / fn_env / fn_shape`

这三个是 projection。

lesson 级含义：

```text
fn_code(obj)   -> callable 的 code entry
fn_env(obj)    -> callable 的 hidden env/capture payload
fn_shape(obj)  -> callable 的 visible signature id
```

当前 verifier 对它们故意很窄：

`projection operand 必须能 resolve 回 fn_make。`

这不是因为以后永远只能这样，而是因为第一版 canonical contract 先保证：

```text
callable projection 不会从任意 int temp 上乱读
```

也就是说，当前 projection 是 conservative IR legality surface，不是最终 optimizer/devirtualization 模型。

## 7. verifier 真正守哪些边界

这一点非常重要：callable-object IR 现在已经不是“dump 看起来像对象”。

`src/ir/ir_verify.inc` 会明确检查这些不变量。

最值得记的编号：

| verifier code | lesson 级含义 |
| --- | --- |
| `IR-VERIFY-074` | `fn_make` result 必须是 temp |
| `IR-VERIFY-075` | `fn_make` callee 不能是空名 |
| `IR-VERIFY-076` | `fn_make` shape id 必须存在 |
| `IR-VERIFY-077` | `fn_make` target function 必须存在 |
| `IR-VERIFY-078` | target 参数数必须匹配 hidden-env + shape visible args |
| `IR-VERIFY-079` | target return type 必须匹配 shape |
| `IR-VERIFY-080` | target visible parameter type 必须匹配 shape |
| `IR-VERIFY-085` | `call_indirect` callee 必须是 temp |
| `IR-VERIFY-086` | `call_indirect` callee 必须 resolve 到 `fn_make` |
| `IR-VERIFY-088` | `call_indirect` visible arg count 必须匹配 shape |
| `IR-VERIFY-090` | `fn_code/env/shape` projection operand 必须 resolve 到 `fn_make` |

这些编号背后的统一意思是：

```text
fn_make decides callable identity + env + shape
call_indirect must obey that shape
projection must come from a real callable object
```

## 8. 为什么 `IR-VERIFY-088` 经常重要

`IR-VERIFY-088` 是 function 主线里很值得单独记的一个坑。

它表示：

```text
call_indirect passes N args but shape expects M
```

这类 bug 最常出现在：

1. zero-arg closure
2. zero-arg void closure
3. higher-order passthrough
4. returned closure actual argument
5. wrapper/helper arity 修复过程中

原因是：

```text
hidden env count
visible arg count
capture payload slot count
```

三者看起来都像“参数数量”，但语义完全不同。

正确模型是：

```text
wrapper physical params:
    hidden env + visible args

call_indirect visible args:
    exactly shape.parameter_count

closure payload slots:
    captured values / captured callable payload
```

所以 zero-arg void closure 仍然可能有 env/payload，但 `call_indirect` visible arg count 必须是 0。

## 9. dynamic family 怎么映射到 object path

dynamic noncapturing：

```c
int f(int)=add1;
if(c) f=add2;
return f(40);
```

lesson 级 IR story：

```text
if selected add1:
    obj = fn_make __fnwrap_add1, 0, shape(int)->int
    call_indirect obj(40)
else:
    obj = fn_make __fnwrap_add2, 0, shape(int)->int
    call_indirect obj(40)
```

dynamic returned closure：

```c
int f(int)=pick(5,c);
return f(3);
```

lesson 级 IR story：

```text
call pick(...) once
materialize returned payload:
    ftag
    closurecap

if ftag == closure_f:
    obj = fn_make __fnwrap_closure_pick__closure_f_..., closurecap, shape(int)->int
    call_indirect obj(3)
else:
    obj = fn_make __fnwrap_closure_pick__closure_g_..., closurecap, shape(int)->int
    call_indirect obj(3)
```

所以 callable-object IR 并不自动消灭 branch。

它先消灭的是：

`每个 branch 里到底怎样构造/调用 callable 的 ad hoc contract。`

## 10. 和 returned closure payload 的关系

returned closure transport 负责：

```text
producer call
  -> ftag / closurecap payload
  -> local / immediate / actual-argument / return-again view
```

callable-object IR 负责：

```text
selected payload view
  -> fn_make wrapper, env, shape
  -> call_indirect
```

所以二者分工是：

```text
returned payload answers:
    which target family and captures do we have?

callable object answers:
    how do we represent the final callable invocation in IR?
```

这也是为什么同一个 witness 里你可能同时看到：

```text
__retclosure_argslot_*
__retclosure_argcap_*
fn_make __fnwrap_closure_*
call_indirect
```

它们不是重复概念，而是两层 contract。

## 11. 和 specialization helper 的关系

specialization helper 没有完全消失。

当前更准确的说法是：

```text
old:
    source function-value call
      -> specialized helper
      -> direct call

current:
    some families still need specialized helper
    many consumer leaves now converge to fn_make + call_indirect
```

例如 higher-order line 里可能仍然有：

```text
relay__fv_0_pass_1_apply_2_add1
pass__fv_0_apply_1_add1
apply__fv_0_add1
```

但 leaf callable dispatch 可能已经是：

```text
fn_make __fnwrap_add1, 0, shape.0
call_indirect tmp(...)
```

所以不要用“是否还有 `__fv_*` helper”粗暴判断这条线有没有进 object model。

更好的判断是：

```text
最终 callable consumer 有没有落到 shape + fn_make + call_indirect
```

## 12. 当前源码地图

最值得看的文件：

1. `include/ir.h`
   - `IR_INSTR_FN_MAKE`
   - `IR_INSTR_FN_CODE`
   - `IR_INSTR_FN_ENV`
   - `IR_INSTR_FN_SHAPE`
   - `IR_INSTR_CALL_INDIRECT`
   - `IrFunctionShape`
2. `src/ir/ir_core.inc`
   - `ir_emit_fn_make(...)`
   - `ir_emit_call_indirect(...)`
   - function shape allocation/free helpers
3. `src/ir/ir_dump.inc`
   - `shape shape.N(...) -> ...`
   - `fn_make`
   - `fn_code / fn_env / fn_shape`
   - `call_indirect`
4. `src/ir/ir_verify.inc`
   - `IR-VERIFY-074..090`
5. `src/ir/ir_lower_expr.inc`
   - current callable-object dispatch emission paths
6. `tests/ir/ir_verifier_test.c`
   - minimal verifier contract witnesses
7. `tests/ir/ir_regression_test.c`
   - source-family regression expectations around `shape + fn_make + call_indirect`

最值得搜的关键词：

```text
IR_INSTR_FN_MAKE
IR_INSTR_CALL_INDIRECT
IrFunctionShape
fn_make
call_indirect
IR-VERIFY-088
__fnwrap_
```

## 13. 测试地图

最小 verifier tests：

```text
test_ir_verifier_rejects_fn_make_missing_shape
test_ir_verifier_rejects_call_indirect_arg_mismatch
test_ir_verifier_rejects_call_indirect_non_callable_temp
test_ir_verifier_accepts_call_indirect_with_moved_callable_temp
```

典型 IR regression 断言会找：

```text
shape shape.0(int) -> int
fn_make __fnwrap_add1, 0, shape.0
call_indirect tmp.0(41)
```

或者 closure-backed：

```text
fn_make __fnwrap_closure_..., capture_local, shape.0
call_indirect tmp(...)
```

returned closure / dynamic family 则经常同时断言：

```text
call pick(...)
__retclosure_*
fn_make __fnwrap_closure_pick__closure_*
call_indirect
```

## 14. 当前 still-kept boundary

不能把这篇误读成：

1. generic function pointer ABI 已经完成
2. callable object 可以任意存入 aggregate/global/array 并自然逃逸
3. closure env 生命周期已经有统一 heap runtime
4. optimizer 已经能对所有 callable object 做完美 devirtualization
5. specialization / helper / payload/tag machinery 已经可以删掉

更准确的边界是：

`canonical IR 已经有第一版显式 callable-object 合约，并且很多 live function-value consumer 已经迁到这条合约上；但它仍然服务于 conservative source-family lowering，不是最终 full generic callable runtime。`

## 15. 读完后接哪篇

最自然往下接：

1. `returned_closure_transport_lesson.md`
2. `returned_callable_lesson.md`
3. `higher_order_callable_lesson.md`
4. `function_evaluation_reuse_lesson.md`
5. `generic_function_values_lesson.md`

因为 callable-object IR 是这些线最后汇合的地方：

```text
returned payload
  + higher-order specialization
  + closure capture payload
  + exact-once materialization
  -> shape + fn_make + call_indirect
```
