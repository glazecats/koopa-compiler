# Generic Function Values Lesson（compiler_lab）

> 目标：专门讲 `GENERIC_FIRST_CLASS_FUNCTION_VALUES_PLAN.md` 这条 authority 到底在回答什么，也就是当前仓库为什么需要一个统一 end-state 来收敛 noncapturing / closure / returned / callable-object 这些看起来已经很多、但还没真正统一的 slice。

## 一句话定位

这条线回答的是：

`如果不再满足于“再补一个 witness-specific family”，那么函数值最终应该长成什么统一模型。`

## 1. 为什么这篇是“路线图课”而不是“已完成课”

当前 live tree 已经有很多 function-value 相关能力：

- non-capturing function-valued parameter
- local function-typed binding
- returned non-capturing function value
- closure literal 的第一版
- returned closure 的保守 slice
- callable-object IR 的第一批显式操作

但这些能力还不能直接等价成：

`generic first-class function values 已经全部落地`

所以这篇最重要的 lesson 口径是：

`它讲的是 end-state architecture，不是“仓库现在已经全做完了”的总结。`

## 2. 为什么仓库必须有一个 end-state

如果没有统一 end-state，function-value 这条线就会很容易继续变成：

1. 一个 source shape 一套 lowering glue
2. 一个 returned family 一套 tag/payload naming
3. 一个 closure family 再来一套 helper 约定

短期能跑，长期会越来越难维护。

所以这篇真正回答的是：

`现在这些 conservative slice 最后要收敛成什么共同对象模型。`

## 3. 最核心的三元组

当前最值得背下来的不是几十个 helper 名，而是这个 object view：

```text
fnobj = {
  code,
  env,
  shape
}
```

也就是：

1. `code`
   - 最终调到哪段 callable code
2. `env`
   - capture 环境；对 non-capturing value 可以是空
3. `shape`
   - 这个 callable 的静态函数形状是什么

这三元组之所以重要，是因为它第一次让：

- top-level function
- non-capturing returned function
- closure
- returned closure

有机会沿着一条共同语义来 transport。

## 3.1 伪代码：generic object 真正想统一什么

这篇最值得补上的实现视角，是 generic end-state 不只是“字段更多”。

它真正想统一的是这条流程：

```text
source callable
    -> materialize fnobj { code, env, shape }
    -> store / assign / return / pass one shared object
    -> indirect-call through one shared ABI
```

lesson 级伪代码可以直接写成：

```text
make_fnobj(source):
    return {
        code  = lower_code_entry(source),
        env   = lower_environment(source),
        shape = intern_function_shape(source.type),
    }

call_fnobj(fnobj, args):
    return indirect_call(fnobj.code, fnobj.env, args, fnobj.shape)
```

这里最重要的是：

- top-level function
- returned noncapturing function
- closure
- returned closure

最后都不该再各有一套 transport family。

## 4. 最小 end-state mental model

目标中的 source 大概像这样：

```c
int apply(int f(int), int x) {
  return f(x);
}

int make_adder(int x)(int) {
  return closure [x] int (int y) { return x + y; };
}

int main() {
  int f(int) = make_adder(3);
  int g(int) = add1;
  int h(int) = cond ? f : g;
  return apply(h, 4);
}
```

当前这篇真正想让你记住的，不是某一行语法，而是：

- `h` 应该是一种统一的 function value
- 它可能指向 plain function，也可能指向 closure helper
- `apply(h, 4)` 最终应该走一条共享 indirect-call 路

## 5. 为什么 `env` 不能一直靠特殊 payload family 顶着

当前 returned closure / dynamic closure line 已经说明一件事：

`capture payload 如果永远只是“这一家 helper 特有的隐藏槽协议”，后面会越来越难统一。`

所以 generic lesson 的关键转向是：

- function object 指向 environment
- environment 本身也应该是一个显式对象

这和 earlier conservative slice 的差别是：

- 以前更像“某个 family 的 transport struct”
- 这里开始变成“callable object + environment object”

## 6. 为什么 `shape` 非常重要

最容易被忽略的是 `shape`。

很多人会直觉上以为：

- 有 `code`
- 有 `env`
- 应该就够了

但对这个仓库来说，`shape` 非常关键，因为它把下面这些事统一到一起：

1. assignment compatibility
2. return compatibility
3. argument passing compatibility
4. indirect call validation
5. 后续 aggregate field transport 的类型约束

所以这篇也可以看成在替 `type_system_lesson.md` 打前站：

