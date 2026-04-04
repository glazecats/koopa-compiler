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

`semantic.c` 现在也不只是“include 四个分片”的薄入口了。它当前会先集中放置：

1. 共享 visitor typedef：`ExpressionPostVisitor`
2. 跨分片共享上下文：`CallableRuleVisitCtx`
3. flow 分析摘要：`FlowLoopExitSummary`
4. 一组跨分片 helper 前置声明（postorder walker、constant-if helper、callable/scope/flow 入口）

所以现在更准确的组织关系是：

$$
\texttt{semantic.c} = \text{shared analysis scaffolding} + \text{aggregator}
$$

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
2. callable shadow precheck（局部遮蔽优先）
3. callable 规则检查（`SEMA-CALL-001..006`）
4. scope 规则检查（`SEMA-SCOPE-*`）
5. return-flow 分析（函数是否所有路径返回）
6. 顶层符号一致性检查（重复定义/冲突/参数数一致）

数据流：

$$
\text{Top-level Init} \rightarrow \text{Callable} \rightarrow \text{Scope} \rightarrow \text{Flow} \rightarrow \text{Top-level Consistency}
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

最近的源码整理也让这个分工更明显了：`ExpressionPostVisitor`、`CallableRuleVisitCtx` 和 walker 原型现在都集中定义在 `semantic.c`，分片里的 `callable` / `scope` / `flow` 逻辑直接复用这些共享骨架，而不再各自保留一份局部 typedef/前置声明。

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

这里“可能改 guard”的判定范围也比旧文档宽，当前会把下面这些都算进去：

- 直接赋值 `a = ...`
- 复合赋值 `a += 1`、`a <<= 1`、`a &= b`
- 前后缀更新 `++a`、`a++`
- 普通函数调用（保守视为可能改外部状态）

所以现在的 “call-driven exits” / “step-driven exits” 之所以能被接受，不是靠特殊白名单，而是靠这套统一的 `may_change_guard_state` 启发式。

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

### 4.3 constant-if narrowing

`AST_STMT_IF` 的 flow merge 现在也会先尝试判断条件是不是常量真/假，而不是一律保守合并 then/else。

核心判断仍然复用同一套常量求值族：

- `flow_try_eval_constant_int`
- `expression_is_constant_true_for_flow`
- `expression_is_constant_false_for_flow`

所以这些形态现在都会被当成“恒真条件”：

- `if(1)`
- `if((1))`
- `if(+1)`
- `if((0,1))`

而 `if(0)` / `if((0,0))` 这类会被当成恒假。

这一步的价值是：先把不可达分支裁掉，再做 return-flow 合成，避免把本来不可达的分支错误并进来，造成假的 `SEMA-CF-001`。

### 4.4 flow 常量求值的真实支持面

最近这版文档里最容易低估的一点是：`flow_try_eval_constant_int(...)` 现在已经不只是识别 `1`、`+1` 这种简单字面量了。

它当前能在 semantic flow 里直接求值的 AST 形态包括：

- 括号表达式
- 一元 `+ - ! ~`
- 三目 `?:`
- 逻辑短路 `&& ||`
- 逗号表达式 `,`
- 比较 `== != < <= > >=`
- 位运算 `& ^ |`
- 算术 `+ - * / %`
- 移位 `<< >>`

所以这些条件现在都能被收窄：

- `while(+1){...}`
- `while((1||0)){...}`
- `while((1?1:0)){...}`
- `while((1<<3)){...}`
- `if((0?1:0)) ... else ...`

但这套常量求值不是“无条件相信一切字面量表达式”，它仍然保守地拒绝不安全或未定义边界，比如：

- 除零：`1/0`
- 会溢出的移位：`1<<63`
- 负移位或超宽移位

对这些表达式，semantic 会回退成“无法证明常量”，而不是强行当成恒真/恒假。

这也是当前回归里这些 case 的真实行为来源：

- `while((1<<3)){continue;}`：拒绝
- `while((1/0)){continue;}`：保守接受
- `for(;(1<<63);){continue;}`：保守接受

---

## 5. callable 规则（`SEMA-CALL-001..006`）

核心函数：

- `semantic_check_call_expr_postorder`
- `semantic_check_single_callable_rule`

callee 分类由 `classify_ast_call_callee` 给出：

- `DIRECT_IDENTIFIER`
- `CALL_RESULT`
- `NON_IDENTIFIER`

这里的分类不是只看最外层 token，而是先 unwrap 掉多余括号再看 AST 形状，所以：

- `foo(1)`、`(foo)(1)`、`((foo))(1)` 仍算 `DIRECT_IDENTIFIER`
- `f()(1)`、`((f)(1))(2)` 算 `CALL_RESULT`
- `(f+1)()`、`(f?f:f)()`、`(f&&f)()` 算 `NON_IDENTIFIER`

这也是为什么现在“括号本身”不会改变 callable 分类；真正决定 `CALL-005` 还是 `CALL-006` 的，是去掉括号后的 callee AST 形状。

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

实现上这个窗口差异就是 `CallableRuleVisitCtx.include_current_external`：

- function body：`include_current_external = 1`
- top-level initializer：`include_current_external = 0`

scope 规则那边也沿用了同样的可见性窗口，而不是只在 callable 分支单独实现一份。

### 5.1 诊断里的名字预览是截断过的

很多 `SEMA-CALL-*` / `SEMA-TOP-*` / `SEMA-SCOPE-*` 诊断里都会带 `identifier=` 或 `callee=` 预览串。

当前这些预览统一走 `format_callee_preview(...)`，对超长名字会做“头尾保留、中间省略”的截断，而不是把整段名字原样塞进错误消息。

所以这些带 preview 的诊断现在有两个稳定特征：

