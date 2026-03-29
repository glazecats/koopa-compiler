# Semantic Lesson（compiler_lab）

> 目标：解释“当前 semantic 阶段到底做了什么、怎么做、为什么这么做”，并对齐你现在看到的 S2 迁移日志。

---

## 1. 文件定位与职责

- 接口定义：`include/semantic.h`
- 实现文件：`src/semantic/semantic.c`
- 关键回归：`tests/semantic/semantic_regression_test.c`

semantic 输入是 `AstProgram`，输出是 `success/fail + SemanticError`：

$$
\mathrm{Analyze}: \mathrm{AstProgram} \rightarrow \{0,1\} \times \mathrm{SemanticError}
$$

---

## 2. `semantic.h` 接口契约

`SemanticError`:

- `line`
- `column`
- `message[512]`

公开入口只有一个：

```c
int semantic_analyze_program(const AstProgram *program, SemanticError *error);
```

语义：

- 返回 `1`：通过
- 返回 `0`：失败，并在 `error` 里给诊断

伪代码：

```text
if analyze_ok:
    return 1
else:
    fill error
    return 0
```

---

## 3. `semantic.c` 总体结构

当前 semantic 可以分成 4 个层次：

1. AST 访问与调用点收集（visitor 风格后序遍历）
2. 调用视图构建（AST-primary / metadata-fallback）
3. 函数调用规则检查（`SEMA-CALL-001..006`）
4. 顶层符号一致性检查（重定义、冲突、参数数一致性等）

数据流：

$$
\text{AST} \xrightarrow{\text{collect call view}} \text{Callable Checks}
\xrightarrow{} \text{Top-level Checks} \xrightarrow{} \text{Result}
$$

---

## 4. S2 迁移策略是怎么实现的

文件开头有迁移策略枚举：

- `SEMA_MIGRATION_STRICT_PARITY`
- `SEMA_MIGRATION_AST_PRIMARY_ONLY`

当前常量：

```c
static const SemanticMigrationStrategy kSemanticMigrationStrategy =
    SEMA_MIGRATION_AST_PRIMARY_ONLY;
```

含义：

- `STRICT_PARITY`：AST 收集结果必须和 parser metadata 对齐，否则直接 `SEMA-INT-00x`
- `AST_PRIMARY_ONLY`：AST 作为语义真源，parity 只做可选影子校验

对应函数：`semantic_strategy_requires_callable_parity()`

---

## 5. 可复用 AST 遍历骨架（你前面问的 visitor）

现在 semantic 里已经有一套可复用后序遍历骨架：

- `walk_expression_postorder(const AstExpression*, ExpressionPostVisitor, void*)`
- `walk_statement_postorder(const AstStatement*, ExpressionPostVisitor, void*)`

访问策略是“先子节点，后当前节点”（postorder）：

$$
\mathrm{visit}(n)=\mathrm{visit}(children(n));\ \mathrm{apply}(n)
$$

伪代码：

```text
walk_expr(expr):
    for each child in expr:
        walk_expr(child)
    visitor(expr)
```

这套骨架让语义逻辑和“递归遍历细节”解耦。

---

## 6. 调用点收集怎么做

### 6.1 基础容器

`CollectedCall` 记录每个调用点：

- `name`（可能为空）
- `line/column`
- `arg_count`
- `kind`（`DIRECT_IDENTIFIER` / `CALL_RESULT` / `NON_IDENTIFIER`）

`CollectedCallList` 是动态数组，扩容规则：

$$
cap_{new}=\begin{cases}
8, & cap=0 \\
2\cdot cap, & cap>0
\end{cases}
$$

### 6.2 callee 分类

`classify_ast_call_callee`：

1. 先去掉外层 `AST_EXPR_PAREN`
2. 若底层是 `IDENTIFIER` => `DIRECT_IDENTIFIER`
3. 若底层是 `CALL` => `CALL_RESULT`
4. 否则 `NON_IDENTIFIER`

这一步和 parser 里的 callee kind 语义对齐。

### 6.3 实际收集函数

- `collect_call_expression_postorder`：只在 `expr->kind == AST_EXPR_CALL` 时 append
- `collect_calls_from_statement`：用 statement postorder + expression visitor 收集整函数调用

---

## 7. 两种调用视图（S2 核心）

semantic 内部把调用输入显式区分为：

- `CALL_VIEW_SOURCE_AST_PRIMARY`
- `CALL_VIEW_SOURCE_METADATA_FALLBACK`

由 `semantic_build_function_call_view` 决定来源。

### 7.1 AST-primary 分支

如果 `func->function_body` 存在：

1. 从 statement AST 收集调用
2. 若策略要求 parity，再和 metadata 对比
3. 标记 `source = AST_PRIMARY`

### 7.2 metadata fallback 分支

如果 `function_body` 不存在：

