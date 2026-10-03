# 配置说明

[← 返回首页](../README_CHS.md) | [English](./CONFIGURATION.md)

所谓「配置」，即 `create_log` 和 `reset_config` 函数中的 `config` 字符串。
该字符串采用 **properties 文件格式**，支持 `#` 单行注释（需独立成行并以 `#` 开头）。

---

## 完整示例

```ini
# 这个配置给 log 对象配置了 5 个 Appender，其中有两个 TextFileAppender，会输出到不同的文件。

# 第一个 Appender 名叫 appender_0，类型为 ConsoleAppender
appenders_config.appender_0.type=console
appenders_config.appender_0.time_zone=localtime
appenders_config.appender_0.levels=[verbose,debug,info,warning,error,fatal]

# 第二个 Appender 名叫 appender_1，类型为 TextFileAppender
appenders_config.appender_1.type=text_file
appenders_config.appender_1.time_zone=gmt
appenders_config.appender_1.levels=[info,warning,error,fatal]
appenders_config.appender_1.base_dir_type=1
appenders_config.appender_1.file_name=bqLog/normal
appenders_config.appender_1.max_file_size=10000000
appenders_config.appender_1.expire_time_days=10
appenders_config.appender_1.capacity_limit=100000000

# 第三个 Appender 名叫 appender_2，类型为 TextFileAppender
appenders_config.appender_2.type=text_file
appenders_config.appender_2.levels=[all]
appenders_config.appender_2.base_dir_type=0
appenders_config.appender_2.file_name=bqLog/new_normal

# 第四个 Appender 名叫 appender_3，类型为 CompressedFileAppender
appenders_config.appender_3.type=compressed_file
appenders_config.appender_3.levels=[all]
appenders_config.appender_3.file_name=~/bqLog/compress_log
# appender_3 输出内容将使用下方 RSA2048 公钥进行混合加密
appenders_config.appender_3.pub_key=ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+Fo........rest of your rsa2048 public key...... user@hostname

# 第五个 Appender 名叫 appender_4，类型为 ConsoleAppender
appenders_config.appender_4.type=console
appenders_config.appender_4.enable=false
appenders_config.appender_4.levels=[all]
appenders_config.appender_4.categories_mask=[ModuleA,ModuleB.SystemC]

# Log 对象配置
log.buffer_size=65535
log.recovery=true
log.categories_mask=[*default,ModuleA,ModuleB.SystemC]
log.thread_mode=async
log.print_stack_levels=[error,fatal]

# 快照配置
snapshot.buffer_size=65536
snapshot.levels=[info,error]
snapshot.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
```

---

## 详细解释

### `appenders_config`

`appenders_config` 是一组关于 Appender 的配置。
`appenders_config.<name>.xxx` 中 `<name>` 即 Appender 名称，相同 `<name>` 的配置共同作用于同一个 Appender 实例。

