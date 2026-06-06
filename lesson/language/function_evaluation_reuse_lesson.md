# Function Evaluation Reuse Lesson（compiler_lab）

> 目标：专门讲当前 function/closure/returned-callable 主线里最容易出严重 bug 的一类问题：什么时候 lowering 可以复用已经 materialize 的 callable payload，什么时候必须重新求值，以及 side-effect 普通实参为什么会让一些 helper/body-eval shortcut 直接退出。

## 一句话定位

这条线回答的是：

`function-value lowering 里的“复用”只能复用同一次求值已经产生的结果，不能把两个看起来一样的表达式、两个不同闭包实例、或者两个本来应该执行两次的 producer 合并掉。`

## 1. 为什么这篇必须单独讲

当前 function 主线已经很宽：

- returned closure
- returned noncapturing
- closure captures callable
- higher-order passthrough
- multiple dynamic callable arguments
- mixed returned producer

这些 family 都会反复问同一个危险问题：

```text
我现在需要一个 callable payload/view。
之前是不是已经 materialize 过一个可以复用的？
```

这个问题答错，后果有两种：

1. 重复求值
   - `make(3)` / `pick(getint())` 被 call 多次
   - side effect 多执行
   - asm/IR 里出现重复 producer call
2. 错误合并
   - 本来应该执行两次的 producer 被执行一次
   - 两个不同闭包实例被当成同一个
   - runtime tag / capture payload 用错

所以这里的核心规则很硬：

```text
reuse evaluation result
!=
dedupe same-looking expression
```

## 2. 一个最小危险例子

看这个形状：

```c
int make(int x)(int){
  return closure [x] int (int y){ return x + y; };
}

int wrap(int f(int), int x){ return f(x); }

int main(){
  return wrap(make(3), 4);
}
```

lowering 过程中可能有多个消费者想看 `make(3)`：

1. actual-argument path 想知道它是不是 returned closure
2. dynamic-family probe 想知道它的 target-set
3. payload materializer 想拿到 `tag + capture slots`
4. callable-object rebuild 想做 `fn_make + call_indirect`

如果每个消费者都自己 lower 一遍 `make(3)`，就会变成：

```text
call make(3)
call make(3)
call make(3)
...
```

这不只是代码膨胀。只要 producer 里面有 side effect，语义就错。

## 3. 当前安全复用的口径

当前 lesson 级最稳口径是：

```text
只复用同一个 AST call expression 已经 materialize 出来的 payload locals。
```

也就是说，可以复用的是：

```text
same AstExpression* call_expr
  -> previous payload locals
```

不是：

```text
same source text
same callee name
same argument value
same generated local prefix
```

这一点很关键。

两个 `make(3)` 即使源码长得完全一样，也可能代表两次求值：

```c
int f(int)=make(3);
int g(int)=make(3);
```

它们应该产生两个独立 closure value。

只有这种情况，才应该复用：

```text
lowering 同一个 call expression 的不同消费视图
    第一次已经 materialize payload
    后续消费者只拿同一组 payload locals
```

## 4. 源码里的 bookkeeping 心智模型

当前源码里可以直接看到 returned-call materialization 记录。

lesson 级伪代码可以写成：

```text
record_returned_call_materialization(call_expr, prefix, payload_locals):
    table.append({
        call_expr identity,
        payload local name prefix,
        payload local ids,
        producer external,
        closure-family flag
    })

find_returned_call_materialization(call_expr, prefix):
    return exact previous entry for this same call expression and prefix

find_returned_call_materialization_any_prefix(call_expr):
    return any previous entry for this same call expression
```

对应源码入口：

```text
ir_lower_record_returned_call_materialization(...)
ir_lower_find_returned_call_materialization(...)
ir_lower_find_returned_call_materialization_any_prefix(...)
```

这里的 `any_prefix` 不是“任意表达式都能共用”。

它更像是在说：

```text
同一个 returned producer call，
之前可能以 __retclosure_argcap / __retclosure_immediate / __retclosure_specarg
这些不同消费前缀之一 materialize 过。

只要 AST call identity 相同，后续视图可以复用已经产生的 payload locals。
```

## 5. 为什么不能按字符串或 callee 名 dedupe

不能这么做：

```text
if callee name == "make" and argument dump == "3":
    reuse old payload
```

因为这会把应该执行多次的代码合并掉。

危险例子：

```c
int tick(){ putint(1); return 3; }
int f(int)=make(tick());
int g(int)=make(tick());
```

正确语义是：

```text
tick()
tick()
```

如果按“长得一样” dedupe，就只会执行一次。

所以 lesson 里要牢牢记住：

`复用单位是 lowering 内同一个 AST 节点的已求值结果，不是源码文本等价类。`

## 6. side-effect 普通实参为什么又是另一条规则

returned-call payload 复用管的是：

```text
producer call 本身别被不同 callable consumer 重复 materialize
```

最近的 side-effect 修复管的是另一件事：

```text
function-valued-parameter dispatch probe 别重复求值普通实参
```

典型例子：

