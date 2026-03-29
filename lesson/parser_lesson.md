# Parser Lesson（compiler_lab）

> 目标：按“文件 + 板块”解释 parser 怎么实现，并把“实现思路/伪代码”融合在每一节里，方便后续跟代码同步更新。

---

## 1. `include/parser.h`：对外契约

### 1.1 `ParserError` 怎么实现

`ParserError` 结构体是 parser 的统一错误出口：

```c
typedef struct {
    int line;
    int column;
    char message[128];
} ParserError;
```

含义可以写成：

$$
\mathrm{Err} = (\text{line}, \text{column}, \text{message})
$$

实现策略：

- 解析过程中只要失败，就尽量把最有信息量的错误位置与消息写入这个结构。
- 成功返回前会清空错误（`line=0, column=0, message[0]='\0'`），避免调用者读到陈旧错误。

伪代码：

```text
if parse_success:
    err = zero
else:
    err = best_error
```

### 1.2 API 族的实现定位

`parser.h` 暴露 3 类接口：

1. `parser_parse_translation_unit(...)`：只做语法验证，不产 AST。  
2. `parser_parse_translation_unit_ast(...)`：解析 + 构建 `AstProgram`。  
3. `parser_parse_expression_ast_assignment(...)`：解析独立表达式并产 `AstExpression`。  

还有兼容别名：

- `parser_parse_expression_ast_primary`
- `parser_parse_expression_ast_additive`
- `parser_parse_expression_ast_relational`
- `parser_parse_expression_ast_equality`

实现上它们都转发到 assignment/comma 入口，所以语义是：

$$
\text{ExprAlias}(T) \equiv \text{ExprAssignment}(T)
$$

### 1.3 输入契约

所有入口都依赖 token 流契约：

$$
|T| > 0 \land T_{|T|-1} = \texttt{TOKEN\_EOF}
$$

不满足直接失败（常见报错：`Empty token stream` 或 `Token stream missing EOF terminator`）。

---

## 2. `src/parser/parser.c`：核心状态与基础设施

### 2.1 `Parser` 状态机

`Parser` 结构体核心字段可以抽象为：

$$
S = (i, E, D_e, D_s, L, M)
$$

其中：

- $i$：当前 token 下标（`current`）
- $E$：错误状态（`has_error`, `error_index`, `ParserError*`）
- $D_e$：表达式递归深度
- $D_s$：语句递归深度
- $L$：循环嵌套深度（`loop_depth`）
- $M$：函数级统计/调用元数据缓存

实现上所有 parse 函数都是对这个状态做转移。

伪代码：

```text
while not EOF:
    S <- step(S)
```

### 2.2 游标原语：`peek/advance/check/match/consume`

这是递归下降 parser 的基础组合：

- `peek`: 读当前 token
- `advance`: 前进一个 token
- `check(type)`: 当前 token 是否为给定类型
- `match(type)`: 若匹配则消费
- `consume(type, what)`: 必须匹配，否则报错

`consume` 是“语法约束点”，等价于：

$$
\text{consume}(x)=
\begin{cases}
\text{advance} & \text{if } \text{peek}=x \\
\text{error} & \text{otherwise}
\end{cases}
$$

伪代码：

```text
if check(type):
    advance(); return success
set_error("Expected ...")
return fail
```

### 2.3 错误选择：最远失败优先

`set_error` 用 `error_index` 记录“最远走到哪里才失败”。

规则：

$$
idx_{new} < idx_{best} \Rightarrow \text{忽略}
$$

$$
idx_{new} \ge idx_{best} \Rightarrow \text{覆盖为新错误}
$$

这个策略是为了解决“回溯分支污染错误”的问题（`parser_regression_test.c` 有专门 case）。

### 2.4 递归保护（防爆栈）

表达式和语句分别限制深度（默认 2048）：

- `enter_expression_recursion/leave_expression_recursion`
- `enter_statement_recursion/leave_statement_recursion`

判定：

$$
\text{depth} \ge \text{limit} \Rightarrow \text{fail with recursion-limit diagnostic}
$$

伪代码：

```text
if depth >= limit:
    error("recursion limit")
    return fail
depth++
... parse ...
depth--
```

### 2.5 函数调用元数据缓存（给 semantic 用）

函数体解析时，parser 会记录 call-site 信息：

- 名字数组 `function_called_names`
- 行列 `function_called_lines/columns`
- 参数个数 `function_called_arg_counts`
- callee 形态 `function_called_kinds`

扩容策略：

$$
cap_{new} =
\begin{cases}
8 & cap=0 \\
2\cdot cap & cap>0
\end{cases}
$$

实现要点：

- 每个数组单独 `realloc`。
- 任一步失败都报 OOM 并终止该解析路径。
- 成功后所有权转交到 `AstExternal`。

### 2.6 token slice 工具函数（可调用形态判定）

`token_slice_is_callable_form`、`token_slice_callable_name_token`、`trim_wrapping_parentheses` 用于把 `(...)` 外壳剥掉并判断：

- 直接标识符 `f`
- 调用结果 `(f())`
- 其他表达式 `(f+1)`

这会映射到：

