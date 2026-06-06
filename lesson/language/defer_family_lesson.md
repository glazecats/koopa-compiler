# Defer Family Lesson（compiler_lab）

> 目标：把当前仓库里和“延迟执行/退出清理”有关的 language features 讲清楚：`defer`、`fndefer`、`capdefer`，以及相邻的小控制流扩展 `unless`。

## 一句话定位

这条线回答的是：

`当前仓库怎么在 -extension 下保守地表达“退出时做点事”，以及它故意没有开到哪里。`

## 1. `defer`：当前 scope-exit 版本

当前 `defer` 的 lesson 级口径可以直接记成：

- `defer stmt;`
- 绑定到**当前最内层 compound scope**
- 按 **LIFO** 执行
- 在作用域退出时执行

包括这些退出路径：

1. 正常 fallthrough
2. `return`
3. `break`
4. `continue`

最小 mental model 不是“运行时栈回调”，而是：

`像把 defer body 复制到每条作用域退出边上一样`

这也是为什么当前第一版实现更偏 structural lowering，而不是先发明一套 runtime defer stack。

## 1.1 伪代码：`defer` 怎么 lower

这条线最稳的实现心智模型是：

```text
enter_scope():
    push empty defer_list

on "defer stmt":
    append stmt to current_scope.defer_list

on scope exit edge:
    replay current_scope.defer_list in reverse order
    then emit real exit action
```

更具体一点可以写成：

```text
lower_scope_exit(exit_kind):
    for stmt in reverse(current_scope.defer_list):
        lower(stmt)
    emit(exit_kind)
```

所以当前第一版 `defer` 的核心不是 runtime callback stack，而是：

`把 defer body 显式复制/回放到每条 scope-exit 边上。`

## 1.2 为什么 `defer` 天然更像 structural clone

一个最小 source：

```c
{
  stmt_a;
  defer stmt_b;
  stmt_c;
  return x;
}
```

lesson 级 lowering intent 可以直接写成：

```text
stmt_a
stmt_c
stmt_b
ret x
```

也就是说，当前最稳的设计不是：

```text
push runtime callback
...
pop callback at runtime
```

而是：

```text
把 defer body 变成 CFG exit edge 上的显式代码
```

这也正好符合当前 canonical IR / verifier 的风格。

## 2. `fndefer`：function-exit 版本

`fndefer` 和普通 `defer` 最大的区别不是语法，而是绑定位置：

- `defer`
  - 作用域退出就跑
- `fndefer`
  - 只有函数退出才跑

所以它更适合：

- whole-function cleanup
- return path 收口

当前第一版边界也很明确：

- payload 只能引用
  - globals
  - parameters
  - function top-level locals
- 不允许随便吃 nested-block / loop-local 变量

也就是说，这条线当前故意避开了真正 closure/capture lifetime 问题。

## 2.1 伪代码：`fndefer` 怎么和普通 `defer` 分家

`fndefer` 最稳的实现模型是：

```text
function_ctx:
    fndefer_sites = []

on "fndefer stmt":
    register site in function-level list

on final return path:
    unwind ordinary scope defers first
    replay function-level fndefer sites in reverse order
    emit final ret
```

也就是说，区别不在“语法多一个字”，而在绑定对象不同：

- `defer`
  - 绑定当前 scope
- `fndefer`
  - 绑定整个 function

## 2.2 为什么现在会有 hidden counter / replay loop

最近 landed 的 slice 已经比最初的“每个 site 只注册一次”更真实了：

- 单个 `fndefer` site 可以在 loop / repeated path 中多次执行

所以更贴近 live tree 的 lesson 伪代码其实像：

```text
on executing one fndefer site:
    site_counter += 1

on function exit:
    while site_counter > 0:
        replay payload once
        site_counter -= 1
```

这也解释了为什么现在这条线不是单纯“把 stmt 挂个列表”那么简单，而是：

`function-exit action registration 已经开始有 per-site runtime multiplicity。`

## 3. `capdefer`：registration-time snapshot 版本

`capdefer` 是这条 family 里最像“捕获”语义的一步，但它还不是 closure。

当前第一版形状大致是：

```c
capdefer (y = x, z = a[i]) putint(y + z);
```

lesson 口径上最重要的三件事：

1. capture expression 在**注册时**求值
2. 值会被存进 hidden snapshot slots
3. payload 在 function exit 时回放，读的是 snapshot，不是 later mutated source

所以它更像：

`captured defer`

而不是：

`通用 closure environment`

## 3.1 伪代码：`capdefer` 的 snapshot 是怎么想的

当前最稳的 lesson 级模型可以直接写成：

