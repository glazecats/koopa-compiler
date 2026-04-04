# Parser Lesson（compiler_lab）

> 目标：按“文件 + 板块”解释 parser 怎么实现，并把“实现思路/伪代码”融合在每一节里，形成可讲解、可跟读的讲义。

## 导学

这份讲义建议按“从外到内”的顺序学习：

1. 先掌握接口契约与状态机原语（peek/match/consume）。
2. 再理解表达式、语句、顶层 external 的递归下降主线。
3. 最后结合回归测试，建立“实现-行为-断言”的对应关系。

学完你应该能：

1. 能口述 parser 的递归下降层次和结合律处理方式。
2. 能解释为什么函数优先解析、失败再回退声明。
3. 能把一个 parser 回归 case 对应到具体解析阶段。

---

## 1. 文件布局与对外契约

当前 parser 已拆分为“聚合入口 + 功能分片”：

- 聚合入口：`src/parser/parser.c`
- AST 兼容层：`src/parser/parser_ast_compat.inc`
- 表达式核心：`src/parser/parser_core_expr.inc`
- 语句/声明/TU：`src/parser/parser_stmt_decl_tu.inc`

`parser.c` 现在不只是“放一个 `Parser` 再 include 进去”。

它当前实际负责 4 件事：

1. 定义共享状态结构：`Parser`、`ParserLocalControl`
2. 收口跨分片共享 typedef：例如 `ParsedFunctionSignature`
3. 集中声明跨分片 helper 原型：游标原语、AST helper、statement/decl/TU 入口
4. 最后聚合 `parser_ast_compat.inc`、`parser_core_expr.inc`、`parser_stmt_decl_tu.inc`

所以现在更准确的理解方式是：

$$
\texttt{parser.c} = \text{shared types} + \text{forward declarations} + \text{aggregator}
$$

## 2. `include/parser.h`：对外契约

### 2.1 `ParserError` 怎么实现

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

### 2.2 API 族的实现定位

`parser.h` 暴露 3 类接口：

1. `parser_parse_translation_unit(...)`：只做语法验证，不产 AST。  
2. `parser_parse_translation_unit_ast(...)`：解析 + 构建 `AstProgram`。  
3. `parser_parse_expression_ast_assignment(...)`：解析独立表达式并产 `AstExpression`。  

当前公开表达式入口只保留这一条 canonical API。虽然函数名里带 `assignment`，但实现上它会从 comma-level 开始解析，覆盖完整表达式层级，所以语义更接近：

$$
\text{ExprEntry}(T)=\text{parse\_comma}(T)
$$

### 2.3 输入契约

所有入口都依赖 token 流契约：

$$
|T| > 0 \land T_{|T|-1} = \texttt{TOKEN\_EOF}
$$

不满足直接失败（常见报错：`Empty token stream` 或 `Token stream missing EOF terminator`）。

---

## 3. `src/parser/parser.c` + `*.inc`：核心状态与基础设施

### 3.1 `Parser` 状态机

`Parser` 结构体核心字段可以抽象为：

$$
S = (i, E, D_e, D_s, L)
$$

其中：

- $i$：当前 token 下标（`current`）
- $E$：错误状态（`has_error`, `error_index`, `ParserError*`）
- $D_e$：表达式递归深度
- $D_s$：语句递归深度
- $L$：循环嵌套深度（`loop_depth`）

实现上所有 parse 函数都是对这个状态做转移。

伪代码：

```text
while not EOF:
    S <- step(S)
```

### 3.2 游标原语：`peek/advance/check/match/consume`

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

### 3.3 错误选择：最远失败优先

`set_error` 用 `error_index` 记录“最远走到哪里才失败”。

规则：

$$
idx_{new} < idx_{best} \Rightarrow \text{忽略}
$$

$$
idx_{new} \ge idx_{best} \Rightarrow \text{覆盖为新错误}
$$

这个策略是为了解决“回溯分支污染错误”的问题（`parser_regression_test.c` 有专门 case）。

### 3.4 递归保护（防爆栈）

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

### 3.5 公共入口收敛：`parser_prepare`

3 个公开入口现在都会先走 `parser_prepare(...)`，统一做：

1. token stream 合法性检查
2. `Parser` 状态清零
3. `ParserError` 清空

它锁定的前置条件是：

