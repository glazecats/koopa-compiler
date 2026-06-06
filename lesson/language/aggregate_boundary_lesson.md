# Aggregate Boundary Lesson（compiler_lab）

> 目标：专门讲 aggregate line 的下一阶段边界，也就是 aggregate parameter/return 和 shared type system 为什么值得单独成课，以及当前仓库在这条线上到底是“已经能过函数边界”还是“还没变成完整 ABI”。

## 一句话定位

这条线回答的是：

`aggregate 现在已经不再只是在 local scope 里存两三个字段，而是在保守地跨函数边界流动，同时逼出一个更统一的类型层。`

## 1. 为什么 `aggregate_lesson` 之后还需要这篇

`aggregate_lesson.md` 已经讲了：

- `pair`
- `struct`
- hidden-slot lowering

但 `docs/language` 里现在还有两份更细的 authority：

1. `AGGREGATE_PARAM_RETURN_PLAN.md`
2. `TYPE_SYSTEM_PLAN.md`

这说明 aggregate line 已经从“一个语言点”变成了两条更深的问题：

1. **函数边界怎么过**
2. **类型系统怎么统一**

## 2. aggregate parameter / return：当前到底开到了哪

当前最重要的 lesson 口径是：

**已经支持/正在开的 slice：**

1. `pair` parameter
2. known `struct Name` parameter
3. returning `pair`
4. returning known `struct Name`
5. aggregate call-result local copy-init / assignment
6. 一些 aggregate return/forwarding call chain

这说明 aggregate 现在已经不仅仅是：

`local object lowering`

而开始回答：

`call boundary flow`

## 3. 为什么这仍然不是完整 aggregate ABI

这个边界很重要。

当前 docs 一直在强调：

- 复用 already-landed hidden-scalar-slot model
- 不要太早发明 full runtime object ABI

所以 lesson 最该记住的一句话是：

`aggregate parameter/return 已经 landed 一部分，不等于仓库现在有了完整的 C aggregate ABI。`

当前更像：

- flatten / recursive slot model
- hidden result slot
- hidden scalar parameter sequence

而不是：

- user-visible aggregate object ABI
- generic pointer/reference semantics
- arbitrary aggregate value positions everywhere

## 3.1 伪代码：aggregate parameter / return 怎么过边界

这条线最值得补的实现视角，是“函数边界其实还在走 flatten slots”。

一个 lesson 级模型可以写成：

```text
pair id(pair p) { return p; }
```

当前更像被 lower 成：

```text
callee sees:
    p.first_slot
    p.second_slot

return path:
    ret_slot.first  = p.first_slot
    ret_slot.second = p.second_slot
```

也就是：

```text
aggregate parameter
    -> hidden scalar parameter sequence

aggregate return
    -> hidden result destination slots
```

更一般的递归伪代码可以写成：

```text
lower_param(T, source):
    for leaf in flatten_type(T):
        emit_param_leaf(source, leaf)

lower_return(T, value, dst_slots):
    for leaf in flatten_type(T):
        emit_store_return_leaf(dst_slots[leaf], value[leaf])
```

这能帮助读者把“aggregate 已经能过函数边界”和“已经有完整 ABI”区分开：

前者现在是：

`flatten + hidden slots`

后者还不是。

## 3.2 伪代码：aggregate call chain 为什么还能继续往下传

这条线再往前一个非常关键的实现判断是：

```text
mk() -> hidden return slots
id(mk()) -> caller materializes return slots, then re-expands into callee param slots
wrap(id(mk())) -> same flatten/re-expand contract continues
```

也就是说，当前 aggregate call chain 并不是靠“aggregate object 在寄存器里自然传来传去”。

更贴近 live tree 的伪代码其实像：

```text
tmp_ret_slots = alloc_hidden_slots(flatten_type(T))
call mk(tmp_ret_slots)

tmp_param_slots = project_or_copy(tmp_ret_slots)
call id(tmp_param_slots)
```

所以 aggregate return/forwarding line 现在能闭合，不是因为已经有 generic ABI，而是因为：

`同一套 flatten-slot contract 同时被 producer、caller、next callee 复用。`

## 3.3 伪代码：field read 为什么还能在 param/return family 上工作

当前 aggregate boundary 里另一个容易低估的点是：

- aggregate parameter
- aggregate return local copy
- nested aggregate chain

这些都要求 field read 继续工作。

lesson 级心智模型可以写成：

```text
read_field(value, field_path):
    slots = flatten_view(value)
    offset = compute_flattened_field_offset(type_of(value), field_path)
    return slots[offset]
```

这就是为什么现在 aggregate param/return 线不只是“能传过去”，还会继续逼：

- flatten view
- field offset
- aggregate descriptor

走向共享实现。

## 4. current kept reject matrix：为什么它值得单独说

当前 aggregate param/return 线最容易被讲松的地方，就是：

