# Function Implementation Map Lesson（compiler_lab）

> 目标：把当前仓库 function-value / closure / returned-callable 这条大线，直接映射回真实源码结构。它不再回答“概念上是什么”，而是回答“如果我要去代码里追一条 shape，它现在大概在哪几层、哪几个 helper、哪几类中间结构里转”。

## 一句话定位

这条线回答的是：

`当前 function 主线不是散落在一堆 if-else 里的偶然逻辑，而是已经长成“前门语义 -> lowering 描述符/载荷 -> callable-object contract”三段式结构。`

## 1. 为什么现在需要这篇

前面的 language lessons 已经把大图景讲出来了：

- `function_values_lesson`
- `function_value_callee_lowering_lesson`
- `returned_callable_lesson`
- `closure_capture_callable_lesson`
- `closure_object_lesson`
- `recursive_signature_lesson`
- `type_system_lesson`

但如果你真的想回源码追一条 witness，现在最容易迷路的地方是：

1. semantic gate 在哪
2. returned payload 逻辑在哪
3. closure capture / local binding / target-set merge 在哪
4. `fn_make + call_indirect` 的 contract 到底在哪一层定死

所以这篇不再补概念，而是补：

`源码地图 + 数据流地图`

## 2. 最稳的三段总图

如果把当前 function 主线压成一个最稳的源码结构图，可以先记成：

```text
parser / ast
  -> 更深 function-type / closure signature 前门

semantic
  -> admission / shape compatibility / local binding gate

canonical IR lowering
  -> descriptor / payload / tag / rebuild / dispatch

verifier
  -> explicit callable-object contract
```

也就是说，当前 function 这条线最不该再被理解成：

`只有 lowering 在硬撑`

更准确的是：

`前门、描述符、显式 object contract 都已经各自有落点了`

## 3. 第一段：前门和 signature view

### 3.1 parser / AST

如果你在追更深 function-type 或 closure signature，当前最值得先看：

- `src/parser/parser_stmt_decl_tu.inc`
- `src/ast/ast.c`

特别是 AST 这一侧，当前最该记住的名字是：

1. `ast_build_signature_from_legacy(...)`
2. `ast_expression_get_closure_signature_view(...)`

这两者说明的事很明确：

```text
旧 metadata
  -> recursive signature view
```

也就是说，前门现在已经不是“只存 parameter_count 凑合用”，而是在尽量重建更完整的 function signature view。

### 3.2 lesson 级伪代码

这条线最稳的实现心智模型可以写成：

```text
parse_declarator(tokens):
    produce nested function-type syntax tree

build_signature_view(ast_node):
    if old metadata exists:
        reconstruct recursive signature
    else:
        use explicit signature node directly
```

这正是 `recursive_signature_lesson.md` 想讲的源码事实。

## 4. 第二段：semantic gate

如果你在追“这个 function-value / closure / returned shape 为什么语义上允许/拒绝”，当前最值得先看的文件是：

- `src/semantic/semantic.c`
- `src/semantic/semantic_entry.inc`

### 4.1 当前最重要的函数名

这里最值得先记住的几类 helper 是：

1. **shape / descriptor**
   - `semantic_populate_function_type_descriptor_from_ast_signature_view(...)`
   - `semantic_populate_function_type_descriptor_from_shape(...)`
   - `semantic_function_shapes_are_compatible(...)`
2. **local binding / initializer**
   - `semantic_current_function_find_direct_local_function_value_declaration_type_descriptor(...)`
   - `semantic_extension_function_value_initializer_expr_matches_expected(...)`
   - `semantic_extension_resolve_local_function_value_binding_with_expected_type(...)`
3. **general function-value admission**
   - `semantic_extension_check_function_value_expression(...)`

如果把它们压成一句：

```text
semantic 现在已经有一套显式 function-shape / descriptor / local-binding gate
```

而不是只有“见到 function value 就分支判断一下”。

### 4.2 它们分别在回答什么

最稳的记法是：

```text
shape helper
  -> 两个 function type 到底配不配

initializer/binding helper
  -> 这个 local function value / closure binding 能不能成立

expression gate
  -> 这个 source shape 在当前位置能不能被当作 function value
```

### 4.3 lesson 级伪代码

一个很实用的 semantic 骨架可以写成：

```text
analyze_function_value_expr(expr, expected_shape):
    actual = resolve_expr_type_descriptor(expr)
    require shape_compatible(actual, expected_shape)

resolve_local_binding(name, expected_shape):
    find direct local declaration or parameter
    if closure-backed:
        mark closure-backed
    require shape_compatible(binding_shape, expected_shape)
```

