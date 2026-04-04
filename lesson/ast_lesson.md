# AST Lesson（compiler_lab）

> 目标：回答“AST 现在到底实现了什么”，并按文件/板块解释实现方案、内存所有权、与 parser/semantic 的连接方式，以及测试应该怎么写。

## 导学

这份讲义建议按“模型 -> 生命周期 -> 消费关系”的顺序阅读：

1. 先看 AST 结构：Program/External/Statement/Expression 的职责。
2. 再看所有权与释放：成功路径和失败路径如何收口。
3. 最后看下游消费：semantic 规则和测试如何依赖这些字段。

学完你应该能：

1. 能解释 `AstProgram` / `AstExternal` / `AstStatement` / `AstExpression` 的分工。
2. 能说清 AST 生命周期（成功路径与失败路径）如何避免泄漏。
3. 能把一个语义规则映射回它依赖的 AST 字段。

---

## 1. 文件地图与职责

- 公开接口与数据结构：`include/ast.h`
- 生命周期 helper 的实例化入口：`include/ast_internal.h`
- 共享生命周期模板：`include/ast_lifecycle_template.h`
- 对外 API 包装：`src/ast/ast.c`
- 主要行为测试来源：
  - `tests/parser/parser_regression_test.c`（AST 结构与元数据主回归）
  - `tests/semantic/semantic_regression_test.c`（AST 合约 + 下游消费）
  - `tests/parser/parser_test.c`（端到端 dump 验证）

可抽象成映射：

$$
\text{Token Stream} \xrightarrow{Parser} \text{AST} \xrightarrow{Semantic} \text{Rules/Diagnostics}
$$

---

## 2. AST 现在实现了什么（总览）

当前 AST 支持 3 层：

1. 顶层外部符号层（`AstProgram` / `AstExternal`）  
2. 语句层（`AstStatement`）  
3. 表达式层（`AstExpression`）

并且实现了：

- 表达式节点：标识符、数字、括号、一元、后缀、调用、二元、三目
- 语句节点：复合块、声明语句、表达式语句、if/while/for/return/break/continue
- 函数外部契约：参数信息、是否定义、`function_body`、`return_statement_count`
- 调用检查所需信息主要由语义阶段遍历 AST 实时计算
- 全量递归释放（避免泄漏）
- Program 级动态扩容 append

---

## 3. `ast.h`：数据模型怎么设计

### 3.1 顶层种类枚举

`AstExternalKind`：

- `AST_EXTERNAL_FUNCTION`
- `AST_EXTERNAL_DECLARATION`

这是 top-level 的二分：

$$
External \in \{Function, Declaration\}
$$

### 3.2 表达式节点 `AstExpression`

`AstExpression` 是 tagged union：

- `kind` 决定 union 哪个分支有效
- `line/column` 记录定位

可以写成代数数据类型：

$$
E = Id(name) \mid Num(v) \mid Paren(E) \mid Unary(op,E) \mid Postfix(op,E) \mid Call(E,[E]) \mid Binary(op,E,E) \mid Ternary(E,E,E)
$$

实现方案（解析时）就是：

```text
parse at precedence level L
if operator found:
    create node(kind=unary/postfix/binary/ternary/call)
    wire children pointers
return root expression pointer
```

### 3.3 语句节点 `AstStatement`

`AstStatement` 结构有两个关键数组：

- `expressions[]`：这个语句挂载的表达式子树
- `children[]`：子语句（复合块、if then/else、循环体）

此外有“槽位索引”字段：

- `has_primary_expression / primary_expression_index`
- `has_condition_expression / condition_expression_index`
- `has_for_init_expression / for_init_expression_index`
- `has_for_step_expression / for_step_expression_index`

这允许统一表达：

$$
Stmt = (kind, ExprList, ChildList, SlotMetadata)
$$

比如 `for(init; cond; step) body`：

- `ExprList` 常有 3 项
- 用 3 个 index 明确哪一个是 init/cond/step

### 3.4 外部节点 `AstExternal`

`AstExternal` 当前是“精简外部契约”：主要保存顶层声明/定义的必要结构信息：

- 名字与定位：`name/name_length/line/column`
- 声明信息：`has_initializer` + `declaration_initializer`（顶层声明 initializer 的表达式 AST）
- 函数信息：`parameter_count`, `is_function_definition`, `function_body`
- 控制流统计：`return_statement_count`（仅保留 return 计数）
- 不再在 external 上暴露 `called_function_*` 调用元数据数组

