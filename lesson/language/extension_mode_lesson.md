# Extension Mode Lesson（compiler_lab）

> 目标：解释 `-extension` 为什么不是“再加一个编译命令行别名”，而是当前 language-feature round 的统一边界；它和 `-riscv` / `-perf` 的区别是什么，以及为什么这条线默认更保守。

## 一句话定位

`-extension` 是仓库当前所有语言特性的统一入口，也是“更保守、少依赖 aggressive pass”的翻译管线入口。

## 常见误解

- 误解一：`-extension` 只是“多开几个关键字”。
  - 更准确地说，它同时控制语法接受面和翻译/优化边界。
- 误解二：只要功能在 `-extension` 跑通，就说明 `-riscv` / `-perf` 也该默认支持。
  - 当前项目明确不是这个策略。

## 1. 为什么需要单独的 `-extension`

当前仓库的 language-feature round 有一个核心约束：

- 新特性要先有一条**正确、保守、可回归锁定**的主线
- 不能强依赖 course-facing 主线里的 aggressive cleanup / peephole / pass 组合

所以 `-extension` 的意义不是“方便实验”，而是：

`给新语法一个不和课程兼容模式绑死的稳定落地点`

这也是为什么当前 docs 反复强调：

- `-riscv`
- `-perf`

仍然优先保持课程兼容，而不是自动吞掉所有 feature-round 语法。

## 2. 当前 `-extension` 已经承接哪些东西

按当前仓库状态，`-extension` 已经不只支持 parser 小实验。

至少已经承接了几条真实主线：

1. defer family
   - `defer`
   - `fndefer`
   - `capdefer`
2. small control-flow sugar
   - `unless`
3. callable/function-value line
   - non-capturing function values
   - closure literal 的第一版 slice
   - returned closure / callable-object follow-up
4. aggregate line
   - `pair`
   - `struct`
   - aggregate parameter / return 方向
5. float line
   - conservative transport
   - helper-backed arithmetic
   - explicit conversion
   - recursive pure-float condition follow-up

也就是说，`-extension` 现在更像：

`语言特性总开关`

而不是：

`单个 feature 的临时开关`

## 3. 当前 conservative pipeline 大概是什么样

lesson 口径上，当前 `-extension` 最好先理解成：

```text
parse
-> semantic
-> canonical IR lowering
-> minimal correctness-preserving cleanup
-> downstream lowering / backend
```

重点不是这条线里“绝对没有优化”，而是：

- 不应该依赖 aggressive mid-end cleanup 才让 feature 成立
- 先把 feature-bearing program 诚实翻译下去

所以如果你在 lesson 里看到：

- helper-backed lowering
- specialization-backed lowering
- hidden slot / hidden payload
- structural clone / replay

这通常不是“实现还不成熟”的简单同义词，而是当前 deliberate design choice：

`先要一条保守可靠的 feature path`

## 3.1 伪代码：`-extension` 模式到底在切什么

如果把这条课压成实现视角，最稳的记法不是“加了个命令行参数”，而是：

```text
compile(mode, source):
    ast = parse(source, mode)
    sema = semantic(ast, mode)

    if mode == extension:
        ir = lower_feature_honest(ast)
        ir = run_minimal_required_cleanup(ir)
    else:
        ir = lower_default_course_path(ast)
        ir = run_default_course_pipeline(ir)

    return lower_backend(ir, mode)
```

这里最重要的不是某个函数名，而是：

- `extension` 模式同时影响
  - parser/semantic admission
  - lowering strategy
  - pass boundary

也就是说，它切的是：

`功能面 + 管线风格`

不是只切：

`要不要认一个关键字`

## 3.2 什么叫“更保守的 feature path”

当前最稳的 lesson 级伪代码可以写成：

```text
lower_feature_honest(ast):
    emit explicit helper-backed / specialization-backed / slot-backed shapes
    do not assume later aggressive fold/rewrite is required for correctness
```

比如：

- `defer`
  - structural clone/replay
- function values
  - specialization helper / tag family / explicit callable-object spine
- aggregate
  - hidden-slot flatten
- float
  - helper-backed arithmetic / explicit bridge

这些都体现同一个原则：

`先把 feature-bearing source 诚实翻译成现有 IR/backend 真能承担的形状。`

## 4. 这条线和后面的 SSA/backend 是什么关系

`-extension` 不是绕开 SSA/backend。

更准确地说：

- 它仍然会进 canonical IR / lower IR / ValueSSA / machine*
- 只是当前更强调：
  - shape honesty
  - helper-backed lowering
  - 不依赖 aggressive rewrite 才正确

所以这条线最值得记的一句话是：

`-extension` 不是另一套编译器，而是同一条编译器管线上的保守 feature mode。`

## 5. 当前最重要的判断标准

如果你在看一个 language feature，最该先问的不是“它快不快”，而是：

1. 是否只在 `-extension` 下开放？
2. 当前语义边界是不是明确写死了？
3. 当前 lowering 是不是故意 helper-/specialization-/slot-backed？
4. 有没有 focused regression 把它锁住？

## 5.1 一个更实现化的判断问题

如果你在看某个 extension feature，除了“语法开没开”，更推荐问这三个实现问题：

1. lowering 是不是显式 helper/specialization/slot 模型？
2. 这个 feature 若离开某个 aggressive pass 还对不对？
3. current kept boundary 是不是在 semantic 和 IR 两侧都说清了？

因为 `-extension` 当前最重要的工程价值，正是：

`feature correctness 不再偷偷寄生在默认主线的激进重写上。`

只要这四个问题回答得清楚，这条 feature line 基本就是健康的。

## 5.2 源码/测试地图：怎么确认一个 feature 真在 `-extension` 里

读具体实现时，最实用的入口不是先找所有 feature helper，而是先找 mode gate：

1. parser / semantic
   - 看 feature 是否只在 extension mode 下 admitted
   - 常见位置是 `src/parser/`、`src/semantic/`
2. compiler driver
   - 看 `-extension` 选项怎样进入 compile pipeline
   - 常见测试在 `tests/compiler/compiler_driver_test.c`
3. IR / lower-IR regression
   - 看 feature 是否有自己的 lowering shape，而不是靠某个后续 pass 凑出来
4. extension runtime
   - 看运行时可见语义是否真的闭合
   - 常见入口是 `tools/test_extension_runtime.sh`

lesson 级检查顺序可以写成：

```text
source accepted only under -extension
  -> semantic boundary explicit
  -> canonical IR shape honest
  -> lower IR preserves the shape
  -> compiler-driver / runtime focused witness green
```

所以如果某个 feature 只在 semantic 过了，但没有 IR/lower/runtime witness，就不能把它讲成“已闭合”。

反过来，如果 default optimized path 把 helper/slot 形状折掉，也不一定是 regression。

更稳的问法是：

```text
translation-only / focused extension surfaces 是否还能看到或证明原始 feature contract
```

## 6. 读完后接哪篇

最自然往下接：

1. `defer_family_lesson.md`
2. `function_values_lesson.md`
3. `aggregate_lesson.md`
4. `float_lesson.md`
5. `function_checkpoints_lesson.md`

前四篇讲具体 feature family，第五篇讲“怎么判断已经闭合”。
