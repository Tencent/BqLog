# BqLog 性能设计三篇

[English](PERFORMANCE_DESIGN.md) · [项目首页](../README_CHS.md)

本系列按 BqLog 2.5.0、源码基线 `424948bd` 重写，核对日期为 2026-09-25。每篇都先从直观方案开始，解释它在真实日志场景下的成本，再逐步推导当前的结构与实现。第一、二篇保留旧文件名，方便已有链接继续使用。

| 文章 | 重点 |
|---|---|
| [一：从一行文本推导出压缩日志格式](<文章1_为何BqLog如此快 - 高性能实时压缩日志格式.MD>) | 文本重复 → 模板与参数 → item 位布局 → 内存与文件表示 → 多段结构、加密和恢复 |
| [二：从环形队列到自适应数据总线](<文章2_为何BqLog如此快 - 高并发环形队列.MD>) | 单生产者 ring → CAS 与 fetch-add 回滚 → 缓存行竞争 → LP/HP、Group 与跨路径顺序 |
| [三：一次内存读取，能顺便做多少事情](<文章3_为何BqLog如此快 - 压缩日志执行路径优化.MD>) | 重复工作 → 融合拷贝哈希 → CRC 依赖链 → 缓存与插入位置复用 → 回填、批处理与加密成本 |

代码中的机制、历史测试数据和应当如何设计新的实验，在文中分别说明。公开数字不外推为所有平台、语言、Unity/UE 帧率或任意故障下的保证。

16 张结构图参考作者原有图示与 Excalidraw 草图，按当前源码重新绘制。相邻字段表示连续字节，图中标出 bit、字节偏移、游标、指针与索引关系；依赖 ABI 的位置使用符号。中英文共用英文技术标注，并由正文逐图解释。

- [文件容器与记录](img/performance/compressed-file-v10.png)
- [Item 类型位与长度](img/performance/compressed-item-bits.png)
- [模板与参数结构](img/performance/compressed-templates.png)
- [同一条记录的内存和文件表示](img/performance/compressed-record-example.png)
- [UTF-Mixed 字节示例](img/performance/compressed-utf-mixed.png)
- [分段加密的布局与成本](img/performance/encryption-segments.png)
- [环形队列的逻辑与物理游标](img/performance/ring-basics.svg)
- [MISO 内存与提交状态](img/performance/miso-memory.svg)
- [Fetch-add 与回滚反例](img/performance/miso-rollback.svg)
- [SISO 内存与批量读取](img/performance/siso-memory.svg)
- [数据总线](img/performance/adaptive-log-bus.svg)
- [Block 生命周期](img/performance/block-lifecycle.svg)
- [Context 与跨路径顺序](img/performance/bus-order.svg)
- [拷贝与哈希](img/performance/fused-copy-hash.png)
- [模板缓存](img/performance/template-cache.png)
- [写缓存与长度回填](img/performance/write-cache-backfill.png)

SVG 可以直接编辑，也可运行 `node docs/img/performance/draw_diagrams.mjs` 从仓库中的绘图源重新生成。

原图保持为设计参考，不作为当前实现的规范。代码路径是判断同步、数据布局和加密行为的依据。特别是当前 `rsa_aes_xor` 方案与“直接 AES 加密全部正文”不同，第一、三篇对此作了准确区分。
