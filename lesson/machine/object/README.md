# Machine Object Lessons

这条线回答的是：

`layout / emit / encode 之后，最终 bytes、object、reloc、container、ELF 怎么长出来`

最推荐的阅读顺序：

1. `machine_bytes_lesson.md`
2. `machine_object_lesson.md`
3. `machine_reloc_lesson.md`
4. `machine_container_lesson.md`
5. `machine_elf_lesson.md`

每篇大概在回答：

- `machine_bytes`
  - 指令最终是什么字节
- `machine_object`
  - 字节怎么变成 section / symbol / fixup 容器
- `machine_reloc`
  - 哪些地方还要 relocation、怎么标出来
- `machine_container`
  - 仓库内部最终容器字节怎么组织
- `machine_elf`
  - 最后怎么投影成标准 ELF object

如果你只想抓最关键的两篇，优先：

1. `machine_bytes_lesson.md`
2. `machine_elf_lesson.md`

中间三篇更多是在解释：

- object 内部容器怎么逐层收束到最终标准格式
