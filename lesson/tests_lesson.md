# Tests Lesson (compiler_lab)

> 目标：把“怎么写测试”讲清楚，让你能从用例设计角度理解 lexer/parser/semantic 的行为边界。

## 导学

这份讲义建议按“模板 -> 策略 -> 设计能力”的顺序读：

1. 先掌握通用模板：Arrange/Act/Assert/Cleanup。
2. 再按模块学习：lexer、parser、semantic 各自如何设断言。
3. 最后回到设计层：如何从需求反推测试矩阵。

学完你应该能：

1. 能为新语法点设计一组正向/反向/边界测试。
2. 能写出可维护的表驱动回归测试。
3. 能判断某个断言是否足够“稳”（返回值、结构、诊断、位置）。

## 1. 总览：`tests/` 目录怎么组织

当前测试入口 C 文件（8 个）：

- `tests/lexer/test.c`
- `tests/lexer/lexer_test.c`
- `tests/lexer/lexer_regression_test.c`
- `tests/parser/test.c`
- `tests/parser/parser_test.c`
- `tests/parser/parser_legacy_link_test.c`
- `tests/parser/parser_regression_test.c`
- `tests/semantic/semantic_regression_test.c`

当前拆分片段（8 个 `.inc`）：

- `tests/parser/parser_regression_cases_core.inc`
- `tests/parser/parser_regression_cases_expr_ast_a.inc`
- `tests/parser/parser_regression_cases_expr_ast_b.inc`
- `tests/parser/parser_regression_cases_ast_meta.inc`
- `tests/parser/parser_regression_intellisense_prelude.inc`
- `tests/semantic/semantic_regression_callable_flow.inc`
- `tests/semantic/semantic_regression_scope_cf.inc`
- `tests/semantic/semantic_regression_intellisense_prelude.inc`

建议理解为三层：

1. `test.c`：输入样例文件（fixture）
2. `*_test.c`：可执行冒烟/调试程序（CLI harness）
3. `*_regression_test.c`：回归断言（CI 主力）

可抽象为测试收益函数：

$$
\mathrm{Coverage}=\mathrm{Smoke}+\mathrm{Regression}+\mathrm{Negative\ Cases}
$$

---

## 2. 通用测试写法模板（所有文件适用）

### 2.1 基本结构

```c
static int test_xxx(void) {
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[mod-reg] FAIL: lexer failed for xxx\n");
        return 0;
    }

    // 断言：成功或失败

    lexer_free_tokens(&tokens);
    return 1;
}
```

### 2.2 失败用例模板

```c
if (parser_parse_translation_unit(&tokens, &err)) {
    fprintf(stderr, "[parser-reg] FAIL: parser unexpectedly accepted xxx\n");
    lexer_free_tokens(&tokens);
    return 0;
}
if (strstr(err.message, "Expected") == NULL) {
    fprintf(stderr, "[parser-reg] FAIL: expected diagnostic snippet, got: %s\n", err.message);
    lexer_free_tokens(&tokens);
    return 0;
}
```

### 2.3 表驱动模板（推荐）

当同类 case 数量变多，用矩阵更稳：

$$
\text{for each case } c_i:\ \mathrm{run}(c_i)\land\mathrm{assert}(c_i)
$$

```c
typedef struct {
    const char *case_id;
    const char *source;
    int expect_success;
    const char *required_msg;
} Case;
```

---

## 3. `tests/lexer`：怎么写

### 3.1 `tests/lexer/test.c`（fixture）

用途：给 `make test-lexer` 的 CLI 程序提供综合输入。

怎么写：

- 放“尽量多语法元素”而不是断言代码。
- 重点覆盖新 token 邻接边界（例如 `<<=`, `>>=`, `!=`, `++`, `--`）。
- 每次新增 lexer 能力时，先补这里，再补回归断言。

### 3.2 `tests/lexer/lexer_test.c`（CLI 冒烟）

用途：读文件后打印 token 流，人工定位问题。

怎么写新增逻辑：

- 尽量只改“打印字段”或“命令行参数”，不写大量业务断言。
- 这个文件更像调试工具，不是严格回归集合。

### 3.3 `tests/lexer/lexer_regression_test.c`（回归主力）

用途：精确锁行为，防止回归。

已有模式（建议复用）：

1. 正向序列断言：`expected[]` 对比 `tokens.data[i].type`
2. 契约负例：非法 `TokenArray` 状态应失败
3. 边界词素：`&& || << >> ? :` 等序列
4. 统一 `main()` 串联，任一失败立即 `return 1`

新增测试建议：

- 新增 token 时，至少两类 case：
  - 正向：token 序列精确匹配
  - 反向：最接近的非法邻接应失败或被拆成预期 token

示例（正向）：

```c
const TokenType expected[] = {
    TOKEN_IDENTIFIER, TOKEN_SHIFT_LEFT, TOKEN_IDENTIFIER,
    TOKEN_SHIFT_RIGHT, TOKEN_IDENTIFIER, TOKEN_SEMICOLON, TOKEN_EOF,
};
```

