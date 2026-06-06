# Function Supported Code Lesson（compiler_lab）

> 目标：从使用者视角总结当前 `-extension` 下 function-value / closure / higher-order 已经能写通的代码形状，以及哪些看起来相近但仍然不是当前目标。它不是完整语言规范，而是当前 live tree 的“能写什么”速查表。

## 一句话定位

这条线回答的是：

`如果现在就拿这个仓库写 function/closure 代码，哪些 family 比较稳，哪些只是部分 checkpoint，哪些应该先别期待。`

## 1. 总原则

当前所有例子默认都是：

```text
compiler -extension
```

不要把这里的能力自动理解成：

```text
default mode / -riscv / -perf 都已经同等支持
```

当前 function line 的真实状态是：

```text
很多 conservative source family 已经能跑通
但 full generic first-class function value 还没有完成
```

所以这篇按“能写什么”分组，而不是按最终语言理想形态分组。

## 2. 稳定第一层：传 top-level 函数

可以写：

```c
int add1(int x){ return x + 1; }
int apply(int f(int), int x){ return f(x); }
int main(){ return apply(add1, 41); }
```

也可以写多个 function-valued 参数：

```c
int add1(int x){ return x + 1; }
int twice(int x){ return x * 2; }
int compose(int f(int), int g(int), int x){ return f(g(x)); }
int main(){ return compose(add1, twice, 20); }
```

当前支持的直接 sibling：

1. `int f(int)`
2. zero-arg `int f()`
3. `void f()`
4. `void f(int)`
5. builtin binding，比如 `putint`

最小例子：

```c
void applyv(void f(int), int x){ f(x); }
int main(){ applyv(putint, 7); return 0; }
```

实现上，这一组主要靠：

```text
specialization helper
```

而不是 generic function pointer ABI。

## 3. local function value / alias / reassignment

可以写 local alias：

```c
int add1(int x){ return x + 1; }
int main(){
  int f(int)=add1;
  return f(41);
}
```

可以 alias chain：

```c
int add1(int x){ return x + 1; }
int main(){
  int f(int)=add1;
  int g(int)=f;
  return g(41);
}
```

可以 reassignment：

```c
int add1(int x){ return x + 1; }
int add2(int x){ return x + 2; }
int main(){
  int f(int)=add1;
  f=add2;
  return f(40);
}
```

可以 runtime branch rebinding：

```c
int add1(int x){ return x + 1; }
int add2(int x){ return x + 2; }
int main(){
  int f(int)=add1;
  if(getint()) f=add2;
  return f(40);
}
```

这里的重点是：

```text
branch merge 后必须保留 target-set / runtime tag
```

不能把 `f` 静态冻结到某一个 target。

这一组对应的测试关键词通常可以从这些名字开始搜：

```text
DYNAMIC-FNVAL
TERNARY-FNVAL-CALLEE
RETURNED-FNVAL-REASSIGN
WRAPPED-FNVAL-REASSIGN
ASSIGNMENT-RESULT-FNVAL-REASSIGN
PARAM-LOCAL-*-FNVAL-*-NO-SHELL
```

如果你只是想判断“local function value 是不是已经能当普通 local 一样 rebinding”，先看这些比直接翻 helper 名更稳。

## 4. function value 作为实际参数继续转发

可以把 local function value 传进 function-valued parameter：

```c
int add1(int x){ return x + 1; }
int apply(int f(int), int x){ return f(x); }
int main(){
  int g(int)=add1;
  return apply(g, 41);
}
```

也可以是动态 local：

```c
int add1(int x){ return x + 1; }
int add2(int x){ return x + 2; }
int apply(int f(int), int x){ return f(x); }
int main(){
  int g(int)=add1;
  if(getint()) g=add2;
  return apply(g, 40);
}
```

也可以经过 comma / assignment-result / ternary wrapper：

```c
return apply((0, g), 40);
return apply((h = g), 40);
return apply(c ? f : g, 40);
```

这一组现在已经比最早的 direct `apply(add1, 41)` 宽很多。

但仍然要记住：

`这些 wrapper 是被 source-shape-aware lowering 识别的 conservative family，不是任意 expression 都已经能变成 generic callable object。`

## 5. closure literal / local closure

可以写本地 closure：

```c
int main(){
  int x=3;
  int f(int)=closure [x] int (int y){ return x + y; };
  return f(4);
}
```

