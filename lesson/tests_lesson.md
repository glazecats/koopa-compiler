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

当前测试入口 C 文件（13 个）：

- `tests/lexer/test.c`
- `tests/lexer/lexer_test.c`
- `tests/lexer/lexer_regression_test.c`
- `tests/ir/ir_regression_test.c`
- `tests/ir/ir_pass_test.c`
- `tests/ir/ir_verifier_test.c`
- `tests/lower_ir/lower_ir_regression_test.c`
- `tests/lower_ir/lower_ir_verifier_test.c`
- `tests/parser/test.c`
- `tests/parser/parser_test.c`
- `tests/parser/parser_legacy_link_test.c`
- `tests/parser/parser_regression_test.c`
- `tests/semantic/semantic_regression_test.c`

当前拆分片段（11 个 `.inc`）：

- `tests/ir/ir_pass_test_intellisense_prelude.inc`
- `tests/parser/parser_regression_cases_core.inc`
- `tests/parser/parser_regression_cases_expr_ast_a.inc`
- `tests/parser/parser_regression_cases_expr_ast_b.inc`
- `tests/parser/parser_regression_cases_ast_meta.inc`
- `tests/parser/parser_regression_intellisense_prelude.inc`
- `tests/ir/ir_pass_test_direct.inc`
- `tests/ir/ir_pass_test_pipeline.inc`
- `tests/semantic/semantic_regression_callable_flow.inc`
- `tests/semantic/semantic_regression_scope_cf.inc`
- `tests/semantic/semantic_regression_intellisense_prelude.inc`

建议理解为三层：

1. `test.c`：输入样例文件（fixture）
2. `*_test.c`：可执行冒烟/调试程序（CLI harness）
3. `*_regression_test.c`：回归断言（CI 主力，当前已覆盖 lexer/parser/semantic/ir/lower_ir）

可抽象为测试收益函数：

$$
\mathrm{Coverage}=\mathrm{Smoke}+\mathrm{Regression}+\mathrm{Negative\ Cases}
$$

当前 IR 测试也已经不只是“直线表达式”了，当前至少锁住了这些行为族：

如果只按最近几轮 IR / lower-IR 改动来归类，新增测试重点主要也是几组：

- global-aware lowering：`global ...`、global update、global/local shadowing
- runtime global init：`__global.init()`、`__program.init()`、`IR-LOWER-022` 依赖顺序拒绝、`dependency path` / `dependency cycle` 诊断、依赖收集对 direct call callee body 的递归覆盖、以及 `&&/||/?:` 与常量条件 `if/while/for` 的有限裁剪
- runtime-init verifier：`IR-VERIFY-048` 到 `IR-VERIFY-060`
- ir-pass：`fold -> const -> copy -> fold -> const -> copy -> simplify-cfg -> dead-temp-elim` 默认管线、safe immediate folding、单边 immediate 恒等归约、temp constant propagation、temp copy propagation、CFG simplification、dead temp elimination、invalid pass spec、NULL pass table
- lower-ir：canonical IR -> lower IR 的 source-lowered shape、显式 `load_local/store_local/load_global/store_global` dump 形状、runtime-init helper exact dump，以及 lower-IR verifier 对 value/slot 边界、runtime-init helper、call 契约、terminator、名字表与 dense table 的拒绝路径

