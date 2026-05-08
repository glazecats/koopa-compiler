# Machine Runtime Lessons

这条线回答的是：

`ELF / image 之后，如果真的要加载、启动、单步解释、更新状态，这条运行时主线怎么长`

当前如果只看最近几轮代码同步，这条 runtime 线最重要的背景更新其实不是“runtime 自己又多了一个新 stage”，而是：

- 上游 `machine_elf -> machine_image` 的 provenance / policy 信息现在更完整
- 所以下游 runtime lesson 现在更适合被理解成：
  - 不只是消费“某些 bytes”
  - 也是在消费一份越来越诚实的 executable/image artifact

最推荐的阅读顺序：

1. `machine_image_lesson.md`
2. `machine_exec_lesson.md`
3. `machine_load_lesson.md`
4. `machine_runtime_lesson.md`
5. `machine_launch_lesson.md`
6. `machine_step_lesson.md`
7. `machine_decode_lesson.md`
8. `machine_payload_decode_lesson.md`
9. `machine_interp_lesson.md`
10. `machine_transition_lesson.md`
11. `machine_state_lesson.md`
12. `machine_mutation_lesson.md`
13. `machine_writeback_lesson.md`
14. `machine_commit_lesson.md`
15. `machine_apply_lesson.md`

可以把这条线拆成四小段：

1. 装载准备段
   - `machine_image`
   - `machine_exec`
   - `machine_load`
2. 启动准备段
   - `machine_runtime`
   - `machine_launch`
3. 单步语义段
   - `machine_step`
   - `machine_decode`
   - `machine_payload_decode`
   - `machine_interp`
   - `machine_transition`
   - `machine_state`
4. 状态落地段
   - `machine_mutation`
   - `machine_writeback`
   - `machine_commit`
   - `machine_apply`

如果你只想知道“怎么从 ELF 走到能执行”，先读：

1. `machine_image_lesson.md`
2. `machine_exec_lesson.md`
3. `machine_load_lesson.md`
4. `machine_runtime_lesson.md`
5. `machine_launch_lesson.md`

如果你只想知道“取到一个 byte 之后如何变成状态变化”，先读：

1. `machine_step_lesson.md`
2. `machine_decode_lesson.md`
3. `machine_payload_decode_lesson.md`
4. `machine_interp_lesson.md`
5. `machine_transition_lesson.md`
6. `machine_state_lesson.md`

如果你只想知道“状态变化最后怎么落地”，再接：

1. `machine_mutation_lesson.md`
2. `machine_writeback_lesson.md`
3. `machine_commit_lesson.md`
4. `machine_apply_lesson.md`

当前这条线的 lesson 数量最多，所以最不建议跳着乱读。
