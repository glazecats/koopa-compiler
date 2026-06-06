# Float Lesson（compiler_lab）

> 目标：解释当前仓库的 float 线到底开到了哪，为什么它一直被描述成 conservative / helper-backed slice，以及现在哪些地方已经能用、哪些地方还故意不让用。

## 一句话定位

这条线回答的是：

`float 现在已经不是“完全不能用”，但它也还远不是“完整 C float 语义”。`

## 1. 当前 float 线的统一风格

当前最重要的 lesson 口径不是某一个运算符，而是一个统一设计：

- 先在 `-extension` 下开
- 先支持保守 transport
- 再一点点开 value-producing family
- 当前算术/转换继续走 helper-backed lowering

所以 lesson 里最该先记的一句话是：

`当前 float line 追求的是“诚实可翻译”，不是“假装后端已经有完整原生 float ALU”。`

## 2. transport：最早开的那一层

当前 float 最稳定的基础层是：

- declaration / parameter / return 可用 `float`
- direct identifier / direct call / float literal 这类 direct root 可以运输

也就是说，这类 shape 现在是有稳定 surface 的：

```c
float g = 1.25;
float id(float x){ return x; }
float get(){ return id(g); }
```

而且这条 transport 线已经不只停在前端：

- canonical IR
- lower IR
- translation-only ValueSSA
- 默认 machine report surfaces

都已经能看到相应 float metadata 或合法优化后的 transport shape。

## 2.1 伪代码：transport line 怎么想

当前最稳的实现心智模型不是“float 真正住成硬件浮点值”，而更像：

```text
float value
    -> preserve float type/slot metadata
    -> carry 32-bit payload through IR/lower-IR/SSA/report surfaces
```

最小 lesson 级伪代码可以写成：

```text
lower_float_transport(expr):
    bits = lower_float_payload(expr)
    keep type metadata = float
    return bits
```

也就是说，这条线最先保证的是：

- type 不丢
- payload 不丢

而不是：

- backend 已经有 native float ALU

## 3. condition / comparison：控制流和比较先开

当前 float 线并不是一上来就开泛化算术，而是先开：

1. direct float condition
2. logical condition composition
3. same-type float comparison

lesson 口径上要记：

- `if(g)` / `while(g)` 这类 direct root 条件现在是可以的
- `!g` / `g && h` / `g || h` 这类 condition-composition 也已经开了一部分
- same-type `float ==/!=/< /<=/>/>= float` 也已经有 conservative lowering

但 current boundary 仍然很重要：

- 混合标量比较不开
- arbitrary derived float expression 也不是到处都能直接进 condition

## 3.1 伪代码：condition / compare 为什么是独立 gate

这条线最适合这样记：

```text
allow_float_condition(expr):
    accept direct float root
    accept explicitly-open logical condition composition
    reject broader derived families unless separately opened

allow_float_compare(lhs, rhs):
    require same-type float roots/families
    lower through normalized compare bridge
```

也就是说，当前并不是：

`能算 float -> 自动能拿去 branch/compare`

而是：

`condition` 和 `compare` 都是单独开的 family gate

## 3.2 伪代码：truthiness / compare 大致怎么 lower

当前最诚实的 lesson 级伪代码可以压成：

```text
lower_float_truthiness(bits):
    return (bits & 0x7fffffff) != 0

lower_float_eq(lhs_bits, rhs_bits):
    return normalize_zero(lhs_bits) == normalize_zero(rhs_bits)
```

而 relational family 更像：

```text
lower_float_rel(lhs_bits, rhs_bits):
    lhs_key = normalize_order_key(lhs_bits)
    rhs_key = normalize_order_key(rhs_bits)
    compare lhs_key and rhs_key
```

这也解释了为什么 lesson 一直强调：

`当前是 conservative compare bridge，不是完整 native float compare ISA story`

## 4. arithmetic：helper-backed，不是假原生

当前已开的算术 family 已经不少了：

- `+`
- `-`
- `*`
- `/`

而且还包括：

- unary-sign transport
- recursive pure-float tree 的一部分

但这里最重要的 honesty boundary 是：

`当前 lowering 还是 helper-backed`

也就是 lesson 里应该先把这些 helper 当成第一等事实：

- `__builtin_fadd32`
- `__builtin_fsub32`
- `__builtin_fmul32`
- `__builtin_fdiv32`

所以不要把当前 float arithmetic 讲成：

- “后端已经有 native float ALU”

更准确的是：

- “language/IR surface 已经开到算术，但当前实现仍然通过 helper call 诚实翻译”

## 4.1 伪代码：helper-backed arithmetic 到底是什么意思

当前最稳的实现模型就是：