---

## 4. `tests/parser`：怎么写

### 4.1 `tests/parser/test.c`（fixture）

用途：给 `parser_test` 的集成路径喂输入。

怎么写：

- 新语法能力上线时，把最小可运行示例放进这里。
- 维持“可被 parser + semantic 同时接受”的状态，避免把故障样例塞进 fixture。

### 4.2 `tests/parser/parser_test.c`（CLI 集成）

用途：`lexer -> parser(ast) -> semantic` 的串联验证，支持 `--dump-tokens` / `--dump-ast`。

怎么写：

- 这里适合加“观测输出”功能，不适合加大量细粒度断言。
- 如果要锁行为，优先去 `parser_regression_test.c`。

### 4.3 `tests/parser/parser_legacy_link_test.c`（兼容冒烟）

用途：保证旧 API `parser_parse_translation_unit` 仍可链接并可用。

怎么写：

- 固定一个最小输入（如 `int x;`）。
- 只验证“能跑通”，不做复杂语义断言。

### 4.4 `tests/parser/parser_regression_test.c`（超大回归集）

用途：parser 主要行为锁（成功、失败、诊断、优先级、结合性、边界）。

当前已按 `*.inc` 拆分（`core`/`expr_ast_a`/`expr_ast_b`/`ast_meta`），主文件只保留 harness 和执行顺序。

写法策略：

1. 单测函数命名要表达行为：
   - `test_translation_unit_rejects_...`
   - `test_expression_ast_accepts_...`
2. 失败用例要校验诊断关键词（`required_msg_a/b`）
3. 大量同类 case 用表驱动（例如 callable matrix）
4. `main()` 按功能分组顺序执行，便于定位回归块

S3 之后 parser 回归新增一个重点：声明器 initializer 槽位对齐。

建议固定补这两组：

1. 普通声明对齐：`int x, y=1, z;` 要验证 `declaration_names` 与 `expressions` 等长，且槽位为 `[NULL, 1, NULL]`  
2. for-init 声明对齐：`for(int i, j=0, k; ... )` 要验证 for-init 槽位也保持同样对齐规则

推荐新增测试时使用的“先验公式”：

$$
\mathrm{New\ Grammar\ Rule}
\Rightarrow
(\mathrm{positive\ accept\ case})
+ (\mathrm{negative\ reject\ case})
+ (\mathrm{boundary\ precedence/associativity\ case})
$$

---

## 5. `tests/semantic`：怎么写

### 5.1 `tests/semantic/semantic_regression_test.c`（语义回归主力）

用途：语义诊断码、行列定位、矩阵策略、控制流当前行为锁。

当前也拆分为 `semantic_regression_callable_flow.inc` 与 `semantic_regression_scope_cf.inc`；另外 `semantic_regression_intellisense_prelude.inc` 专门承载编辑器补全前置声明。

已有高价值模式（建议保持）：

1. `parse_source_to_ast` 统一准备流程
2. `run_callable_diag_case` / `run_callable_pass_case` 表驱动
3. 同时断言：
   - 错误码（如 `SEMA-CALL-00x`）
   - 关键 payload 片段
   - 行列位置
4. 控制流用 `CfCase` 分组：
   - `must_pass_or_fail_now`
   - `known_limitation_lock`

S3 之后建议把两类矩阵当成必选：

1. callable shadow 诊断优先级矩阵：local/param/inner-block/for-init/parenthesized callee 都应稳定报 `SEMA-CALL-003`  
2. 同声明顺序可见性矩阵：前向引用拒绝、反向引用通过（普通声明与 for-init 各一组）

新增语义规则时建议：

- 至少补 1 个 pass + 1 个 fail。
- fail 至少校验两个维度：`error_code` 与 `message_snippet`。
- 若涉及位置，额外校验 `line/column`。

前向/反向引用模板（推荐直接复用）：

```c
/* fail: forward reference in same declaration */
const char *s1 = "int f(){int x=y,y=1;return x;}\n";

/* pass: reverse reference in same declaration */
const char *s2 = "int f(){int y=1,x=y;return x;}\n";

/* fail: forward reference in for-init declaration */
const char *s3 = "int f(){for(int i=j,j=0;i<1;i=i+1){return i;}return 0;}\n";

/* pass: reverse reference in for-init declaration */
const char *s4 = "int f(){for(int j=0,i=j;i<1;i=i+1){return i;}return 0;}\n";
```

语义断言强度可写成：

$$
\mathrm{Strength}=\mathrm{Code\ Match}+\mathrm{Snippet\ Match}+\mathrm{Location\ Match}
$$

---

## 6. “所有文件都加入测试写法”的落地规则

以后你每加一个源码文件（如 `src/xxx/yyy.c`），建议同步做 3 件事：

