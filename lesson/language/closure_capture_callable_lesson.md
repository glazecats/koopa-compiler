# Closure Captures Callable Lesson（compiler_lab）

> 目标：专门讲当前 language round 里一个非常新的扩张面，也就是 closure 不再只 capture 标量，而开始 capture function value / function-valued parameter / returned function-parameter family，这条线为什么重要、当前到底开到了哪。

## 一句话定位

这条线回答的是：

`closure 一旦开始捕获 callable，本质上就不再只是“把几个 int 拷进去”，而是在逼 callable transport、specialization、callable-object rebuild 这几条线真正接起来。`

## 1. 为什么这篇值得单独存在

以前我们讲 closure，最自然的最小心智模型是：

```c
int x = 3;
int f(int) = closure [x] int (int y) { return x + y; };
return f(4);
```

这时 capture 的只是普通标量。

但最近这条线真正往前迈的一步是：

```c
int f(int)=add1;
int g(int)=closure [f] int (int y) { return f(y); };
return g(4);
```

这里 capture 的已经不是 `int`，而是 callable 本身。

这就让问题从：

- capture payload 怎么拷贝

变成：

- 被 capture 的 callable family 怎么表示
- closure helper 里怎么恢复它
- 它如果本身还来自 parameter / returned family，又怎么继续 specialization

## 2. 最小 mental model

最稳的 lesson 记法不是把它理解成“闭包里套函数指针”。

当前 live tree 更像是在做：

```text
capture callable
  -> capture callable identity / tag / family metadata
  -> closure helper body 在真正调用前重新恢复这个 callable
  -> 再走已有的 wrapper / specialization / call_indirect spine
```

也就是说，这条线的关键不是“capture list 语法通过了”，而是：

`callable 作为被捕获对象，终于开始进入 closure transport contract。`

## 2.1 一个更贴近实现的 capture-layout 心智模型

当前最稳的 lesson 级伪代码不是：

```text
env = { x, y, z }
```

而更像：

```text
env = {
  scalar_capture_0,
  scalar_capture_1,
  callable_capture_tag_0,
  callable_capture_payload_0?,
}
```

也就是说，一旦 capture 的是 callable，env 里装的就不再只是普通标量。

它还可能需要：

- callable family tag
- callable target-set identity
- 后续 rebuild 所需的最小 payload

这也是为什么这条线会和 returned-callable / callable-object rebuild 一起长。

## 3. 第一层：capture static noncapturing local

当前第一层已 landed 的形状是：

```c
int f(int)=add1;
int g(int)=closure [f] int (int y) { return f(y); };
return g(4);
```

lesson 上最重要的口径是：

- capture 的不是普通 scalar local
- capture 的是一个已知 noncapturing function-value family
- helper body 最终会把它恢复成 callable-object / wrapper call 路径

一个很稳的伪代码可以写成：

```text
capture payload:
  callable-tag-for-add1

closure body:
  recovered = rebuild_callable(add1-family)
  return call_indirect(recovered, y)
```

这一步的工程意义非常大，因为它证明了：

`closure helper 自己也能成为 higher-order consumer`

## 3.1 伪代码：closure helper 里怎么恢复 captured callable

这条线的实现心智模型可以直接写成：

```text
lower_closure_literal(captures, body):
    env_layout = build_env_layout(captures)
    helper = synthesize_closure_helper(body, env_layout)
    return make_local_closure_value(helper, env_layout)

synthesize_closure_helper(body, env_layout):
    when body refers to captured callable f:
        tag = load env.callable_tag_for_f
        recovered = rebuild_callable_from_tag(tag)
        lower f(y) as call_indirect(recovered, y)
```

所以 closure helper 不是“神奇地还能看见外层函数值”。

更准确地说，它是在：

- env 里拿回 captured callable identity
- 再通过已有 rebuild/call spine 把它恢复出来

## 4. 第二层：capture function-valued parameter

再往前一步，shape 会变成：

```c
int wrap(int f(int), int x) {
  int g(int)=closure [f] int (int y) { return f(y); };
  return g(x);
}
```

这时事情会更难，因为被 capture 的 `f`：

- 不是本地静态已知名字
- 而是 outer function 的 function-valued parameter

当前仓库能把这条线打通，关键不是“泛化成任何 callable 都行”，而是：

`closure helper 会继承 enclosing function 的 function-valued-parameter metadata，等外层 actual target 确定后，再合成具体 specialization。`

也就是说，当前更像：

```text
wrap(add1, 4)
  -> outer binding knows f == add1
  -> synthesize closure helper specialized for captured add1
  -> closure body calls recovered add1-family callable
```