可以写 multi-capture / two-arg：

```c
int main(){
  int x=3;
  int y=5;
  int f(int,int)=closure [x, y] int (int a, int b){
    return x + y + a + b;
  };
  return f(4, 6);
}
```

可以 forward closure 进 function-valued parameter：

```c
int apply(int f(int), int x){ return f(x); }
int main(){
  int x=3;
  int f(int)=closure [x] int (int y){ return x + y; };
  return apply(f, 4);
}
```

也可以直接把 closure literal 放进 actual argument：

```c
int apply(int f(int), int x){ return f(x); }
int main(){
  return apply(closure [] int (int z){ return z + 1; }, 41);
}
```

当前 closure body 已经支持不少 `int` 表达式/前缀形状，比如：

1. local declaration prefix
2. local assignment prefix
3. compound assignment
4. prefix/postfix increment
5. bitwise/logical/shift expressions
6. comma / assignment expression in return

但这仍然不是 arbitrary full closure runtime。

## 6. returned noncapturing function value

可以返回 top-level function：

```c
int add1(int x){ return x + 1; }
int pick()(int){ return add1; }
int main(){ return pick()(41); }
```

也可以 bind 后再 call：

```c
int add1(int x){ return x + 1; }
int pick()(int){ return add1; }
int main(){
  int f(int)=pick();
  return f(41);
}
```

可以 dynamic return：

```c
int add1(int x){ return x + 1; }
int add2(int x){ return x + 2; }
int pick(int c)(int){
  int f(int)=add1;
  if(c) f=add2;
  return f;
}
int main(){ return pick(1)(40); }
```

这一组的实现关键词：

```text
returned tag / payload
caller-side rebuild
branch-selected dispatch
```

## 7. returned closure

可以返回 closure：

```c
int make(int x)(int){
  return closure [x] int (int y){ return x + y; };
}

int main(){
  return make(3)(4);
}
```

可以 bind 后 forward：

```c
int apply(int f(int), int x){ return f(x); }
int make(int x)(int){
  return closure [x] int (int y){ return x + y; };
}

int main(){
  int f(int)=make(3);
  return apply(f, 4);
}
```

可以 zero-arg / void sibling：

```c
int make(int x)(){
  return closure [x] int (){ return x; };
}

int main(){ return make(3)(); }
```

```c
void makev(int x)(){
  return closure [x] void (){ putint(x); return; };
}

int main(){ makev(7)(); return 0; }
```

returned closure 的关键边界：

```text
producer call must be evaluated once
payload carries tag + capture slots
caller rebuilds callable view before call
```

当前比较值得当作“能写通”的 returned-closure transport family 还包括：

```c
int id(int f(int))(int){ return f; }

int wrap(int c)(int){
  int h(int)=id(make(c));
  return h;
}

int main(){
  return wrap(3)(4);
}
```

以及 producer-side merge：

```c
int pick(int x, int c)(int){
  int f(int)=closure [x] int (int y){ return x + y; };
  int g(int)=closure [x] int (int y){ return x - y; };
  if(c) f=g;
  return f;
}

int wrap(int c, int d)(int){
  int h(int)=pick(5, c);
  int k(int)=pick(7, c);
  int m(int)=d ? h : k;
  return m;
}

int main(){
  return wrap(1, 0)(3);
}
```

这类代码的测试关键词通常是：

```text
DYNAMIC-RETURNED-CLOSURE-PASSTHROUGH
DYNAMIC-RETURNED-CLOSURE-LOCAL-BOUNCE
DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE
DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE
DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE
DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE
```

这说明 returned closure 不再只会 `make(3)(4)` 这种贴着 producer 的形状。

它已经能穿过：

1. local bind
2. local bounce
3. passthrough wrapper
4. producer-side ternary merge
5. statement reassignment
6. zero-arg / zero-arg void siblings

## 8. closure captures callable

可以 capture function value：

```c
int add1(int x){ return x + 1; }
int main(){
  int f(int)=add1;
  int g(int)=closure [f] int (int y){ return f(y); };
  return g(4);
}
```

可以 capture function-valued parameter：

```c
int wrap(int f(int), int x){
  int g(int)=closure [f] int (int y){ return f(y); };
  return g(x);
}
```

也可以让 returned closure capture callable：

```c
int make(int f(int))(int){
  return closure [f] int (int y){ return f(y); };
}
```

