# Type System Lesson（compiler_lab）

> 目标：专门讲当前 `TYPE_SYSTEM_PLAN.md` 这条 authority 在 language round 里的位置，也就是仓库为什么已经不能继续只靠“pair 一套规则、struct 一套规则、float 再补一套规则”往前走。

## 一句话定位

这条线回答的是：

`当前 language-feature round 真正开始长“共享地基”的地方在哪里。`

## 1. 为什么这篇值得单独成课

如果只看 surface feature，最近这一轮像是在并行加很多东西：

1. `pair`
2. `struct`
3. aggregate parameter / return
4. `float`
5. function-value signature compatibility

但 `TYPE_SYSTEM_PLAN.md` 其实在讲另一件更底层的事：

`这些 feature 继续扩下去之前，语义层需要一个共享 type/compatibility story。`

所以这篇不是“又一个语法点”。

它更像：

`为什么 language round 现在开始从 feature patching 走向 shared semantic machinery`

## 2. 当前最重要的统一问题

现在仓库反复在回答的，其实不是“能不能多支持一个 witness”，而是：

1. declaration type 怎么描述
2. assignment/init/return/param 怎么判兼容
3. member lookup 怎么统一
4. function shape 怎么判等/判匹配
5. scalar kind 怎么在 `int / void / float` 间保持边界

如果这些问题都继续各写一套临时逻辑，后面会越来越难维护。

## 3. 最小 mental model

lesson 级别最稳的记法，是把当前类型层想成“共享 descriptor + 共享 compatibility rule”。

概念上可以先记成：

```text
type =
  int
  void
  float
  pair
  struct Point
  fn(int)->int
```

然后让下面这些检查尽量走共享路径：

- declaration
- initialization
- assignment
- return
- parameter passing
- member lookup

也就是说，这篇想讲的不是“descriptor 结构体长什么样”，而是：

`为什么越来越多语义判断必须收敛到同一套 type language。`

## 3.1 伪代码：shared descriptor/compatibility 怎么想

如果把这条课压成实现视角，最稳的 lesson 级模型是：

```text
TypeDesc =
    Int
  | Void
  | Float
  | Pair
  | Struct(name, fields...)
  | Function(ret, params...)
```

然后让大多数语义入口都先走：

```text
analyze_decl(decl):
    desc = build_type_desc(decl.type_syntax)
    check_decl_rules(desc)

compatible(lhs, rhs):
    if lhs.kind != rhs.kind:
        return false
    if lhs/rhs are scalar:
        return same scalar kind
    if lhs/rhs are aggregate:
        return same known aggregate shape
    if lhs/rhs are function:
        return same return kind + same parameter shapes
```

这一层最重要的不是某个结构体字段，而是：

`assignment / init / return / param / member lookup 最终最好都别再各写一套“猜类型”的逻辑。`

## 4. 这条线已经带来了什么

当前最直观的收益，其实已经不是抽象设计，而是 live-tree 上的真实收口：

1. `pair` / `struct` member lookup 开始共用 aggregate-oriented path
2. aggregate assignment / copy-init 不再永远是 pair-specific 特判
3. function-valued local / parameter / return 的 signature check 更像共享 shape check
4. `float` 这条线也开始复用共享 scalar/type descriptor 路

lesson 口径上可以压成一句：

`shared type layer 已经不是未来计划，它已经在帮当前 aggregate / float / callable shape 收边界。`

## 4.1 伪代码：同一套 compatibility 怎么服务多条 feature 线

一个很实用的 lesson 级伪代码可以写成：

```text
check_assignment(lhs, rhs):
    if not compatible(type_of(lhs), type_of(rhs)):
        reject

check_return(fn_ret_type, expr):
    if not compatible(fn_ret_type, type_of(expr)):
        reject

check_callarg(param_type, arg_expr):
    if not compatible(param_type, type_of(arg_expr)):
        reject
```

然后：

- `pair/struct` 用它守 aggregate copy/param/return
- `float` 用它守 same-type transport 和 mixed reject
- function values 用它守 signature/shape matching

这就是为什么这条课其实是在讲：

`language round 的共享语义地基`

而不是某一个 feature。

## 4.2 源码地图：shared function-shape 收敛现在主要看哪

这条线如果回源码，当前最值得先看的其实就是 semantic 入口层：

- `src/semantic/semantic.c`
- `src/semantic/semantic_entry.inc`

重点名字包括：

1. `semantic_populate_function_type_descriptor_from_ast_signature_view(...)`
2. `semantic_populate_function_type_descriptor_from_shape(...)`
3. `semantic_function_shapes_are_compatible(...)`

这几个名字本身就说明当前工程方向：

```text
AST signature view
  -> type descriptor
  -> function shape compatibility
```

所以 lesson 里讲的 shared type layer 不是空概念。

