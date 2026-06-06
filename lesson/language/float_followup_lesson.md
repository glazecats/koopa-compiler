# Float Follow-Up Lesson（compiler_lab）

> 目标：专门讲当前 float 线里已经超出“最初 conservative transport”那部分内容，也就是 explicit conversion、ternary/helper-membrane family、recursive pure-float value family、以及为什么这些 follow-up 现在已经需要单独成课。

## 一句话定位

这条线回答的是：

`float 现在不只是“能运输”，而是已经开始沿着几条窄但真实的 value-producing family 往前扩，但每一步都还是 helper-backed / boundary-heavy 的。`

## 1. 为什么 `float_lesson` 之后还需要这篇

`float_lesson.md` 已经讲了大图景：

- transport
- condition / comparison
- helper-backed arithmetic
- explicit conversion

但当前 live tree 的真实 follow-up 已经比“两个计划名”更宽了。它至少已经开始回答：

1. derived float value 怎么合法桥接到新上下文
2. ternary-derived family 哪些重新打开、哪些继续封住
3. recursive pure-float tree 哪些 value context 已经能进
4. helper-backed tree 什么时候能当 condition root

所以这篇更准确的定位是：

`float 的后半段现在已经不是单点 broadening，而是一张 family map。`

## 2. explicit conversion：为什么它是“桥”而不是“系统”

当前 explicit conversion 最重要的 lesson 口径是：

- `int(float_expr)`
- `float(int_expr)`

它解决的是：

`我明确写了转换，就给我一条诚实的桥`

而不是：

`顺手打开整个 implicit scalar conversion system`

当前 helper-backed lowering 也要继续强调：

- `__builtin_f2i32`
- `__builtin_i2f32`

所以 lesson 最该记住的一句话是：

`explicit conversion 是 opt-in bridge，不是类型系统已经自动会转。`

## 2.1 伪代码：semantic gate 怎么看 conversion

这条线最适合先补一个语义 gate 伪代码：

```text
analyze_conversion(expr, target_keyword):
    src = analyze(expr)

    if target_keyword == int:
        accept only if src.type == float
        lower via __builtin_f2i32

    if target_keyword == float:
        accept only if src.type == int
        lower via __builtin_i2f32

    reject redundant same-type conversion
    reject aggregate/void/non-scalar source
```

这能帮助读者把：

- “我显式写了 bridge”

和：

- “编译器会自己替我做 scalar promotion”

彻底分开。

## 3. ternary / helper-membrane：为什么这条线非常容易读错

现在最容易被误讲的一类 follow-up，不是 conversion，而是：

- same-type float ternary value
- ternary feeding later float family
- helper-return wrapping ternary value

当前 lesson 最重要的口径是：

`ternary 不是“一开全开”，而是被拆成了非常细的邻域 family。`

当前已经值得单独记住的几层是：

1. same-type float ternary value
   - 在一些 return / assignment / initializer family 里已经开了一部分
2. ternary -> same-type float call argument
   - 也已经有一条窄 opening
3. helper-membrane family
   - 当 ternary 先被包进 helper return，再作为 direct call root 参与后续比较/算术 family 时，又是一条单独的 opening
4. ternary-derived arithmetic/comparison neighbors
   - 仍然有不少刻意 kept reject

也就是说，当前更像：

```text
same-type ternary value
  -> 某些 value context 已开
  -> 某些 later arithmetic/compare neighbor 仍封住
  -> 被 helper 包一层后，又能形成新的 direct-root family
```

这条线最关键的 lesson 判断是：

`helper membrane 允许的是“先把值变成一个已知 direct call root，再参与后续 family”，不是“把所有 ternary-derived float operand 全部洗白”。`

## 3.1 伪代码：helper-membrane family 怎么判断

一个很实用的 lesson 级伪代码可以写成：

```text
is_open_float_root(expr):
    if expr is identifier/direct-float-literal/direct-call:
        return true
    if expr is explicit float(int_expr):
        return true
    if expr is helper return whose result type is accepted float root:
        return true
    return false

allow_later_float_family(expr):
    if is_open_float_root(expr):
        return true
    reject as still-derived/unsupported neighbor
```

也就是说，`helper membrane` 当前不是“泛化表达式支持”，而更像：

`把一部分复杂值重新包装成仓库已经愿意承认的 direct root。`

## 4. recursive pure-float value family：为什么它不只是 condition 问题

很多人会把 recursive pure-float 只记成：

- `if ((x + y) + z)`
- `while (-a * (b / c))`

但当前真实进展已经更宽。

这条线现在至少已经覆盖了几种 context：

