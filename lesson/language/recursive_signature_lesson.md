# Recursive Signature Lesson（compiler_lab）

> 目标：专门讲最近很新的前门扩张，也就是 function-type 本身开始更深层递归之后，parser / AST / semantic 为什么都必须跟着长，当前 live tree 到底开到了哪，以及这条线为什么不只是“多支持几个括号”。

## 一句话定位

这条线回答的是：

`当前仓库的 function-type front door 已经不再停在浅层 declarator，而开始允许更深的高阶函数签名进入前半段管线。`

## 1. 为什么这篇值得单独存在

如果只看后半段的 function-values 线，很容易以为难点都在：

- specialization
- returned payload
- callable-object rebuild

但最近 `NEXT_STEPS` 明确暴露了另一个前门问题：

`parser/AST 自己能不能先把更深的 function-type 结构收进来`

以前这条线有一个很明显的旧边界：

- function signature 深度到某个层数就会死

现在 reopened 之后，已经不只是在修一个 parser bug，而是在回答：

1. deeper nested function-type parameter/return 能不能 parse
2. AST metadata 能不能 reconstruct 递归签名
3. semantic / lowering 后续能不能继续吃这些更深的 shape

## 2. 最小心智模型

最小 mental model 不是“括号更复杂了”，而是：

```text
source declarator
    -> recursive function-type AST
    -> shared function-shape metadata
    -> later semantic/lowering consumers
```

也就是说，这条线真正重要的地方是：

`function-value world 的前门，不再只能吃很浅的一层 type syntax。`

## 3. 旧边界是什么

`NEXT_STEPS` 里已经明确提到，之前这条线卡在类似：

- “five levels” guard

也就是说，以前系统更像是：

```text
if function-type nesting depth > fixed bound:
    reject or fail front door
```

这对真正的 higher-order line 是个硬门槛。

因为一旦你写：

```c
omega(ultra, meta, relay, pass, apply, add1, 41)
```

这种更深层的高阶 family，问题还没到 lowering，就可能先死在前门。

## 4. 当前 reopened 的方向

现在 lesson 里最重要的口径是：

- parser-side recursive signature metadata 已经在变深
- AST bridge 已经在重建 recursive function signature
- 兼容旧 metadata reader 的同时，前门已经比以前宽了

所以这条线当前不是“某个单独 witness 过了”，而是：

`front-half function-type representation 正在从浅层 flatten 走向更诚实的 recursive shape。`

## 4.1 伪代码：recursive signature AST 怎么想

一个 lesson 级实现模型可以写成：

```text
TypeSyntax =
    Int
  | Void
  | Float
  | FunctionType(ret_type, param_types...)
```

然后 parser/AST 不是只存“参数个数 + 返回类型名”，而是更像：

```text
parse_function_type(tokens):
    parse return/base type
    parse nested declarator structure recursively
    build FunctionType node whose params may themselves be FunctionType
```

也就是说：

```text
int apply(int f(int), int x)
```

和更深的：

```text
int omega(int ultra(int meta(int relay(int pass(int apply(int), int), int), int), int), int)
```

在前门上开始属于同一类问题：

`递归函数类型树能不能先被 honest 地收进 AST`

## 4.2 伪代码：为什么需要 AST bridge reconstruction

当前仓库并不是把旧世界完全丢掉再重写一遍。

更贴近现在的设计，是：

```text
new recursive parser metadata
    -> bridge layer reconstructs recursive signature view
    -> older consumers still see compatible shape where needed
```

lesson 级伪代码可以压成：

```text
reconstruct_signature(ast_decl):
    if decl is scalar:
        return scalar_type
    if decl is function-typed:
        ret  = reconstruct_signature(decl.return_part)
        args = map reconstruct_signature over decl.params
        return FunctionType(ret, args)
```

这样做的意义不是形式上更优雅，而是：

`后面的 semantic/type-system/function-values 线终于能吃到真正更深的 shape，而不是 parser 阶段已经被裁掉的近似值。`

## 4.3 源码地图：前门现在主要在哪几层

如果你要顺着源码看这条前门扩张，当前最值得先记三处：

1. `src/parser/parser_stmt_decl_tu.inc`
   - 看更深 declarator / function-type 前门怎么 parse
2. `src/ast/ast.c`
   - 看 closure/function signature view 怎么从 legacy metadata 重建
   - 重点有：
     - `ast_expression_get_closure_signature_view(...)`
     - `ast_build_signature_from_legacy(...)`
3. `src/semantic/semantic.c` / `semantic_entry.inc`
   - 看 reconstructed shape 怎么继续进入 type descriptor / compatibility

如果压成一句更实用的话：

```text
parser
  先把更深 type syntax 收进来

ast
  再把旧 metadata 桥接成 recursive signature view

semantic
  再把这份更深 shape 用在真正的匹配/兼容上
```

## 5. 它和 closure-transport 修复为什么会一起出现

`NEXT_STEPS` 里这条线和另一类修复是绑在一起出现的：

- direct closure literal actual argument 重新打通
- declaration-only function local 可以从 closure literal 首次赋值
- closure-backed local 可以被新 closure literal 重绑

这不是巧合。

因为一旦 function-type front door 更深了，很多原本“source shape specific reject”的 family 也会一起暴露出来。

所以 lesson 上最稳的判断是：

`recursive signature front door reopening` 和 `closure/function-value transport repair`

其实是同一个大方向的前后门两侧：

- 前门：
  - deeper type syntax 能不能进来
- 后门：
  - 进来之后 transport / binding / lowering 能不能接住

## 6. 为什么这条线不只是 parser 课题

这篇最容易被低估的地方是：

- 看起来像 parser 支持更深 declarator

但真正受影响的是：

1. AST representation
2. semantic shape matching
3. function-value compatibility
4. later lowering view reconstruction

也就是说，这条课其实和：

- `function_values_lesson.md`
- `higher_order_callable_lesson.md`
- `type_system_lesson.md`

都直接相关。

## 7. 当前 still-kept boundary

这条线现在也不能被误讲成：

1. 任意复杂 function-type grammar 已经 fully solved
2. deeper front-door automatically means every later consumer already generic
3. 所有高阶 family 的 remaining bug 都只是 parser 问题

更准确的口径仍然是：

`前门已经明显放宽，但后续 semantic / specialization / payload / rebuild 仍然按 conservative family 一条条收。`

## 8. 最稳的分层记法

建议把这条线记成三层：

1. **parse**
   - deeper nested function-type syntax survives
2. **reconstruct**
   - AST bridge rebuilds recursive signature shape
3. **consume**
   - semantic / function-values / lowering 后续开始真正用这份更深的 shape

这样就不容易把：

- “parser 终于不炸了”
- 和
- “高阶 function-value world 已经 fully generic”

混成一句话。

## 9. 读完后接哪篇

最自然往下接：

1. `function_values_lesson.md`
2. `higher_order_callable_lesson.md`
3. `closure_capture_callable_lesson.md`
4. `callable_object_ir_lesson.md`
5. `type_system_lesson.md`

因为这条 front-door 扩张最后真正服务的就是：

- deeper higher-order function-value shapes
- callable capture/return family
- `shape + fn_make + call_indirect` 这种 IR 合约需要的 visible signature
- shared function-shape compatibility