```c
int apply(int f(int), int x){ return f(x); }
int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }
int relay(int q(int h(int f(int), int x), int f(int), int x),
          int h(int f(int), int x),
          int f(int),
          int x){
  return q(h, f, x);
}

int main(){
  int f(int)=add1;
  if(getint()) f=add2;
  return relay(pass, apply, f, getint());
}
```

这里 lowering 可能想先做 helper/body evaluation：

```text
probe relay/pass/apply body
pre-bind arguments
see whether a specialized leaf call can be emitted
```

但最后那个 `getint()` 是普通实参，而且有 side effect。

如果 probe 阶段先 lower 一次，最终真实 call 又 lower 一次，就会读两次输入。

所以当前规则是：

```text
if non-function actual arg has side effects:
    do not use helper/body-eval shortcut that pre-lowers scalar args
```

对应源码入口：

```text
ir_call_has_side_effectful_non_function_args(...)
ir_try_build_function_body_eval_bindings_from_callable_views(...)
```

## 7. function actual 和 scalar actual 的不同待遇

这条规则为什么只说 `non-function actual arg`？

因为 function-valued argument 的探测本来就是当前 mainline 的核心工作：

```text
f / closure literal / returned callable
    -> inspect as callable view
    -> maybe dispatch/specialize
```

但普通 scalar argument 不应该为了“探测 callable”被提前执行。

lesson 级伪代码：

```text
for each actual argument:
    if parameter is function-valued:
        inspect callable view
    else:
        if expression has side effects:
            decline shortcut
        else:
            may pre-lower scalar value for helper-body evaluation
```

所以这里不是“所有 side effect 都完全不能出现”。

更准确地说：

`side-effect scalar actual 可以出现，但它会关掉某些会提前求值的 shortcut。`

## 8. 两条 exactly-once 规则合起来怎么用

当前 function 主线可以把 exactly-once 规则分成两类：

1. producer call exactly once
   - 同一个 returned call expression 的 payload materialization 可以复用
   - 目标是避免 `make(...)` / `pick(...)` 被不同消费视图重复 lower
2. scalar actual exactly once
   - side-effect 普通实参会让 helper/body-eval shortcut 退出
   - 目标是避免 `getint()` / `putint(...)` / assignment 这类实参被 probe 和 real call 各求一次

对应 lesson 级总伪代码：

```text
lower_callable_consumer(expr):
    if expr is returned call:
        if materialized_payload_exists_for_same_call_expr(expr):
            return reuse_payload_locals(expr)
        payload = materialize_returned_call_once(expr)
        record_payload(expr, payload)
        return payload

lower_function_parameter_call(call):
    if has_side_effectful_non_function_args(call):
        skip helper_body_eval_shortcut
    else:
        try helper_body_eval_shortcut

    continue with normal dispatch / specialization / call lowering
```

## 9. 正确性边界：什么时候必须重新求值

下面这些都必须重新求值：

1. 两个不同 AST call expression
   - `make(3)` 写了两次
2. 同一个 callee 不同实参
   - `make(3)` 和 `make(4)`
3. 同一个 source 文本但在循环里每轮重新执行
   - 每轮都应该得到新的 evaluation
4. producer 有 side effect 且 source 语义要求多次执行
   - 不能因为 IR 里“看起来能省”就合并
5. 闭包 capture payload 不同
   - 不能因为 wrapper 名相同就共用 env

可以复用的是：

1. 同一个 AST call expression 在同一次 lowering 路径里已经 materialize 的 payload locals
2. 同一个 payload 被不同 prefix/consumer view 读取
3. 已经 materialize 的 callable-object view 被同一路径后续 dispatch 消费

这就是“安全复用”的核心。

## 10. 为什么这不是优化，而是正确性

看到重复 `call make(...)` 时，第一反应可能是：

```text
代码变大了，编译也慢了。
```

这是真的，但还不是最严重的。

更严重的是：

```c
int make_count = 0;
int make(int x)(int){
  make_count = make_count + 1;
  return closure [x] int (int y){ return x + y; };
}
```

如果同一个 source evaluation 被 lowering 重复 materialize，`make_count` 会错。

所以这条线 lesson 上应该被归类为：

```text
semantic correctness / evaluation order
```

而不是：

```text
code size cleanup only
```

## 11. 和 generic first-class function values 的关系

generic end-state 理想上会更像：

```text
evaluate expression once
produce callable object value
all consumers read that value
```

当前还没有完全到这个世界，所以 live tree 需要这些 conservative 记录：

```text
AST call identity
payload locals
target-set metadata
side-effect scalar arg guard
```

它们不是最终美学最好的模型，但它们是在当前 architecture 下守住正确性的关键桥。

## 12. 读完后接哪篇

最自然往下接：

1. `function_family_table_lesson.md`
2. `returned_closure_transport_lesson.md`
3. `callable_object_ir_lesson.md`
4. `returned_callable_lesson.md`
5. `function_implementation_map_lesson.md`
6. `generic_function_values_lesson.md`

因为这篇讲的是：

```text
producer evaluation 什么时候可以复用
```

而后面两篇分别讲：

```text
returned_closure_transport_lesson:
    复用出来的 payload locals 怎么被不同 __retclosure_* view 消费

callable_object_ir_lesson:
    最后这些 payload/view 怎么变成 shape + fn_make + call_indirect
```