1. same-type float call argument
2. same-type local assignment / initializer
3. same-type float comparison
4. recursive pure-float condition

也就是说，最稳的 lesson 级伪代码不是单独记某个 `if`，而是记这个 family：

```c
float t;
t = (x + y) + z;
if ((x + y) + z) return 1;
return wrap(-a * (b / c)) == z;
```

这一整组表达的是同一个判断：

`helper-backed recursive pure-float tree 已经开始被承认成“真的 float value family”，只是仍然是 narrow family。`

## 4.1 伪代码：recursive pure-float tree 怎么被 gate

最稳的实现心智模型是：

```text
is_recursive_pure_float(expr):
    if expr is direct float root:
        return true
    if expr is unary_sign(sub):
        return is_recursive_pure_float(sub)
    if expr is helper-backed float binary(lhs, rhs):
        return is_recursive_pure_float(lhs)
           and is_recursive_pure_float(rhs)
    return false
```

然后不同上下文再分别问：

```text
allow_float_callarg(expr):
    return is_recursive_pure_float(expr)

allow_float_local_value(expr):
    return is_recursive_pure_float(expr)

allow_float_compare(expr):
    return is_recursive_pure_float(expr)

allow_float_condition(expr):
    return is_recursive_pure_float(expr)
       and expr not in still-closed ternary-derived neighbors
```

这也是为什么当前 float 后半段最好被讲成：

`同一个 recursive family，在不同上下文逐步被单独放开。`

## 4.2 为什么这些 family 会被一层层单独锁 checkpoint

这条线很值得再补一个“为什么 regression 会长成这样”的实现视角。

当前更像：

```text
family_1: recursive pure-float -> callarg
family_2: recursive pure-float -> local value
family_3: recursive pure-float -> compare
family_4: recursive pure-float -> condition
```

每一层都在问不同问题：

- semantic gate 有没有放宽
- helper call chain 有没有保持
- truthiness/compare bridge 有没有接住
- later optimized shape 有没有还诚实

所以 lesson 上最稳的记法不是“为什么不一次全开”，而是：

`因为每个 value context 都会碰到不同的 lowering/optimization honesty boundary。`

## 5. recursive pure-float condition：为什么它仍然值得单独看

当前 direct float condition 已经不新鲜了：

- `if(g)`
- `while(g)`
- `for(;g;)`

但 recursive pure-float condition 是另外一层：

```c
if ((x + y) + z) { ... }
while (-a * (b / c)) { ... }
```

这条线的意义不是又多开一个语法位，而是：

- 它逼 semantic / IR / lower-IR / ValueSSA / machine-report 一起面对
  helper-backed pure-float tree 的 truthiness lowering

所以当前 lesson 口径要记：

- 已经有 recursive pure-float condition landed
- 但它仍然是 narrow opening
- 不等于“所有 float-derived expression 现在都能进 condition”

## 5.1 伪代码：truthiness lowering 仍然复用同一条桥

当前最诚实的 lowering 伪代码依然可以压成：

```text
lower_float_condition(expr):
    bits = lower_float_value(expr)
    masked = bits & 0x7fffffff
    return masked != 0
```

所以即使 source 从：

- `if(g)`

扩到：

- `if((x + y) + z)`

当前 truthiness model 其实没有换。

变的是：

`哪些 expr 被允许先进入这条桥`

而不是：

`桥本身突然变成 native float branch ISA`

## 5.2 helper-backed recursive tree 为什么不等于“全 derived float condition”

这条线最容易被误读成：

- 既然 `(x + y) + z` 能进 condition
- 那 float derived condition 应该差不多都开了

但当前更准确的 lesson 级 gate 是：

```text
allow_condition(expr):
    if expr is recursive pure-float tree already accepted elsewhere:
        accept
    else if expr is ternary-derived arithmetic neighbor:
        reject
```

也就是说，当前不是：

`所有结果类型为 float 的表达式都能过 condition gate`

而是：

`只有已经被纳入 pure-float helper-backed family 的递归树，才复用 truthiness bridge`

## 6. 一个更实现化的 family-route 总图

如果想把整篇课压成一个流程图，最稳的记法可以写成：

```text
expr
  -> is explicit bridge?
       yes -> helper conversion
  -> is accepted direct root?
       yes -> direct float family
  -> is recursive pure-float tree?
       yes -> helper-backed recursive family
  -> is ternary/helper-membrane route?
       maybe reopen under narrower family
  -> else reject
```

这张图比按运算符背更接近现在 live tree 的真实实现方式。

## 7. 源码 / 测试地图：follow-up 不是凭感觉分 family

这篇 lesson 里提到的 family，最直接的回查入口是：