所以这一步的正确理解是：

`closure capture 已经开始吃 function-shape metadata，不只是吃普通 capture slots。`

## 4.1 伪代码：为什么要等 outer binding 已知再 specialization

这一层最稳的实现伪代码是：

```text
lower_wrap_call(wrap, actual_f=add1, x=4):
    bind outer function-valued parameter:
        f -> add1

    when lowering closure helper inside wrap:
        inherited_metadata says captured f has same shape as outer slot
        synthesize specialized helper for captured add1
```

也就是：

```text
outer parameter binding
    -> inherited closure-helper metadata
    -> captured-callable specialization
```

这解释了为什么当前 kept bridge 仍然是保守的：

不是 closure helper 自己在运行时“推断”被 capture 的 callable，
而是外层 binding 一旦确定，helper 才跟着被 concretize。

## 5. 第三层：returned function-parameter capture

再往前就是：

```c
int make(int f(int))(int) {
  return closure [f] int (int y) { return f(y); };
}
```

这条线一打开，仓库必须同时回答三件事：

1. closure capture 里装的 callable 怎么放进 returned payload
2. caller 怎么把这个 payload 重建成 closure env
3. closure body 之后怎么继续通过 wrapper / indirect-call spine 调被 capture 的 callable

一个 lesson 级伪代码可以压成：

```text
callee:
  returned payload =
    closure-family-tag
    + captured-callable-tag

caller:
  rebuild env from returned payload
  obj = fn_make(retclosure_wrapper, env, shape)
  call_indirect(obj, visible_args...)
```

这一步的重要意义在于：

`returned callable` 和 `closure captures callable` 两条线在这里第一次真正汇流。`

## 5.1 伪代码：returned parameter capture 的两段式 lower

这条线很适合直接用两段式伪代码讲：

```text
callee make(...):
    capture outer function-valued parameter f
    write captured callable tag into returned payload
    write closure-family tag into returned payload
    return payload

caller:
    payload = call make(...)
    env.callable_tag = payload.captured_callable_tag
    env.family_tag = payload.family_tag
    obj = fn_make(retclosure_wrapper, env, shape)
    call_indirect(obj, arg)
```

这个模型能帮助读者理解：

- 为什么 returned payload 会越来越像“小对象”
- 为什么 closure-capture family 一旦跨 return boundary，就必须借用 returned_callable 的 rebuild spine

## 6. 第四层：dynamic returned parameter capture

再往上，source 会变成这种 family：

```c
int make(int c, int f(int))(int) {
  int g(int)=closure [f] ...;
  int h(int)=closure [f] ...;
  if(c) g=h;
  return g;
}
```

然后 caller 再做：

```c
int g(int)=make(1, add1);
return g(4);
```

这里最核心的变化是：

- returned payload 不只要带 closure-family 信息
- 还要带 captured callable family 信息
- bind-and-call 点还可能需要 branch-selected specialized helper

所以当前 kept path 仍然是保守的：

`outer returned family dynamic + inner captured callable family known`

而不是：

`所有层级都 fully generic runtime callable`

## 7. 为什么最近会反复出现 passthrough / alias / wrapper

这条线最近真正成熟，不是因为一个最小例子过了，而是因为它开始撑住：

1. local alias
2. passthrough call chain
3. returned-call materialization recursion
4. ternary returned-call consumer
5. mixed producer family

也就是说，closure capture callable 现在已经不是：

`只能在 producer 旁边当场消费`

而是：

`它开始能沿着 returned_callable 那条 rebuild spine 多跳传递`

## 7.1 伪代码：多跳 passthrough 为什么会难

一个更接近最近提交的心智模型是：

```text
find_real_callable_producer(expr):
    if expr is local alias:
        return find_real_callable_producer(alias_target)
    if expr is passthrough call like id(expr1):
        return find_real_callable_producer(expr1)
    if expr is ternary(cond, lhs, rhs):
        return pair(find_real_callable_producer(lhs),
                    find_real_callable_producer(rhs))
    return direct_producer(expr)
```

而 closure capture callable 这条线之所以麻烦，是因为：

```text
producer lookup
    + returned payload materialization
    + captured-callable specialization lookup
