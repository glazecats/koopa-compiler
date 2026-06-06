# Aggregate Lesson（compiler_lab）

> 目标：解释当前 `pair` / `struct` / aggregate parameter-return 这条线到底开到了哪，为什么仓库一直强调 conservative hidden-slot lowering，以及 shared type layer 在这里起什么作用。

## 一句话定位

这条线回答的是：

`仓库现在怎么在不引入完整对象 ABI 的前提下，保守地支持复合值。`

## 1. `pair`：最小复合值切片

`pair` 是当前 aggregate line 最小、也最稳定的一步。

lesson 口径上可以直接记：

- 只在 `-extension` 下开放
- 先支持 local `pair`
- 支持 `.first` / `.second`
- 支持 local copy-init / assignment

当前最重要的实现味道不是语法，而是 lowering 策略：

`每个 pair local -> 两个 hidden scalar locals`

也就是说，仓库当前不想太早引入新的 backend aggregate representation。

## 2. `struct`：从 builtin aggregate 走到 named aggregate

`struct` 比 `pair` 多的不是“字段变多”，而是：

1. field names 变成用户定义
2. type name 变成 top-level declaration

当前 landed slice 仍然很保守：

- local struct object
- field read/write
- local copy-init / assignment

但还没全开：

- globals
- struct params
- struct returns
- arrays of structs

所以当前 `struct` line 最好被理解成：

`比 pair 更一般，但还没有跳成 full C struct ABI`

## 3. aggregate parameter / return：函数边界正在打开

这条线是 aggregate family 里当前最值得单独记的下一步。

docs 已经把当前边界写得很明确：

**已支持/在开：**

- `pair` parameter
- known `struct Name` parameter
- returning `pair`
- returning known `struct Name`
- aggregate-returning call chain 的保守传播

**还故意没开：**

- arbitrary aggregate values in plain scalar positions
- aggregate globals / arrays
- generic object ABI

也就是说，这条线已经不再只是在 local scope 里玩复合值，而是开始回答：

`跨函数边界的 aggregate flow 怎么做`

## 4. 为什么一直强调 hidden-slot lowering

这条 aggregate line 最重要的统一设计，不是 `pair` 还是 `struct`，而是：

`不要为了语言层看起来干净，就过早引入一套完整 runtime/backend aggregate object model。`

当前 lesson 最该记住的 lowering 口径是：

- pair / struct local object
  - lower 成 hidden scalar slots
- aggregate parameter / return
  - 也优先沿着当前 recursive slot model 走

这意味着：

- semantic/type layer 可以先变丰富
- backend object/ABI story 可以后面再诚实打开

## 4.1 一个最小实现心智模型

如果把 aggregate 线先压成最小实现视角，当前更像：

```text
pair p = {1, 2};
```

lower 成：

```text
p.first_slot  = 1
p.second_slot = 2
```

而不是：

```text
p = one runtime aggregate object
```

同样地：

```c
struct Point p = {1, 2};
```

当前 lesson 最稳的记法也是：

```text
p.x_slot = 1
p.y_slot = 2
```

也就是说，当前 aggregate lowering 的第一原则是：

`先把复合值展平成已有标量槽模型，而不是立刻发明新对象运行时。`

## 4.2 伪代码：local aggregate 怎么 flatten

一个 lesson 级 flatten 伪代码可以写成：

```text
flatten_type(T):
    if T is int:
        return [scalar]
    if T is pair:
        return [scalar, scalar]
    if T is struct { f1:T1, f2:T2, ... }:
        return flatten_type(T1) + flatten_type(T2) + ...

lower_local_decl(name, T, init):
    slots = allocate_hidden_slots(flatten_type(T))
    if init exists:
        lower_aggregate_init(slots, init)
```

而 member access 更像：

```text
lower_member(obj, field_path):
    offset = compute_flattened_field_offset(obj.type, field_path)
    return obj.hidden_slots[offset]
```

这也解释了为什么：

- nested struct
- pair-in-struct
- aggregate copy-init / assignment

都在逼一套 shared flatten/offset path，而不只是若干特判。

## 5. shared type layer：为什么它重要

当前 aggregate line 如果只看 feature 名，会像三条不同的小功能：

1. pair
2. struct
3. aggregate param/return

但 docs 里其实已经把它们往同一层收：

- `TYPE_SYSTEM_PLAN.md`

这说明当前仓库的真正目标不是：

`永远给 pair/struct 各写一套 ad hoc 规则`

而是：

`把 aggregate compatibility、field lookup、assignment/init/return/param 规则拉成共享 type layer`

所以 lesson 里最重要的一句话是：

`aggregate line 的核心已经不只是“又加一种语法”，而是在逼仓库长出更统一的类型系统。`

## 5.1 源码地图：aggregate 现在主要看哪几层

如果你要从 lesson 跳回源码，aggregate 线最好按三层找：

1. semantic / type descriptor
   - aggregate type 是否已知
   - field lookup 是否合法
   - assignment / init / return / param 是否 compatible
2. canonical IR lowering
   - aggregate object 被展开成哪些 hidden scalar slots
   - copy-init / assignment 是否逐 slot 搬运
   - member access 是否算对 flatten offset
3. lower-IR / downstream
   - hidden slots 是否继续以普通 scalar/local slot 形态存在
   - downstream 不需要假装已经有完整 aggregate ABI

lesson 级反查关键词：

```text
pair
struct
aggregate
field
flatten
slot
return
param
```

这条线目前最该看的不是“有没有一个 aggregate object instruction”，而是：

```text
type system 是否知道它是 aggregate
lowering 是否诚实把它展开成现有 slot model
```

## 5.2 测试地图：aggregate witness 应该锁什么

aggregate 的 focused regression 最好分层理解：

```text
semantic:
    field / init / assignment / parameter / return compatibility

IR / lower-IR:
    hidden slot flatten shape
    member access offset
    aggregate copy transport

compiler-driver:
    source witness 能不能真的编译通过
```

如果某个 witness 是 aggregate return/call-chain，尤其要看：

```text
return slots
call result transport
member read after returned aggregate call
```

因为这里最容易把“局部 aggregate 能用”误讲成“函数边界 aggregate 已经完全 ABI 化”。

## 6. 当前最容易误解的地方

### 6.1 “pair/struct 都有了，是不是 aggregate 已经普遍可值传递了？”

不是。

当前更准确的是：

- 某些 local + function-boundary slice 已经 landed
- 但 generic aggregate object flow 还远没全开

### 6.2 “既然 param/return 开了，是不是 backend 已经有完整 aggregate ABI？”

也不是。

当前 lesson 要刻意记住：

- 这条线依然强调 hidden-slot / recursive slot model
- 不是完整 ABI 承诺

## 7. 读完后接哪篇

最自然往下接：

1. `float_lesson.md`
2. `function_values_lesson.md`
3. `aggregate_boundary_lesson.md`
4. `type_system_lesson.md`
5. `lesson/core/semantic_lesson.md`

因为 aggregate line 的下一步难点，正好是：

- richer scalar types
- richer callable/object types
- shared compatibility machinery
