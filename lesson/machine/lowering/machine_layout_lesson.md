# Machine Layout Lesson（compiler_lab）

> 目标：解释 `machine_layout` 为什么在 `machine_select` 之后还必须单独存在，它到底在决定什么，`src/machine/lowering/machine_layout/` 里那套 block-order / fallthrough heuristic 是怎么工作的，以及这层为什么是后面 `machine_emit` / `machine_encode` / `machine_bytes` 的必要前置层。

## 一句话定位

`machine_layout` 是 selected CFG 正式进入“线性 block 顺序 + fallthrough 视角”的那一层。

## 常见误解

- 误解一：layout 只是把 block 随便排一下。
  - 当前这层在决定 block order、fallthrough、shared tail defer、seed ordering 等真实后端前置语义。
- 误解二：layout 属于 emit 的一部分。
  - emit 依赖 layout 结果，但 layout 自己是独立的 block-order / control presentation 层。

## 导学

`machine_select` 之后，程序虽然已经有 selected ops 了，但它还是：

- CFG 形状
- block 还是逻辑块
- control transfer 还没变成“线性布局后”的视角

所以我们还需要一层专门回答：

- block 最后按什么顺序排？
- 哪条边能直接 fallthrough？
- 哪条边必须保留显式 jump / branch？

这就是 `machine_layout`。

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_select` 之后还不能直接去 `machine_emit`
2. 再看 `include/machine/layout.h`，建立 layout terminator family 的整体印象
3. 再看 `src/machine/lowering/machine_layout/` 的文件分工，理解真正的 heuristic 在哪
4. 最后带着最小例子和伪代码去看：
   - block order 怎么选
   - fallthrough 是怎么确定的
   - 为什么 shared-tail defer / seed ordering 会单独存在

学完你应该能：

1. 解释 `machine_layout` 和 `machine_select` / `machine_emit` 的边界
2. 说明 layout 层相对 selected CFG 新增的到底是什么信息
3. 能看懂一个 `machine_select -> machine_layout` 的 before/after 例子
4. 能说明当前 heuristic 在源码里真实是怎么跑的

---

## 前置阅读

最推荐先读：

1. `lesson/machine/lowering/machine_select_lesson.md`
2. `lesson/machine/lowering/machine_ir_lesson.md`

因为 layout 默认前提就是：

- selected op / terminator family 已经形成
- 现在要解决的是 block order 和 fallthrough，而不是“选成什么 op”

如果上游两层没先读，layout 很容易被误看成 select 的附属小优化。

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/lowering/machine_emit_lesson.md`
2. `lesson/machine/lowering/machine_encode_lesson.md`

因为 layout 之后最自然的问题就是：

- 线性 block 怎么继续显式化 label / control surface
- 这些布局结果怎么继续变成稳定 offset / encoding-prep artifact

---

## 1. 为什么需要 `machine_layout`

`machine_select` 已经回答了：

- op family 是什么
- terminator family 是什么
- compare 和 branch 是否已经被合成
- call / store / return 有没有 immediate-specialized shape

但 `machine_select` 还没有回答：

- 这些 block 最后按什么顺序出现？
- 哪条 branch arm 正好落在“下一块”？
- 哪条 jump 已经没有必要保留显式跳转？

也就是说，`machine_select` 还是：

`selected CFG`

而不是：

`linearized backend entry artifact`

所以 `machine_layout` 引入的真正边界是：

`selected CFG -> explicit layout order + fallthrough-aware control presentation`

---

## 2. 为什么不能把这层塞回 `machine_select`

这一点很重要。

`machine_select` 负责的是：

- selected op family
- selected terminator family
- selected-side local cleanup

它不应该再继续负责：

- block order policy
- fallthrough exploitation
- shared merge tail 何时延后
- function-level seed stitching

不然 `machine_select` 会变成：

`既是 operation selector，又是 block layout engine`

这样会把边界揉坏。

项目现在的拆法很清楚：

- `machine_select`
  - 回答“块里放什么”
- `machine_layout`
  - 回答“块按什么顺序排，哪些边能直接 fallthrough”
- `machine_emit`
  - 回答“这些 laid-out block 用什么稳定 label 表示”

所以这一层当前的准确身份是：

`first linear-layout authority`

---

