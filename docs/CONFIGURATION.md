# Configuration

[← Back to Home](../README.md) | [简体中文](./CONFIGURATION_CHS.md)

"Configuration" refers to the `config` string in `create_log` and `reset_config` functions.
This string uses **properties file format**, supports `#` single-line comments (must be on a separate line and start with `#`).

---

## Full example

```ini
# This config configures 5 Appenders for the log object, including two TextFileAppenders outputting to different files.

# The first Appender is named appender_0, type is ConsoleAppender
appenders_config.appender_0.type=console
appenders_config.appender_0.time_zone=localtime
appenders_config.appender_0.levels=[verbose,debug,info,warning,error,fatal]

# The second Appender is named appender_1, type is TextFileAppender
appenders_config.appender_1.type=text_file
appenders_config.appender_1.time_zone=gmt
appenders_config.appender_1.levels=[info,warning,error,fatal]
appenders_config.appender_1.base_dir_type=1
appenders_config.appender_1.file_name=bqLog/normal
appenders_config.appender_1.max_file_size=10000000
appenders_config.appender_1.expire_time_days=10
appenders_config.appender_1.capacity_limit=100000000

# The third Appender is named appender_2, type is TextFileAppender
appenders_config.appender_2.type=text_file
appenders_config.appender_2.levels=[all]
appenders_config.appender_2.base_dir_type=0
appenders_config.appender_2.file_name=bqLog/new_normal

# The fourth Appender is named appender_3, type is CompressedFileAppender
appenders_config.appender_3.type=compressed_file
appenders_config.appender_3.levels=[all]
appenders_config.appender_3.file_name=~/bqLog/compress_log
# appender_3 output content will use RSA2048 public key below for hybrid encryption
appenders_config.appender_3.pub_key=ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+Fo........rest of your rsa2048 public key...... user@hostname

# The fifth Appender is named appender_4, type is ConsoleAppender
appenders_config.appender_4.type=console
appenders_config.appender_4.enable=false
appenders_config.appender_4.levels=[all]
appenders_config.appender_4.categories_mask=[ModuleA,ModuleB.SystemC]

# Log object configuration
log.buffer_size=65535
log.recovery=true
log.categories_mask=[*default,ModuleA,ModuleB.SystemC]
log.thread_mode=async
log.print_stack_levels=[error,fatal]

# Snapshot configuration
snapshot.buffer_size=65536
snapshot.levels=[info,error]
snapshot.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
```

---

## Detailed explanation

### `appenders_config`

`appenders_config` is a set of configurations about Appender.
`<name>` in `appenders_config.<name>.xxx` is the Appender name, configurations with same `<name>` act on the same Appender instance.

| Name                         | Mandatory | Configurable Values                                | Default Value             | ConsoleAppender | TextFileAppender | CompressedFileAppender |
|------------------------------|---------|-----------------------------------------|--------------------|-----------------|------------------|------------------------|
| `type`                       | Yes       | `console` / `text_file` / `compressed_file` | -                  | Yes               | Yes                | Yes      |
| `enable`                     | No       | `true` / `false`                        | `true`             | Yes               | Yes                | Yes                      |
| `levels`                     | No       | Log level array (`[verbose,...]` or `[all]`) | `[all]`            | Yes               | Yes                | Yes                      |
| `time_zone`                  | No       | `gmt` / `localtime` / `Z` / `UTC` / `utc+8` / `utc-2` / `utc+11:30` etc. | `localtime` | Yes               | Yes                | Yes (Affects rolling date)      |
| `file_name`                  | Yes (File type) | Relative or absolute path (no extension)                | -                  | No               | Yes                | Yes                      |
| `base_dir_type`             | No       | `0` / `1`                               | `0`                | No               | Yes                | Yes                      |
| `max_file_size`             | No       | Positive integer or `0`                            | `0` (Unlimited)       | No               | Yes                | Yes                      |
| `expire_time_seconds`       | No       | Positive integer or `0`                            | `0` (No clean)       | No               | Yes                | Yes                      |
| `expire_time_days`          | No       | Positive integer or `0`                            | `0` (No clean)       | No               | Yes                | Yes                      |
| `capacity_limit`            | No       | Positive integer or `0`                            | `0` (Unlimited)       | No               | Yes                | Yes                      |
| `write_cache_size`          | No       | Positive integer (`65536` to `4194304`)            | `65536`               | No               | Yes                | Yes                      |
| `format_template_cache_max_entries` | No | Positive integer (`8` to `16777216`)              | `100000`              | No               | No                 | Yes                      |
| `thread_info_cache_max_entries` | No   | Positive integer (`8` to `1048576`)                | `2048`                | No               | No                 | Yes                      |
| `categories_mask`           | No       | String array (`[]`)                       | Empty (No filtering)        | Yes               | Yes                | Yes                      |
| `always_create_new_file`    | No       | `true` / `false`                        | `false`            | No               | Yes                | Yes                      |
| `enable_rolling_log_file`    | No       | `true` / `false`                        | `true`            | No               | Yes                | Yes                      |
| `pub_key`                   | No       | RSA2048 Public Key (OpenSSH `ssh-rsa` text)  | Empty (No encryption)        | No               | No                | Yes (Enable hybrid encryption)      |

