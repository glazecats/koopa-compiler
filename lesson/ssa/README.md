# SSA Lessons

这条线回答的是：

`lower_ir` 之后，程序为什么还要进入 SSA，以及 SSA 怎么继续接到 allocator 和 machine backend

可以先把这条线粗记成两半：

1. `value_ssa`
   - 值流怎么被变成 SSA
2. `memory_ssa`
   - 内存版本和 memory-side pass 怎么被显式化

而 `value_ssa` 这半又会继续往下接：

- pass
- allocator
- machine register model
- SSA execution / interpretation support

---

## 1. 最推荐的阅读顺序

最推荐按下面顺序：

1. `value_ssa_lesson.md`
2. `value_ssa_pass_lesson.md`
3. `value_ssa_alloc_lesson.md`
4. `value_ssa_machine_lesson.md`
5. `value_ssa_interp_lesson.md`
6. `memory_ssa_lesson.md`
7. `memory_ssa_pass_lesson.md`

如果你是从 `core/lower_ir_lesson.md` 接过来，这个顺序最顺。

---

## 2. 每篇在讲什么

### `value_ssa_lesson.md`

重点看：

- value SSA 的基本表示
- block / phi / instruction / use-def 关系怎么长
- 为什么 lower IR 之后要先处理 value flow

### `value_ssa_pass_lesson.md`

重点看：

- SSA-native pass 在做什么
- 哪些 cleanup / analysis / transform 已经在 SSA 层出现
- 为什么这些 pass 不直接塞回 core IR pass

### `value_ssa_alloc_lesson.md`

重点看：

- allocator 现在在解决什么问题
- SSA 值怎么被映射到更接近 machine 的资源
- spill / assignment / allocation policy 在 lesson 里怎么讲

### `value_ssa_machine_lesson.md`

重点看：

- allocator 之后为什么还要有 machine register model
- SSA world 和 machine IR world 的接口在哪里
- 哪些事实会继续往 `machine_ir` 传

### `value_ssa_interp_lesson.md`

重点看：

- 为什么 SSA 这里还会有执行/解释 support
- 这层 support 在验证什么，服务谁
- 它和 machine runtime/observe 不是同一层

### `memory_ssa_lesson.md`

重点看：

- memory SSA 为什么要和 value SSA 分开
- version、phi、tracked slot、memory use/def 怎么表达
- 它主要在补哪种 value SSA 不擅长表达的事实

### `memory_ssa_pass_lesson.md`

重点看：

- Memory-SSA-backed pass 在做什么
- 哪些 load/store / dead store / memory version 事实可以在这里利用

---

## 3. 按问题选读

### 3.1 想知道“lower IR 之后为什么还要有 value SSA”

先读：

1. `value_ssa_lesson.md`
2. `value_ssa_pass_lesson.md`

### 3.2 想知道“allocator 和 machine backend 是怎么接上的”

先读：

1. `value_ssa_alloc_lesson.md`
2. `value_ssa_machine_lesson.md`
3. 然后再去 `../machine/README.md`

### 3.3 想知道“SSA 这层有没有执行 support”

先读：

1. `value_ssa_interp_lesson.md`

### 3.4 想知道“memory SSA 到底是补什么的”

先读：

1. `memory_ssa_lesson.md`
2. `memory_ssa_pass_lesson.md`

---

## 4. 和前后目录的关系

这条线最自然的上下游关系是：

- 上游：
  - `core/lower_ir_lesson.md`
- 中间：
  - `value_ssa_*`
  - `memory_ssa_*`
- 下游：
  - `machine/README.md`

也就是说，`ssa/` 最好被理解成：

- `core` 和 `machine` 之间的中间桥

它不是独立宇宙，而是在回答：

- lower IR 之后，怎么把程序整理成更适合后端的形状

---

## 5. 现在这个目录该怎么用

如果你只是看 `machine`，可能会跳过 SSA。

但如果你想真的理解：

- `machine_ir` 之前那些值/寄存器/分配事实是从哪来的

那 `ssa/` 这一整段最好不要跳。

最推荐的实际节奏通常是：

1. `core/lower_ir_lesson.md`
2. `ssa/README.md`
3. `value_ssa_lesson.md`
4. `value_ssa_alloc_lesson.md`
5. `value_ssa_machine_lesson.md`
6. `machine/README.md`
