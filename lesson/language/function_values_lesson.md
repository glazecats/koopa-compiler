# Function Values Lesson（compiler_lab）

> 目标：把当前仓库的 function-value / closure 主线讲成一张真正的总图，而不是只列几个 feature 名。重点回答：这条线到底分成哪几层、为什么最近会爆出这么多子课、以及这些能力在实现上是怎么一段段接起来的。

## 一句话定位

这条线回答的是：

`函数什么时候还只是“名字”，什么时候开始像“值”，什么时候又逼着 IR/object model 真正往下长。`

## 1. 为什么这篇现在必须是一张“总图”

如果只看最近 `NEXT_STEPS` 和近几十次提交，这条线已经不再是一个单一 feature：

1. non-capturing function-valued parameter
2. direct-callee specialization
3. local alias / reassignment / branch-rebinding
4. returned noncapturing function value
5. closure literal
6. returned closure
7. callable-object IR
8. closure captures callable
9. mixed returned producers / mixed higher-order consumers

所以现在最需要的不是“再举一个例子”，而是先有一张地图：

`哪些是静态 specialization 线，哪些是 returned payload 线，哪些已经在逼 generic callable-object architecture。`

## 2. 当前最稳的四层总图

建议先把整条线记成四层：

1. **non-capturing static line**
   - function-valued parameter
   - local alias
   - direct-callee specialization
2. **dynamic noncapturing line**
   - branch-rebinding
   - runtime tag family
   - returned noncapturing function value
3. **closure line**
   - local closure
   - returned closure
   - closure captures callable
4. **object-model / end-state line**
   - callable-object IR
   - `fn_make + call_indirect`
   - generic first-class function values as forward authority

如果先把这四层分清，就不会把：

- “已经能传函数值”
- “已经能返 closure”
- “已经有 generic function object runtime”

误讲成一回事。

## 3. 第一层：non-capturing static line

最早、也最保守的一步，是：

```c
int add1(int x) { return x + 1; }

int apply(int f(int), int x) {
  return f(x);
}

int main() {
  return apply(add1, 41);
}
```

当前最稳的实现口径不是“编译器支持了函数指针”，而是：

```text
apply(add1, 41)
  -> apply__fv_0_add1(41)
```

也就是说：

- actual argument 是已知 top-level/builtin target
- callee body 按这个已知 binding 重新 lower
- IR 尽量仍然保持 direct call

这条线一路往外扩，才有了：

1. forwarding function-valued parameter
2. multiple function-valued parameters
3. zero-arg / void sibling
4. local alias

## 3.1 伪代码：static specialization 怎么做

最小实现心智模型可以直接写成：

```text
lower_call(call):
    if actual function value is statically known:
        helper = get_or_build_specialized_helper(callee, binding)
        return emit_direct_call(helper, visible_non_function_args)
    else:
        hand off to dynamic path
```

其中 helper 构造更像：

```text
build_specialized_helper(callee, binding):
    relower callee body under context:
        function_slot_i -> concrete_target
```

这就是为什么现在这条线虽然看起来越来越宽，本质上仍然是：

`specialization-backed higher-order call`

不是：

`generic indirect-call ABI`

## 4. 第二层：dynamic noncapturing line

一旦 source 进入这种 family：

```c
int f(int)=add1;
if(c) f=add2;
return f(40);
```

问题就变了。

现在已经不能只靠静态 specialization，因为：

- callee identity 到 runtime 才确定

当前 kept 路径更像：

```text
local function value
  = tag / target-family metadata

call site
  = branch on tag
  -> call specialized leaf helper
```

所以这条线的关键词是：

- runtime tag
- branch-selected helper
- dynamic family, but still conservative

## 4.1 returned noncapturing 是这条线真正的转折点

再往前一步就是：

```c
int pick()(int) { return add1; }
int f(int)=pick();
return f(41);
```

现在问题不只是“local 里动态选哪个 target”，而是：

- callable 要先跨 function boundary
- caller 再把它重建回来

所以从这里开始，lesson 上最重要的心智模型就变成：