```

这三件事现在会在同一个 family 里叠起来。

## 8. 源码地图：closure captures callable 现在主要看哪

这条线如果要回源码，当前最值得先看两处：

1. `src/semantic/semantic_entry.inc`
   - 看 front-door admission
   - 特别是：
     - `semantic_extension_resolve_local_function_value_binding_with_expected_type(...)`
     - `semantic_extension_function_value_initializer_expr_matches_expected(...)`
   - 这几处现在已经明确区分：
     - closure-backed
     - non-closure-backed
     - local binding / parameter binding / returned binding
2. `src/ir/ir_lower_stmt.inc`
   - 看 payload / transport / specialization 真正怎么 lower
   - 特别是：
     - closure payload copy
     - returned payload materialization
     - binding reassignment / tag propagation

如果把这条课和源码的关系压成一句：

```text
semantic_entry.inc
  决定“这个 capture-callable shape 算不算允许”

ir_lower_stmt.inc
  决定“允许之后 tag/payload/helper/rebuild 怎么真的接起来”
```

## 8.1 为什么 `semantic_entry.inc` 现在特别重要

这条线之所以不能只看 lowering，是因为很多最近 reopened 的 family 本质上先是 front-door 问题：

- direct closure literal actual argument
- declaration-only local from closure literal
- closure-backed local reassignment
- returned function-parameter capture compatibility

当前这些问题在源码里并不是分散在很多处。

它们已经明显往同一套入口靠：

```text
semantic_extension_check_function_value_expression(...)
semantic_extension_function_value_initializer_expr_matches_expected(...)
semantic_extension_resolve_local_function_value_binding_with_expected_type(...)
```

所以 lesson 上最好把这条线理解成：

`capture-callable line 现在已经不仅是 lowering 问题，也是在逼 semantic front door 收敛成共享 function-value gate。`

## 8.2 读这条线时，最值得盯着哪种“错位”

closure captures callable 这条线最常见的问题，不是“完全没支持”，而是某一层和另一层错位：

1. semantic 觉得这个 shape 合法
2. lowering 没把 captured callable tag/payload 接对
3. caller-side rebuild 以为它还是 simpler family

所以 lesson 级最稳的源码排查顺序可以写成：

```text
shape admission
  -> payload slot/tag transport
  -> specialization / rebuild choice
```

这比笼统地“看 closure 代码”更接近现在 live tree 的真实故障模式。

## 8.3 这条线和 returned 主线共享的内部对象是什么

closure captures callable 这条线之所以值得单独讲源码，是因为它已经明显和 returned 主线共用一批内部对象：

1. captured callable tag
2. returned payload slot
3. function object descriptor
4. target-set / specialization choice

所以它现在最不该被理解成：

`closure 线里一个完全独立的小 feature`

更准确的是：

`closure payload kind broadening，正在直接复用 returned-callable/object 那一整套内部表示。`

## 9. 它和 `returned_callable_lesson` 的关系

这两篇很容易混。

最稳的分工是：

- `returned_callable_lesson`
  - 讲 callable 从 producer return 出来后，caller 怎么 rebuild / dispatch
- `closure_capture_callable_lesson`
  - 讲 closure env 里装的已经不是普通标量，而是 callable family，本地 helper 和 returned payload 怎么继续带着它走

也就是说：

前者更像：

`return boundary`

后者更像：

`capture payload kind broadening`

## 10. 当前 still-kept boundary

这条线最容易被误讲成：

- 既然 callable 都能 capture 了
- 那 closure/object model 应该已经 generic 了吧

并没有。

当前仍然要刻意守住这些边界：

1. capture 的 callable family 仍然很保守
2. specialization 仍然承担很大一部分恢复工作
3. helper / wrapper / tag / returned payload 仍然是主角
4. 这不等于 aggregate field / global / arbitrary container 里的 callable capture 都自然工作

所以 lesson 上最稳的口径仍然是：

`这是 closure payload kind 的重大扩张，但还不是 full generic closure runtime。`

## 11. 最稳的分层记法

建议把这条线记成四层：

1. **capture static callable**
   - local noncapturing target
2. **capture function-valued parameter**
   - outer metadata 进入 closure helper specialization
3. **capture callable across return boundary**
   - returned parameter capture
4. **capture callable in dynamic returned family**
   - returned payload + caller-side rebuild + branch-selected helper 一起工作

这样最不容易把：

- “closure 能 capture callable”
- 和
- “所有 callable 都已经成统一 heap object”

讲成一回事。

## 12. 读完后接哪篇

最自然往下接：

1. `returned_callable_lesson.md`
2. `returned_closure_transport_lesson.md`
3. `callable_object_ir_lesson.md`
4. `closure_object_lesson.md`
5. `generic_function_values_lesson.md`

因为这条线后面真正还会继续碰到的就是：

- returned payload rebuild
- callable-object IR
- generic function-object end-state