1. 表达式 family：算术、comparison、bitwise、shift、ternary、逗号表达式
2. 更新表达式 family：compound assignment、prefix/postfix `++`、prefix/postfix `--`
3. 一元 family：`-`、`~`、`!`
4. 调用 family：直接调用、带表达式参数的调用
5. 逻辑短路值 lowering：`!`、`&&`、`||`
6. `if` 无 `else` 时的 then + continuation
7. `if/else` 双边 fallthrough 时的 join block
8. `if/else` 双边都 `return` 时“不生成多余 join block”
9. comparison / logical 条件驱动的 `if`
10. `while` 的 header/body/exit + backedge
11. `while` 里的 `break`
12. `for` 的 init/header/body/step/exit
13. 嵌套 loop 的 `break/continue` 就近匹配
14. declaration-aware IR：prototype 会保留成 `declare` 签名项，而不是直接丢掉
15. global-aware IR：顶层声明会 lower 成 `global ...`，global bitwise/shift compound、global initializer 依赖、静态求值失败到 runtime-init 的回退、按需懒创建并复用的 `__global.init()`、没有可注入 `main` 定义时导出的 `__program.init()`、直接/间接依赖未初始化 global 的 `IR-LOWER-022`、以及 local-shadow-over-global 都有回归
16. verifier 结构与数据流校验：缺 terminator、不可达 block、非法 binary op、binary/call 结果不是 temp、空 callee、unknown callee、NULL/非 NULL call-args payload 不匹配、undefined/use-before-def temp、同一 block 内重复 temp 定义、call arity 漂移、count-capacity/table-pointer 破坏、global table/global id/global name 破坏、duplicate global / function-global name collision、constant/runtime initializer flag 冲突、runtime-init helper 契约破坏、startup helper 非法调用、declaration-only function 被错误塞入 blocks

另外现在还有单独的 `test-ir-pass`：

- `tests/ir/ir_pass_test.c`：测试聚合入口和共享 helper
- `tests/ir/ir_pass_test_direct.inc`：锁直接调用单个 pass 的行为
- `tests/ir/ir_pass_test_pipeline.inc`：锁 default pipeline 的整体行为
- 目前主要验证：
  - default pipeline 会减少可 fold 的 immediate binary 指令
  - fold pass 会吃掉安全的单边 immediate 恒等归约机会
  - default pipeline 会跳过当前不能安全归约的 immediate/identity 形状
  - temp constant propagation 会把可证明恒定的 temp use 压成立即数
  - temp copy propagation 会减少可传播的 temp-copy use
  - `ir_pass_simplify_cfg(...)` 会把常量条件 branch 收成 `jmp`，并删掉不可达 block
  - `ir_pass_simplify_cfg(...)` 也会覆盖空 trampoline threading、`jmp -> ret` folding、same-return diamond folding、以及单前驱 jump-block merge
  - default pipeline 会在常量条件 CFG case 和 same-return diamond case 上自动完成这类 CFG 化简和 block collapse
  - dead-temp elimination 会删除纯 `mov/binary` 的 dead temp defs
  - default pipeline 会把 binary identity chain 继续塌到零指令或更短 IR
  - default pipeline 在 dead-result call 周围清纯计算、做 CFG revisit 之后，仍然必须保留 side-effecting `call`
  - default pipeline 在 multi-def constant / multi-def copy 路径上必须保留路径区分，不能把 join 后的值误塌成单一路径事实
  - default pipeline 连跑两次后，第一轮和第二轮 dump 必须一致，而且两轮结果都继续通过 verifier
  - default pipeline 会减少总指令数，并在 cleanup case 结束后不再留下 dead temp defs / temp-copy propagation opportunities
  - invalid pass spec 会报 `IR-PASS-003`
  - `pass_count > 0 && passes == NULL` 会报 `IR-PASS-002`

另外现在也有单独的 lower-IR 测试目标：

- `make test-lower-ir-regression`
  - 对应 `tests/lower_ir/lower_ir_regression_test.c`
  - 当前主要锁 source-lowered 代表形状：`return a;`、`a=b+1; return a;`、`g=a; return g;`、`if(a)...`、`id(a)`、runtime-init helper exact dump、单臂/双臂分支 join、short-circuit 条件 materialize、`while` 回边 / `break`、`for` init-cond-step、nested-loop control、控制流里的 global write/read、call-heavy CFG slice、以及 value-materialization join