## 3. 文件定位

- 接口：`include/machine/layout.h`
- 实现：`src/machine/lowering/machine_layout/`
- 测试：`tests/machine/lowering/machine_layout/`
- 规划文档：`docs/backend/MACHINE_LAYOUT_PLAN.md`

当前目录很小，但已经不是“一个文件随便写点逻辑”：

- `machine_layout.c`
- `machine_layout_core.inc`
- `machine_layout_lower.inc`
- `machine_layout_verify.inc`
- `machine_layout_dump.inc`

所以这层虽然实现体量不算大，但已经是一个真实 module，而不是草图。

---

## 4. `src/machine/lowering/machine_layout/` 目录怎么读

最简单的读法不是按文件名硬背，而是按三组职责去记。

### 4.1 core / artifact 基础层

- `machine_layout_core.inc`
- `machine_layout_verify.inc`
- `machine_layout_dump.inc`

这几块负责：

- 生命周期
- verifier
- dump
- summary helper

也就是说，它们在回答：

`layout artifact 怎么被建、被验、被看`

### 4.2 lowering + heuristic 主线

- `machine_layout_lower.inc`

真正的 layout policy 几乎都在这一个分片里，包括：

- predecessor count
- trace span
- successor ordering
- defer shared tail
- next seed picking
- linear block order construction
- selected terminator -> layout terminator lowering

所以 lesson 里最应该重点讲的，其实就是这一块。

### 4.3 聚合入口

- `machine_layout.c`

这一块主要负责把 split includes 聚起来，把公共 API 收口到一个编译单元里。

---

## 5. `include/machine/layout.h`：当前数据模型

### 5.1 operand / op 本体基本保持不变

这一层没有重新发明一套 operand，也没有重新发明一套 op。

当前：

- `MachineLayoutOperand` 其实就是 `MachineSelectOperand`
- `MachineLayoutOp` 其实就是 `MachineSelectOp`

所以这层并没有改：

- 值住在 register / spill / immediate 的事实
- selected op family 本体

这也说明它的重点不在“op 选得更细”，而在：

`control-flow presentation`

### 5.2 真正变化的是 terminator family

这一层最大的结构性变化，是 terminator 被改成了 layout-aware family：

- `RETURN`
- `RETURN_IMM`
- `RETURN_SPILL`
- `FALLTHROUGH`
- `JUMP`
- `BRANCH`
- `BRANCH_FALLTHROUGH`
- `COMPARE_BRANCH`
- `COMPARE_BRANCH_IMM`
- `COMPARE_BRANCH_FALLTHROUGH`
- `COMPARE_BRANCH_IMM_FALLTHROUGH`

也就是说，到了这层之后，我们不再只关心：

- then target / else target

而开始显式区分：

- taken edge
- fallthrough edge

### 5.3 block identity 也变了

`MachineLayoutBlock` 里现在会同时带：

- `layout_index`
- `original_block_id`

这点非常关键，因为它说明：

- block 的“原始 CFG 身份”还保留着
- 但它已经额外拥有一个“在线性布局中的位置身份”

所以这层新增的，不只是 terminator 变体，而是：

`显式 layout order`

---

## 6. 这层现在是什么，不是什么

### 当前它是什么

当前 `machine_layout` 的角色是：

1. 消费 `MachineSelectProgram`
2. 给 block 决定线性 layout order
3. 把 control transfer lower 成带 fallthrough 语义的 terminator family
4. 保留 selected op stream 不丢失
5. 提供 verifier / dump / summary

### 当前它不是什么

它不是：

1. label 层
2. offset 层
3. byte 层
4. final encoding 层

所以最准确的理解是：

`selected CFG -> linearized layout program`

---

## 7. 输入契约：它更喜欢吃已经 canonicalized 的 selected program

这一点和 `machine_select` 的 lesson 很像，也很重要。

当前更推荐的主线不是：

`raw machine_ir -> raw machine_select -> layout`

而是：

`machine_ir canonicalize -> machine_select -> machine_layout`

换句话说，这层更喜欢吃的是：

- selected shape 已经比较规整
- trivial family cleanup 已经做过
- upstream CFG 里一些 wrapper/jump cleanup 已经发生

的输入。

所以 current bridge contract 其实是：

