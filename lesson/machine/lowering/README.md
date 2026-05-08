# Machine Lowering Lessons

这条线回答的是：

`Value-SSA / machine register model 已经准备好之后，怎么一步步落成 machine-facing artifact`

如果按当前未提交代码再补一个总览更新，这条 lowering 线现在最值得先记三件事：

1. `machine_ir`
   - bridge 不再只有 full allocate+rewrite 一条路径，已经有 conservative allocate-only / all-spill fallback
2. `machine_select`
   - selected cleanup 已经不只是 fallback；同时 indirect-memory 路线也开始有 targeted cleanup
3. `machine_encode`
   - bare return / value return contract 现在已经被显式 verifier 锁住

最推荐的阅读顺序：

1. `machine_ir_lesson.md`
2. `machine_select_lesson.md`
3. `machine_layout_lesson.md`
4. `machine_emit_lesson.md`
5. `machine_encode_lesson.md`

每篇大概在回答：

- `machine_ir`
  - 值最后住在 register / spill / immediate 哪
- `machine_select`
  - generic machine op 被选成哪类 selected op
- `machine_layout`
  - CFG block 最后怎么线性排布
- `machine_emit`
  - label / branch target 怎么显式化
- `machine_encode`
  - 每条 op / terminator 最终占多少编码单位、偏移怎么定

如果你现在最关心 `select`，最少先读：

1. `machine_ir_lesson.md`
2. `machine_select_lesson.md`

如果你现在最关心“字节从哪来”，就把：

3. `machine_layout_lesson.md`
4. `machine_emit_lesson.md`
5. `machine_encode_lesson.md`

一起接着读下去。
