# Lexer Lesson (compiler_lab)

> 目标：把 lexer 当成“可解释的状态机”来理解，而不只是记住函数名。

## 导学

这份讲义的建议学习方式：

1. 先建立词法契约：token 类型、位置信息、数组状态。
2. 再跟主循环：字符分流、最大吞噬、注释与错误处理。
3. 最后回到测试视角：如何用断言锁住边界行为。

学完你应该能：

1. 能解释 lexer 如何把字符流映射为 token 流。
2. 能说明最大吞噬策略与注释处理为什么要这么写。
3. 能用回归测试模板锁住一个词法边界行为。

## 1. 文件定位

- 头文件: `include/lexer.h`
- 实现文件: `src/lexer/lexer.c`
- 主要测试: `tests/lexer/lexer_regression_test.c`

目标：把源码字符流 `source` 映射为 token 序列 `TokenArray`。

$$
\mathrm{Lex}: \Sigma^* \rightarrow T^*
$$

其中 $\Sigma^*$ 表示字符序列集合，$T^*$ 表示 token 序列集合。

---

## 2. `lexer.h` 的实现契约

### 2.1 `TokenType`（词法类型系统）

`TokenType` 把词法单元分层：

- 控制类：`TOKEN_EOF`, `TOKEN_INVALID`
- 基础类：`TOKEN_IDENTIFIER`, `TOKEN_NUMBER`
- 关键字：`int/return/if/else/while/for/break/continue`
- 运算符：`+ - * / % ! ~ & ^ | && || = == != < <= > >= << >>` 及复合赋值
- 符号：`() {} ; , ? :`

分类函数：

$$
\mathrm{classify}(\text{lexeme})=\text{token\_type}
$$

### 2.2 `Token`（单个 token 载体）

字段含义：

- `type`: token 类型
- `lexeme`: 指向源码中的起始地址
- `length`: token 长度
- `number_value`: 数值字面量值（仅数字 token 有意义）
- `line`, `column`: 1-based 源码位置

token 对应源码区间（半开区间）：

$$
\mathrm{span}(t)=[t.\text{lexeme},\ t.\text{lexeme}+t.\text{length})
$$

### 2.3 `TokenArray`（动态数组 + 合法性哨兵）

- `magic`: 初始化哨兵，防止未初始化对象被误用
- `data`, `size`, `capacity`: 动态数组三元组

### 2.4 API 语义

- `lexer_init_tokens`: 初始化为安全空状态
- `lexer_tokenize`: 扫描源码并输出 token
- `lexer_free_tokens`: 释放内存并重置
- `lexer_token_type_name`: token 类型字符串映射

---

## 3. `lexer.c` 的状态机与内存结构

### 3.1 `Lexer` 结构

`Lexer` 保存扫描状态：

- `current`: 当前字符指针
- `line`, `column`: 当前位置
- `tokens`: 输出缓冲区

状态建模：

$$
S=(p,\ell,c,\text{out})
$$

其中 $p$ 是当前指针。

### 3.2 动态数组管理（实现 + 伪代码）

- `token_array_reserve`: `realloc` 到新容量
- `token_array_next_capacity`: 计算下一次容量，并先做溢出保护
- `token_array_push`: 满容时通过 `token_array_next_capacity` 扩容

扩容策略：

$$
\mathrm{cap}_{next}=\begin{cases}
32, & \mathrm{cap}=0 \\
2\cdot \mathrm{cap}, & \mathrm{cap}>0
\end{cases}
$$

但当前实现不会盲目做 `capacity * 2`，而是先检查：

1. `current > SIZE_MAX / 2` 时拒绝继续倍增
2. `next_capacity > SIZE_MAX / sizeof(Token)` 时拒绝分配

所以这里的真实语义是“倍增策略 + `size_t` 溢出保护”，`push` 的摊还复杂度仍然是 $O(1)$。

实现伪代码：

```text
function token_array_push(arr, token):
    if arr.size == arr.capacity:
        next_cap = checked_next_capacity(arr.capacity)
        if next_cap overflow:
            return false
        if reserve(arr, next_cap) failed:
            return false
    arr.data[arr.size] = token
    arr.size = arr.size + 1
    return true
```

### 3.3 初始化/释放/合法性校验（实现 + 伪代码 + 测试写法）

- `lexer_init_tokens`: 写入 `TOKEN_ARRAY_MAGIC` 并清空
- `lexer_free_tokens`: magic 合法才释放；不合法则重置
- `token_array_state_is_valid`: 定义合法状态
- `lexer_tokenize`: 接受全零 `{0}` 状态并自动补做一次 `lexer_init_tokens`

合法性条件：

$$
\mathrm{magic}=\mathrm{CONST}
\land
\big(
(\mathrm{data}=\varnothing \Rightarrow \mathrm{size}=0 \land \mathrm{capacity}=0)
\land
(\mathrm{data}\neq\varnothing \Rightarrow \mathrm{capacity}>0 \land \mathrm{size}\le\mathrm{capacity})
\big)
$$

