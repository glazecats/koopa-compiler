# Semantic Lesson（compiler_lab）

> 目标：解释当前 `semantic.c` 在做什么、怎么做，以及和 parser/测试的对应关系（按你现在这版代码，而不是旧迁移期描述）。

## 导学

这份讲义建议按“规则优先级”来读：

1. 先建立主流程：return-flow、callable、scope、top-level consistency。
2. 再理解报错顺序：为什么某些错误会优先出现。
3. 最后看子集边界：哪些是规则，哪些是当前实现限制。

学完你应该能：

1. 能解释 `SEMA-CALL-001..006` 的判定分层。
2. 能说明为什么“同声明顺序”会影响 initializer 可见性。
3. 能区分“语义规则”与“当前子集限制”。

---

## 1. 文件定位

- 接口：`include/semantic.h`
- 实现：`src/semantic/semantic.c`
- 结构：`semantic.c` 是聚合入口，核心规则分片在 `semantic_core_flow.inc`、`semantic_callable_rules.inc`、`semantic_scope_rules.inc`、`semantic_entry.inc`
- 回归：`tests/semantic/semantic_regression_test.c` + `semantic_regression_callable_flow.inc` + `semantic_regression_scope_cf.inc`

语义入口：

```c
int semantic_analyze_program(const AstProgram *program, SemanticError *error);
```

数学上可写成：

$$
\mathrm{Analyze}: \mathrm{AstProgram} \to \{0,1\} \times \mathrm{SemanticError}
$$

---

## 2. 当前总体结构（AST-only）

当前语义主路径已经是 AST-only：直接遍历 `AstProgram` / `AstStatement` / `AstExpression`，不依赖 parser 侧旧的 metadata 补丁链路。

主流程分 6 层：

1. 顶层声明 initializer 规则（callable + scope）
2. return-flow 分析（函数是否所有路径返回）
3. callable shadow precheck（局部遮蔽优先）
4. callable 规则检查（`SEMA-CALL-001..006`）
5. scope 规则检查（`SEMA-SCOPE-*`）
6. 顶层符号一致性检查（重复定义/冲突/参数数一致）

数据流：

$$
\text{Top-level Init} \rightarrow \text{Flow} \rightarrow \text{Callable} \rightarrow \text{Scope} \rightarrow \text{Top-level Consistency}
$$

---

## 3. Visitor 骨架怎么用

语义复用了后序遍历骨架：

- `walk_expression_postorder(...)`
- `walk_statement_postorder(...)`

后序规则：

$$
visit(n)=visit(children(n));\ apply(n)
$$

伪代码：

```text
walk_expr(expr):
    for child in expr.children:
        walk_expr(child)
    visitor(expr)
```

这让“遍历逻辑”和“规则逻辑”解耦。

---

## 4. return-flow 分析

核心函数：

- `semantic_analyze_statement_flow`
- `semantic_compute_function_returns_all_paths`

内部分析函数实际维护 5 个流标志：

$$
\mathrm{Flow}(stmt)=(R,F,B,C,N)
$$

- `R`: guaranteed return
- `F`: may fallthrough
- `B`: may break
- `C`: may continue loop
- `N`: may non-terminating

函数定义必须满足“所有控制流路径都返回”（由 AST 流分析直接计算，不依赖 parser 外挂字段）。

### 4.1 loop guard stability analysis

最近这版 semantic 对 `while` / `for` 的核心变化，不是把循环做成完整符号执行器，而是加入了一个“稳定 guard 死循环”启发式。

目标很明确：

1. 不因为值依赖循环而轻易误报 `SEMA-CF-001`
2. 只在能可靠证明“guard 稳定且不会退出”时，才把循环视为 `may_non_terminating=1`

实现骨架主要由这几组 helper 组成：

- `flow_collect_expression_identifiers`
- `flow_collect_loop_exit_summary`
- `flow_statement_may_change_guard_state`
- `flow_expression_may_change_guard_state`

可以把它理解成三步：

1. 收集 loop guard 依赖的标识符集合
2. 看循环体里是否存在“所有路径都能退出循环”的 break/return 形态
3. 看这些 guard 标识符在 body/step 里是否可能被改变