- 统一格式
- 超长标识符不会把 message 撑爆

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
4. 对函数定义来说，callable/scope/depth 相关诊断会先于 `SEMA-CF-001`，因为 `semantic_entry.inc` 现在先跑 shadow/callable/scope，再做 return-flow gate。
5. `SEMA-INT-012` 这类深度护栏也会先于 flow 诊断，因为 scope/call-shadow walker 本身带表达式深度上限。

可抽象为：

$$
CALL\text{-}003/005/006 \succ SCOPE\text{-}002\ (callee\ path),\quad
SCOPE\text{-}002\ applies\ to\ non\text{-}callee\ identifiers
$$

另外现在也已经有回归专门锁这一条：

$$
SEMA\text{-}INT\text{-}012 \succ SEMA\text{-}CF\text{-}001
$$

也就是深表达式输入不会先掉到 “all control-flow paths return” 的报错上。

---

## 8. scope 规则（含同声明顺序可见性）

核心函数：

- `semantic_check_function_scope_rules`
- `semantic_scope_check_statement`
- `semantic_scope_check_expression`

### 8.1 作用域栈

显式 `ScopeStack`：`push/pop` 管理 `{}` 与 `for` 作用域。

名字解析顺序也很明确：

$$
\text{resolve}(x)=
\begin{cases}
\text{nearest local binding}, & x \in LocalScope \\
\text{visible top-level external}, & x \notin LocalScope
\end{cases}
$$

也就是“先局部，后顶层”，并且顶层是否可见还要继续受 `include_current_external` 窗口控制。

### 8.1.1 为什么 callee 自己不会先报 `SEMA-SCOPE-002`

scope walker 现在区分了两种上下文：

- `in_call_callee = 1`
- `in_call_callee = 0`

当表达式正位于 call 的 callee 槽位时，未解析名字不会立刻报 `SEMA-SCOPE-002`，而是把诊断机会留给 callable 规则去决定：

- `SEMA-CALL-001`
- `SEMA-CALL-002`
- `SEMA-CALL-003`
- `SEMA-CALL-005`
- `SEMA-CALL-006`

只有参数表达式、普通赋值右侧、条件表达式等“非 callee 位置”的未解析标识符，才会走 `SEMA-SCOPE-002`。

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

- `SEMA-INT-013`：`semantic_analyze_program(NULL, ...)`
- `SEMA-INT-005`：函数定义缺失 `function_body`
- `SEMA-INT-007`：return-flow AST 分析失败
- `SEMA-INT-009`：scope 栈空时处理声明名
- `SEMA-INT-010`：scope 栈扩容失败
- `SEMA-INT-011`：顶层声明标记了 initializer，但缺失 `declaration_initializer` AST
- `SEMA-INT-012`：表达式深度超过 semantic 分析上限（当前常量为 `4096`）

其中 `SEMA-INT-012` 不只可能从一条路径抛出：

- scope/call-shadow 的表达式遍历会报它
- flow 常量求值 / postorder walker 也会受同一上限约束

所以它本质上是 semantic 全局共享的深度护栏，不是某一个子规则独占的错误码。

另外当前还有一个值得强调的 AST-primary 契约：

- function definition 若缺 `function_body`，会直接报 `SEMA-INT-005`

这条不仅发生在“正常 parser 产出的 AST 被破坏”时，连“只手工塞 metadata、没给 body AST”的输入也会被拦住。也就是说，semantic 现在明确把 `function_body` 当成 AST-primary 时代的必需契约，而不是可选 metadata。

---

## 10. 回归测试怎么对应

`tests/semantic/semantic_regression_test.c` 现在是主 harness，实际 case 体拆到：

- `semantic_regression_callable_flow.inc`
- `semantic_regression_scope_cf.inc`

另外 `semantic_regression_intellisense_prelude.inc` 现在会被这两个片段直接 `#include`，承担共享 typedef / helper 声明，同时也保留在 Makefile 依赖清单里。

建议固定覆盖三组：

1. callable 诊断矩阵（`CALL-001..006` + shadow 变体）
2. scope/cf 矩阵（局部可见性、for-init 生命周期、constant-if narrowing、深表达式护栏）
3. 顶层 initializer / 同声明顺序矩阵（前向拒绝 / 反向通过）

再细一点说，当前 `scope_cf` 回归实际上已经锁了这些 semantic flow 行为：

1. constant-if narrowing：`if(1)`、`if(+1)`、`if((0,1))`
2. 常量循环条件：逻辑、三目、算术、位运算、移位
3. 保守求值失败边界：`1/0`、`1<<63` 这类不强判常量
4. loop guard stability：stable reject / mutation-driven accept / call-driven accept
5. shadow-sensitive guard tracking：内层同名声明不应伪装成外层 guard mutation
6. rebuilt-guard acceptance：通过 declaration initializer 重建 break guard 的形态继续接受

---

## 11. 顶层一致性规则（函数/变量冲突）

除了函数体内部规则，`semantic_analyze_program` 还会在顶层做名字一致性检查：

1. 同名函数声明参数个数必须一致，否则报 `SEMA-TOP-002`。
2. 同名函数不能重复定义，否则报 `SEMA-TOP-003`。
3. 同名顶层变量若都带 initializer，报 `SEMA-TOP-001`（duplicate top-level variable definition）。
4. 函数名与变量名不能在顶层同名共存，否则报 `SEMA-TOP-004`。

反过来说，下面这些当前是被接受的：

- 多个同名函数声明，只要参数个数一致
- 顶层变量重复声明，只要不是“两个都带 initializer”

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
4. loop guard stability 仍是保守启发式，不是跨迭代符号执行；它只在“稳定 guard 且无法可靠退出”时才拒绝。

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