实现伪代码：

```text
function lexer_init_tokens(tokens):
    if tokens == NULL: return
    tokens.magic = TOKEN_ARRAY_MAGIC
    tokens.data = NULL
    tokens.size = 0
    tokens.capacity = 0

function lexer_free_tokens(tokens):
    if tokens == NULL: return
    if tokens.magic != TOKEN_ARRAY_MAGIC:
        lexer_init_tokens(tokens)
        return
    free(tokens.data)
    lexer_init_tokens(tokens)
```

测试写法（契约型负例，贴项目风格）：

```c
static int test_invalid_token_array_state_rejected(void) {
    TokenArray tokens;
    memset(&tokens, 0, sizeof(tokens));
    tokens.data = NULL;
    tokens.size = 1;
    tokens.capacity = 0;

    if (lexer_tokenize("int c = 3;", &tokens)) {
        fprintf(stderr, "[lexer-reg] FAIL: invalid TokenArray state should be rejected\n");
        return 0;
    }
    return 1;
}
```

这里要注意当前实现的一个小细节：

- `TokenArray tokens = {0};` 这种“全零态”现在会被 `lexer_tokenize` 视为可恢复状态，并自动初始化
- 但像“`size=1, capacity=0`”这种部分损坏或伪造状态，仍然会稳定报 `TokenArray is not initialized; call lexer_init_tokens first`

也就是说，当前契约更准确地写成：

$$
\text{accepted state}=
\text{initialized state}
\ \cup\ 
\text{all-zero recoverable state}
$$

---

## 4. 基础扫描原语

### 4.1 光标函数

- `is_at_end`: 判断 `*current == '\0'`
- `peek`: 看当前字符，不前进
- `peek_next`: 看下一个字符，不前进
- `match(expected)`: 若当前字符等于 `expected`，则消费并返回 `true`

### 4.2 `advance` 的行列更新规则

若读到换行：

$$
\ell'=\ell+1,\quad c'=1
$$

否则：

$$
\ell'=\ell,\quad c'=c+1
$$

### 4.3 `add_token`

封装 `Token` 并压入 `TokenArray`，失败通常表示 OOM。

---

## 5. 标识符与关键字识别（实现 + 伪代码 + 测试写法）

### 5.1 `scan_identifier`

识别规则（正则风格）：

$$
[A\text{-}Za\text{-}z\_][A\text{-}Za\text{-}z0\text{-}9\_]^*
$$

先吃完整词素，再交给 `keyword_or_identifier` 判定是否关键字。

伪代码：

```text
function scan_identifier(lx, start, line, col):
    while isalnum(peek(lx)) or peek(lx) == '_':
        advance(lx)
    len = lx.current - start
    type = keyword_or_identifier(start, len)
    return add_token(lx, type, start, len, 0, line, col)
```

### 5.2 `keyword_or_identifier`

按“长度 + 字符串内容”匹配关键字集合，命中则返回关键字 token，否则返回 `TOKEN_IDENTIFIER`。

测试写法（关键字覆盖）：

```c
static int test_break_continue_keywords(void) {
    const char *source = "while(1){continue;break;}";
    TokenArray tokens;
    size_t i;
    int saw_break = 0;
    int saw_continue = 0;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) return 0;

    for (i = 0; i < tokens.size; ++i) {
        if (tokens.data[i].type == TOKEN_KW_BREAK) saw_break = 1;
        if (tokens.data[i].type == TOKEN_KW_CONTINUE) saw_continue = 1;
    }

    lexer_free_tokens(&tokens);
    return saw_break && saw_continue;
}
```

---

## 6. 数字识别与溢出控制（实现 + 伪代码 + 测试写法）

### 6.1 `scan_number`

十进制累积：

$$
v_{next}=10\cdot v + d,\quad d\in[0,9]
$$

### 6.2 溢出判定

溢出保护条件：

$$
v > \frac{\mathrm{LLONG\_MAX}-d}{10}
$$

触发后报错：`Integer literal out of range`。

返回值协议：

- `SCAN_OK = 1`
- `SCAN_OOM = 0`
- `SCAN_ERROR = -1`

伪代码：

```text
function scan_number(lx, start, line, col):
    value = digit(start[0])
    while isdigit(peek(lx)):
        d = digit(advance(lx))
        if value > (LLONG_MAX - d) / 10:
            report "Integer literal out of range"
            return SCAN_ERROR
        value = value * 10 + d
    len = lx.current - start
    if add_token(NUMBER, start, len, value, line, col) failed:
        return SCAN_OOM
    return SCAN_OK
```

测试写法建议：

- 正向：普通整数应通过，且 `TOKEN_NUMBER.number_value` 正确。
- 反向：超出 `LLONG_MAX` 的字面量应失败（返回 `0`）。

示例（反向）：