`layout 通常站在 canonicalized selected CFG 上工作`

这也是为什么测试口径里会专门区分：

- direct `machine_select -> machine_layout`
- canonicalized `machine_ir -> machine_select -> machine_layout`

这两种 authority。

---

## 8. Lowering 总体策略：它先排序，再重写 terminator

如果把 `machine_layout_lower.inc` 的精神压成一段 lesson 级伪代码，大致就是：

```text
for each function:
  compute predecessor counts
  compute trace spans

  start from entry block
  greedily grow a local trace
  defer some shared merge tails

  while still have unvisited blocks:
    pick next seed block by seed-class ordering
    continue growing traces

  once final layout order is known:
    copy selected ops into layout blocks
    lower selected terminators into:
      fallthrough / jump / branch_fallthrough / cmpbr_fallthrough / ...
```

所以这层的形态很清楚：

1. 先决定 block order
2. 再根据“谁是下一块”重写 terminator

它不是一边随便排，一边随机改 terminator。

---

## 9. heuristic 第一步：predecessor count 和 trace span 到底在干什么

当前 `machine_layout_lower.inc` 里先算了两类很关键的信息：

### 9.1 predecessor count

它会先算每个 block 有多少 predecessor。

这个信息后面会被拿来判断：

- 某个 successor 是不是 single-predecessor
- 某个 block 是不是 shared merge tail
- 一个 seed 是 ready continuation 还是 ready shared merge

所以 predecessor count 在这层不是附带统计，而是 heuristic 输入。

### 9.2 trace span

它还会递归估计每个 block 沿着 single-predecessor chain 往下能拖出多长 trace。

lesson 口径上可以先把它理解成：

`如果从这个 block 往下走，局部直链能走多长`

这个值后面会被用来：

- 同分 successor 之间的 tie-break
- seed picking 时的后续优先级比较

所以 trace span 不是为了 dump 好看，而是为了：

`让 layout 偏向更长的局部直链`

---

## 10. heuristic 第二步：branch 两个 successor 先选谁

这一步是 `machine_layout` 最值得讲的局部策略。

当前它不是简单按：

- then before else

或者：

- else before then

而是会做一轮 local successor ordering。

### 10.1 single-predecessor preference

如果某个 successor 是 single-predecessor，它会有更高分。

lesson 口径上，这可以理解成：

`优先把更像“局部直链延续”的那一边放成 fallthrough`

### 10.2 chain-extending preference

如果某个 successor 自己后面还能继续长出 trace，它也会更占便宜。

也就是说，layout 不只看一跳，而是在 very local 的范围里看：

- “这边接过去像不像一条顺手的链”

### 10.3 trace-span tie-break

如果前面的局部评分差不多，就继续看 trace span：

- 哪一边通向的局部 trace 更长

那么哪一边更容易被选成首选 successor。

所以它不是固定顺序，而是：

`branch-local heuristic order`

---

## 11. heuristic 第三步：shared-tail defer 到底是什么意思

这也是这层最容易让人迷糊的点。

假设 CFG 长这样：

```text
bb.0:
  br cond, bb.1, bb.2
bb.1:
  jmp bb.3
bb.2:
  jmp bb.3
bb.3:
  ret
```

如果 layout 太贪心，可能会：

1. 从 `bb.0` 进 `bb.1`
2. 然后立刻冲进 `bb.3`
3. 结果把 `bb.2` 晾在外面

当前 heuristic 明确在避免这种情况。

它会识别：

- 某个 successor 其实只是“立刻通向 shared merge tail”

然后选择：

- 先 defer 这个 shared tail
- 让 branch arms 先各自排出来

lesson 口径上可以直接把它讲成：

`不要让 shared merge tail 过早吞掉局部 branch-arm trace`

这是当前 layout policy 最有辨识度的一条规则之一。

---

## 12. heuristic 第四步：seed picking 到底在比什么

当 entry trace 跑完之后，可能还有一堆 block 因为 shared-tail defer 等原因没排进去。

这时 layout 要决定：

- 下一个从哪个 block 继续起新 trace

当前 `machine_layout_pick_next_seed(...)` 不是随便找一个未访问块，而是按 seed class 选。

它至少区分：

- `DETACHED`
- `ATTACHED_UNREADY`
- `READY_CONTINUATION`
- `READY_SHARED_MERGE`