1. semantic gate
   - `src/semantic/semantic.c`
   - `src/semantic/semantic_entry.inc`
2. helper-backed lowering / downstream shape
   - `src/ir/ir_gen.c`
   - `src/lower_ir/lower_ir.c`
3. focused semantic witness
   - `tests/semantic/semantic_regression_test.c`

尤其值得记住这些 filter 名字：

- explicit bridge：
  `SEMANTIC-FLOAT-EXPLICIT-INT-FROM-FLOAT-DIRECT-ACCEPT`,
  `SEMANTIC-FLOAT-EXPLICIT-FLOAT-FROM-INT-ACCEPT`,
  `SEMANTIC-FLOAT-EXPLICIT-SAME-TYPE-REJECT`
- same-type ternary value：
  `SEMANTIC-FLOAT-TERNARY-VALUE-ASSIGN-FLOAT-ACCEPT`,
  `SEMANTIC-FLOAT-TERNARY-VALUE-INIT-FLOAT-ACCEPT`,
  `SEMANTIC-FLOAT-TERNARY-VALUE-CALLARG-FLOAT-ACCEPT`
- recursive helper-backed value：
  `SEMANTIC-FLOAT-CHAIN-ADD-CALLARG-FLOAT-ACCEPT`,
  `SEMANTIC-FLOAT-NESTED-MUL-DIV-CALLARG-FLOAT-ACCEPT`,
  `SEMANTIC-FLOAT-CHAIN-ADD-LT-COMPARE-ACCEPT`,
  `SEMANTIC-FLOAT-NESTED-MUL-DIV-GE-COMPARE-ACCEPT`
- helper membrane：
  `SEMANTIC-FLOAT-HELPER-TERNARY-CALL-ARITH-ACCEPT`,
  `SEMANTIC-FLOAT-HELPER-TERNARY-CALL-COMPARE-ACCEPT`,
  `SEMANTIC-FLOAT-HELPER-TERNARY-CALL-PLUS-INT-REJECT`
- still-closed neighbor：
  `SEMANTIC-FLOAT-RECURSIVE-COND-TERNARY-NEIGHBOR-REJECT`,
  `SEMANTIC-FLOAT-TERNARY-VALUE-COMPARE-FLOAT-REJECT`,
  `SEMANTIC-FLOAT-TERNARY-PLUS-FLOAT-CALLARG-REJECT`

这组名字本身就是一张地图：

```text
ACCEPT filters = 当前 narrow opening
REJECT filters = 刻意保留的邻域边界
```

所以读这条 float follow-up 时，不要只看“某个语法能不能过”，要看它被归到哪一个 family。

## 8. 最稳的 family map

如果只想记一张最实用的小图，建议记成：

1. **explicit bridge family**
   - `int(float_expr)`
   - `float(int_expr)`
2. **same-type ternary family**
   - 某些 return / assign / init / callarg 已开
   - 一些 arithmetic / compare 邻域继续封住
3. **helper-membrane family**
   - ternary / helper-backed value 先通过 direct helper return，再参与下一层 family
4. **recursive pure-float family**
   - callarg
   - local value
   - compare
   - condition

这样比“float 后面又开了一点点”更接近当前仓库真实状态。

## 9. 为什么 helper-backed honesty 仍然是第一原则

float follow-up 这条线最容易被误讲成：

- “既然比较、condition、arithmetic、conversion 都开了不少，那是不是已经差不多是完整 float 语言了？”

答案还是不是。

当前这条线依然被以下原则控制：

1. helper-backed first
2. explicit boundary first
3. narrow family first
4. mixed scalar/general implicit system last

所以如果你看到一个新 witness 通过，优先问：

- 它是不是 direct root / recursive pure-float tree / explicit bridge 之一？

而不是直接以为：

- float line 已经整体放开

## 10. 当前 still-closed 边界

这条 follow-up 线里，最重要的 kept boundary 是：

1. implicit conversion 仍然不开
2. promotion lattice 仍然不开
3. arbitrary mixed scalar arithmetic 仍然不开
4. ternary-derived / neighbor family 仍然经常是单独 gate
5. “结果类型还是 float，所以整个子树都自动合法” 仍然不开

也就是说，当前最容易读错的地方不是“哪些开了”，而是：

`开的是窄 family，不是整个 float world。`

## 11. 读完后接哪篇

最自然往下接：

1. `float_lesson.md`
2. `lesson/core/semantic_lesson.md`
3. `lesson/ssa/value_ssa_lesson.md`

因为 follow-up 的真正难点正在：

- semantic gate 怎么继续拆 family
- IR/lower-IR/SSA 怎么继续保持 helper-backed honesty