## 5. 第三段：canonical IR lowering

如果你在追真正最复杂的实现，那目前最值得长期盯的还是：

- `src/ir/ir_lower_stmt.inc`

因为现在 function 主线的大部分“活”都集中在这里：

- static specialization
- local binding transport
- returned payload analysis
- closure payload materialization
- target-set merge / tag local
- caller-side rebuild descriptor

### 5.1 当前最值得先记的函数名

这一层可以先按四组记：

1. **identifier/static binding**
   - `ir_try_build_identifier_function_value_descriptor(...)`
2. **returned family analysis**
   - `ir_try_analyze_returned_function_value_family(...)`
   - `ir_try_build_returned_function_value_descriptor(...)`
3. **payload/tag preparation**
   - `ir_prepare_returned_function_value_tag_local_and_targets(...)`
   - `ir_append_named_capture_local_slots(...)`
   - `ir_copy_materialized_returned_payload_into_binding(...)`
4. **descriptor / binding finalize**
   - `ir_materialize_returned_function_object_descriptor(...)`
   - `ir_finalize_materialized_returned_closure_binding_descriptor(...)`
   - `ir_finalize_returned_function_object_descriptor_metadata(...)`

如果想再往下一点，还可以记：

```text
binding transport / target-set / reassignment
```

这批 helper 也大多在同一个文件里成片出现。

### 5.2 这一层最重要的中间结构

读源码时最值得盯着的几个结构/变量是：

1. `IrLowerFunctionObjectDescriptor`
   - 代表当前 lower 到手里的 callable view / binding descriptor
2. `IrLowerReturnedFunctionValueFamilyInfo`
   - 代表当前 returned family 属于哪种 producer family
3. `function_target_tag_local_id`
   - 动态 family 的 tag transport
4. `capture_local_ids`
   - closure payload capture slot

如果把这一层压成一句：

```text
family info -> payload/tag locals -> descriptor -> final binding/callable view
```

这几乎就是当前 returned/closure/object 线的实现骨架。

### 5.2.1 `IrLowerFunctionObjectDescriptor` 为什么要单独记

这东西在 lesson 里最稳的理解不是“某个临时 struct”。

更准确地说，它是在 lowering 这一侧承接：

```text
一个 callable 现在应该如何被看待、如何被运输、后续如何被调用
```

它往往会隐含或直接携带：

1. direct target 是谁
2. 是 static 还是 dynamic family
3. payload slot/capture slot 在哪
4. later consumer 该走 direct helper 还是 callable-object path

所以 lesson 级口径上，它更像：

```text
lowering-side callable view/descriptor
```

而不是：

```text
只给某一个 narrow witness 用的一次性辅助变量
```

### 5.2.2 `IrLowerReturnedFunctionValueFamilyInfo` 为什么重要

另一个必须点名的是：

- `IrLowerReturnedFunctionValueFamilyInfo`

它更像是一份 family analysis 结果：

```text
family_info =
  { producer_kind, target_count, target_names,
    is_closure_family, capture_count, payload_slot_count,
    needs_tag_local, ... }
```

这东西的意义不是“分类漂亮一点”，而是：

`caller-side rebuild / dispatch / tag local / capture local 后面全都要依赖这份分析。`

也就是说，当前 returned 主线已经不是“见到 returned call 就直接 lower”。

更准确的是：

```text
先分析 family，再决定 payload/rebuild 策略
```

### 5.2.3 target-set 为什么是现在的核心对象

当前 function 这条线越来越复杂的真正原因之一，是 binding 已经不再永远只有一个 target。

所以源码里现在有整组：

- `function_value_target_sets`
- `ir_lower_find_function_value_target_set(...)`
- `ir_lower_clone_function_value_target_set(...)`
- `ir_lower_function_value_target_sets_merge_union(...)`

lesson 级最稳的理解是：

```text
target-set = 这个 binding 名在当前位置可能指向哪些 callable family
```

这也是为什么：

- branch rebinding
- if/ternary merge
- returned dynamic family
- mixed producer consumer

都在往同一类 helper 上收。

### 5.2.4 伪代码：target-set merge 到底在做什么

最实用的 lesson 级伪代码可以写成：

```text
then_targets = clone(current_targets)
else_targets = clone(current_targets)

lower then branch under then_targets
lower else branch under else_targets

merged_targets[binding] =
    union(then_targets[binding], else_targets[binding])
```

也就是说，现在这条线已经不是：

`binding -> one const target name`