- 若是函数定义：直接报 `SEMA-INT-005`
- 若是声明类路径：从 `called_function_*` 组装 fallback view

并标记 `source = METADATA_FALLBACK`。

---

## 8. `SEMA-INT-*` 内部一致性护栏

当前内部错误码对应关系：

- `SEMA-INT-001`：AST 调用收集失败
- `SEMA-INT-002`：AST 调用数与 metadata 数量不一致
- `SEMA-INT-003`：AST 与 metadata 明细不一致（name/line/col/arg_count/kind）
- `SEMA-INT-004`：构建 metadata fallback 失败
- `SEMA-INT-005`：函数定义缺失 statement AST body（迁移阶段禁止）
- `SEMA-INT-006`：函数定义 callable 检查未使用 AST-primary 视图

`SEMA-INT-006` 是“定义路径必须 AST-primary”的硬门：

$$
is\_function\_definition \Rightarrow call\_view.source = AST\_PRIMARY
$$

---

## 9. `SEMA-CALL-001..006` 规则怎么判

主函数：`semantic_check_function_callable_rules`

对每个调用点做检查，核心状态：

- `found_decl`
- `found_matching_param_count`
- `found_non_function_symbol`
- `has_later_function_decl`

### 9.1 非标识符调用先拦截

如果 `called_name` 为空：

- `kind == CALL_RESULT` => `SEMA-CALL-005`
- 其他（通常 `NON_IDENTIFIER`）=> `SEMA-CALL-006`

### 9.2 可命名调用的符号检查

扫描当前函数之前（含当前）的 external：

1. 若同名函数声明存在：
   - 参数个数匹配 => 通过
   - 不匹配 => `SEMA-CALL-004`
2. 若同名但不是函数 => `SEMA-CALL-003`
3. 若之前没声明，再看后面有没有同名函数：
   - 有 => `SEMA-CALL-002`（先调用后声明）
   - 无 => `SEMA-CALL-001`（未声明函数）

可写成判定树：

$$
\neg decl \land later\_decl \Rightarrow CALL\text{-}002
$$

$$
\neg decl \land \neg later\_decl \Rightarrow CALL\text{-}001
$$

$$
decl \land arg\_mismatch \Rightarrow CALL\text{-}004
$$

$$
\neg decl \land non\_function\_symbol \Rightarrow CALL\text{-}003
$$

---

## 10. 顶层语义检查（不只 callable）

`semantic_analyze_program` 还做这些：

1. `program == NULL` 防御
2. 函数定义必须 `returns_on_all_paths == 1`
3. 顶层同名冲突检查：
   - 函数-函数：参数数必须一致；不能双定义
   - 函数-变量：报冲突
4. unnamed external 允许（AST 合约保留）

函数声明一致性约束：

$$
same\_name \land both\_function \Rightarrow parameter\_count\_equal
$$

---

## 11. 诊断字符串实现细节

### 11.1 `format_callee_preview`

长标识符会中间截断，防止 message 太长：

- 保留头 `24` + 尾 `16`
- 中间用 `...`

这对应你日志里“防 message-tail truncation”的持续加固方向。

### 11.2 稳定 payload

`SEMA-CALL-*` 文本中带稳定键值片段：

- `callee=...`
- `expected=...; got=...`
- `decl_line=...; decl_col=...`
- `callee_kind=...`

便于回归测试和 IDE 脚本匹配。

---

## 12. 测试代码怎么写（semantic）

文件：`tests/semantic/semantic_regression_test.c`

### 12.1 已有测试结构

- `parse_source_to_ast(...)`：共享 fixture
- `run_callable_diag_case(...)`：失败矩阵（期望 code + 片段 + 位置）
- `run_callable_pass_case(...)`：通过矩阵

### 12.2 推荐基础块模板

```c
static int test_semantic_rejects_xxx(void) {
    const char *source = "int main(){return foo(1);}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError perr;
    SemanticError serr;

    if (!parse_source_to_ast(source, &tokens, &program, &perr)) return 0;
    if (semantic_analyze_program(&program, &serr)) return 0;
    if (strstr(serr.message, "SEMA-CALL-001") == NULL) return 0;

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}
```

### 12.3 迁移期专用测试建议

1. tamper metadata 后 AST-primary 仍正确
2. 函数定义缺 body 时必须 `SEMA-INT-005`
3. `STRICT_PARITY` 下构造一例 mismatch，锁定 `SEMA-INT-002/003`

---

## 13. 当前 semantic 的一句话总结

当前实现已经从“单纯 metadata 驱动”升级为“AST-primary + 迁移护栏”：

$$
\text{Semantic Today}=
\text{AST-primary callable analysis}
+\text{migration parity gates}
+\text{stable diagnostic contracts}
$$

这就是你在 `NEXT_STEPS.md` 看到的 S2-boot 到 S2-6 的本质落地。
