# Function Terminology Lesson（compiler_lab）

> 目标：统一当前 function 主线里最容易混掉的一批词：`shape`、`descriptor`、`family`、`target-set`、`payload`、`rebuild`、`helper-backed`、`specialization`。这篇不主讲某个 feature，而是主讲“我们现在到底在用哪些实现词汇描述 function 世界”。

## 一句话定位

这条线回答的是：

`如果你在读 function 相关 lesson、源码、NEXT_STEPS 和记测试名时老觉得术语像，但不完全一样，那这些词现在在仓库里各自到底是什么意思。`

## 1. 为什么这篇值得单独存在

当前 function 主线已经很宽：

- noncapturing
- returned noncapturing
- closure
- returned closure
- closure captures callable
- mixed producer / mixed consumer
- callable-object IR

这导致一个新问题：

`不是功能不够多，而是词越来越多，容易混词。`

最常见的混淆就是：

1. `shape` 和 `descriptor`
2. `family` 和 `target`
3. `payload` 和 `env`
4. `rebuild` 和 `dispatch`
5. `helper-backed` 和 `specialization`

所以这篇更像 function 主线的术语表。

## 2. `shape`

最稳的 lesson 级定义是：

```text
shape = callable 的静态函数签名形状
```

也就是至少包括：

1. return kind
2. parameter count
3. parameter kinds
4. nested function parameter shape

在 lesson 里看到：

- `shape.0`
- `function shape`
- `shape compatibility`

大多都在讲这件事。

一句更具体的话：

`shape` 回答的是 “这个 callable 应该接收什么参数、返回什么类型”。

## 3. `descriptor`

最稳的 lesson 级定义是：

```text
descriptor = 当前这一层为了表示某个 callable 值/绑定/返回结果而构造的内部描述对象
```

在 semantic 侧，它更像：

- type descriptor

在 lowering 侧，它更像：

- function object descriptor

也就是说：

`shape` 更偏“静态签名事实”，`descriptor` 更偏“当前实现层拿来描述/运输这个 callable 的对象”。`

## 4. `family`

最稳的定义是：

```text
family = 一类同源/同策略的 callable producer 或 returned/captured line
```

比如：

- returned noncapturing family
- returned closure family
- two-target dynamic family

`family` 强调的是：

`这不是一个具体 target name，而是一整个可共同处理的来源族。`

## 5. `target-set`

这是当前 function 主线一个非常关键的词。

最稳的定义是：

```text
target-set = 某个 binding 在当前位置可能指向的 target family 集合
```

也就是说：

- 不是一个单一 target
- 是“目前还可能是哪些”

所以 lesson 里看到：

- clone target set
- merge union
- branch family preserve

其实都在讲：

`动态 line 现在已经在维护 binding -> possible callable families`

## 6. `payload`

最稳的定义是：

```text
payload = callable 跨 binding / return / capture 边界时，需要被运输的那部分运行时内容
```

常见成分包括：

1. dynamic tag
2. copied capture slots
3. captured callable tag

也就是说：

`payload` 不是“整个 callable object”，而是 transport 过程中要先被装起来的那部分内容。`

## 7. `env`

`env` 最稳的 lesson 级定义是：

```text
env = callable object 在真正调用时携带给 wrapper/closure helper 的环境视图
```

所以：

- `payload`
  - 更偏 transport
- `env`
  - 更偏 callable object 已经 materialize 之后给 call 用的环境

很多时候两者来自同一批 slot，但语义阶段不同。

## 8. `rebuild`

最稳的定义是：

```text
rebuild = 从 transported payload / local binding state / returned result 重新构造一个可消费的 callable view 或 object
```

典型脑图就是：

```text
payload
  -> rebuild
  -> fn_make/code/env/shape
  -> call_indirect or later collapse
```

所以 `rebuild` 讲的是：

`值跨边界之后，调用前怎么重新恢复成 callable。`

## 9. `dispatch`

`dispatch` 和 `rebuild` 很像，但不一样。

最稳的定义是：

```text
dispatch = 当 callable family 还不唯一时，如何根据 tag/branch 选到正确 leaf call 路径
```

也就是说：

- `rebuild`
  - 更偏“把 object/view 拼回来”
- `dispatch`
  - 更偏“现在到底走哪条分支/leaf helper”

## 10. `helper-backed`

最稳的定义是：

```text
helper-backed = 当前 feature 不是直接靠现有 IR/backend 原生能力完成，而是先借一个显式 helper / wrapper / builtin bridge 存活
```

最常见的两条线就是：

1. function
   - `__fnwrap_*`
2. float
   - `__builtin_fadd32` / `__builtin_i2f32`

所以 lesson 里如果说“helper-backed”，一般是在强调：

`当前是诚实 bridge，不是假装底层早就原生支持。`

## 11. `specialization`

最稳的定义是：

```text
specialization = 当某个 callable binding 已知到足够窄时，提前生成或选择一个更具体的 helper/callee 版本
```

比如：

```text
apply(add1, 41)
  -> apply__fv_0_add1(41)
```

所以：

- `specialization`
  - 更偏静态 concretize
- `dispatch`
  - 更偏运行时分支选择

## 12. 一张统一词汇图

如果把这些词压成一张最实用的图，推荐记成：

```text
shape
  -> callable 静态签名

descriptor
  -> 当前层用来描述 callable 的内部对象

family / target-set
  -> 当前可能属于哪一类 target / 哪几类 target

payload
  -> 跨边界要运输的运行时内容

payload view
  -> 同一份 returned payload 在不同 consumer 位置上的视图，比如 __retclosure_immediate / argslot / argcap

rebuild
  -> 从 payload/local state 恢复 callable view

dispatch
  -> 在多个可能 family 里选 leaf path

helper-backed / specialization
  -> 当前仓库用来让 feature 先诚实活下来的两种典型工程策略
```

## 13. 读完后接哪篇

最自然往下接：

1. `function_values_lesson.md`
2. `function_family_table_lesson.md`
3. `returned_closure_transport_lesson.md`
4. `callable_object_ir_lesson.md`
5. `function_evaluation_reuse_lesson.md`
6. `function_implementation_map_lesson.md`
7. `function_checkpoints_lesson.md`

因为术语统一之后，再回去看：

- 总图
- family 表
- returned payload view
- explicit callable-object contract
- 求值复用边界
- 源码地图
- checkpoint 地图

会顺很多。