```c
TokenArray tokens;
lexer_init_tokens(&tokens);
if (lexer_tokenize("int x = 9223372036854775808;", &tokens)) {
    fprintf(stderr, "[lexer-reg] FAIL: out-of-range integer should fail\n");
    lexer_free_tokens(&tokens);
    return 0;
}
```

---

## 7. `lexer_tokenize` 主流程（说明 + 伪代码 + 测试写法）

### 7.1 前置契约

1. `source` 与 `out_tokens` 不能为空
2. 若 `out_tokens` 是全零态，则自动 `init`
3. 若状态非法，报错并返回 `0`
4. 若已有旧数据，先释放并重置

### 7.2 扫描主循环

$$
\text{while }\neg\mathrm{is\_at\_end}()
$$

每轮记录 token 起点 `(tok_start, tok_line, tok_col)`，读取字符 `c` 后分流：

- 空白字符：跳过
- 字母或 `_`：扫描标识符/关键字
- 数字：扫描数字
- 其他：进入 `switch(c)` 处理符号与运算符

### 7.3 最大吞噬（Max-Munch）策略

原则：优先匹配更长词素。

例子：

- `+`: `++` > `+=` > `+`
- `<`: `<<=` > `<<` > `<=` > `<`
- `>`: `>>=` > `>>` > `>=` > `>`
- `&`: `&&` > `&=` > `&`
- `|`: `||` > `|=` > `|`

抽象表达：

$$
\arg\max_{m\in M(c)} |m|\quad \text{s.t. } m \text{ matches input prefix}
$$

### 7.4 注释处理

- 行注释 `//`: 吃到换行或 EOF
- 块注释 `/* ... */`: 吃到最近 `*/`
- 若 EOF 前未闭合块注释：报 `Unterminated block comment`

### 7.5 错误与收尾

- 遇未知字符：报 `Unexpected character` 并失败
- 正常结束：追加 `TOKEN_EOF`
- 任意 OOM：统一走 `oom:` 清理并失败

主流程伪代码：

```text
function lexer_tokenize(source, out_tokens):
    validate input pointers and TokenArray contract
    reset out_tokens to empty

    init lexer state (current=source, line=1, column=1)

    while not end_of_input:
        tok_start = current
        tok_line = line
        tok_col  = column
        c = advance()

        if isspace(c):
            continue
        if isalpha(c) or c == '_':
            if not scan_identifier(...): fail
            continue
        if isdigit(c):
            st = scan_number(...)
            if st != SCAN_OK: fail
            continue

        switch c:
            case operators/punctuators:
                apply max-munch rule and emit token
            case '/':
                handle //, /* */, /, /=
            default:
                report unexpected character and fail

    emit TOKEN_EOF
    return success
```

测试写法（token 序列断言，贴项目风格）：

```c
static int test_shift_operator_token_sequence(void) {
    const char *source = "a<<b>>c;";
    const TokenType expected[] = {
        TOKEN_IDENTIFIER, TOKEN_SHIFT_LEFT, TOKEN_IDENTIFIER,
        TOKEN_SHIFT_RIGHT, TOKEN_IDENTIFIER, TOKEN_SEMICOLON, TOKEN_EOF,
    };
    TokenArray tokens;
    size_t i;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) return 0;
    if (tokens.size != sizeof(expected) / sizeof(expected[0])) return 0;

    for (i = 0; i < tokens.size; ++i) {
        if (tokens.data[i].type != expected[i]) {
            lexer_free_tokens(&tokens);
            return 0;
        }
    }
    lexer_free_tokens(&tokens);
    return 1;
}
```

### 7.6 如何运行 lexer 测试

对应 Makefile 目标：

- `make test-lexer`
- `make test-lexer-regression`

全量回归：

- `make test`

---

## 8. `lexer_token_type_name`（实现 + 测试用途）

为所有 `TokenType` 返回稳定字符串，用于测试输出、诊断文本和调试日志。

测试里常见用法是打印“期望/实际”对比：

```c
fprintf(stderr,
        "[lexer-reg] FAIL: token %zu mismatch: expected %s got %s\n",
        i,
        lexer_token_type_name(expected[i]),
        lexer_token_type_name(tokens.data[i].type));
```

---

## 9. 总体实现方案评估

- 时间复杂度：$O(n)$（每字符常数次处理）
- 空间复杂度：$O(t)$（$t$ 为 token 数）
- 工程特性：
  - 状态机结构清晰
  - 初始化契约严格（magic）
  - 错误定位到行列
  - 内存失败路径集中处理
  - 回归测试以“token 序列精确断言 + 失败路径锁定”为主

这套 lexer 的定位是：可维护、错误信息清晰、对 parser 友好的基础词法器，而不是极致性能型 DFA 生成器。

---

## 可选思考题

1. 为什么 `lexer_tokenize` 在失败时要清理并重置输出数组，而不是“保留已扫 token”？
2. `<<=`, `<<`, `<=`, `<` 这类邻接词素，如果不使用最大吞噬会出现什么具体歧义？