`function value 也需要 shared type descriptor，而不只是“有个 helper 能跑”。`

## 6.1 伪代码：shape 怎么进入普通语义动作

一个很实用的 lesson 级伪代码可以写成：

```text
compatible_fn_value(lhs_shape, rhs_shape):
    return lhs_shape.return_kind  == rhs_shape.return_kind
       and lhs_shape.param_count  == rhs_shape.param_count
       and lhs_shape.param_kinds  == rhs_shape.param_kinds

assign_fn_value(lhs, rhs):
    require compatible_fn_value(lhs.shape, rhs.shape)
    lhs = rhs
```

也就是说，generic 线最终想实现的不是：

`看上去都像 callable，就先塞过去`

而是：

`callable 也像 aggregate/float 一样，走共享 shape compatibility story`

## 7. 它和 callable-object IR 的关系

这篇是 end-state，而 `callable_object_ir_lesson.md` 更偏当前 IR 合约本身。

两者关系最稳的记法是：

1. `callable_object_ir_lesson`
   - 讲 live tree 已经落地的 `shape + fn_make + call_indirect` / verifier contract
2. `closure_object_lesson`
   - 讲 closure / returned closure 为什么会把 object boundary 往 IR 推
3. `generic_function_values_lesson`
   - 讲这条 boundary 最终想收敛成什么统一 runtime model

所以如果你看到：

- `fn_make`
- `fn_code`
- `fn_env`
- `fn_shape`
- `call_indirect`

不要只把它当成“又一组 IR 指令”。

更准确地说，它们是在替这个 end-state 铺路。

## 7.1 live-tree 证据地图：哪些东西已经是路标，哪些还只是终点图

如果回当前仓库查证，这条 generic 课要分两种证据看。

第一种是已经落地的 IR/object 路标：

- `src/ir/ir.c`
- `src/ir/ir_gen.c`
- `src/lower_ir/lower_ir.c`
- `tests/ir/ir_verifier_test.c`
- `tests/ir/ir_regression_test.c`
- `tests/lower_ir/lower_ir_regression_test.c`

重点关键词：

```text
fn_make
fn_code
fn_env
fn_shape
call_indirect
shape.0
__fnwrap_*
```

这些说明的是：

`统一 callable object 的 IR 形状已经开始真实存在。`

第二种是仍然偏 conservative 的 source-family 证据：

- `tests/compiler/compiler_driver_test.c`
- `tests/semantic/semantic_regression_test.c`

重点搜索短语：

```text
function-valued parameter
function-valued return
returned closure forwarding into function-valued parameter
closure literal forwarding into function-valued parameter
dynamic runtime closure local forwarding
```

这些说明的是：

`很多 source family 已经能过，但它们还经常靠 specialization / payload / wrapper / callable-object bridge 混合完成。`

所以这篇课读源码时要很小心：

```text
看到 fn_make/call_indirect
    => 说明 object boundary 正在落地

看到很多 function-valued witness 通过
    => 不等于 generic first-class function value 已经完整落地
```

## 8. 当前 still-not-landed 的部分

这篇最重要的一节其实是承认还没到的地方。

当前仍然不能把 live tree 讲成：

1. 所有 function value 都可自由进 ordinary value position
2. closure env 已经有统一 lifetime / heap strategy
3. aggregate field 里的 callable transport 已经稳定
4. 所有 higher-order call 都已统一成一个 generic indirect ABI

所以这篇的价值不是“宣布完成”，而是：

`避免我们把一堆 conservative landed slice 错讲成已经统一。`

## 9. 最稳的三层分法

建议把这条线分成三层记：

1. **今天已经 landed 的保守 slice**
   - specialization
   - tag family
   - returned narrow slices
2. **今天正在形成的 IR/object 边界**
   - callable-object IR
3. **明天真正想收敛到的统一模型**
   - `code + env + shape`
   - generic indirect call

这样就不容易把：

- “当前能处理很多具体 witness”
- 和
- “统一 architecture 已经落地”

混成一句话。

## 10. 读完后接哪篇

最自然往下接：

1. `callable_object_ir_lesson.md`
2. `closure_object_lesson.md`
3. `returned_closure_transport_lesson.md`
4. `higher_order_callable_lesson.md`
5. `type_system_lesson.md`
6. `lesson/core/ir_lesson.md`

因为 end-state 真正落地时，最后一定会同时碰到：

- explicit IR object
- higher-order callable transport
- shared function shape/type descriptor
- downstream indirect-call lowering