- `AST_CALL_CALLEE_DIRECT_IDENTIFIER`
- `AST_CALL_CALLEE_CALL_RESULT`
- `AST_CALL_CALLEE_NON_IDENTIFIER`

---

## 3. 表达式解析：验证路径 + AST 路径

### 3.1 两条路径如何保持一致

`parser.c` 同时维护：

- `int` 返回的“只验证语法”路径：`parse_*`
- `AstExpression*` 返回的“构树”路径：`parse_expression_ast_*`

两条路径共享同一优先级结构，形式上：

$$
\mathcal{G}_{check} \cong \mathcal{G}_{ast}
$$

### 3.2 优先级分层（低到高）

$$
E_{comma}
\rightarrow E_{assign}
\rightarrow E_{cond}
\rightarrow E_{lor}
\rightarrow E_{land}
\rightarrow E_{bor}
\rightarrow E_{bxor}
\rightarrow E_{band}
\rightarrow E_{eq}
\rightarrow E_{rel}
\rightarrow E_{shift}
\rightarrow E_{add}
\rightarrow E_{mul}
\rightarrow E_{unary}
\rightarrow E_{postfix}
\rightarrow E_{primary}
$$

左结合层（如 `+,-,*,/,...`）实现模式：

```text
left = parse_lower()
while match(op):
    right = parse_lower()
    left = node(op, left, right)
return left
```

对应结合律：

$$
a-b-c = (a-b)-c
$$

### 3.3 条件与赋值：右结合

条件表达式（`?:`）和赋值表达式采用右递归：

$$
a ? b : c ? d : e = a ? b : (c ? d : e)
$$

$$
a=b=c = a=(b=c)
$$

赋值在 AST 路径的实现目前约束为“左侧必须是标识符表达式”：

```c
if (!(left->kind == AST_EXPR_IDENTIFIER && match_assignment_operator(...)))
    return left;
```

这属于当前项目子集策略，和完整 C lvalue 语义不同。

### 3.4 前后缀 `++/--` 的 lvalue 检查

非 AST 路径通过 token slice 判定“是否是（可带括号的）标识符形式”；AST 路径通过 `ast_expression_is_identifier_lvalue` 检查。

约束可写成：

$$
\text{operand} \in \mathcal{L}_{id} \Rightarrow \text{allow } ++/--
$$

否则报错：`Expected identifier lvalue ...`。

### 3.5 函数调用的语法与元数据

后缀解析里遇到 `(` 时：

1. 先验证当前表达式可调用。
2. 解析参数列表（每个参数是 assignment-expression）。
3. 记录调用点信息。
4. 在 AST 路径构建 `AST_EXPR_CALL`。

伪代码：

```text
expr = parse_primary()
while next is '(' or postfix-op:
    if '(':
        args = parse_arg_list_as_assignment_expr()
        record_call_metadata()
        expr = Call(expr, args)
    elif ++/--:
        require identifier-lvalue
        expr = Postfix(op, expr)
```

---

## 4. 语句解析与控制流方程

`parse_statement_with_flow` 同时产出语法结果与 3 个流属性：

$$
\mathrm{Flow}(stmt) = (R,F,B)
$$

- $R$: `guaranteed_return`
- $F$: `may_fallthrough`
- $B$: `may_break`

### 4.1 复合语句 `{...}`

`parse_compound_statement_with_flow` 顺序扫描子语句，做流传播。核心思想是“前面路径已不再 fallthrough 时，后续语句不再影响 return 保证”。

伪代码：

```text
block_F = 1
for child in block:
    flow = Flow(child)
    if block_F and flow.B: block_B = 1
    if block_F:
        if flow.R: block_R=1; block_F=0
        elif not flow.F: block_R=0; block_F=0
```

### 4.2 `if` 合成规则

有 `else`：

$$
R = R_t \land R_e
$$

$$
F = F_t \lor F_e
$$

$$
B = B_t \lor B_e
$$

无 `else`：

$$
F = 1
$$

因为条件为假时会直接落到 if 后面。

### 4.3 `while` 与 `for`（近似分析）

通过 `expression_slice_is_constant_true` 识别“条件是单个非零字面量”这一类常真条件。

对于常真循环：

- 若 body `may_break=0`，则 `may_fallthrough=0`。
- 若 body `may_break=1`，则 `may_fallthrough=1`。

可写成：

$$
F =
\begin{cases}
1, & \neg C_{true} \\
1, & C_{true} \land B_{body} \\
0, & C_{true} \land \neg B_{body}
\end{cases}
$$

`R` 在常真且不可 break 时继承 body 的 `R`；其他情况置 0。

### 4.4 `break/continue` 的合法性

利用 `loop_depth` 约束：

$$
loop\_depth = 0 \Rightarrow \text{reject break/continue}
$$

这在 `parse_break_statement` 与 `parse_continue_statement` 中直接检查。

### 4.5 语句 AST 的实现方案

控制流语句会把表达式片段切成 token slice，再调用 `parser_parse_expression_ast_assignment` 生成表达式 AST，挂到 `AstStatement` 的 `expressions[]`。

这样语句与表达式 AST 始终共享同一表达式语法实现。