```text
on "capdefer (y = expr1, z = expr2) payload":
    snapshot.y = eval(expr1)
    snapshot.z = eval(expr2)
    register payload + snapshot slots for function-exit replay
```

然后真正回放时更像：

```text
replay_capdefer(site):
    bind y to snapshot.y
    bind z to snapshot.z
    lower payload under those bound snapshot names
```

所以它和普通 `defer` / `fndefer` 最核心的不同，不是“也会晚点执行”，而是：

`它读的是 registration-time snapshot，不是 exit-time live variable state。`

## 3.2 为什么它还不是 closure

这条线虽然很像 capture，但当前 kept boundary 仍然非常明确：

```text
capdefer:
    explicit scalar capture bindings
    hidden snapshot slots
    payload at function exit

not:
    general lexical environment
    escaping closure object
    capture-by-reference runtime
```

所以实现 lesson 上最重要的是把它讲成：

`captured defer replay`

而不是：

`闭包系统已经偷偷出现`

## 4. `unless`：小控制流糖

`unless` 这条线其实比前面三者轻得多。

当前口径直接记成：

```c
unless (cond) stmt;
```

等价于：

```c
if (!cond) stmt;
```

当前推荐理解方式是：

- parser-first desugar
- AST/semantic/IR 后面尽量复用已有 `if` 逻辑

也就是说，这不是一条新的下游控制流语义线，而是一条：

`语法糖 + mode gate`

## 4.1 伪代码：为什么 `unless` 最好 parser-first desugar

这条线最适合直接用一句伪代码讲清：

```text
parse_unless(cond, stmt):
    return AST_IF(
        condition = AST_UNARY_NOT(cond),
        then_stmt = stmt
    )
```

然后只保留 extension-origin marker 给 semantic mode gate：

```text
if node came from unless and mode != extension:
    reject
```

也就是说，`unless` 当前最重要的设计不是“增加一类下游控制流 IR”，而是：

`尽可能早地复用现有 if/! 逻辑。`

## 5. 这几条线共同的设计味道

这几个 feature 虽然表面不一样，但当前仓库对它们的态度其实很统一：

1. 先放在 `-extension`
2. 先保守翻译
3. 先把 semantic boundary 写死
4. 先用 focused regression 锁住

所以 lesson 里最该避免的误解是：

- 看到 `capdefer` 就以为 closure 已经全开
- 看到 `fndefer` 就以为有 generic exit-action runtime
- 看到 `unless` 就以为多了一整套新下游控制流 lowering

当前更准确的说法是：

`它们都是围绕“退出/控制”问题的保守 extension slice。`

## 5.1 一张统一实现图

如果想把这几条线一起记，最稳的统一图其实是：

```text
defer:
    scope-exit replay

fndefer:
    function-exit replay

capdefer:
    function-exit replay
    + registration-time snapshot slots

unless:
    parser-first desugar to if (!cond)
```

所以这整条 family 的共同实现味道不是“都有新 runtime 机制”，而是：

`尽量复用现有 CFG / lowering / replay / desugar 骨架，把新语义放在最小新增点上。`

## 6. 当前最该记住的边界

**已开：**

- `defer`
- `fndefer`
- `capdefer`
- `unless`

**还没开成一般能力：**

- closure-backed defer payload
- arbitrary loop-local / nested-local capture
- generic escape/callback model
- 新的下游 control-flow statement family

## 6.1 源码/测试地图：defer family 应该怎么查

defer family 最值得按“exit edge”反查，而不是按语法名硬搜。

源码上最该追三件事：

1. parser / semantic admission
   - `defer` / `fndefer` / `capdefer` / `unless` 是否只在 `-extension` 下开放
2. lowering 的 replay 点
   - scope exit
   - function return
   - `break`
   - `continue`
3. capture/snapshot slot
   - `capdefer` registration-time expression 是否只求值一次
   - replay 时是否读 snapshot slot，而不是重新读 later live variable

lesson 级测试地图：

```text
defer:
    fallthrough / return / break / continue / LIFO

fndefer:
    multiple return paths
    loop registration count
    function-exit replay order

capdefer:
    capture expression evaluated at registration time
    payload replay reads snapshot

unless:
    extension-only syntax
    if (!cond) behavior
```

如果一个 defer witness 只测了普通 fallthrough，不要据此推断 return/break/continue 都闭合。

这类 feature 的真实闭合标准是：

```text
所有相关 exit edge 都 replay 正确
```

## 7. 读完后接哪篇

最自然往下接：

1. `function_values_lesson.md`
2. `aggregate_lesson.md`
3. `extension_mode_lesson.md`
4. `lesson/core/semantic_lesson.md`

因为这两条线正好是：

- capture/closure
- richer type/object flow

这两个 language round 的下一层难点。
