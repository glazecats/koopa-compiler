# Value SSA Interp Lesson（compiler_lab）

> 目标：专门讲 `value_ssa_interp`。这里聚焦解释器为什么单独成模块、当前支持哪些 SSA 指令、执行状态怎么组织、以及它在测试/oracle 里的角色。

## 导学

`value_ssa_interp` 不是 pass，也不是 core representation。

它是 `value_ssa` 旁边的一个 sibling module，负责：

- 直接执行 verifier-legal SSA
- 给测试提供 oracle
- 给后续单步执行/可视化留执行语义基础

---

## 1. 为什么解释器不该放进 `value_ssa_pass`

这是最重要的边界。

`value_ssa_pass` 负责的是：

- cleanup
- rewrite
- canonicalization
- optimization

而解释器负责的是：

- 执行语义
- 状态维护
- call / return
- trace / oracle / 单步基础

所以解释器如果塞进 `value_ssa_pass/`，职责会混掉。

当前仓库已经把这个边界落实成事实：

- 接口：`include/value_ssa_interp.h`
- 实现：`src/value_ssa_interp/`

这就是正确的工程位置。

---

## 2. 文件定位

- 接口：`include/value_ssa_interp.h`
- 聚合入口：`src/value_ssa_interp/value_ssa_interp.c`
- 执行逻辑：`src/value_ssa_interp/value_ssa_interp_exec.inc`
- 执行状态：`src/value_ssa_interp/value_ssa_interp_state.inc`

相关测试：

- `tests/value_ssa/value_ssa_interp_test.c`
- `tests/value_ssa/value_ssa_oracle_test.c`

Make 目标：

- `make test-value-ssa-interp`
- `make test-value-ssa-oracle`

---

## 3. 这层当前的定位

当前这版解释器要按下面这句话理解：

`test-oriented, narrow, verifier-legal SSA interpreter`

也就是：

1. 面向测试 / oracle
2. 只吃 verifier-legal `value_ssa`
3. 边界故意收得很窄

所以它不是：

- 完整 runtime
- 完整 debugger
- memory SSA executor
- backend simulator

### 3.1 当前支持 / 不支持什么

如果把 interpreter 单独列个对照表，当前最准确的是：

**已经支持：**

- verifier-legal SSA 执行
- local/global slot 状态
- branch / phi
- internal call
- extern callback
- return value + global state 结果导出

**还没有：**

- interactive debugger UI
- 单步 trace 输出 API
- memory SSA execution
- pointer / object / heap model
- 更完整 runtime safety layer

---

## 4. 当前支持什么

当前第一版边界已经包括：

- `mov`
- `binary`
- `load_local`
- `store_local`
- `load_global`
- `store_global`
- `jmp`
- `br`
- `ret`
- 程序内函数调用
- extern-call callback

这意味着它已经足够执行：

- straight-line SSA
- branch / phi 相关的 representative case
- simple internal-call case
- 一部分带 extern stub 的 case

### 4.1 一个 straight-line 执行例子

当前最小 straight-line case 可以长成：

```text
global g.0

func main(a.0) {
  bb.0:
    ssa.0 = load_local a.0
    ssa.1 = add ssa.0, 1
    store_global g.0, ssa.1
    ret ssa.1
}
```

如果传入参数 `a = 41`，当前解释器会给出：

- `return_value = 42`
- `global_values[g.0] = 42`

这类 case 说明它已经不是只能“跑 ret immediate”，而是真能执行 slot + SSA value 混合语义。

### 4.2 一个 phi / branch 例子

phi case 也已经是第一版 authority：

```text
func main(a.0) {
  bb.0:
    ssa.0 = load_local a.0
    br ssa.0, bb.1, bb.2
  bb.1:
    ssa.1 = mov 10
    jmp bb.3
  bb.2:
    ssa.2 = mov 20
    jmp bb.3
  bb.3:
    ssa.3 = phi [bb.1: ssa.1], [bb.2: ssa.2]
    ret ssa.3
}
```

当前 interpreter 的期望语义很直接：

- `a != 0` 时返回 `10`
- `a == 0` 时返回 `20`

也就是说，它真的在消费：

- branch 选边
- predecessor-specific phi input

而不是只跑直线块。

### 4.3 一个 internal-call 例子

当前 internal-call 的代表 authority 更像：

```text
func inc(x.0) {
  bb.0:
    ssa.0 = load_local x.0
    ssa.1 = add ssa.0, 1
    ret ssa.1
}

func main() {
  bb.0:
    ssa.0 = call inc(41)
    ret ssa.0
}
```

当前解释器应该能直接返回：

- `42`

这说明它已经不只是“单函数 evaluator”，而是维护了一层真实的调用语义。

---

## 5. 当前不支持什么

当前这层仍然故意不往这些方向扩：

1. memory SSA 语义
2. target/backend 执行语义
3. 完整 debugger UI
4. 更复杂内存对象模型
5. 真正的安全检查框架

而且它对坏运行状态不是“帮你猜”，而是会直接拒绝。

最典型的例子就是：

- 未初始化 local load

这类运行时非法状态不会被偷偷修补。

---

## 6. 执行状态怎么理解

虽然这份 lesson 不逐行拆状态结构，但从职责上可以把它理解成三层：

1. **程序级状态**
   - program / function 查找
   - global slot 状态

2. **调用级状态**
   - call stack
   - 当前函数
   - 返回点