$$
|T|>0 \land T_{|T|-1}=\texttt{TOKEN\_EOF}
$$

### 3.5.1 共享签名结构也已经上收：`ParsedFunctionSignature`

最近这版 parser 还有一个源码组织层面的变化：函数 external 解析共用的
`ParsedFunctionSignature` 已经从 `parser_stmt_decl_tu.inc` 上收到了 `parser.c`。

这个结构当前承载的是：

- `name_tok`
- `parameter_count`
- `parameter_names`
- `parameter_name_lines`
- `parameter_name_columns`
- `has_unnamed_parameter`

它之所以上收到聚合入口，是因为下面这些 helper 都会共享它：

- `parse_function_external_signature`
- `parse_function_external_definition_body`
- `parse_function_external`

这说明 parser 现在的分片边界更清晰了：

- `.inc` 主要放函数体
- 跨分片共享的状态结构和前置声明尽量收口到 `parser.c`

### 3.6 token slice 工具函数（可调用形态判定）

当前 parser 里和“括号包裹表达式”相关的关键 helper 是：

- `token_slice_has_balanced_parens`
- `trim_wrapping_parentheses`
- `token_slice_is_parenthesized_identifier_form`
- `parse_primary_with_flags`

它们负责两件事：

1. 判断一个 token slice 去掉外层括号后是不是“标识符 lvalue 形式”
2. 在非 AST postfix 路径里给 primary 打上两个语法标志：
   - `is_identifier_lvalue`
   - `is_callable_target`

当前语法策略是：

- 直接标识符是 callable target
- 任意被括号包起来的完整表达式也可以作为 postfix call callee
- 更细的 callee 分类（direct identifier / call result / non-identifier）已经下放到 semantic 的 `classify_ast_call_callee`

### 3.7 `parser_ast_compat.inc`（重要兼容边界）

parser 现在通过本地兼容层管理 AST 生命周期与 external append：

- `parser_ast_program_add_external`
- `parser_ast_program_free`
- `parser_ast_statement_free`
- `parser_ast_expression_free`

这层的意义：

1. parser 旧链路（legacy-link）不强依赖 `src/ast/ast.c` 也能正确链接运行。
2. parser 自己对“解析中临时 AST”的释放路径可控，失败回滚更稳定。
3. parser 与 AST 内部实现解耦，公开契约仍以 `include/ast.h` 为准。

维护规则建议：

- 新增 parser AST 节点字段时，必须同步更新这层 free/初始化逻辑。
- 若只更新 `ast.c` 释放逻辑而忘记兼容层，`parser_legacy_link_test` 可能会第一时间暴露问题。

---

## 4. 表达式解析：验证路径 + AST 路径

### 4.1 两条路径如何保持一致

`parser.c` + 分片实现当前同时维护：

- `int` 返回的“只验证语法”路径：`parse_*`
- `AstExpression*` 返回的“构树”路径：`parse_expression_ast_*`

两条路径共享同一优先级结构，形式上：

$$
\mathcal{G}_{check} \cong \mathcal{G}_{ast}
$$

### 4.2 优先级分层（低到高）

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

### 4.3 条件与赋值：右结合

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

另外这里的 `match_assignment_operator(...)` 已经覆盖了完整的 assignment family，不只是 `=`，还包括：

- `+= -= *= /= %=`
- `&= ^= |=`
- `<<= >>=`

所以 parser 这一层当前真正接受的是：

$$
\text{identifier} \; op_{assign} \; E_{assign}
$$

其中 `op_assign` 是上面整组赋值/复合赋值运算符。

当前 ternary AST 的真实形状也值得单独记一下：

- 根节点是 `AST_EXPR_TERNARY`
- 三个孩子分别是 `condition / then_expr / else_expr`
- `else` 分支继续递归吃 conditional-expression，所以天然右结合

这也是为什么回归里会专门锁：

- `a ? b : c ? d : e`
- `a ? b,c : d`

这两类形状的树结构。

### 4.4 前后缀 `++/--` 的 lvalue 检查

非 AST 路径通过 token slice 判定“是否是（可带括号的）标识符形式”；AST 路径通过 `ast_expression_is_identifier_lvalue` 检查。

约束可写成：

$$
\text{operand} \in \mathcal{L}_{id} \Rightarrow \text{allow } ++/--
$$