1. 在对应模块 regression 文件新增测试函数。
2. 在对应 fixture（`tests/*/test.c`）补一个可运行示例（若适用）。
3. 在 lesson 文档中补“实现 + 伪代码 + 测试写法 + 运行命令”。

最小完成判据：

$$
\Delta\text{Feature}
\Rightarrow
\Delta\text{Regression Test}
\land
\texttt{make test}\ \text{green}
$$

---

## 7. 运行命令（基于当前 Makefile）

- `make test-lexer`
- `make test-lexer-regression`
- `make test-parser`
- `make test-parser-regression`
- `make test-parser-legacy-link`
- `make test-semantic-regression`
- `make test`

建议日常节奏：

1. 改 lexer 后先跑 `make test-lexer-regression`
2. 改 parser 后先跑 `make test-parser-regression`
3. 改 semantic 后先跑 `make test-semantic-regression`
4. 合并前跑一次 `make test`

Milestone C（静态分析清理）建议加跑下面两条：

1. `-fanalyzer` 严格检查（不改 Makefile，直接覆盖 `CFLAGS`）

```bash
make clean && make CFLAGS="-std=c11 -Wall -Wextra -Werror -fanalyzer -Iinclude" test
```

2. AddressSanitizer 路径（内存错误与越界）

```bash
make clean && make CFLAGS="-std=c11 -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer -g -Iinclude" test
```

可选：若环境支持 LeakSanitizer，可附加：

```bash
ASAN_OPTIONS=detect_leaks=1 make test
```

---

## 8. 基本测试块应该怎么写（伪代码 + C 模板）

这一节是“最小可复用模板”，你在 `tests/lexer`、`tests/parser`、`tests/semantic` 都可以直接套。

测试块推荐统一成四步：

$$
\mathrm{TestBlock}=\mathrm{Arrange}+\mathrm{Act}+\mathrm{Assert}+\mathrm{Cleanup}
$$

### 8.1 伪代码模板（通用）

```text
function test_xxx():
    # Arrange: 准备输入和上下文
    init resources

    # Act: 调用被测接口
    rc = run_under_test(...)

    # Assert: 断言返回值/结构/诊断
    if expectation not met:
        print fail message
        cleanup
        return 0

    # Cleanup: 释放资源
    cleanup
    return 1
```

### 8.2 C 模板：期望成功的测试块

```c
static int test_xxx_accepts(void) {
    const char *source = "int main(){return 0;}\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[mod-reg] FAIL: lexer failed in accepts-case\n");
        return 0;
    }

    if (!parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr, "[mod-reg] FAIL: parser should accept, got: %s\n", err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}
```

### 8.3 C 模板：期望失败的测试块（含诊断断言）

```c
static int test_xxx_rejects(void) {
    const char *source = "int main( { return 0; }\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[mod-reg] FAIL: lexer failed in rejects-case\n");
        return 0;
    }

    if (parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr, "[mod-reg] FAIL: parser unexpectedly accepted invalid input\n");
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (strstr(err.message, "Expected") == NULL) {
        fprintf(stderr, "[mod-reg] FAIL: expected diagnostic snippet, got: %s\n", err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}
```

### 8.4 C 模板：表驱动测试块（推荐大量 case 时用）

```c
typedef struct {
    const char *case_id;
    const char *source;
    int expect_success;
    const char *must_contain;
} Case;

static int run_case(const Case *c) {
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(c->source, &tokens)) return 0;

    if (c->expect_success) {
        if (!parser_parse_translation_unit(&tokens, &err)) {
            lexer_free_tokens(&tokens);
            return 0;
        }
    } else {
        if (parser_parse_translation_unit(&tokens, &err)) {
            lexer_free_tokens(&tokens);
            return 0;
        }
        if (c->must_contain && strstr(err.message, c->must_contain) == NULL) {
            lexer_free_tokens(&tokens);
            return 0;
        }
    }

    lexer_free_tokens(&tokens);
    return 1;
}
```

### 8.5 `main()` 串联模板（统一出口）

```c
int main(void) {
    if (!test_xxx_accepts()) return 1;
    if (!test_xxx_rejects()) return 1;
    if (!test_matrix_group()) return 1;

    printf("[mod-reg] all tests passed.\n");
    return 0;
}
```

### 8.6 断言强度建议（从低到高）

$$
\mathrm{AssertStrength}=\mathrm{ReturnCode} + \mathrm{StructureMatch} + \mathrm{DiagnosticMatch} + \mathrm{LocationMatch}
$$

- 最低：只看返回值（通过/失败）
- 建议：返回值 + 关键结构（token 序列 / AST shape）
- 强建议（语义）：再加错误码与 `line:column`

---

## 9. 课堂讨论题（可选）

1. 对同一个新特性，为什么“只写通过用例”通常不够？
2. 在 parser 回归里，诊断子串断言和 AST 结构断言应该如何搭配？
3. 你会如何设计一组用例来验证“诊断优先级稳定”而不是“偶然通过”？