3. **帧内状态**
   - 当前 block / instruction 位置
   - local slot 当前值
   - SSA value 当前值

所以它执行的不是“源码语句”，而是：

`ValueSsaProgram` 里的 block / instruction / terminator

一个很粗的伪代码直觉可以写成：

```text
while current frame exists:
    read current block / pc
    execute instruction or terminator
    update:
      - current SSA values
      - local/global slot state
      - current block / next block
      - call stack
```

所以它和 pass 的最大区别是：

- pass 改程序
- interp 跑程序

### 6.1 执行时最容易出错的边界

这一层课上最好顺手提醒三类 runtime boundary：

1. 未初始化 local load
2. 不支持的 extern 行为
3. dangerous/trapping binary

当前策略不是“尽量容错”，而是：

`一旦越过当前第一版解释器边界，就直接报运行时错误`

---

## 7. 它和 SSA 本身是什么关系

解释器不是在重新定义 SSA 语义，而是在消费 SSA 已有语义：

- SSA value 只有一个 def
- `phi` 在控制流汇合处决定值来源
- `load_* / store_*` 继续保留 slot 语义

所以它正好也说明了为什么当前 `value_ssa` 不是 memory SSA：

- value flow 已经 SSA 化
- slot 状态仍然显式存在
- 解释器仍然要维护 local/global slot 当前值

这反而很适合做教学展示，因为：

- SSA value
- slot state
- branch / phi

三者都能同时看见。

---

## 8. 它在测试里干什么

### 8.1 `value_ssa_interp_test.c`

这份测试主要锁解释器自身边界：

- straight-line execution
- phi / branch execution
- internal-call execution
- external-call callback execution
- uninitialized local load 的拒绝路径

extern-call callback 的接口现在是：

```c
typedef int (*ValueSsaInterpExternCallFn)(const ValueSsaProgram *program,
    const char *callee_name,
    const long long *args,
    size_t arg_count,
    int has_result,
    long long *out_result,
    long long *global_values,
    size_t global_count,
    void *user_data,
    ValueSsaInterpError *error);
```

所以当前 extern 不是靠“随便假装返回一个值”，而是：

- 把 callee 名字
- 参数
- 是否需要结果
- 当前 global 状态

显式交给外部 callback。

这也是为什么它很适合做 test double / oracle stub。

一个很典型的 extern stub 例子是：

- `callee_name = "ext"`
- 输入参数是 `9`
- callback 返回 `18`

那 main 里的：

```text
ssa.0 = call ext(9)
ret ssa.0
```

就可以在不真的实现 `ext` 的情况下继续跑完整个 SSA 程序。

也就是说，它锁的是：

`解释器自己能不能正确执行 SSA`

### 8.2 `value_ssa_oracle_test.c`

这份测试更像组合边界测试。

它的角色是：

- 把解释器当执行 oracle
- 不再把“每个 pass 后应返回什么值”都手写成另一套推理
- 而是让执行结果直接参与行为对照

所以 oracle test 的意义不是“又一份 interp test”，而是：

`让 SSA 执行结果成为 conversion / canonicalization / pass 的行为参照`

最典型的用法就是：

1. 先解释执行 before-program
2. 再 canonicalize / pass
3. 再解释执行 after-program
4. 对比：
   - return value
   - global state

如果 before/after 结果一致，就说明：

- pass 改了形状
- 但没改语义

这也是 oracle test 比单纯 expected dump 更强的一点：

- dump 锁的是形状
- oracle 还能锁行为

### 8.3 一个 before/after oracle authority 例子

当前 oracle test 最适合课堂上讲的，不是“它也能跑”，而是：

`它可以证明 canonicalize 前后行为一致`

比如一条典型的 local-chain canonicalize authority，形状上可能从一串：

```text
store_local ...
load_local ...
add ..., 0
mov ...
ret ...
```

一路被 pass 收缩成：

```text
ret 8
```

但 oracle test 不会只信 dump，它会做：

1. 解释执行 before-program
2. canonicalize
3. 解释执行 after-program
4. 对比：
   - `return_value`
   - `global_values`

所以 oracle 真正锁住的是：

`程序行为没变，只是 IR 形状变得更规范`

### 8.4 一个 global-side oracle authority 例子

oracle 另一类很有价值的 case 是 global + extern callback。

比如：

```text
store_global g.0, 1
call touch()
store_global g.0, 2
ret 2
```

这里 shape 上当前 canonicalization 会做一些局部清理，但 oracle 真正验证的是：

- `touch()` 作为 extern callback 仍然被执行
- before/after 的 global state 一致
- return value 一致

这类 case 正好把三层接口接在一起：

- `value_ssa_pass`
- `value_ssa_interp`
- `value_ssa_oracle_test`

---

## 9. 为什么这层对课程展示有价值

如果后面要做：

- 单步执行
- trace
- 可视化
- memory checking

解释器会是非常自然的基础层。

因为这些功能都需要：

- 一个真实执行状态
- 当前控制流位置
- 当前 call stack
- 当前 local/global / SSA value 状态

也就是说，`value_ssa_interp` 现在虽然只是第一版，但它已经把“后续动态展示”最难的那层语义地基搭出来了。

---

## 10. 一句话总结

`value_ssa_interp` 现在是一个独立的 sibling execution module：它不做优化，而是直接执行 verifier-legal SSA，给测试、oracle 和后续动态可视化提供统一执行语义。