semantic 的 return-flow、callable、scope 规则现在直接基于 `function_body`（语句/表达式 AST）进行分析，不依赖 external 附加调用元数据。


### 3.5 Program 容器 `AstProgram`

`AstProgram` 是动态数组：

$$
Program = (externals[], count, capacity)
$$

append 时扩容策略在内部实现为倍增（初始 16）。

---

## 4. `ast_internal.h` + `ast_lifecycle_template.h`：核心实现细节

当前 AST 生命周期逻辑不是手写两份，而是“一个共享模板 + 两处实例化”：

1. `include/ast_internal.h` 通过宏映射实例化 AST 主实现
2. `src/parser/parser_ast_compat.inc` 用同一模板实例化 parser 兼容层

所以你在源码里仍然会看到这些 helper 名字：

- `ast_expression_free_internal`
- `ast_statement_free_internal`
- `ast_program_clear_storage`
- `ast_program_append_external`

但它们的函数体来自同一份 `ast_lifecycle_template.h`。

### 4.1 递归释放表达式树

`ast_expression_free_internal` 根据 `expr->kind` 递归释放子节点。

递归关系可以写成：

$$
Free(E)=
\begin{cases}
free(name)+free(E), & E=Id(name) \\
free(E_1)+free(E_2)+free(E), & E=Binary(op,E_1,E_2) \\
... & \text{其它分支同理}
\end{cases}
$$

关键点：`AST_EXPR_CALL` 先 free `callee`，再循环 free 参数数组中的每一项，最后 free 参数数组本身。

### 4.2 递归释放语句树

`ast_statement_free_internal`：

1. 递归释放 `children[]`
2. 递归释放 `expressions[]`
3. 释放数组本体
4. 释放当前 `stmt`

伪代码：

```text
for child in stmt.children: free_stmt(child)
for expr in stmt.expressions: free_expr(expr)
free(stmt.children)
free(stmt.expressions)
free(stmt)
```

### 4.3 Program 清空与重置

`ast_program_clear_storage` 会：

- 遍历每个 external
- 释放 `name`
- 释放 `parameter_names`、`declaration_initializer` 与 `function_body`，并清理 external 数组本体
- 最后释放 `externals` 并把 `count/capacity` 归零

这保证不变量：

$$
\text{clear\_storage}(program) \Rightarrow count=0, capacity=0, externals=NULL
$$

### 4.4 append external 的实现

`ast_program_append_external`：

- `count==capacity` 时扩容
- 扩容前会先做 `size_t` 溢出保护，避免极大输入下的容量乘法回绕
- 初始化一个 `AstExternal external` 临时对象（所有字段先置默认值）
- 若 `name_token` 有效则复制字符串
- 写入 `program->externals[program->count++]`

扩容公式：

$$
cap_{new}=\begin{cases}16,&cap=0\\2\cdot cap,&cap>0\end{cases}
$$

---

## 5. `ast.c`：公开 API 做了什么

`src/ast/ast.c` 本身很薄，主要是对内部 helper 的公开包装：

- `ast_program_init`
- `ast_program_free`
- `ast_program_add_external`
- `ast_expression_free`
- `ast_statement_free`
- `ast_external_kind_name / ast_expression_kind_name / ast_statement_kind_name`

实现策略是：

- 把“复杂逻辑”放进 internal helper
- 对外暴露稳定、简洁的 API

这样 parser/semantic/test 只用公共接口即可。

---

## 6. 所有权转移：AST 与 parser 如何配合

你前面问到的“成功即转移所有权”，在 AST 上体现为：

$$
Owner: Parser \rightarrow AST
$$

### 6.1 成功路径

- parser 在解析时分配节点/数组
- 一旦 external 或 statement 成功挂到 `AstProgram`
- 后续由 `ast_program_free` 负责释放

### 6.2 失败路径

- 若中途失败且未挂入 Program
- parser 必须立即释放临时对象
- `ast_program_clear_storage` 确保 Program 不留脏状态

对应回归：`test_ast_failure_clears_previous_program`。

伪代码：

```text
tmp = build_ast_piece()
if success_append_to_program:
    transfer_ownership(tmp -> program)
else:
    free(tmp)
```

---

## 7. 为什么当前 AST 字段足够 semantic 使用（semantic 视角）

`semantic.c` 对 AST 的消费说明了字段设计目的：

1. `function_body` + 表达式/语句子树
用于 return-flow、callable、scope 三类语义规则的 AST 直接分析。

2. `parameter_count` / `parameter_names` / `kind` / `name`  
用于函数声明一致性、重定义冲突、函数/变量同名冲突检查。