这一组的心智模型：

```text
closure env 不只装 int
还要装 callable tag / payload view
```

这不是 full generic closure environment，但它已经是非常重要的 payload-kind broadening。

这一组现在最值得记的实际 witness family 是：

```text
CLOSURE-CAPTURE-STATIC-FNVAL
CLOSURE-CAPTURE-PARAM-FNVAL
RETURNED-CLOSURE-CAPTURE-PARAM-FNVAL
DIRECT-RETURNED-CLOSURE-ACTUAL-CAPTURE-PARAM-FNVAL
DIRECT-RETURNED-ZERO-ARG-VOID-CLOSURE-ACTUAL-CAPTURE-PARAM-FNVAL
RETURNED-CLOSURE-CAPTURE-RETURNED-CLOSURE-IMM-CALL
MIXED-CAPTURE-RETURNED-CLOSURE-IMM-CALL
```

这些名字背后的统一含义是：

```text
closure env 里装的 callable
  may be static function value
  may be function-valued parameter
  may be returned closure payload
  may itself need wrapper/callable-object rebuild
```

## 9. higher-order 显式 signature

可以写二阶：

```c
int apply(int f(int), int x){ return f(x); }
int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }
int add1(int x){ return x + 1; }
int main(){ return pass(apply, add1, 41); }
```

可以写三阶：

```c
int apply(int f(int), int x){ return f(x); }
int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }
int relay(int q(int h(int f(int), int x), int f(int), int x),
          int h(int f(int), int x),
          int f(int),
          int x){
  return q(h, f, x);
}
int add1(int x){ return x + 1; }
int main(){ return relay(pass, apply, add1, 41); }
```

这一组的真实边界：

```text
明确写出的 finite nested function signature 可以被保守处理
auto/generic/self-recursive callable 不是当前目标
```

所以不要把它误扩成：

```c
auto f = [=](auto f, int x){ return f(f, x); };
```

那是另一整个类型系统和泛型实例化问题。

这组代码对应的测试关键词通常很直接：

```text
SECOND-ORDER-FNVAL-FORWARD
SECOND-ORDER-CLOSURE-FNVAL-FORWARD
SECOND-ORDER-ZERO-FNVAL-FORWARD
SECOND-ORDER-ZERO-VOID-FNVAL-FORWARD
THIRD-ORDER-FNVAL-FORWARD
FOURTH-ORDER-FNVAL-FORWARD
FIFTH-ORDER-FNVAL-FORWARD
SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-*-FNVAL
```

如果看到 `SECOND/THIRD/FOURTH/FIFTH-ORDER`，lesson 上应该先理解成：

```text
finite explicit nested signature support
```

而不是：

```text
unbounded generic callable type inference
```

## 10. mixed returned / closure / higher-order family

当前已经能跑一些很强的 mixed family，比如：

```c
int compose(int f(int), int g(int), int x){ return f(g(x)); }

int pick(int c, int f(int))(int){
  int a(int)=closure [f] int (int y){ return f(y); };
  int b(int)=closure [f] int (int y){ return f(f(y)); };
  if(c) a=b;
  return a;
}

int make(int x)(int){
  return closure [x] int (int y){ return x+y; };
}

int add1(int x){ return x+1; }

int main(){
  return compose(pick(0, add1), make(3), 4);
}
```

这类代码说明：

```text
multiple function-valued arguments
dynamic returned closure
static returned closure
captured callable
```

已经能在同一个 call family 里组合。

但三目标 returned dynamic mixed higher-order 那类更深 witness 当前仍然要保守讲：

```text
can lower/compile on focused path
but residual shell cleanup / broad regression convergence may still be in progress
```

更具体一点，当前混合 family 的测试关键词可以按这几组搜：

```text
MULTI-DYNAMIC-RETURNED-CLOSURE-FNARGS
THREE-TARGET-DYNAMIC-RETURNED-CLOSURE-FNARGS
MIXED-DYNAMIC-STATIC-RETURNED-CLOSURE-FNARGS
PASSTHROUGH-RETURNED-DYNAMIC-HO-MIXED-CLOSURE-FNARGS
PASSTHROUGH-RETURNED-DYNAMIC-HO-THREE-TARGET-MIXED-CLOSURE-FNARGS
```

这里要守住一个很细的口径：

```text
unblocked / compile-pass
!=
all residual helper shells gone
```