| 名称                         | 是否必须 | 可配置值                                | 默认值               | ConsoleAppender | TextFileAppender | CompressedFileAppender |
|------------------------------|---------|-----------------------------------------|-------------------|-----------------|------------------|------------------------|
| `type`                       | ✔       | `console` / `text_file` / `compressed_file` | -                 | ✔               | ✔                | ✔（加密需此类型）      |
| `enable`                     | ✘       | `true` / `false`                        | `true`            | ✔               | ✔                | ✔                      |
| `levels`                     | ✘       | 日志等级数组（`[verbose,...]` 或 `[all]`） | `[all]`           | ✔               | ✔                | ✔                      |
| `time_zone`                  | ✘       | `gmt` / `localtime` / `Z` / `UTC` / `utc+8` / `utc-2` / `utc+11:30` 等 | `localtime` | ✔               | ✔                | ✔（影响滚动日期）      |
| `file_name`                  | ✔（文件类） | 相对或绝对路径（不含扩展名）                | -                 | ✘               | ✔                | ✔                      |
| `base_dir_type`             | ✘       | `0` / `1`                               | `0`               | ✘               | ✔                | ✔                      |
| `max_file_size`             | ✘       | 正整数或 `0`                            | `0`（不限制）          | ✘               | ✔                | ✔                      |
| `expire_time_seconds`       | ✘       | 正整数或 `0`                            | `0`（不清理）          | ✘               | ✔                | ✔                      |
| `expire_time_days`          | ✘       | 正整数或 `0`                            | `0`（不清理）          | ✘               | ✔                | ✔                      |
| `capacity_limit`            | ✘       | 正整数或 `0`                            | `0`（不限制）          | ✘               | ✔                | ✔                      |
| `write_cache_size`          | ✘       | 正整数（`65536` 到 `4194304`）          | `65536`              | ✘               | ✔                | ✔                      |
| `format_template_cache_max_entries` | ✘ | 正整数（`8` 到 `16777216`）             | `100000`             | ✘               | ✘                | ✔                      |
| `thread_info_cache_max_entries` | ✘   | 正整数（`8` 到 `1048576`）               | `2048`               | ✘               | ✘                | ✔                      |
| `categories_mask`           | ✘       | 字符串数组（`[]`）                       | 空（不过滤）            | ✔               | ✔                | ✔                      |
| `always_create_new_file`    | ✘       | `true` / `false`                        | `false`           | ✘               | ✔                | ✔                      |
| `enable_rolling_log_file`    | ✘       | `true` / `false`                        | `true`            | ✘               | ✔                | ✔                      |
| `pub_key`                   | ✘       | RSA2048 公钥（OpenSSH `ssh-rsa` 文本）  | 空（不加密）            | ✘               | ✘                | ✔（启用混合加密）      |

#### `appenders_config.xxx.type`

指定 Appender 类型：
- `console` → ConsoleAppender
- `text_file` → TextFileAppender
- `compressed_file` → CompressedFileAppender

#### `appenders_config.xxx.enable`

是否默认启用该 Appender，默认为 `true`。
如为 `false`，Appenders 会在初始化时被创建但不实际输出，可在运行时通过 `set_appender_enable` 切换。

#### `appenders_config.xxx.levels`

使用 `[]` 包裹的数组，内容为：
- 任意组合：`[verbose,debug,info,warning,error,fatal]`
- 或特殊值 `[all]`，表示所有等级均输出。

#### `appenders_config.xxx.time_zone`

指定时间戳格式化使用的时区，同时也影响按日期滚动文件的「日期边界」：

- `"gmt"`、`"Z"`、`"UTC"`：使用 UTC0（格林威治时间）；
- `"localtime"`：使用系统当地时间；
- `"utc+8"`、`"utc-2"`、`"utc+11:30"` 等：明确指定偏移。

影响：
- ConsoleAppender / TextFileAppender：决定日志文本中时间字段的显示；
- TextFileAppender / CompressedFileAppender：决定文件按日期滚动的切分点（每天 0 点）。

#### `appenders_config.xxx.base_dir_type`

指定当 `file_name` 为相对路径时的基准目录：

- `0`
  - Android：依次尝试 `getFilesDir()` → `getExternalFilesDir()` → `getCacheDir()`
  - iOS：`/var/mobile/Containers/Data/Application/[APP]/Library/Application Support`
  - HarmonyOS：`/data/storage/el2/base/files`
  - 其他平台：进程当前工作目录
- `1`
  - Android：依次尝试 `getExternalFilesDir()` → `getFilesDir()` → `getCacheDir()`
  - iOS：`/var/mobile/Containers/Data/Application/[APP]/Documents`
  - HarmonyOS：`/data/storage/el2/base/cache`
  - 其他平台：进程当前工作目录

#### `appenders_config.xxx.file_name`

指定日志文件的路径与文件名前缀，示例：

```ini
appenders_config.appender_3.file_name=~/bqLog/compress_log
```