```text
lower_float_add(lhs, rhs):
    lhs_bits = lower_float_value(lhs)
    rhs_bits = lower_float_value(rhs)
    return call __builtin_fadd32(lhs_bits, rhs_bits)

lower_float_mul(lhs, rhs):
    lhs_bits = lower_float_value(lhs)
    rhs_bits = lower_float_value(rhs)
    return call __builtin_fmul32(lhs_bits, rhs_bits)
```

也就是说，当前算术线最重要的 honesty boundary 是：

`float expression 现在是通过 helper call 活下来的，不是通过现有中后端的原生 float op family 活下来的`

## 4.2 语义 gate 和 lowering gate 是分开的

这条线再补一个非常重要的实现判断：

```text
semantic:
    只放开 narrow same-type families

lowering:
    一旦语义放行，就统一走 helper-backed bridge
```

这就是为什么：

- 语义上已经可以有 `+ - * /`

但：

- lesson 仍然不能说“float backend 已经成熟”

## 5. explicit conversion：下一条很关键的桥

float 线另一个很值得单独记住的 checkpoint 是 explicit conversion。

当前 target shape 是：

```c
int x = int(g);
float y = float(3);
```

lesson 里最重要的口径：

- 这是**显式** conversion
- 不是 implicit conversion / promotion lattice

而且 lowering 也是 helper-backed：

- `__builtin_f2i32`
- `__builtin_i2f32`

所以这条线当前解决的是：

`我明确写了转换，就给我一条诚实的桥`

不是：

`语言已经有完整 usual arithmetic conversions`

## 5.1 伪代码：conversion lowering 和 arithmetic lowering 的关系

当前最稳的 lesson 级记法是：

```text
lower_int_from_float(expr):
    bits = lower_float_value(expr)
    return call __builtin_f2i32(bits)

lower_float_from_int(expr):
    value = lower_int_value(expr)
    return call __builtin_i2f32(value)
```

这和 arithmetic lowering 的共同点正是：

`都先借 helper，把前端已经放开的 narrow family 诚实送下去。`

## 6. 当前还故意没开的东西

这是 float lesson 最容易讲松的地方，所以单列出来：

**当前还故意没开：**

1. implicit `int <-> float` conversion
2. promotion lattice
3. arbitrary mixed scalar arithmetic
4. generic float array/object ABI
5. 把所有 derived float expressions 都当作已支持

也就是说，如果你看到某些 witness 已经能跑：

- 这通常代表“某个 narrow family 已 landed”
- 不代表“整个 float 世界已经开完了”

## 6.1 源码/测试地图：float 线应该看什么

float 线最容易读错，因为 default optimized path 可能把很明显的 helper/transport shape 折掉。

所以反查时建议按 surface 分工：

1. semantic
   - 看同型允许、异型拒绝、explicit conversion 是否分清
   - 典型边界：
     - `float = float` allowed
     - `int = float` rejected unless explicit conversion family
     - `float + int` rejected
2. canonical IR / lower IR
   - 看 helper-backed path 是否存在
   - 例如 `__builtin_fadd32` / `__builtin_f2i32`
3. translation-only ValueSSA
   - 看 float metadata / helper bridge 是否没有被默认 pass 抹掉
4. default ValueSSA / machine report
   - 允许合法 constant fold / direct load fold
   - 不要把“形状变短”自动当成 lowering 消失

lesson 级测试关键词：

```text
FLOAT
FADD32
FSUB32
FMUL32
FDIV32
F2I32
I2F32
FLOAT-CONDITION
FLOAT-COMPARE
MIXED
```

最稳的 checkpoint 读法：

```text
front-half surfaces prove the helper/metadata contract
default optimized surfaces may prove equivalent folded behavior
```

这也是为什么 float lesson 里一直强调：

`legal fold is not a regression`

只要类型/metadata/observable behavior 没丢，默认路径折叠成更短形状是可以接受的。

## 7. 读这条线最稳的方式

推荐按三层记：

1. **transport**
   - declaration / assignment / return / call-root
2. **condition / comparison**
   - control-flow roots
   - same-type compare
3. **helper-backed arithmetic / conversion**
   - `+ - * /`
   - explicit `int(...)` / `float(...)`

这样比较不容易把：

- “direct root 已开”
- “recursive pure-float family 开了一部分”
- “mixed scalar/general implicit conversion 还没开”

这三件事讲混。

## 8. 读完后接哪篇

最自然往下接：

1. `lesson/core/semantic_lesson.md`
2. `lesson/core/ir_lesson.md`
3. `float_followup_lesson.md`
4. `type_system_lesson.md`
5. `lesson/ssa/value_ssa_lesson.md`

因为 float 这条线最有意思的地方正是：

- semantic gate 怎么切
- IR/lower IR 怎么保 metadata
- default ValueSSA / machine report 又能合法折到什么程度