3. `return_statement_count`  
作为轻量统计辅助（例如 parser/测试侧可快速断言函数体最基本结构）。

4. `has_initializer` + `declaration_initializer`（顶层声明 external）  
用于顶层声明 initializer 的 callable/scope 规则，以及区分“声明”与“带初始化的定义”。

语义输入可简写为：

$$
SemanticInput = \{function\_body,\ params,\ top\_level\ externals\}
$$




---

## 8. 测试告诉我们 AST 已经“落地”的能力

### 8.1 `tests/parser/parser_regression_test.c`

已覆盖的 AST 能力（部分）：

- 顶层 external 收集与顺序
- 函数参数计数、是否 definition
- `function_body` 结构正确性
- 语句槽位索引（while/for/if/return）
- unnamed 参数：prototype 允许、definition 拒绝
- `return_statement_count` 计数（其余控制流统计已下沉为语义遍历阶段计算）
- 顶层声明 `has_initializer` 与 `declaration_initializer`
- 多 declarator 拆分成多个 external
- 失败后 Program 清空
- 表达式 AST 形状：优先级、结合性、调用链、赋值、条件、逗号等

### 8.2 `tests/semantic/semantic_regression_test.c`

已覆盖 AST 合约与消费：

- `ast_program_add_external(..., NULL)` 的 unnamed external 合约
- 函数定义必须携带 `function_body` 的语义契约检查
- callable/scope/return-flow 规则均由 AST 直接驱动

这说明当前语义层已以 AST 结构为主输入，parser external 字段保持精简并聚焦契约信息。

---

## 9. 你可以怎么写 AST 测试（模板）

### 9.1 基础块模板：表达式树形断言

```c
static int test_ast_expr_shape(void) {
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize("a+b*c\n", &tokens)) return 0;
    if (!parser_parse_expression_ast_assignment(&tokens, &expr, &err)) {
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_PLUS) {
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}
```

### 9.2 基础块模板：Program metadata 断言

```c
static int test_ast_function_metadata(void) {
    const char *src = "int f(int a){return a;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(src, &tokens)) return 0;
    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 || program.externals[0].kind != AST_EXTERNAL_FUNCTION ||
        program.externals[0].parameter_count != 1 ||
        program.externals[0].return_statement_count != 1) {
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}
```

### 9.3 基础块模板：失败清理合约

```c
static int test_ast_failure_clears_program_contract(void) {
    AstProgram program;
    ParserError err;
    Token bad_tokens[1];
    TokenArray bad_stream;

    ast_program_init(&program);
    /* 先构造一次成功 Program（略） */

    bad_stream.data = bad_tokens;
    bad_stream.size = 1;          /* 缺 EOF */
    bad_stream.capacity = 1;

    if (parser_parse_translation_unit_ast(&bad_stream, &program, &err)) return 0;
    if (program.count != 0) return 0;

    ast_program_free(&program);
    return 1;
}
```

---

## 10. 课堂易错点（很重要）

当前 AST 生命周期实现更准确地说是“同一模板的两处实例化”：

1. AST 主实现：`include/ast_internal.h` + `src/ast/ast.c`
2. parser 兼容层：`src/parser/parser_ast_compat.inc`

这意味着当你新增 `AstExpression`/`AstStatement`/`AstExternal` 字段时，至少要同步检查：

1. `ast_lifecycle_template.h` 是否已经覆盖了新的所有权成员
2. `include/ast_internal.h` 与 `src/parser/parser_ast_compat.inc` 的宏映射是否仍然对应正确
3. append 初始化路径（默认值、所有权转移）是否同步更新

这一条在本轮代码里已经有一个具体例子：`AstExternal.declaration_initializer` 新增后，模板里的 clear/free 与默认初始化都必须一起更新。

否则最常见的问题是“主链路正常，但 legacy-link 路径泄漏或崩溃”。

---

## 11. 一句话结论

当前 AST 实现已经不是“只有语法树形”，而是：

$$
\text{AST} = \text{Structure} + \text{Minimal external contract} + \text{Ownership contract}
$$

它能支撑 parser 回归、semantic 规则、以及后续继续扩展（比如更多类型系统信息或更丰富语句节点）。

---

## 可选思考题

1. 为什么 `AstStatement` 里既有 `expressions[]`，又要有 `*_expression_index` 这类槽位索引？
2. 如果给 `AstExternal` 增加一个新字段，哪些释放/初始化路径必须同步修改？