---

## 5. 顶层 external：函数优先，失败回退声明

### 5.1 `parse_function_external` 的策略

流程：

1. 解析 `int ident ( parameter-list )`
2. 若后跟 `;`，这是函数声明（prototype）
3. 若后跟 `{`，这是函数定义
4. 其他 token 直接报错

参数规则：

- prototype 允许 unnamed parameter（如 `int f(int);`）
- definition 不允许 unnamed parameter

约束：

$$
\text{is\_definition} \Rightarrow \neg \text{has\_unnamed\_parameter}
$$

### 5.2 顶层主循环（`parser_parse_translation_unit_ast`）

外层策略是“先函数再声明”：

```text
while not EOF:
    start = current
    if parse_function_external succeeds:
        append function external
    else:
        current = start
        if parse_declaration fails:
            fail
```

这是处理 C 顶层二义入口的常见 deterministic 回退法。

### 5.3 `parse_declaration` 实现点

支持：

- `int a;`
- `int a=1;`
- `int a=1, b, c=2;`

并把每个 declarator 展开为一个 `AST_EXTERNAL_DECLARATION` external，`has_initializer` 单独记录。

---

## 6. 公共入口函数的关系

### 6.1 `parser_parse_translation_unit`

- 仅语法检查
- 不构建 `AstProgram`
- 更轻量，适合快速路径或旧链路

### 6.2 `parser_parse_translation_unit_ast`

- 语法 + AST + 函数元数据
- 失败时会清理 `out_program`，避免半成品 AST 残留

### 6.3 `parser_parse_expression_ast_assignment`

- 入口是 comma-level 解析
- 结尾强制 `is_at_end`

约束：

$$
\text{parse\_expr\_ast}(T)=\text{success} \Rightarrow \text{current}=EOF
$$

---

## 7. `tests/parser/*`：测试代码怎么写（含模板）

### 7.1 `tests/parser/test.c`

这是手工样例输入文件，用于 `parser_test` 读取并展示 token/AST。

实现意义：

- 覆盖 if/for/while
- 覆盖表达式优先级与一元/后缀/调用/复合赋值

### 7.2 `tests/parser/parser_test.c`

CLI 冒烟测试程序：

- 读文件
- lexer tokenize
- parser AST
- semantic analyze
- 可选 `--dump-tokens` / `--dump-ast`

这是“端到端链路”验证，不是精细断言回归。

### 7.3 `tests/parser/parser_legacy_link_test.c`

最小旧 API 链接冒烟：只测 `parser_parse_translation_unit` 能正常链接并跑通。

### 7.4 `tests/parser/parser_regression_test.c`（重点）

这个文件是 parser 行为契约主集合。组织方式：

- 小粒度 `test_xxx`
- 通用 helper（例如 `expect_translation_unit_parse_failure`）
- 表驱动矩阵（callable entry cases）
- `main()` 逐条串联，任何失败立即 `return 1`

### 7.5 基础测试块（C 模板）

模板 A：预期失败 + 诊断子串断言

```c
static int test_rejects_case(void) {
    TokenArray tokens;
    ParserError err;
    const char *source = "int main(){1(2);}\n";

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) return 0;

    if (parser_parse_translation_unit(&tokens, &err)) {
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (strstr(err.message, "callable") == NULL) {
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}
```

模板 B：表达式 AST 结构断言

```c
static int test_expr_tree_shape(void) {
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize("a+b*c\n", &tokens)) return 0;
    if (!parser_parse_expression_ast_assignment(&tokens, &expr, &err)) {
        lexer_free_tokens(&tokens);
        return 0;
    }

    /* 断言根为 +，右子为 * */
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

模板 C：表驱动矩阵

```c
typedef struct {
    const char *name;
    const char *source;
    int expect_success;
    const char *msg_a;
    const char *msg_b;
} Case;

for (i = 0; i < n; ++i) {
    if (!run_case(&cases[i])) return 0;
}
```

### 7.6 注册新测试到 `main`

当前风格是显式串联：

```c
if (!test_xxx()) return 1;
```

新增测试时保持这个顺序式结构，便于第一时间定位失败点。

### 7.7 Parser 改动时的测试策略

当你修改 parser，建议按这个顺序补/跑：

1. 先写一个最小失败 case（验证旧 bug）
2. 再写一个相邻成功 case（防误杀）
3. 若涉及优先级/结合律，补 AST 形状断言
4. 最后补回归矩阵 case（若属于一类输入）

---

## 8. 快速维护指南（代码还在变化时）

如果 parser 后续继续扩展，推荐固定这个落地顺序：

1. 先改 grammar 函数（`parse_*` 与 `parse_expression_ast_*` 对齐）
2. 再改 flow 合成规则（若影响 `if/while/for`）
3. 再改 `AstExternal/AstStatement` 元数据灌入
4. 最后补 `parser_regression_test.c`：失败 + 成功 + AST 结构 + 边界深度

你可以把每次改动看成验证这个不变式：

$$
\text{Grammar correctness} \land \text{AST consistency} \land \text{Diagnostic stability}
$$

满足这三项，parser 演进会很稳。