否则报错：`Expected identifier lvalue ...`。

当前 AST 形状也已经把前后缀区分开了：

- prefix `++x` / `--x`：`AST_EXPR_UNARY`
- postfix `x++` / `x--`：`AST_EXPR_POSTFIX`

### 4.5 函数调用的语法与元数据

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
        record_call_site_state()
        expr = Call(expr, args)
    elif ++/--:
        require identifier-lvalue
        expr = Postfix(op, expr)
```

这里还有一个很关键的边界：参数列表里的每一项吃的是 assignment-expression，而不是最外层 comma-expression。

所以：

- `f(a=1, b=2)` 是两个参数
- `f((a=1, b=2))` 才是“一个参数，参数值是逗号表达式”

---

## 5. 语句解析与局部控制信息

parser 现在的语句主入口是 `parse_statement_with_local_control`，它只维护“局部解析约束”需要的两个标志：

$$
\mathrm{LocalControl}(stmt) = (F,B)
$$

- $F$: `may_fallthrough`
- $B$: `may_break`

这套信息只服务于 parser 自己的局部判断。函数是否“所有路径都 return”已经交给 semantic 的 AST flow 分析，不再由 parser 输出权威结论。

### 5.1 复合语句 `{...}`

`parse_compound_statement_with_local_control` 顺序扫描子语句，做局部传播。核心思想是“前面路径一旦不再 fallthrough，后续语句不再影响 block 的局部 fallthrough 状态”。

伪代码：

```text
block_F = 1
for child in block:
    ctrl = LocalControl(child)
    if block_F and ctrl.B: block_B = 1
    if block_F:
        if not ctrl.F:
            block_F = 0