对非恒真、非恒假的循环条件，semantic 现在会先取出 condition 里依赖的名字：

$$
GuardIds = Ids(cond)
$$

然后判断：

$$
StableGuard \equiv GuardIds \neq \varnothing \land \neg Changed(body,step)
$$

若同时又没有可可靠证明的全路径退出：

$$
StableGuard \land \neg ExitOnAllPaths \Rightarrow may\_non\_terminating = 1
$$

否则默认保持保守接受：

$$
\neg ProvedDeadLoop \Rightarrow may\_fallthrough = 1
$$

这就是为什么现在的策略是：

- `while(a){}`：拒绝
- `while(a){if(a){break;}}`：仍拒绝
- `while(a){a=a-1; if(a==0) break;}`：接受
- `for(;a;){continue;}`：拒绝
- `for(;a; step_changes_a)`：倾向接受

### 4.2 shadow-sensitive guard tracking

这套 guard 分析还有一个最近补上的关键细节：名字不仅按字符串看，还要按 binding 看。

实现上，`FlowIdentifierScopeStack` 会给名字解析一个“作用域绑定位点”，所以：

- 外层 `a`
- 循环体里 `int a = ...`

在 guard tracking 里不是同一个标识符。

因此下面这种写法不会再被误判成“修改了外层 guard”：

```c
while(a){int a=0; a=1;}
```

相反，若内层声明是从外层状态重建出来的，semantic 又会继续追 initializer 依赖：

```c
while(a){
    int c=b;
    int a=c;
    if(a){break;}
    b--;
}
```

这类“通过 per-iteration declaration initializer 重建 break guard”的形态现在会被接受，因为退出条件依赖的外层状态 `b` 的确在变化。

---

## 5. callable 规则（`SEMA-CALL-001..006`）

核心函数：

- `semantic_check_call_expr_postorder`
- `semantic_check_single_callable_rule`

callee 分类由 `classify_ast_call_callee` 给出：

- `DIRECT_IDENTIFIER`
- `CALL_RESULT`
- `NON_IDENTIFIER`

规则摘要：

- 非标识符 callee：
  - `CALL_RESULT` -> `SEMA-CALL-005`
  - 其他 -> `SEMA-CALL-006`
- 标识符 callee：
  - 同名非函数符号 -> `SEMA-CALL-003`
  - 函数存在但参数不匹配 -> `SEMA-CALL-004`
  - 仅后方声明存在 -> `SEMA-CALL-002`
  - 无任何声明 -> `SEMA-CALL-001`

顶层声明 initializer 也会复用同一套 callable 规则，但可见性窗口不同：

- 函数体里检查调用时，当前 external 视为已可见
- 顶层 initializer 检查时，当前 external 自己还不可见，只能看“它前面已经出现的顶层 external”

---

## 6. S3-2: callable shadow precheck

新增 precheck 的目标是固定诊断优先级：

$$
callee=Id(x) \land x\in LocalScope \Rightarrow \text{先报 }SEMA\text{-}CALL\text{-}003
$$

避免局部遮蔽场景被 `CALL-001/002` 抢先。

实现入口：

- `semantic_check_function_callable_scope_shadow_rules`
- `semantic_scope_check_call_shadow_statement`
- `semantic_scope_check_call_shadow_expression`

伪代码：

```text
walk_shadow(stmt, scope):
    push/pop for compound/for
    on declaration:按声明器顺序处理
    on call:
        if direct callee in local scope:
            error CALL-003
```

---

## 7. 诊断优先级（非常重要）

当前 semantic 明确锁定了“谁先报错”的顺序，避免同一输入在不同版本中飘移。

优先级要点：

1. 局部非函数遮蔽 callee 时，优先报 `SEMA-CALL-003`（shadow precheck 先行）。
2. `call result` / `non-identifier callee` 先报 `SEMA-CALL-005/006`，不会被 `SEMA-SCOPE-002` 抢先。
3. 若 callee 合法，但参数表达式里有未声明标识符，才报 `SEMA-SCOPE-002`。

可抽象为：