```text
producer returns payload
caller rebuilds callable view
consumer finally calls
```

这也是为什么 `returned_callable_lesson.md` 需要单独存在。

## 5. 第三层：closure line

closure 线又可以再拆成三步：

1. **local closure**
   - capture scalar
   - local bind-and-call
2. **returned closure**
   - capture payload 跨函数边界
   - caller-side rebuild
3. **closure captures callable**
   - capture payload 里不再只是标量
   - 还会装 function-value family/tag

一个最小 closure slice 是：

```c
int x = 3;
int f(int)=closure [x] int (int y) { return x + y; };
return f(4);
```

而更近的扩张已经是：

```c
int f(int)=add1;
int g(int)=closure [f] int (int y) { return f(y); };
return g(4);
```

这说明 closure helper 自己已经开始成为 higher-order consumer。

## 5.1 伪代码：closure helper 现在更像什么

一个最小实现心智模型是：

```text
env = {
  scalar captures...
  callable tags...
}

closure helper body:
  recover captured callable from env
  rebuild callable view if needed
  lower call through wrapper / call_indirect spine
```

所以当前 closure line 已经不只是“把几个 int 拷进去”。

它开始真的接上：

- callable transport
- returned payload
- caller-side/object-side rebuild

## 6. 第四层：callable-object IR

随着 returned family、mixed producer、closure-captures-callable 越来越多，仓库开始明确承认：

`只靠 lowering-side ad hoc naming / helper glue 已经不够了`

所以现在 lesson 里必须把这组三元组当成第一等事实：

```text
callable_object = { code, env, shape }
```

以及这组显式动作：

- `fn_make`
- `fn_code`
- `fn_env`
- `fn_shape`
- `call_indirect`

最稳的伪代码可以写成：

```text
obj = fn_make(code, env, shape)
result = call_indirect(obj, visible_args...)
```

这一步的关键意义不是“新 IR opcode 比较高级”，而是：

`callable 的构造和消费第一次变成显式 IR contract。`

更具体的 IR 合约细节不要在这篇总图课里硬背。

如果你想看：

```text
shape shape.0(int) -> int
tmp.0 = fn_make __fnwrap_add1, 0, shape.0
tmp.1 = call_indirect tmp.0(41)
```

到底怎么被 verifier 约束，应该接着读：

```text
callable_object_ir_lesson.md
```

如果你想看：

```text
make(3)
  -> __retclosure_immediate / __retclosure_argslot / __retclosure_argcap
  -> fn_make + call_indirect
```

中间 payload view 怎么保持同一次 producer evaluation，应该接着读：

```text
returned_closure_transport_lesson.md
```

## 7. 为什么最近会有那么多 returned / mixed / higher-order 提交

因为现在真正难的已经不是“支持一个 function-valued parameter”。

真正难的是：

1. returned producer family 怎么找
2. ternary / passthrough / wrapper 里怎么找到真正 producer
3. payload 怎么完整 materialize
4. caller-side 怎么 rebuild
5. mixed producer / multiple function-valued argument 怎么组合 dispatch

这条线最近可以压成一句：

`现在在攻的不是“函数像值”的存在性，而是“callable payload 在复杂 expression tree 里还能不能被诚实恢复”。`

## 7.1 伪代码：mixed family 的统一心智模型

一个足够实用的总图伪代码可以写成：

```text
lower_callable_value(expr):
    if expr is static target:
        return static_binding

    if expr is returned producer:
        return materialized_payload

    if expr is passthrough(expr1):
        return lower_callable_value(expr1)

    if expr is ternary(cond, lhs, rhs):
        return branch_select(lower_callable_value(lhs),
                             lower_callable_value(rhs))

    if expr is closure-backed value:
        return callable_object_view
```

然后最终 consumer 再做：

```text
consume_callable(v, args):
    obj = rebuild_or_materialize_callable_object(v)
    return call_indirect_or_selected_dispatch(obj, args)
```

这也是为什么现在 function-values 主线已经必须拆成多篇 lesson：

一篇很难同时把：