最终实际文件名会由 BqLog 按日期与滚动编号自动补全，例如：
- `compress_log_20250101_0001.logcompr`（CompressedFileAppender）
- `normal_20250101_0001.log`（TextFileAppender）

若是相对路径，则基于 `base_dir_type` 对应的目录。

#### 其他字段简要说明

- `max_file_size`：单个文件最大字节数，超过则新建文件；`0` 表示不按大小切分。
- `expire_time_seconds`：按秒数清理超时文件；`0` 关闭该功能。
- `expire_time_days`：按天清理超时文件；`0` 关闭该功能。
- `capacity_limit`：限制该 Appender 输出的文件的总大小，超过时按时间从旧文件开始删除。
- `write_cache_size`：文件写入内存缓存大小（字节）。配置值会限制在 64 KiB～4 MiB，并向上取整为 2 的幂。增大该值可以减少写系统调用次数，但会增加每个文件 Appender 的内存占用。
- `format_template_cache_max_entries`：CompressedFileAppender 保留的 format template 查找缓存最大条目数。默认 `100000`，配置值会限制在 `8`～`16777216`。主查找表内存大致为 `12 * next_power_of_two(2 * max_entries)` 字节，另外还有较小的 L1 cache。单个日志文件使用的不同 format template 超过该值时，不会丢日志，也不影响解码正确性，但被淘汰的 template 再次出现时可能会重新写入压缩文件，从而增加文件体积。通过 `reset_config` 修改后会立即生效；缩小上限时会清空当前 L2 查找表。
- `thread_info_cache_max_entries`：CompressedFileAppender 保留的 thread info 查找缓存最大条目数。默认 `2048`，配置值会限制在 `8`～`1048576`。超过上限不会丢日志，但被淘汰的 thread info template 可能会再次写入。通过 `reset_config` 修改后会立即生效；缩小上限时会清空对应的 L2 查找表。这里两个 cache 配置的单位都是“条目数”，不是字节数。
- `categories_mask`：仅当日志 Category 匹配该数组中的前缀时，才会输出日志（参见 [高级用法 — Category](./ADVANCED_USAGE_CHS.md#2-支持分类category的-log-对象)）。
- `always_create_new_file`：`true` 时，即使同一天内，每次进程重启也新开一个文件；默认 `false` 为追加写。
- `enable_rolling_log_file`：是否启用按日期滚动文件，默认 `true`。
- `pub_key`：为 CompressedFileAppender 提供加密公钥，字符串内容应完整拷贝自 `ssh-keygen` 生成的 `.pub` 文件，且以 `ssh-rsa ` 开头。详情见 [高级用法 — 日志加密和解密](./ADVANCED_USAGE_CHS.md#6-日志加密和解密)。

---

### `log` 配置

`log.xxx` 配置作用于整个 Log 对象。

| 名称                                      | 是否必须 | 可配置值                               | 默认值                                                         | 是否可通过 `reset_config` 修改 |
|-------------------------------------------|---------|----------------------------------------|----------------------------------------------------------------|--------------------------------|
| `log.thread_mode`                         | ✘       | `sync` / `async` / `independent`       | `async`                                                        | ✘                              |
| `log.buffer_size`                         | ✘       | 32 位正整数                            | 桌面/服务器：`65536`；移动设备：`32768`                         | ✘                              |
| `log.recovery`                            | ✘       | `true` / `false`                       | `false`                                                        | ✘                              |
| `log.categories_mask`                     | ✘       | 字符串数组（`[]`）                      | 空（不过滤）                                                   | ✔                              |
| `log.print_stack_levels`                  | ✘       | 日志等级数组                           | 空（不打印调用栈）                                             | ✔                              |
| `log.buffer_policy_when_full`             | ✘       | `discard` / `block` / `expand`         | `block`                                                        | ✘                              |
| `log.high_perform_mode_freq_threshold_per_second` | ✘ | 64 位正整数                            | `1000`                                                         | ✘                              |

#### `log.thread_mode`

决定缓冲区中的数据由哪个线程处理：

- `sync`：同步日志模式。写日志的线程直接负责处理和输出日志，调用结束即完成输出；（性能低，不推荐）
- `async`（默认）：异步模式。写日志线程只写缓冲区，由全局 worker 线程统一处理所有异步 log 对象的输出；
- `independent`：独立异步模式。为该 Log 对象单独创建一个专属 worker 线程。适合单个 Log 写入量极大、希望完全与其他 Log 解耦的场景。

#### `log.buffer_size`

日志缓冲区大小（字节）。
缓冲越大，可承受的突发写入峰值越大，但内存占用也会增加。

#### `log.recovery`

- `true`：当异步日志遇到程序异常退出时，缓冲区中还未落盘的数据会在下次启动后重新写入日志文件。
- `false`：进程异常退出且未调用 `force_flush()` 时，缓冲中的日志数据将丢失。

具体行为详见 [高级用法 — 程序异常退出的数据保护](./ADVANCED_USAGE_CHS.md#3-程序异常退出的数据保护)。

#### `log.categories_mask`

行为与 `appenders_config.xxx.categories_mask` 一致，但作用范围为整个 Log 对象。

- 对同步日志：在调用线程就进行过滤；
- 对异步日志：在写入缓冲时进行过滤，减少不必要的数据进出。

详见 [高级用法 — Category](./ADVANCED_USAGE_CHS.md#2-支持分类category的-log-对象)。

#### `log.print_stack_levels`

配置方式类似 `appenders_config.xxx.levels`，用于指定哪些日志等级需要自动附带调用栈。例如：

```ini
log.print_stack_levels=[error,fatal]
```

建议仅在 Debug / 测试环境启用，避免对线上性能造成明显影响。

#### `log.buffer_policy_when_full`

当缓冲区写满后的行为：

- `discard`：丢弃新写入的日志，直到缓冲区有足够空间；
- `block`（推荐默认）：写日志的线程会阻塞等待缓冲区有空间；
- `expand`（不推荐）：缓冲区会动态扩容为原来两倍，直到可写。
  可能显著增加内存占用，虽然 BqLog 通过良好的线程调度减少了扩容次数，但仍建议谨慎使用。

#### `log.high_perform_mode_freq_threshold_per_second`

该配置项用于控制「高性能模式」触发阈值：

- 当单个线程在一秒内记录的日志条数超过该值时，该线程将自动进入高性能模式；
- 高性能模式下，会为该线程分配更适合高频写入的内部资源；
- 当写入频率下降时，会自动退出高性能模式并释放相关资源。

默认值为 `1000`（条/秒）。
配置为 `0` 表示关闭该功能。

为减少内存碎片，物理内存的分配通常以「若干个高速缓存」为一组进行批量申请（桌面平台为 16 个，高端移动平台通常为 2 个）。因此即使只有一个线程进入高性能模式，也会额外占用一组缓存的空间。

---

<a id="snapshot-配置"></a>

### `snapshot` 配置

`snapshot.xxx` 用于配置该 Log 对象的日志快照功能。
适用于：检测到异常后需要上报该 Log 的「最近一段时间」日志。

| 名称                   | 是否必须 | 可配置值       | 默认值 | 是否可通过 `reset_config` 修改 |
|------------------------|---------|----------------|--------|--------------------------------|
| `snapshot.buffer_size` | ✘       | 32 位正整数    | `0`    | ✔                              |
| `snapshot.levels`      | ✘       | 日志等级数组   | `[all]`| ✔                              |
| `snapshot.categories_mask` | ✘   | 字符串数组     | 空     | ✔                              |

- `snapshot.buffer_size`：快照缓冲大小（字节），为 `0` 或未配置时，快照功能关闭；
- `snapshot.levels`：仅配置中的等级会被写入快照；若不配置则默认为 `[all]`；
- `snapshot.categories_mask`：行为与 `appenders_config.xxx.categories_mask` 一致，仅匹配的 Category 会被写入快照；未配置则不过滤所有 Category。