它现在已经真的落在：

- “如何从 AST/function signature 生成 descriptor”
- “如何用统一 shape 规则比较两个 function type”

这类代码上了。

## 4.3 测试地图：shared type layer 现在主要被哪些边界证明

这条课不是说“已经有一个完整大一统类型系统”，而是说很多原本分散的边界已经开始收敛。对应测试可以按三组记。

第一组是 scalar / float compatibility：

```text
SEMA-TYPE-006
SEMA-TYPE-008
SEMA-EXT-035
SEMANTIC-FLOAT-ASSIGN-TYPE-REJECT
SEMANTIC-FLOAT-ARITH-INT-TYPE-REJECT
SEMANTIC-FLOAT-COMPARE-INT-TYPE-REJECT
```

这组回答的是：

`same-type transport 可以开，但 mixed scalar 不能假装自动 promotion。`

第二组是 aggregate known-shape compatibility：

```text
LOWER-IR-AGGRET-PAIR-CALL-INIT
LOWER-IR-AGGCALL-PAIR-CALLARG
LOWER-IR-AGGRET-NESTED-STRUCT-MULTIHOP
```

这组回答的是：

`aggregate param/return 走的是已知 shape + flattened leaves，而不是任意 object flow。`

第三组是 function-value shape compatibility：

```text
SEMA-EXT-018
SEMA-TYPE-003
function-valued return immediate call arity mismatch
function-valued return immediate call scalar type mismatch
```

这组回答的是：

`function type 不能只看“都是函数”，还要看 return kind、arity、parameter shape。`

所以 shared type layer 最适合用下面这句记：

```text
同型/同形才共享；需要桥的地方必须显式写桥；不确定的 derived family 继续关着。
```

## 5. aggregate 为什么最能逼出这条线

`pair` 和 `struct` 是这条课最容易看懂的入口。

因为它们一开始看起来像两个 feature：

- `pair`
- `struct`

但一旦你问下面这些问题：

- member 名字怎么查
- initializer 个数怎么对齐
- copy-init / assignment 怎么判 compatible
- param / return 怎么判 shape 一致

你就会发现它们其实在逼同一套 aggregate descriptor / compatibility。

所以 aggregate line 的真正 engineering value，不只是“支持复合值”，而是：

`逼 semantic 从 pair-specific / struct-specific 分支转向 aggregate-oriented shared path`

## 6. `float` 为什么也会被拉进来

很多人会直觉上把 `float` 线理解成单独一摊 helper-backed work。

但当前计划很明确：

- `float` 不是只需要 lowering helper
- 它也需要共享的 scalar kind / compatibility / function signature story

最典型的 lesson 口径是：

- `float = float` 这类 same-type transport 可以开
- `int = float` 要明确拒绝
- mixed scalar arithmetic / implicit conversion 要继续有清晰 gate

这背后其实就是：

`float feature 能不能诚实推进，取决于 type layer 能不能把“同型允许、异型拒绝、哪类 bridge 另开”讲清楚。`

## 7. function values 为什么也离不开它

这篇还和 `generic_function_values_lesson.md` 有很强关系。

因为 function value 如果真的要继续长，就必须回答：

1. return kind 相同吗
2. parameter count 相同吗
3. parameter kinds 相同吗
4. aggregate / float parameter shape 相同吗

所以 function value 线并不只是 lowering 问题。

它同样在逼仓库承认：

`函数签名也应该是 shared type descriptor 的一部分。`

## 8. 当前 still-closed 的边界

这条课也要刻意提醒一个误区：

- 有 shared type layer 方向
- 不等于仓库已经进入 full C type system

当前仍然明显没开的东西包括：

1. implicit conversion lattice
2. full aggregate ABI / object model
3. generic aggregate arrays / globals / arbitrary value positions
4. “只要结果都是 float 就都算合法”的宽松推断
5. “参数个数一样就算 function shape compatible”的粗糙函数值规则

所以 lesson 最稳的口径是：

`这是 conservative shared type layer，不是 full language type system。`

## 9. 建议怎么把这条线记进脑图

建议按下面三层记：

1. **surface features**
   - `pair`
   - `struct`
   - `float`
   - function value shape
2. **shared semantic questions**
   - compatibility
   - member lookup
   - signature matching
3. **shared type machinery**
   - descriptor
   - aggregate-oriented vocabulary
   - same-type / known-shape-first boundary

这样会比“又加了一个 type feature”更接近现在仓库真实的工程方向。

## 10. 读完后接哪篇

最自然往下接：

1. `aggregate_boundary_lesson.md`
2. `float_lesson.md`
3. `generic_function_values_lesson.md`

因为这三条线正好分别在逼这套 shared type layer 回答：

- aggregate flow
- scalar compatibility
- function shape compatibility