```

### 5.2 `if` 合成规则

- 无 `else`：固定 `F = 1`，因为条件为假时直接落到 if 后面
- 有 `else`：`F = F_t \lor F_e`，`B = B_t \lor B_e`

### 5.3 `while` 与 `for`（近似分析）

通过 `expression_slice_is_constant_true` 识别“条件是单个非零字面量”这一类常真条件。

这里的判定非常保守：当前只认 token slice 恰好是“一个非零 `TOKEN_NUMBER`”。

因此：

- `while(1){...}`、`for(;1;...){...}` 会被 parser 侧视为常真
- `while(+1){...}`、`while((1)){...}`、`while(1-0){...}` 不会

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

parser 不再输出 `R`；这里只保留“会不会继续往后解析”的近似信息。

### 5.4 `break/continue` 的合法性

利用 `loop_depth` 约束：

$$
loop\_depth = 0 \Rightarrow \text{reject break/continue}
$$

这在 `parse_break_statement` 与 `parse_continue_statement` 中直接检查。

### 5.5 `parse_statement_with_local_control` 的默认出口值

这个 dispatch 函数现在还有一张很实用的“局部控制表”：

- `return`：`(may_fallthrough=0, may_break=0)`
- `break`：`(0, 1)`
- `continue`：`(0, 0)`
- 普通 expression statement：`(1, 0)`

所以这里的 `may_break` 不是泛指“控制流会中断”，而是更具体地表示：

`当前子树里是否存在能让最近一层循环退出的 break 路径`

### 5.6 语句 AST 的实现方案

控制流语句会把表达式片段切成 token slice，再调用 `parser_parse_expression_ast_assignment` 生成表达式 AST，挂到 `AstStatement` 的 `expressions[]`。

这样语句与表达式 AST 始终共享同一表达式语法实现。

---

## 6. 顶层 external：函数优先，失败回退声明

### 6.1 `parse_function_external` 的策略

流程：

1. 解析 `int ident ( parameter-list )`
2. 若后跟 `;`，这是函数声明（prototype）
3. 若后跟 `{`，这是函数定义
4. 其他 token 直接报错

参数规则：

- prototype 允许 unnamed parameter（如 `int f(int);`）
- definition 不允许 unnamed parameter

而且 parser 当前不只是记 `parameter_count`，还会在需要时记录：

- `parameter_names`
- `parameter_name_lines`
- `parameter_name_columns`

这些数组先暂存在 `ParsedFunctionSignature` 里，等确定 external 成功后再通过 `parser_commit_parameter_names(...)` 交给最终的 `AstExternal`。

如果 parameter 没名字（只允许出现在 prototype 里），当前元数据约定是：

- `parameter_names[i] = NULL`
- `parameter_name_lines[i] = 0`
- `parameter_name_columns[i] = 0`

所以“unnamed parameter”不是直接少一个槽位，而是仍然保留参数位置，只是名字元数据为空。

约束：

$$
\text{is\_definition} \Rightarrow \neg \text{has\_unnamed\_parameter}
$$

### 6.2 顶层主循环（`parser_parse_translation_unit_ast`）

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

### 6.3 `parse_declaration` 实现点

支持：

- `int a;`
- `int a=1;`
- `int a=1, b, c=2;`

实现上有两个关键动作（现在这版非常重要）：

1. 顶层 external 仍然“每个 declarator 展开一个 `AST_EXTERNAL_DECLARATION`”，并记录 `has_initializer`；如果该 declarator 带 initializer，还会额外挂一份 `declaration_initializer` 表达式 AST。  
2. 语句 AST 路径中，声明器名字与初始化器表达式槽位做一一对齐。

对齐规则：

$$
\text{slot}_i =
\begin{cases}
E_i, & d_i\ \text{有 initializer} \\
\varnothing, & d_i\ \text{无 initializer}
\end{cases}
$$

也就是：

$$
|declaration\_names| = |expressions|
$$

这解决了旧实现里“只存有 initializer 的表达式”带来的映射歧义，使 semantic 可以稳定按声明器顺序检查可见性。

换句话说，当前 declaration 有两种 AST 落点：

- 顶层 declaration external：`has_initializer` + `declaration_initializer`
- 语句/for-init declaration statement：`declaration_names[]` 与对齐后的 `expressions[]`

伪代码：

```text
for each declarator d_i:
    parse identifier
    if has initializer:
        init_expr = build_ast_from_slice(...)
    else:
        init_expr = NULL

    names.push(d_i)
    expr_slots.push(init_expr)   // 始终 push，保持索引对齐
```

同样规则也用于 `for(int ...; ...; ...)` 的 for-init 声明部分。

另外当前 declaration initializer 的入口是 `parse_initializer_expression(...)`，它内部直接调用 `parse_assignment(...)`。

这意味着：

- initializer 允许 assignment / conditional / logical / arithmetic 这类常规表达式
- 但不会把最外层逗号吞成 initializer 的一部分，从而和 declarator 分隔符冲突

所以：

- `int x = a = b;` 当前是合法 parser 形态
- `int x = a, y = b;` 这里最外层逗号仍然是 declarator 分隔符
- 如果要把逗号表达式放进 initializer，本质上需要把它包进条件分支或括号形成更内层表达式

---

## 7. 公共入口函数的关系

### 7.1 `parser_parse_translation_unit`

- 仅语法检查
- 不构建 `AstProgram`
- 更轻量，适合快速路径或旧链路

### 7.2 `parser_parse_translation_unit_ast`

- 语法 + AST + 函数元数据
- 失败时会清理 `out_program`，避免半成品 AST 残留

还有一个当前实现里很重要的确定性行为：

- 每次进入 `parser_parse_translation_unit_ast(...)` 都会先 `parser_ast_program_free(out_program)`

所以这个入口默认把 `out_program` 当“可复用容器”来处理，而不是要求调用者手动先清空旧 AST。

### 7.3 `parser_parse_expression_ast_assignment`

- 入口是 comma-level 解析
- 结尾强制 `is_at_end`

约束：

$$
\text{parse\_expr\_ast}(T)=\text{success} \Rightarrow \text{current}=EOF
$$

另外这个入口当前还有两个稳定契约：

1. 若 `out_expression == NULL`，解析成功后会主动释放临时 AST，而不是泄漏
2. 成功返回前会把 `ParserError` 再清零一次，避免调用者看到旧错误残留

---

## 8. `tests/parser/*`：测试代码怎么写（含模板）

### 8.1 `tests/parser/test.c`

这是手工样例输入文件，用于 `parser_test` 读取并展示 token/AST。

实现意义：

- 覆盖 if/for/while
- 覆盖表达式优先级与一元/后缀/调用/复合赋值

### 8.2 `tests/parser/parser_test.c`

CLI 冒烟测试程序：

- 读文件
- lexer tokenize
- parser AST
- semantic analyze
- 可选 `--dump-tokens` / `--dump-ast`

这是“端到端链路”验证，不是精细断言回归。

### 8.3 `tests/parser/parser_legacy_link_test.c`

最小旧 API 链接冒烟：只测 `parser_parse_translation_unit` 能正常链接并跑通。

### 8.4 `tests/parser/parser_regression_test.c`（重点）

这个文件是 parser 行为契约主集合。组织方式：

- 小粒度 `test_xxx`
- 通用 helper（例如 `expect_translation_unit_parse_failure`）
- 表驱动矩阵（callable entry cases）
- `main()` 逐条串联，任何失败立即 `return 1`

### 8.5 基础测试块（C 模板）

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

### 8.6 注册新测试到 `main`

当前风格是显式串联：

```c
if (!test_xxx()) return 1;
```

新增测试时保持这个顺序式结构，便于第一时间定位失败点。

### 8.7 Parser 改动时的测试策略

当你修改 parser，建议按这个顺序补/跑：

1. 先写一个最小失败 case（验证旧 bug）
2. 再写一个相邻成功 case（防误杀）
3. 若涉及优先级/结合律，补 AST 形状断言
4. 最后补回归矩阵 case（若属于一类输入）

---

## 9. 快速维护指南（代码还在变化时）

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

---

## 10. 逐函数深挖（按 `src/parser/parser.c` 执行路径）

下面这一节按“实际运行顺序”讲 parser：从 token 游标，到表达式，再到语句和函数体。

### 10.1 游标与状态转移

核心状态可看成：

$$
S=(current, has\_error, error\_index, loop\_depth, recursion\_depths)
$$

每次 `match/consume/advance` 都是对 `current` 的状态转移：

$$
current_{t+1}=current_t+1\quad(\text{仅在成功消费 token 时})
$$

伪代码：

```text
step(type):
    if peek == type:
        advance
        return ok
    else:
        return fail
```

### 10.2 错误策略（最远失败优先）

`set_error` 不是“最后一次错误覆盖”，而是“最远位置优先”：

$$
idx_{new}<idx_{best}\Rightarrow ignore,
\quad idx_{new}\ge idx_{best}\Rightarrow replace
$$

这让回退场景（函数解析失败后尝试声明解析）仍能给出更有意义的错误点。

### 10.3 表达式解析链（优先级 + 结合性）

表达式链是典型递归下降分层：

$$
E_{comma}\to E_{assign}\to \cdots \to E_{postfix}\to E_{primary}
$$

- 左结合层（`+ - * / ...`）用 `while` 循环叠树
- 赋值层保持右结合（递归进右侧）
- 条件层 `?:` 的 then 分支当前会吃 `comma-expression`

伪代码（左结合层）：

```text
left = parse_lower()
while next in ops:
    op = consume()
    right = parse_lower()
    left = Node(op,left,right)
return left
```

这也是当前 parser regression 会单独锁的一条优先级边界：

$$
E_{then\ of\ ?:} = E_{comma}
$$

也就是说，`a ? b,c : d` 会把 `b,c` 作为 then 分支整体来解析，而不是把外层逗号提到更高层。

### 10.4 `parse_postfix`：为什么能识别调用链

`parse_postfix` / `parse_expression_ast_postfix` 的 while 循环每吃到一个 `(`...`)`，就追加一层 postfix call。

链式形式：

$$
a()()() = Call(Call(Call(a,[]),[]),[])
$$

所以 `a()()()` 会自然长成 3 层嵌套调用树；非 AST 路径只保留 `is_callable_target` 这类局部语法标志，不再维护旧的 call-site 元数据缓存。

参数列表本身的形状则是：

```text
'(' [ assignment (',' assignment)* ] ')'
```

也就是“逗号分隔的 assignment-expression 列表”。

### 10.5 `parse_statement_with_local_control`：语句解析 + 局部控制信息

它做两件事：

1. 解析语句结构（`if/while/for/return/...`）
2. 同步计算局部控制标志：

$$
(may\_fall, may\_break)
$$

例如 block（compound）是折叠合成：

```text
for stmt in block:
    merge(local_control, local_control(stmt))
```

### 10.6 `parse_compound_statement_with_local_control`：声明与语句并行处理

在 `{...}` 中：

- 若遇到 `int`，走 `parse_declaration`
- 否则走一般语句解析

现在（S3 后）声明节点会同时挂：

- `declaration_names[]`
- 与其一一对齐的 `expressions[]`（initializer AST，可能为 `NULL`）

### 10.7 `parse_declaration`：声明器序与 initializer 槽位

声明器序列：

$$
int\ d_1(=e_1)?, d_2(=e_2)?,\dots,d_n(=e_n)?;
$$

当前实现的关键不变量：

$$
|names|=|expr\_slots|=n
$$

且：

$$
expr\_slots[i]=
\begin{cases}
AST(e_i), & d_i\ \text{有初始化}\\
NULL, & d_i\ \text{无初始化}
\end{cases}
$$

这正是 semantic 能按声明器顺序做可见性检查的前提。

而真正吃 initializer token slice 的入口，就是上一节提到的 `parse_initializer_expression(...) -> parse_assignment(...)`。

### 10.8 `parse_for_statement_with_local_control`：for-init 的两条路

`for(init; cond; step)` 的 init 有两种：

1. 表达式 init（`i=0`）
2. 声明 init（`int i=0,j=1`）

声明 init 情况下，for 语句也会保存对齐后的声明器槽位，然后再追加 cond/step。

可抽象为：

$$
ExprList_{for} = [InitDeclSlots...] \oplus [cond?] \oplus [step?]
$$

并用索引字段标注 `for_init/condition/for_step`。

### 10.9 顶层回退（函数优先，失败回退声明）

翻译单元循环：

```text
try parse_function_external
if fail:
    rewind
    parse_declaration
```

这是 C 顶层常见模式，目的是在共享前缀（`int ident ...`）下优先识别函数，再安全回退。

现在这一层也已经被收口到单个 helper：

- `parse_translation_unit_external_or_declaration(...)`

所以无论是 AST 入口还是 legacy 语法验证入口，顶层 external 的分派策略都是共用这一份，而不是两套逻辑各写一次。

### 10.10 对外产出（当前精简契约）

顶层 external 成功落盘后，parser 对外主要写入：

1. 函数 external：`function_body`、`return_statement_count`、`parameter_count/parameter_names`
2. 声明 external：`has_initializer`，以及可选的 `declaration_initializer`

换句话说，当前 external 契约更偏“结构主导、统计精简”。

$$
Output_{external} = \{AST\ body,\ params,\ return\_count,\ decl\_initializer?\}
$$

这里还有三个实现细节值得补一句：

1. 对函数 prototype，parser 不会去构函数体 AST，`return_statement_count` 保持 `0`
2. 对函数 definition，只有当 `function_body` 或 `return_statement_count` 至少一项被请求时，才会真的保留 body AST
3. `return_statement_count` 当前是 parser 在函数体 AST 上递归调用 `count_return_statements_in_ast(...)` 现数出来的结构统计值

### 10.11 为什么 parser 仍保留非 AST 路径

当前是“镜像双轨”：

- 旧接口需要 `int parse_*` 语法验证路径
- 新链路需要 `AstExpression*` 构树路径

这是迁移期设计，不是最终形态。后续可逐步收敛为 AST 主路径 + 兼容壳。

---

## 11. Parser 级测试怎么对应到实现

### 11.1 声明器槽位对齐（S3 关键回归）

最小应覆盖：

- `int x, y=1, z;` -> 槽位 `[NULL, 1, NULL]`
- `for(int i, j=0, k; ... )` -> for-init 槽位同样对齐

### 11.2 同声明顺序可见性的 parser 前提

parser 不直接做“前向/反向引用合法性”判定，但必须保证 semantic 可判：

$$
name_i \leftrightarrow init_i\ \text{映射稳定}
$$

也就是上一节的对齐不变量。

### 11.3 callable 链相关基准

建议固定 3 类输入：

1. `f(1)`（direct identifier）
2. `(f())()`（call result）
3. `(f+1)()`（non-identifier）

保证 parser 侧分类/记录稳定，再由 semantic 决定最终诊断。

---

## 可选思考题

1. 为什么 assignment 层当前采用“identifier-only lvalue”策略？它带来了哪些简化与限制？
2. `parse_function_external` 失败后为什么必须回退 token 游标再尝试 declaration？