### 12.1 ready shared merge

如果某个 shared merge tail 的所有 predecessor 都已经 visited 了，它会被当成：

- `READY_SHARED_MERGE`

这说明这个 shared tail 现在终于可以安全拼回来了。

### 12.2 ready continuation

如果某个 block 的 predecessor 已经都铺好了，但它不是 shared merge，也会被看成：

- `READY_CONTINUATION`

### 12.3 freshness / visited predecessor / trace span tie-break

如果 seed class 一样，当前 heuristic 还会再看：

- latest visited predecessor index
- visited predecessor count
- trace span
- block id

lesson 口径上可以压缩成：

`先看种类，再看谁跟当前刚铺好的区域更“新鲜”更“接得上”`

所以 seed picking 不是单纯的“补洞”，而是：

`function-level stitching policy`

### 12.4 源码里其实把这几类 seed 明着编码成了 enum

这一点很适合在 lesson 里直接点出来，因为它能帮你把“课堂术语”和“源码术语”对上。

`src/machine/lowering/machine_layout/machine_layout_lower.inc` 里现在有：

```c
typedef enum {
    MACHINE_LAYOUT_SEED_DETACHED = 0,
    MACHINE_LAYOUT_SEED_ATTACHED_UNREADY,
    MACHINE_LAYOUT_SEED_READY_CONTINUATION,
    MACHINE_LAYOUT_SEED_READY_SHARED_MERGE,
} MachineLayoutSeedClass;
```

它几乎就和我们前面讲的四种语义一一对应：

- `DETACHED`
  - 还没和当前 layout 尾部发生有效连接
- `ATTACHED_UNREADY`
  - 已经挂上来了，但时机还不成熟
- `READY_CONTINUATION`
  - 可以立刻接到当前线性布局尾部
- `READY_SHARED_MERGE`
  - 一个已经 ready 的共享 merge tail

所以 lesson 前面那套 “ready continuation / ready shared merge / attached-unready / detached” 的讲法，不是抽象比喻，而是源码里真实存在的分类。

---

## 13. terminator lowering：layout order 决定了 control-flow presentation

当 block order 确定后，`machine_layout` 才开始真正改 terminator family。

### 13.1 jump -> fallthrough

如果某个 `JUMP` 的 target 正好就是下一块：

```text
layout.i -> layout.i+1
```

那就会改成：

- `FALLTHROUGH`

### 13.2 branch -> branch_fallthrough

如果 `BRANCH` 的 then 或 else 恰好是下一块，就会选：

- `BRANCH_FALLTHROUGH`

并显式记录：

- taken target
- fallthrough target
- `branch_on_true`

### 13.3 compare-branch 也有自己的 fallthrough family

同样地：

- `COMPARE_BRANCH`
- `COMPARE_BRANCH_IMM`

也会在“某个 arm 正好是下一块”时进入：

- `COMPARE_BRANCH_FALLTHROUGH`
- `COMPARE_BRANCH_IMM_FALLTHROUGH`

所以这层真正新增的是：

`fallthrough-aware control semantics`

---

## 14. 一个最小 before/after 例子：plain branch

假设 `machine_select` 还是纯 CFG 视角：

```text
bb.0:
  br reg.0, bb.1, bb.2
bb.1:
  reti 1
bb.2:
  reti 2
```

如果 layout 最后把顺序排成：

- `bb.0`
- `bb.2`
- `bb.1`

那么 `machine_layout` 之后更接近：

```text
layout.0 -> bb.0:
  brft.t reg.0, taken=layout.2, fallthrough=layout.1
layout.1 -> bb.2:
  reti 2
layout.2 -> bb.1:
  reti 1
```

这里真正新增的是：

- `layout_index`
- fallthrough target
- taken/fallthrough 角色分化

而不是新的 value op。

---

## 15. 一个最小 before/after 例子：compare branch

假设 `machine_select` 里已经是：

```text
bb.0:
  cmpbri.t ne reg.0, 0, bb.1, bb.2
bb.1:
  reti 1
bb.2:
  reti 2
```

如果 layout 把 `bb.2` 放在 `bb.0` 后面，那么结果更接近：