所以如果你只是写 demo，可以把它当作“复杂 family 有 focused 支持”。

如果你是在判断架构是否 fully generic，还不能把它当作最终完成。

## 11. side-effect 参数

可以写：

```c
int apply(int f(int), int x){ return f(x); }
int main(){
  return apply(closure [] int (int z){ return z+1; }, getint());
}
```

也可以写 higher-order 里的 side-effect scalar actual：

```c
return relay(pass, apply, f, getint());
```

当前重要保证是：

```text
side-effect scalar actual should be evaluated once
```

所以 helper/body-eval shortcut 遇到 side-effect 普通实参会退出。

更多细节看：

```text
function_evaluation_reuse_lesson.md
```

实际测试名里经常能看到这些 clue：

```text
GETINT
SIDE-EFFECT
COMMA-EVAL
STMT-CALL-PREFIX
IF-RETURN-EVAL
```

判断标准不是“最终 helper 少不少”，而是：

```text
getint()/putint() 这种 scalar side effect 不能因为 callable dispatch 分支被执行多次或漏执行。
```

## 12. 目前不该期待的代码

不要期待：

1. `auto` / generic lambda
2. self-recursive callable type inference
3. arbitrary-depth function type 无限嵌套
4. callable 存进任意 aggregate field / array / global 后自然全工作
5. arbitrary helper body 都能被 helper-body evaluator 解释
6. 任意两个长得一样的 `make(3)` 会被安全 dedupe

当前更准确的能力描述是：

`explicit finite function signatures + conservative callable payload transport + focused helper/object lowering。`

## 13. 源码 / 测试地图：怎么确认某类代码是不是真的支持

这篇是“能写什么”的速查表，但如果要回仓库确认，建议按下面这个顺序查。

第一层是 source admission：

- `tests/semantic/semantic_regression_test.c`
- `tests/semantic/semantic_regression_callable_flow.inc`

重点搜：

```text
SEMANTIC-SECOND-ORDER-*
SEMANTIC-THIRD-ORDER-*
SEMANTIC-*-FNVAL-*
SEMANTIC-*-CLOSURE-*
```

这一层只说明：

`语法 / 类型 / shape gate 已经允许或按预期拒绝。`

第二层是 canonical IR / lower-IR shape：

- `tests/ir/ir_regression_test.c`
- `tests/lower_ir/lower_ir_regression_test.c`

重点搜：

```text
IR-DYNAMIC-FNVAL-*
IR-DYNAMIC-RETURNED-CLOSURE-*
IR-CLOSURE-CAPTURE-*
IR-MULTI-DYNAMIC-RETURNED-CLOSURE-FNARGS
IR-PASSTHROUGH-RETURNED-DYNAMIC-HO-*
LOWER-IR-DYNAMIC-FNVAL-*
LOWER-IR-DYNAMIC-RETURNED-CLOSURE-*
LOWER-IR-SECOND-ORDER-*
```

这一层说明：

`payload / target-set / callable-object / wrapper bridge 的中间形状已经被锁住。`

第三层是 driver/runtime-facing behavior：

- `tests/compiler/compiler_driver_test.c`

重点搜：

```text
COMPILER-DYNAMIC-FNVAL-*
COMPILER-SECOND-ORDER-*
COMPILER-CLOSURE-CAPTURE-*
COMPILER-DYNAMIC-RETURNED-CLOSURE-*
COMPILER-MULTI-DYNAMIC-RETURNED-CLOSURE-FNARGS
COMPILER-PASSTHROUGH-RETURNED-DYNAMIC-HO-*
```

这一层才更接近：

`这段 source 现在真的能在当前 extension 编译路径上走通。`

如果你看到一个新代码形状，只在 semantic 过了，不要立刻把它写进“能稳定写通”。更稳的判断是：

```text
semantic green
  -> 可以说 front door 开了

IR/lower-IR green
  -> 可以说 lowering shape 有 checkpoint

compiler/runtime green
  -> 才适合放进本篇“能写通代码”主列表
```

## 14. 读完后接哪篇

最自然往下接：

1. `function_family_table_lesson.md`
2. `higher_order_callable_lesson.md`
3. `returned_closure_transport_lesson.md`
4. `callable_object_ir_lesson.md`
5. `function_evaluation_reuse_lesson.md`
6. `returned_callable_lesson.md`
7. `closure_capture_callable_lesson.md`
