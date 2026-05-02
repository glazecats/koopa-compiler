# Core Lessons

这套 `lesson/core/` 在讲的是编译器前中端的主线，也就是：

`source text -> tokens -> AST -> semantic facts -> canonical IR -> IR passes -> lower IR -> tests`

如果你把 `lesson/machine/` 理解成 backend 主线，那么 `lesson/core/` 就是它前面的地基。

可以先把这条线粗记成：

1. `lexer`
   - 字符流怎么切成 token
2. `parser`
   - token 怎么变成语法结构
3. `ast`
   - 语法树到底长什么样
4. `semantic`
   - 名字、类型、作用域、语义约束怎么成立
5. `ir`
   - 语义之后，canonical IR 长什么样
6. `ir_pass`
   - canonical IR 怎么做清理和规范化
7. `lower_ir`
   - canonical IR 怎么继续往后端前的 lower IR 落
8. `tests`
   - 这些层怎么被测试锁住

---

## 1. 最推荐的阅读顺序

如果你第一次读，最推荐按下面顺序：

1. `lexer_lesson.md`
2. `parser_lesson.md`
3. `ast_lesson.md`
4. `semantic_lesson.md`
5. `ir_lesson.md`
6. `ir_pass_lesson.md`
7. `lower_ir_lesson.md`
8. `tests_lesson.md`

这个顺序的好处是：

- 前四篇先把“源程序怎么被理解”讲清楚
- 中间三篇再把“理解完以后怎么变成 IR，并继续整理”讲清楚
- 最后一篇再回头看“这些 contract 是怎么被测试锁住的”

---

## 2. 每篇在讲什么

### `lexer_lesson.md`

重点看：

- token 是怎么切出来的
- 哪些字符/关键字/字面量会被识别成什么 token
- lexer 的错误边界在哪

### `parser_lesson.md`

重点看：

- token 流怎么被组织成语法结构
- 表达式、语句、声明的大体形状
- parser 在什么地方决定优先级和结合性

### `ast_lesson.md`

重点看：

- AST 节点种类有哪些
- parser 产物最后怎么落到 AST 数据结构
- AST 和“源码长什么样”之间的对应关系

### `semantic_lesson.md`

重点看：

- 语义分析阶段到底补了哪些事实
- 名字解析、作用域、类型、调用约束怎么检查
- 哪些错误是在 semantic 阶段而不是 parser 阶段发现

### `ir_lesson.md`

重点看：

- canonical IR 的基本形状
- function / block / instruction / terminator 怎么组织
- 为什么 semantic 之后还要专门有一层 IR

### `ir_pass_lesson.md`

重点看：

- canonical IR 上已经有哪些 pass
- 这些 pass 在清理什么、保什么 contract
- verifier 和 pass pipeline 的关系

### `lower_ir_lesson.md`

重点看：

- canonical IR 为什么还要继续降到 lower IR
- value 和 slot 为什么要分开
- lower IR 是怎么为后面的 SSA / machine 线做准备的

### `tests_lesson.md`

重点看：

- 仓库现在主要靠哪些测试保护行为
- 单层测试和整链测试各在锁什么
- 读 lesson 时怎样反过来找测试验证理解

---

## 3. 按问题选读

### 3.1 想知道“前端最小主线怎么走”

先读：

1. `lexer_lesson.md`
2. `parser_lesson.md`
3. `ast_lesson.md`
4. `semantic_lesson.md`

这条线会回答：

- 源码怎么被切分
- 怎么被解析
- AST 怎么长
- 语义事实怎么成立

### 3.2 想知道“IR 到底是什么时候出现的”

先读：

1. `semantic_lesson.md`
2. `ir_lesson.md`
3. `ir_pass_lesson.md`

这条线会回答：

- semantic 之后为什么不直接进 backend
- canonical IR 为什么是一个单独的稳定边界
- pass 在这层具体做什么

### 3.3 想知道“lower IR 和后面的 machine 有什么关系”

先读：

1. `ir_lesson.md`
2. `ir_pass_lesson.md`
3. `lower_ir_lesson.md`
4. 然后再去 `lesson/machine/README.md`

这条线会回答：

- canonical IR 的边界是什么
- pass 把 canonical IR 整理到什么程度
- lower IR 怎么把后面的 backend 入口立起来

### 3.4 想知道“测试到底在锁什么”

先读：

1. `tests_lesson.md`
2. 然后回头对照你正在看的那一篇 lesson

这样读最容易建立：

- lesson 里的说法
- 真实测试里的行为

之间的一一对应。

---

## 4. 如果你是按阶段学

### 第一阶段：先把语言前端看懂

读：

1. `lexer_lesson.md`
2. `parser_lesson.md`
3. `ast_lesson.md`
4. `semantic_lesson.md`

### 第二阶段：再把 IR 边界看懂

读：

1. `ir_lesson.md`
2. `ir_pass_lesson.md`
3. `lower_ir_lesson.md`

### 第三阶段：最后用测试反查

读：

1. `tests_lesson.md`

这三段是最稳的节奏，不容易被后面的 backend 细节淹掉。

---

## 5. 和 machine lesson 的关系

`lesson/core/` 和 `lesson/machine/` 最好这样衔接着看：

- `core`
  - 讲“程序怎么先被理解成 IR”
- `machine`
  - 讲“IR 怎么继续往 machine/backend 落”

最自然的衔接点就是：

1. `ir_lesson.md`
2. `ir_pass_lesson.md`
3. `lower_ir_lesson.md`
4. `../machine/README.md`

也就是说，`lower_ir` 可以视为：

- `core` 的最后一站
- 也是后面 SSA / machine/backend 的上游入口

---

## 6. 现在这个目录该怎么用

如果你只是想找某一篇，直接按文件名找当然可以。

但如果你想真的建立整体感，最建议：

1. 先读这一篇 `README.md`
2. 再按推荐顺序读正文
3. 每读完一篇，就去看它提到的测试或下游 lesson

这样不会变成“md 很多，但不知道先看哪篇”。