- 一旦看到 `pair` / `struct` 能 param/return
- 就误以为 aggregate 已经能像 scalar 一样自由流动

但当前 still intentionally rejected 的东西很多：

1. direct aggregate call-result member access
   - `mk().second`
   - `(mk()).second`
   - `mk().p.second`
2. plain aggregate value in scalar contexts
   - `int x = mk();`
   - `if (wrap())`
3. aggregate globals / arrays / generic aggregate object flow

也就是说，现在 lesson 里要把它讲成：

`aggregate boundary is expanding, but still very deliberately fenced`

## 4.1 为什么 direct call-result member access 还会故意卡住

这条 kept boundary 很适合用实现视角讲清：

```c
mk().second
```

如果真要完全放开，系统需要更像：

```text
tmp_ret_slots = call mk(...)
read field from unnamed temporary aggregate view
```

而当前 conservative line 更偏向：

```text
call mk into explicit local/copy-init destination first
then read fields from that named local aggregate
```

也就是说，这不是“语法还没来得及做”这么简单。

它本质上是在守一个边界：

`aggregate return value 何时能被当作普通 expression object 直接继续消费`

当前答案仍然是：很多地方还不行。

## 5. shared type system：为什么这是 aggregate 线真正的下一层

如果只看 surface feature，`pair` 和 `struct` 像两条平行线。

但 `TYPE_SYSTEM_PLAN.md` 很明确地说明：

- 仓库现在真正想长出来的是 shared type descriptors
- shared compatibility checks
- shared field/member lookup path

这意味着 aggregate line 当前真正的 engineering value 不是：

`又多支持了一种值`

而是：

`逼 semantic/type layer 从 ad hoc feature checks 走向统一 compatibility layer`

## 5.1 源码 / 测试地图：这条线现在该从哪里回查

如果要防止把这条课讲虚，最稳的回查路线是：

1. semantic / lowering 主线
   - `src/semantic/semantic.c`
   - `src/semantic/semantic_entry.inc`
   - `src/ir/ir_gen.c`
   - `src/lower_ir/lower_ir.c`
2. aggregate function-boundary witness
   - `tests/lower_ir/lower_ir_regression_test.c`
   - 重点 filter：
     - `LOWER-IR-AGGRET-PAIR-LOCAL`
     - `LOWER-IR-AGGRET-PAIR-CALL-INIT`
     - `LOWER-IR-AGGCALL-PAIR-CALLARG`
     - `LOWER-IR-AGGRET-PAIR-MULTIHOP`
     - `LOWER-IR-AGGRET-NESTED-STRUCT-MULTIHOP`

这些名字很适合教学，因为它们刚好覆盖了这条 lesson 的核心：

```text
local aggregate value
  -> aggregate return
  -> call-result copy-init
  -> aggregate call argument
  -> multi-hop forwarding
  -> nested struct forwarding
```

也就是说，当前证据链不是“我们觉得 aggregate 能过函数边界”，而是 lower-IR witness 已经锁住了几类典型 flatten/re-expand 形状。

## 5.2 当前实现口径：过边界的是 leaf slots，不是完整对象 ABI

这条 lesson 最容易讲飘的地方，是把下面两个概念混起来：

```text
aggregate call boundary is supported for selected families
```

和：

```text
aggregate now has a full first-class ABI object model
```

当前应该坚持第一种说法。更贴近实现的描述是：

```text
caller:
    materialize aggregate leaves into hidden slots
    pass/re-expand those leaves into callee-facing hidden parameters

callee:
    read aggregate leaves through its own flattened view
    write aggregate return leaves into hidden result slots
```

所以这条线的 checkpoint 重点是：

- flatten view 能不能稳定描述 `pair` / `struct`
- caller 和 callee 对 leaf 顺序有没有一致理解
- call-result copy-init / forwarding 有没有多求值或漏字段
- nested aggregate field path 有没有继续映射到正确 leaf

还不是：

- aggregate 值能不能在任意 expression slot 里自由存在
- aggregate temporary object 有没有完整 lifetime
- aggregate ABI 是否已经可以独立暴露给后端

## 6. 最稳的 lesson 分层

建议把 aggregate 这条线记成三层：

1. **surface features**
   - `pair`
   - `struct`
2. **function-boundary flow**
   - aggregate param / return
3. **shared semantic machinery**
   - type system unification

这三层一旦分开，你就不容易把：

- “现在支持某些 aggregate call chain”
- 误讲成
- “aggregate world 已经整体收口”

## 7. 读完后接哪篇

最自然往下接：

1. `aggregate_lesson.md`
2. `float_lesson.md`
3. `lesson/core/semantic_lesson.md`

因为 aggregate boundary 的下一步要么是：

- richer scalar/type system
- 要么就是把 shared compatibility layer 真正落进语义实现