```text
layout.0 -> bb.0:
  cmpbrift.t ne reg.0, 0, taken=layout.2, fallthrough=layout.1
layout.1 -> bb.2:
  reti 2
layout.2 -> bb.1:
  reti 1
```

也就是说：

- compare family 还在
- 但 control presentation 已经被 layout 重新编码成 fallthrough-aware family

这说明 `machine_layout` 的工作不是重新选 op，而是：

`在不改 selected op family 的前提下，改写 control-flow presentation`

---

## 16. verifier：这层到底在锁什么

`machine_layout_verify.inc` 当前锁的，重点不是 target legality，而是：

`layout artifact 自己的结构合法性`

至少包括这些事情：

1. layout index 必须 dense
2. block 必须有 terminator
3. operand / spill / slot 引用必须合法
4. selected op family 传下来的形状不能被 layout 弄坏
5. return / branch / compare-branch / fallthrough family 要和 payload shape 对上

举个很典型的例子：

如果某个 layout terminator 被标成：

```text
RETURN_IMM
```

但 return value 实际上不是 immediate，那么 verifier 会直接把它当成 layout-family contract 破坏。

也就是说，这层 verifier 锁的不是：

- “控制流大概能跑”

而是：

- “既然你已经进入 `machine_layout`，那 terminator family 和 layout payload 就必须对应”

---

## 17. summary / dump：为什么这层已经是后续 consumer 的稳定输入

`machine_layout` 当前没有像 `machine_select` 那样重的 query/report 层，但它已经有：

- function summary
- verifier-backed dump

function summary 现在会明确统计：

- jump count
- fallthrough count
- branch count
- branch-fallthrough count
- compare-branch count
- compare-branch-fallthrough count
- return family count

这件事很重要，因为后续的 `machine_emit` / `machine_encode` 真正关心的，已经不再是：

- “这是 plain CFG 还是 selected CFG”

而是：

- “当前 layout 里到底有多少显式 jump、多少 fallthrough、多少 compare-branch-fallthrough”

所以这层 summary 已经在给后续 emission/encoding consumer 提供结构化入口。

---

## 18. 测试现在在锁什么

当前 `tests/machine/lowering/machine_layout/machine_layout_test.c` 这条线，lesson 口径上至少应该理解成在锁这些 authority：

1. `machine_select -> machine_layout` lowering
2. `machine_ir canonicalize -> machine_select -> machine_layout` bridge
3. branch / compare-branch 的 fallthrough-aware lowering
4. branch reorder 行为
5. summary 统计行为
6. shared-tail / seed-order / compare-family sibling heuristic authority

所以这组测试锁的不是“能不能把 terminator 打印出来”，而是：

`heuristic choice + layout shape + bridge behavior`

一起锁。

### 18.1 branch reorder / fallthrough 代表 case

最适合先看的几条测试名是：

- `test_machine_layout_reorders_blocks_to_create_branch_fallthrough`
- `test_machine_layout_reorders_blocks_to_create_branch_fallthrough_for_compare_family`
- `test_machine_layout_bridge_reorders_blocks_to_create_branch_fallthrough`
- `test_machine_layout_bridge_reorders_blocks_to_create_branch_fallthrough_for_compare_family`

这组测试锁的是：

- layout 不是机械保留源 block 顺序
- 它会为了制造 fallthrough，主动重排 successor
- 这件事对 plain branch 和 compare family 都成立
- bridge 入口下也要成立

### 18.2 single-predecessor / trace-span 代表 case

下一组很值得读的是：

- `test_machine_layout_prefers_single_predecessor_fallthrough_chain`
- `test_machine_layout_prefers_single_predecessor_fallthrough_chain_for_compare_family`
- `test_machine_layout_prefers_longer_trace_span_when_local_scores_tie`
- `test_machine_layout_prefers_longer_trace_span_when_local_scores_tie_for_compare_family`

这组测试正好对应我们前面讲的：

- single-predecessor preference
- chain-extending preference
- trace-span tie-break

所以你如果想确认“trace span 在这层是不是只写在计划文档里”，这几条测试就是最直接的答案。

### 18.3 shared-tail defer / seed picking 代表 case

这一组最能说明这层不是简单 DFS：