$$
CALL\text{-}003/005/006 \succ SCOPE\text{-}002\ (callee\ path),\quad
SCOPE\text{-}002\ applies\ to\ non\text{-}callee\ identifiers
$$

---

## 8. scope 规则（含同声明顺序可见性）

核心函数：

- `semantic_check_function_scope_rules`
- `semantic_scope_check_statement`
- `semantic_scope_check_expression`

### 8.1 作用域栈

显式 `ScopeStack`：`push/pop` 管理 `{}` 与 `for` 作用域。

### 8.2 同声明顺序

现在按声明器顺序检查：先看 initializer，再引入当前名字。

$$
\text{check}(init_i, S\cup\{d_1,...,d_{i-1}\});\ S:=S\cup\{d_i\}
$$

因此：

- `int x=y,y=1;` 拒绝
- `int y=1,x=y;` 通过
- `for(int i=j,j=0;...)` 拒绝
- `for(int j=0,i=j;...)` 通过

同样的“只看前面已可见名字”规则现在也应用在顶层声明 initializer：

- `int x = y; int y;` 拒绝
- `int y = 1; int x = y;` 通过
- `int x = foo(); int foo(){...}` 报 `SEMA-CALL-002`

---

## 9. 当前活跃的 `SEMA-INT-*`

当前实现里常见内部护栏：

- `SEMA-INT-005`：函数定义缺失 `function_body`
- `SEMA-INT-007`：return-flow AST 分析失败
- `SEMA-INT-009`：scope 栈空时处理声明名
- `SEMA-INT-010`：scope 栈扩容失败
- `SEMA-INT-011`：顶层声明标记了 initializer，但缺失 `declaration_initializer` AST
- `SEMA-INT-012`：表达式深度超过 semantic 分析上限（当前常量为 `4096`）

---

## 10. 回归测试怎么对应

`tests/semantic/semantic_regression_test.c` 现在是主 harness，实际 case 体拆到：

- `semantic_regression_callable_flow.inc`
- `semantic_regression_scope_cf.inc`

另外 `semantic_regression_intellisense_prelude.inc` 现在会被这两个片段直接 `#include`，承担共享 typedef / helper 声明，同时也保留在 Makefile 依赖清单里。

建议固定覆盖三组：

1. callable 诊断矩阵（`CALL-001..006` + shadow 变体）
2. scope/cf 矩阵（局部可见性、for-init 生命周期、深表达式护栏）
3. 顶层 initializer / 同声明顺序矩阵（前向拒绝 / 反向通过）

---

## 11. 顶层一致性规则（函数/变量冲突）

除了函数体内部规则，`semantic_analyze_program` 还会在顶层做名字一致性检查：

1. 同名函数声明参数个数必须一致，否则报冲突。
2. 同名函数不能重复定义。
3. 同名顶层变量若都带 initializer，报 `SEMA-TOP-001`（duplicate top-level variable definition）。
4. 函数名与变量名不能在顶层同名共存。

可抽象为：

$$
same\_name \land both\_function \Rightarrow parameter\_count\_equal
$$

$$
same\_name \land kind\_mismatch(function\ vs\ declaration) \Rightarrow reject
$$

---

## 12. 当前已知限制（需在文档长期保留）

1. return all-path 仍是结构化近似，不是完整路径求解器。
2. callable 子集当前只支持 direct-identifier callee；call-result/chained 统一拒绝（`SEMA-CALL-005`）。
3. 非标识符 callee 仅在 parser 可接受子集下进入 semantic，并按 `SEMA-CALL-006` 报错。

这些限制是当前回归矩阵锁住的行为边界，不是临时偶发现象。

---

## 13. 一句话总结

当前 semantic 已经是“AST 直接判定”架构：

$$
\text{AST callable checks} + \text{scope-order checks} + \text{return-flow checks}
$$

重点不再是 metadata parity，而是规则优先级稳定和声明顺序语义准确性。

---

## 可选思考题

1. 为什么本项目把 callable shadow precheck 放在 callable 主规则之前？
2. 如果未来要支持更完整的 callee 形态（例如函数指针调用），你会先改 parser 还是 semantic？为什么？