- `make test-lower-ir-verifier`
  - 对应 `tests/lower_ir/lower_ir_verifier_test.c`
  - 当前主要锁 lower-IR verifier 对 value/slot 边界、store 结果形状、binary 结果 temp 约束、block terminator 必备性、runtime-init helper / startup call 契约、unknown callee / arg mismatch、duplicate-name / NULL-table 破坏、count-capacity 契约破坏，以及 temp/reachability 级不变量的拒绝

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

当前主测试文件实际 include 的是 `semantic_regression_callable_flow.inc` 与 `semantic_regression_scope_cf.inc`；另外这两个片段现在都会直接 `#include "semantic_regression_intellisense_prelude.inc"`，把共享 typedef / helper 声明集中到一处。

已有高价值模式（建议保持）：

1. `parse_source_to_ast` 统一准备流程
2. `run_callable_diag_case` / `run_callable_pass_case` 表驱动
3. 同时断言：
   - 错误码（如 `SEMA-CALL-00x`）
   - 关键 payload 片段
   - 行列位置
4. 控制流用 `CfCase` 分组：
   - `must_pass_or_fail_now`
   - `conservative_loop_acceptance`
   - `deterministic_loop_rejects`

S3 之后建议把两类矩阵当成必选：

1. callable shadow 诊断优先级矩阵：local/param/inner-block/for-init/parenthesized callee 都应稳定报 `SEMA-CALL-003`  
2. 同声明顺序可见性矩阵：前向引用拒绝、反向引用通过（普通声明与 for-init 各一组）

现在还建议补一组顶层 initializer 矩阵：

1. 顶层变量 initializer 的前向/反向可见性
2. 顶层 initializer 中的 callable 诊断顺序
3. 重复带 initializer 的顶层定义应报 `SEMA-TOP-001`

loop guard stability analysis 相关 case 现在也建议单独成组：

1. 稳定 guard 死循环：`while(a){}`, `while(a){if(a){break;}}`, `for(;a;){continue;}` 应继续拒绝
2. mutation-driven exits：`a=a-1`、`step` 改 guard、或 body 里直接赋值改 guard 的形态应继续接受
3. call-driven exits：循环体里有普通函数调用时，默认视为“可能改 guard”，应避免误报
4. shadow-sensitive cases：内层 `int a=...` 不能伪装成修改外层 guard
5. rebuilt-guard accepts：`int c=b; int a=c; if(a) break; b--;` 这类按 initializer 重建 guard 的路径应继续接受

constant-if narrowing 也值得固定锁一组：

1. `if(1) return 1;`
2. `if((1)) return 1;`
3. `if(+1) return 1;`
4. `if((0,1)) return 1;`
5. `if(0) ; else return 1;`

这些 case 的目标不是测 parser，而是防 semantic 把不可达分支错误合并回 return-flow。

另外建议专门补“诊断顺序”锁：

1. scope 错误优先于 `SEMA-CF-001`
2. depth guard（`SEMA-INT-012`）优先于 `SEMA-CF-001`

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
- `make test-ir-regression`
- `make test-ir-verifier`
- `make test`
- `make test-fanalyzer`
- `make test-asan`
- `make test-strict-warnings`

当前 Makefile 里还有一个值得知道的实现细节：

- `test-parser-legacy-link`、`test-semantic-regression`、`test-ir-regression`、`test-ir-verifier` 等目标在执行前会先把二进制复制成临时 `*.run.$$$$` 文件，再运行临时副本
- 这样做是为了减少调试或并发构建时遇到 `Text file busy` 的概率；讲义里写命令时按 `make test-*` 目标理解即可，不需要手动执行这些临时文件

建议日常节奏：

1. 改 lexer 后先跑 `make test-lexer-regression`
2. 改 parser 后先跑 `make test-parser-regression`
3. 改 semantic 后先跑 `make test-semantic-regression`
4. 改 IR lowering 后先跑 `make test-ir-regression`
5. 改 IR 结构体 / CFG / verifier / declaration-only function 处理后再跑 `make test-ir-verifier`
6. 合并前跑一次 `make test`

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
