# Machine Observe Lessons

这条线回答的是：

`apply 之后，执行结果怎么一步步整理成 observe / delta / trace / event / outcome / history / timeline / log / journal`

最推荐的阅读顺序：

1. `machine_observe_lesson.md`
2. `machine_delta_lesson.md`
3. `machine_trace_lesson.md`
4. `machine_event_lesson.md`
5. `machine_outcome_lesson.md`
6. `machine_history_lesson.md`
7. `machine_timeline_lesson.md`
8. `machine_log_lesson.md`
9. `machine_journal_lesson.md`

可以把这条线再拆成三段：

1. 状态差异段
   - `machine_observe`
   - `machine_delta`
   - `machine_trace`
2. 结果语义段
   - `machine_event`
   - `machine_outcome`
3. 记录收束段
   - `machine_history`
   - `machine_timeline`
   - `machine_log`
   - `machine_journal`

如果你最关心“最后怎么变成一条记录”，最少先读：

1. `machine_outcome_lesson.md`
2. `machine_history_lesson.md`
3. `machine_timeline_lesson.md`
4. `machine_log_lesson.md`
5. `machine_journal_lesson.md`

如果你最关心“差异是怎么被观察到的”，就从前面开始：

1. `machine_observe_lesson.md`
2. `machine_delta_lesson.md`
3. `machine_trace_lesson.md`

这一层已经不再放在 `runtime/`，而是单独作为 `observe/` 主线继续往下长。后面如果还要补更下游的记录/汇总 lesson，也建议继续沿这个目录扩。