- `test_machine_layout_defers_shared_merge_tail_until_after_branch_arms`
- `test_machine_layout_defers_shared_merge_tail_for_compare_branch`
- `test_machine_layout_prefers_ready_merge_seed_over_longer_unready_trace`
- `test_machine_layout_prefers_ready_merge_seed_over_longer_unready_trace_for_compare_family`
- `test_machine_layout_prefers_layout_tail_ready_merge_when_multiple_ready_merges_exist`
- `test_machine_layout_prefers_layout_tail_ready_merge_when_multiple_ready_merges_exist_for_compare_family`
- `test_machine_layout_prefers_fresher_ready_continuation_over_older_longer_trace`
- `test_machine_layout_prefers_fresher_ready_continuation_for_compare_family`

这组测试共同锁的是：

- shared tail 可以 defer
- ready merge seed 可以压过 longer-but-unready trace
- 多个 ready merge 并存时，还要比较和当前 layout tail 的贴近程度
- compare family 不是 branch family 的“以后再说”，而是现在就有完整 sibling coverage

### 18.4 bridge / canonicalization 代表 case

这一组非常适合提醒自己“bridge authority 不是 direct authority 的逐字复刻”：

- `test_machine_layout_bridge_respects_upstream_jump_canonicalization`
- `test_machine_layout_bridge_respects_upstream_canonicalization_of_effectful_shared_tail_shape`
- `test_machine_layout_bridge_defers_effectful_compare_shared_tail`
- `test_machine_layout_bridge_avoids_counting_deferred_shared_tail_as_local_trace`
- `test_machine_layout_bridge_avoids_counting_deferred_compare_shared_tail_as_local_trace`

它们锁的是：

- 上游 canonicalization 可以先折掉一些 block
- `machine_layout` 接收到的 bridge 输入，不一定和 direct selected CFG 一样大
- 所以 bridge 测试锁的是“canonicalized 之后 surviving shape 上 heuristic 还成不成立”

这点在讲课时最好明确说出来，不然读者很容易误以为 direct case 和 bridge case 只是入口名字不同。

---

## 19. 和下一层 `machine_emit` 以及最终 `RISC-V` 方向的关系

这一层和下一层的边界也值得单独讲一下。

### 19.1 相对 `machine_emit`

`machine_layout` 到这里为止，已经回答了：

- block 顺序
- 哪条 edge 做 fallthrough
- 哪些 branch family 要带 `taken/fallthrough`

但它还没有回答：

- block 最终叫什么 label
- label 是否全程序唯一
- 后续 offset / byte / fixup 怎么引用这些 block

这些都是 `machine_emit` 才开始正式回答的问题。

所以可以压成一句话：

- `machine_layout`：排顺序
- `machine_emit`：起名字

### 19.2 和最终 `RISC-V` 方向的关系

根据：

- `docs/ir-conventions.md`
- `docs/NEXT_STEPS.md`

仓库最终目标方向仍然是：

- `RISC-V`

所以虽然当前 `machine_layout` 还没有进入真正的 RISC-V branch-distance / relaxation / encoding，但它已经在做一件对未来 RISC-V 很重要的事：

- 先把控制流整理成稳定的线性 block 顺序
- 先把 taken edge / fallthrough edge 的角色固定下来

这一步越稳定，后面真正进：

- emitted label
- offset assignment
- byte encoding
- 以及未来更具体的 RISC-V branch shaping

就越不会反复回头改 CFG 的展示方式。

所以它不是最终 RISC-V lowering 本身，但它确实是：

`未来 RISC-V 控制流落地之前的必要整理层`

---

## 20. 你现在读这层时最该抓住的主线

建议抓这五句：

1. `machine_select` 决定 op family，`machine_layout` 决定 block order
2. 这层最大的新增信息是 `layout_index + fallthrough-aware terminator`
3. heuristic 的重点是 single-predecessor / trace-span / shared-tail defer / seed picking
4. terminator lowering 发生在 layout order 已经确定之后
5. 这层已经是 `machine_emit` 之前的稳定线性布局输入层

---

## 21. 一句话总结

`machine_layout` 负责把 selected CFG 变成有显式 layout order 和 fallthrough-aware control semantics 的 layout program；它的关键不只是“排块”，而是用一套真实的 local-trace / shared-tail / seed-stitching heuristic，把后续 emitter 需要的线性控制流形状先稳定下来。