而是在认真维护：

`binding -> possible callable family set`

### 5.3 lesson 级伪代码

最稳的 lowering 骨架可以写成：

```text
build_function_value_descriptor(expr):
    if expr is identifier/static target:
        return static descriptor

    if expr is returned call:
        family = analyze_returned_family(expr)
        locals = prepare_tag_and_capture_locals(family)
        payload = materialize_returned_payload(expr, family, locals)
        return finalize_descriptor(family, payload, locals)

    if expr is closure-backed:
        build closure payload descriptor
```

也就是说，当前 lowering 真正在做的不是“直接吐出 call”，而是先构造：

`descriptor / payload / binding view`

再由后面的 use-site 决定怎么消费。

## 6. 第四段：verifier contract

如果你在追 `fn_make + call_indirect` 到底什么时候才算“真的成了公共 contract”，当前最值得先看：

- `src/ir/ir_verify.inc`

### 6.1 当前最重要的 verifier 入口

这层最值得先记住的是三组 instruction：

1. `IR_INSTR_FN_MAKE`
2. `IR_INSTR_FN_CODE / IR_INSTR_FN_ENV / IR_INSTR_FN_SHAPE`
3. `IR_INSTR_CALL_INDIRECT`

以及它们对应的一组错误族：

- `IR-VERIFY-074..090`

这本身就说明：这条线已经不再只是 lowering 自己知道怎么约定。

现在 verifier 也已经开始明确要求：

```text
fn_make target exists
fn_make shape exists
callee signature matches hidden-env + visible args
call_indirect callee must resolve to fn_make
call_indirect arg_count must match shape
```

### 6.2 lesson 级伪代码

最稳的 contract 伪代码可以写成：

```text
verify_fn_make(instr):
    require result is temp
    require callee exists
    require shape exists
    require callee.params == shape.params + hidden_env
    require callee.return == shape.return

verify_call_indirect(instr):
    require callee operand resolves to fn_make
    require arg_count == callee.shape.parameter_count
```

这就是为什么 lesson 里一直强调：

`callable-object line 现在已经是显式 IR contract，不只是 lowering glue。`

### 6.3 为什么 `IR-VERIFY-074..090` 值得在 lesson 里点名

这组编号本身就很有信息量，因为它说明 callable-object 线已经从：

- lowering 自己知道的约定

变成了：

- verifier 明确承认的一组 IR 不变量

当前最该先记的三段是：

1. `IR-VERIFY-074..080`
   - `fn_make` contract
2. `IR-VERIFY-081 / 089 / 090`
   - `fn_code / fn_env / fn_shape` projection contract
3. `IR-VERIFY-082..088`
   - `call_indirect` contract

所以以后如果某条 function witness 坏了，最推荐的排查顺序不是盲搜，而是先问：

```text
这是 semantic gate 问题？
还是 lowering descriptor/payload 问题？
还是最后撞到了 verifier contract？
```

## 7. 如果你要按 witness 反查源码

如果你手里有一个具体 family，最推荐这样追：

### `apply(add1, 41)` 这种 static specialization

先看：

1. semantic shape gate
2. `ir_try_build_identifier_function_value_descriptor(...)`
3. specialized helper 构造和 direct-call rewrite

### `int f(int)=pick(); return f(41);` 这种 returned noncapturing

先看：

1. `ir_try_build_returned_function_value_descriptor(...)`
2. family info / tag local
3. caller-side descriptor finalize

### `int make(int x)(int){ return closure [x] ...; }` 这种 returned closure

先看：

1. capture slot append
2. returned payload materialization
3. materialized returned closure binding finalize

### `int g(int)=closure [f] ...;` 这种 closure captures callable

先看：

1. semantic local binding resolver
2. closure-backed initializer matching
3. closure payload + callable tag transport

## 8. 当前最稳的统一理解

如果只记一件事，建议记这个：

`当前 function 主线已经不是“一个 feature 一个 hack”，而是 parser/AST 前门、semantic shape gate、lowering descriptor/payload、IR verifier contract 四段同时在长。`

如果你想专门看最后两段怎么落成课程：

1. `returned_closure_transport_lesson.md`
   - 讲 `__retclosure_*` payload view、producer materialization、local/actual/immediate/return-again transport
2. `callable_object_ir_lesson.md`
   - 讲 `shape + fn_make + call_indirect` 和 `IR-VERIFY-074..090` 的显式 IR 合约

这也是为什么最近 lesson 会越拆越细：

不是因为内容散，而是因为：

`实现终于开始真的成体系了。`