- specialization
- returned payload
- closure env
- callable-object IR
- generic end-state

都讲清。

## 8. 源码地图：如果你要真去看实现，先看哪里

当前这条总图线如果要落回源码，最值得先记的不是所有细节，而是三段入口：

1. **语义前门**
   - `src/semantic/semantic_entry.inc`
   - 重点看：
     - `semantic_extension_check_function_value_expression(...)`
     - `semantic_extension_function_value_initializer_expr_matches_expected(...)`
     - `semantic_extension_resolve_local_function_value_binding_with_expected_type(...)`
     - `semantic_current_function_find_direct_local_function_value_declaration_type_descriptor(...)`
2. **canonical IR lowering**
   - `src/ir/ir_lower_stmt.inc`
   - 重点看：
     - `ir_try_build_returned_function_value_descriptor(...)`
     - 各种 `ir_*function_value*descriptor*`
     - returned payload / binding transport / target-set merge 相关 helper
3. **callable-object verifier contract**
   - `src/ir/ir_verify.inc`
   - 重点看：
     - `IR_INSTR_FN_MAKE`
     - `IR_INSTR_FN_CODE/FN_ENV/FN_SHAPE`
     - `IR_INSTR_CALL_INDIRECT`

如果把它压成一句更实用的话：

```text
semantic_entry.inc
  -> front-door and shape gate

ir_lower_stmt.inc
  -> binding / payload / rebuild / dispatch lowering

ir_verify.inc
  -> explicit callable-object contract
```

## 8.1 这三个入口分别在守什么

最稳的实现理解可以再压成：

```text
semantic:
    这个 source shape 语义上能不能算 function value / closure / returned callable

lowering:
    一旦算成立，具体把它变成 helper、payload、binding、fn_make 还是 call_indirect

verifier:
    最后这些显式 object/call 形状到底合不合法
```

也就是说，function 主线现在已经不只是“一个 lowering 文件里的特殊分支”，而是：

`前门类型/绑定检查 + 中间 payload/rebuild + 后段 object contract`

三段一起在长。

## 8.2 这条线在源码里已经不是“几处零散特判”

再补一句非常重要的工程判断：

当前 function 主线已经至少有三类“成体系”的内部对象：

1. semantic side 的
   - type descriptor
   - function shape
2. lowering side 的
   - function object descriptor
   - returned family info
   - target-set
3. verifier side 的
   - `fn_make / fn_shape / call_indirect` contract

所以如果以后继续补 lesson，最稳的统一口径应该一直是：

`我们现在不是在加零散 witness，而是在让 function world 的中间表示越来越显式。`

## 9. 当前 still-kept boundary

这条总图课最重要的一部分，是提醒读者不要过度总结。

当前仍然不能把 live tree 讲成：

1. generic function pointer ABI 已成
2. all closures are ordinary values everywhere
3. aggregate/global/container callable transport 已全面打通
4. helper/tag/specialization 已退出历史舞台

更准确的总口径仍然是：

`我们已经有一条非常宽的 conservative function-value mainline，但它还在从 source-shape-specific transport 逐步收敛到 explicit callable-object architecture。`

## 10. 推荐阅读路线

读这条总图课之后，最自然往下分叉是：

1. 想看最早的 direct-callee 机制
   - `function_value_callee_lowering_lesson.md`
2. 想看 return boundary / rebuild
   - `returned_callable_lesson.md`
3. 想看 returned closure payload 怎么一路 transport
   - `returned_closure_transport_lesson.md`
4. 想看 `shape + fn_make + call_indirect` 的 IR 合约
   - `callable_object_ir_lesson.md`
5. 想看 closure payload 里开始装 callable
   - `closure_capture_callable_lesson.md`
6. 想看 end-state
   - `generic_function_values_lesson.md`

## 11. 最后一句统一判断标准

如果你只想记一件事，建议记这个：

`这条线当前最重要的不是“功能名越来越多”，而是 callable 的 binding / payload / rebuild / shape 正在从若干保守 slice 收敛成一套显式 object model。`