#### `appenders_config.xxx.type`

Specify Appender type:
- `console` → ConsoleAppender
- `text_file` → TextFileAppender
- `compressed_file` → CompressedFileAppender

#### `appenders_config.xxx.enable`

Whether to enable this Appender by default, default is `true`.
If `false`, Appenders will be created during initialization but not actually output, can be switched at runtime via `set_appender_enable`.

#### `appenders_config.xxx.levels`

Array wrapped in `[]`, content is:
- Arbitrary combination: `[verbose,debug,info,warning,error,fatal]`
- Or special value `[all]`, indicating all levels are output.

#### `appenders_config.xxx.time_zone`

Specify timezone used for timestamp formatting, also affects "date boundary" for rolling files by date:

- `"gmt"`, `"Z"`, `"UTC"`: Use UTC0 (Greenwich Mean Time);
- `"localtime"`: Use system local time;
- `"utc+8"`, `"utc-2"`, `"utc+11:30"` etc.: Explicitly specify offset.

Effect:
- ConsoleAppender / TextFileAppender: Determine display of time field in log text;
- TextFileAppender / CompressedFileAppender: Determine cut-off point for file rolling by date (0 o'clock every day).

#### `appenders_config.xxx.base_dir_type`

Specify base directory when `file_name` is relative path:

- `0`
  - Android: Try in order `getFilesDir()` → `getExternalFilesDir()` → `getCacheDir()`
  - iOS: `/var/mobile/Containers/Data/Application/[APP]/Library/Application Support`
  - HarmonyOS: `/data/storage/el2/base/files`
  - Other platforms: Process current working directory
- `1`
  - Android: Try in order `getExternalFilesDir()` → `getFilesDir()` → `getCacheDir()`
  - iOS: `/var/mobile/Containers/Data/Application/[APP]/Documents`
  - HarmonyOS: `/data/storage/el2/base/cache`
  - Other platforms: Process current working directory

#### `appenders_config.xxx.file_name`

Specify path and filename prefix of log file, example:

```ini
appenders_config.appender_3.file_name=~/bqLog/compress_log
```

Final actual filename will be auto-completed by BqLog with date and rolling number, for example:
- `compress_log_20250101_0001.logcompr` (CompressedFileAppender)
- `normal_20250101_0001.log` (TextFileAppender)

If it is relative path, it is based on directory corresponding to `base_dir_type`.

#### Other fields

- `max_file_size`: Max bytes per single file, new file created if exceeded; `0` means not split by size.
- `expire_time_seconds`: Clean up expired files by seconds; `0` disables this function.
- `expire_time_days`: Clean up expired files by days; `0` disables this function.
- `capacity_limit`: Limit total size of files output by this Appender, delete from oldest files by time when exceeded.
- `write_cache_size`: In-memory file write cache size in bytes. Values are clamped to 64 KiB–4 MiB and rounded up to a power of two. Larger values reduce write system-call frequency at the cost of memory usage per file Appender.
- `format_template_cache_max_entries`: Maximum number of format-template lookup entries retained by a CompressedFileAppender. The default is `100000`; values are clamped to `8`–`16777216`. The main lookup table uses approximately `12 * next_power_of_two(2 * max_entries)` bytes, plus a small L1 cache. When a log file uses more distinct format templates than this limit, logging and decoding remain correct, but an evicted template may be written to the compressed file again and increase file size. Changes made through `reset_config` take effect immediately; shrinking the limit clears the current L2 lookup table.
- `thread_info_cache_max_entries`: Maximum number of thread-info lookup entries retained by a CompressedFileAppender. The default is `2048`; values are clamped to `8`–`1048576`. Exceeding the limit does not drop logs, but an evicted thread-info template may be emitted again. Changes made through `reset_config` take effect immediately; shrinking the limit clears the corresponding L2 lookup table. Both compressed-cache limits are entry counts, not byte sizes.
- `categories_mask`: Output logs only when log Category matches prefix in this array (see [Advanced Usage — Category](./ADVANCED_USAGE.md#2-log-objects-with-category-support)).
- `always_create_new_file`: When `true`, create new file every time process restarts even within same day; default `false` is append write.
- `enable_rolling_log_file`: When `true` (default), enable rolling file function by date.
- `pub_key`: Provide encryption public key for CompressedFileAppender, string content should be completely copied from `.pub` file generated by `ssh-keygen`, and start with `ssh-rsa `. Details see [Advanced Usage — Log encryption and decryption](./ADVANCED_USAGE.md#6-log-encryption-and-decryption).

---

### `log` Configuration

`log.xxx` configuration acts on the entire Log object.

| Name                                      | Mandatory | Configurable Values                               | Default Value                                                         | Modifiable via `reset_config` |
|-------------------------------------------|---------|----------------------------------------|----------------------------------------------------------------|--------------------------------|
| `log.thread_mode`                         | No       | `sync` / `async` / `independent`       | `async`                                                        | No                              |
| `log.buffer_size`                         | No       | 32-bit Positive Integer                            | Desktop/Server: `65536`; Mobile: `32768`                         | No                              |
| `log.recovery`                            | No       | `true` / `false`                       | `false`                                                        | No                              |
| `log.categories_mask`                     | No       | String array (`[]`)                      | Empty (No filtering)                                                   | Yes                              |
| `log.print_stack_levels`                  | No       | Log level array                           | Empty (No call stack printing)                                             | Yes                              |
| `log.buffer_policy_when_full`             | No       | `discard` / `block` / `expand`         | `block`                                                        | No                              |
| `log.high_perform_mode_freq_threshold_per_second` | No | 64-bit Positive Integer                            | `1000`                                                         | No                              |

#### `log.thread_mode`

Determines which thread processes data in buffer:

- `sync`: Synchronous log mode. The thread writing logs is directly responsible for processing and outputting logs, output is completed when call ends; (Low performance, not recommended)
- `async` (Default): Asynchronous mode. Thread writing logs only writes to buffer, global worker thread uniformly handles output of all asynchronous log objects;
- `independent`: Independent asynchronous mode. Create a dedicated worker thread for this Log object alone. Suitable for scenarios where single Log write volume is huge and complete decoupling from other Logs is desired.

#### `log.buffer_size`

Log buffer size (bytes).
Larger buffer can withstand larger burst write peaks, but memory usage will also increase.

#### `log.recovery`

- `true`: When asynchronous log encounters abnormal program exit, data in buffer that has not been flushed to disk will be rewritten to log file after next startup.
- `false`: Log data in buffer will be lost when process exits abnormally and `force_flush()` is not called.

Detailed behavior see [Advanced Usage — Data protection on abnormal exit](./ADVANCED_USAGE.md#3-data-protection-on-abnormal-exit).

#### `log.categories_mask`

Behavior consistent with `appenders_config.xxx.categories_mask`, but scope is entire Log object.

- For synchronous logs: Filtering is done in calling thread;
- For asynchronous logs: Filtering is done when writing to buffer, reducing unnecessary data entry/exit.

See [Advanced Usage — Category](./ADVANCED_USAGE.md#2-log-objects-with-category-support).

#### `log.print_stack_levels`

Configuration method similar to `appenders_config.xxx.levels`, used to specify which log levels need to automatically attach call stack. For example:

```ini
log.print_stack_levels=[error,fatal]
```

Recommended to enable only in Debug / Test environment to avoid significant impact on online performance.

#### `log.buffer_policy_when_full`

Behavior when buffer is full:

- `discard`: Discard newly written logs until buffer has enough space;
- `block` (Recommended Default): Thread writing logs will block waiting for space in buffer;
- `expand` (Not Recommended): Buffer will dynamically expand to twice original size until writable.
  May significantly increase memory usage, although BqLog reduces expansion frequency through good thread scheduling, it is still recommended to use with caution.

#### `log.high_perform_mode_freq_threshold_per_second`

This configuration item is used to control "High Performance Mode" trigger threshold:

- When number of logs recorded by a single thread in one second exceeds this value, the thread will automatically enter high performance mode;
- Under high performance mode, internal resources more suitable for high frequency writing will be allocated for this thread;
- When writing frequency drops, it will automatically exit high performance mode and release related resources.

Default value is `1000` (entries/second).
Configuring as `0` means disabling this function.

To reduce memory fragmentation, physical memory allocation is usually performed in batches of "several cache lines" as a group (16 for desktop platforms, usually 2 for high-end mobile platforms). Therefore, even if only one thread enters high performance mode, it will occupy extra space of one group of caches.

---

<a id="snapshot-configuration"></a>

### `snapshot` Configuration

`snapshot.xxx` is used to configure log snapshot function of this Log object.
Applicable to: Need to report "recent period" logs of this Log after detecting anomaly.

| Name                   | Mandatory | Configurable Values       | Default Value | Modifiable via `reset_config` |
|------------------------|---------|----------------|--------|--------------------------------|
| `snapshot.buffer_size` | No       | 32-bit Positive Integer    | `0`    | Yes                              |
| `snapshot.levels`      | No       | Log level array   | `[all]`| Yes                              |
| `snapshot.categories_mask` | No   | String array     | Empty     | Yes                              |

- `snapshot.buffer_size`: Snapshot buffer size (bytes), snapshot function disabled when `0` or not configured;
- `snapshot.levels`: Only levels in configuration will be written to snapshot; defaults to `[all]` if not configured;
- `snapshot.categories_mask`: Behavior consistent with `appenders_config.xxx.categories_mask`, only matching Category will be written to snapshot; filter all Categories if not configured.
