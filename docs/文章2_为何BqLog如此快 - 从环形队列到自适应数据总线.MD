# 为何 BqLog 如此快之二：从环形队列到自适应数据总线

[English](<Article 2_Why is BqLog so fast - From Ring Buffer to Adaptive Data Bus.MD>) · [系列目录](PERFORMANCE_DESIGN_CHS.md) · [上一篇：压缩文件](<文章1_为何BqLog如此快 - 高性能实时压缩日志格式.MD>) · [下一篇：执行路径优化](<文章3_为何BqLog如此快 - 压缩日志执行路径优化.MD>)

上一篇讨论了压缩文件如何减少格式化和写入量。但日志在进入文件之前，还要从业务线程交给后台线程。几十个线程同时写入时，即使每条记录都很小，共享队列的写游标也可能成为热点。

BqLog 的 `log_buffer` 为低频线程提供共享队列，为高频线程分配专用队列。下面从普通环形队列讲起，看看多生产者写入、变长记录、线程增删和崩溃恢复分别带来哪些麻烦。

> 本文对应 BqLog 2.5.0。MISO、SISO 是项目里的命名习惯：MISO 表示多生产者单消费者（Multi In, Single Out，也就是常说的 MPSC），SISO 表示单生产者单消费者（Single In, Single Out，即 SPSC）。代码里的对齐常量 `BQ_CACHE_LINE_SIZE` 是 128 字节，下文简称 C。它是库自己选的布局单位，不代表所有 CPU 的硬件缓存行都是 128 字节。

## 1. 测试数据与讨论范围

先看 README 中与本文相关的一组测试。参与比较的组件有：

#### Java
- [Log4j2](https://github.com/apache/logging-log4j2)
- [Logback](https://github.com/qos-ch/logback)
#### C#
- [NLog](https://github.com/NLog/NLog)
- log4net
#### C++
- [SpdLog](https://github.com/gabime/spdlog)
#### 多语言
- [mars-xlog](https://github.com/Tencent/mars)

测试使用相同场景和用例，各组件都用自己最快的配置，比如异步模式。下表是其中五种配置的结果：1–10 个线程，每线程写 200 万条日志，每条带 4 个格式化参数；机器为 MacBook Pro（Apple M4 Pro，48 GB）。

这里每增加一个线程，总日志量也增加 200 万条。例如 10 线程对应 2000 万条日志，比较吞吐时要用这份总量除以耗时。

#### 每条日志带 4 个参数的总耗时（单位：毫秒）

|                              | 1线程 | 2线程 | 3线程 | 4线程 | 5线程 | 6线程 | 7线程 | 8线程 | 9线程 | 10线程 |
|------------------------------|-------|-------|-------|-------|-------|-------|-------|-------|-------|--------|
| BqLog Compress(C++)          | 79    | 99    | 115   | 170   | 222   | 311   | 324   | 372   | 484   | 759    |
| BqLog Compress+Encrypt(C++)  | 83    | 106   | 134   | 189   | 223   | 328   | 337   | 390   | 532   | 995    |
| BqLog Text(C++)              | 202   | 417   | 648   | 910   | 1167  | 1425  | 1718  | 1969  | 2281  | 2718   |
| spdlog                       | 434   | 1366  | 3133  | 4779  | 6228  | 9241  | 10829 | 11348 | 11197 | 12003  |
| Log4j2 (Java)                | 946   | 1841  | 2422  | 3685  | 5542  | 5245  | 5775  | 5786  | 8048  | 8752   |

10 线程时，BqLog 压缩模式用时 759 毫秒，Log4j2 用时 8752 毫秒，相差约 11.5 倍。BqLog 压缩加密模式用时 995 毫秒，比未加密模式多约 31%。这些是整条写入路径的测试结果，不能单独归因于队列。用例和配置见 [Benchmark](https://github.com/Tencent/BqLog/blob/main/README_CHS.md#-benchmark-%E7%BB%93%E6%9E%9C)。

格式化、缓存访问、I/O 和线程间通信都会影响总耗时。本文只讨论最后一项：共享 MISO 队列如何处理多生产者分配，以及高频线程如何转到各自的 SISO 队列。相关方案已在 BqLog 开源前申请专利。

## 2. 两种环形队列的起点

熟悉单生产者环形队列和 Disruptor 的读者可以直接看 [第 3 节](#3-miso-队列用-fetch_add-分配空间)。这里主要讨论并发控制，不展开内存模型的全部细节。

先记住队列要回答的两个问题：生产者能写哪段空间，消费者能读到哪里。前一个问题要防止覆盖未读数据，后一个问题要防止读到尚未写完的数据。单生产者时两个游标就能协调好；生产者一多，空间预留和写入完成就得分开处理。

### 2.1 Linux kFifo：单生产者、单消费者

`kFifo` 是 Linux 内核提供的循环队列（FIFO）实现。这里取其单生产者、单消费者用法：一个线程写，一个线程读。

![kFifo 的环形结构](img/performance/ring-kfifo.png)

<p align="center"><em>图 1：kFifo 的环形结构——<code>In</code>、<code>Out</code> 两个游标各只有一个主人。</em></p>

队列用 `in` 和 `out` 记录累计写入、读出的进度，只有访问内存时才取模映射到环上。例如容量为 16，`out=12`、`in=20`，就有 8 个单位尚未读出，下一处写入的物理位置是 `20 % 16 = 4`。后文出现 1000、1030 这样的游标，也是在表示累计进度。

图中红色格子是尚未消费的数据，`Out` 对应最老的位置，`In` 对应下一处可写位置。容量取 2 的幂时，取模可以写成 `cursor & (size - 1)`。

用 acquire/release 伪接口写出单生产者、单消费者的基本过程：

```c
/// 写线程调用
unsigned int kfifo_in(struct kfifo *fifo, const void *buf, unsigned int len)
{
    unsigned int in = fifo->in.load_relaxed();
    unsigned int out = fifo->out.load_acquire();
    unsigned int n = min(len, fifo->size - (in - out));
    unsigned int offset = in & (fifo->size - 1);
    unsigned int first = min(n, fifo->size - offset);

    memcpy(fifo->buffer + offset, buf, first);                // 先写到环尾
    memcpy(fifo->buffer, (const char*)buf + first, n - first); // 剩余部分写到环首
    fifo->in.store_release(in + n);                          // 数据写好后再发布
    return n;
}

/// 读线程调用
unsigned int kfifo_out(struct kfifo *fifo, void *buf, unsigned int len)
{
    unsigned int out = fifo->out.load_relaxed();
    unsigned int in = fifo->in.load_acquire();
    unsigned int n = min(len, in - out);
    unsigned int offset = out & (fifo->size - 1);
    unsigned int first = min(n, fifo->size - offset);

    memcpy(buf, fifo->buffer + offset, first);
    memcpy((char*)buf + first, fifo->buffer, n - first);
    fifo->out.store_release(out + n); // 读完后通知写线程：这些空间可以复用
    return n;
}
```

写线程独占修改 `in`，读线程独占修改 `out`，因此不需要用 CAS 争取游标。写线程先拷贝数据，再以 release 语义发布 `in`；读线程以 acquire 语义取得这个进度后，也能看到此前写好的数据。读完后的 `out` 则反向告诉生产者：这些空间已经可以覆盖。

这里的原子读写负责双方可见性，CAS 则是用来协调多个修改者。若两个生产者同时执行上述写入过程，它们可能读到相同的 `in`，随后向同一位置拷贝数据。要支持多生产者，首先得让每个线程取得不同的位置。

### 2.2 LMAX Disruptor：先分配，再发布

`LMAX Disruptor` 是 LMAX 开源的并发框架，Log4j2 的异步日志也使用它。代码和文档见 [LMAX-Exchange/disruptor](https://github.com/LMAX-Exchange/disruptor)。

它也使用环形结构。以下按日志场景示意：槽位（Slot）持有 `Log Entry` 的引用，实际数据在对象中。

![LMAX Disruptor 的环形结构](img/performance/ring-disruptor.png)

<p align="center"><em>图 2：Disruptor 的环——槽位里放的是 <code>Log Entry</code> 引用，不是数据本身。</em></p>

多生产者可用 `CAS（Compare-And-Swap）` 争取写入位置：只有当前值与预期值相同才更新；失败的线程重新读取游标并重试。

我们试着用 CAS 改造 `kfifo_in`，让它支持并发写入：

```c
unsigned int kfifo_in_concurrent(struct kfifo *fifo, const void *buf, unsigned int len)
{
    unsigned int old_in, new_in, free_space;

    do {
        // 1. 获取当前的in指针位置
        old_in = fifo->in;

        // 2. 计算ring_buffer剩余的可用空间
        free_space = fifo->size - (old_in - fifo->out);
        if (len > free_space) {
            return 0;  // 如果空间不足，写入失败
        }

        // 3. 计算新的in指针位置
        new_in = old_in + len;

        // 4. 尝试通过CAS更新in指针
        // 如果当前的in值还是old_in，那么成功更新为new_in，否则需要重试
    } while (!__sync_bool_compare_and_swap(&fifo->in, old_in, new_in));

    // 5. 现在已经安全地申请到空间，开始写入数据
    write_data_to(old_in/*to*/, buf/*from*/, len/*size*/);

    return len;
}
```

CAS 成功时，当前线程取得从 `old_in` 开始的区间；失败说明别人推进了游标，需要重新计算容量并重试。空间分开了，接下来还要解决完成状态的发布。

预留游标不能直接当作可读边界。单生产者可以先写数据、再发布 `in`；多生产者先各自取得位置，再写数据，完成顺序可能与预留顺序不同。

![Disruptor Alloc](img/Disruptor_Alloc.png)

<p align="center"><em>图 3：先申请后写入的麻烦：A 和 C 的块已经写完（阴影），B 还没写完，<code>In</code> 不再能当可读边界。</em></p>

图中 A 和 C 已经写完，B 尚未写完。消费者不能越过 B 读取 C，因此需要独立的完成状态。Disruptor 通过槽位序号跟踪发布进度；BqLog 把变长数据直接放在环形内存里，采用后文的块状态。

## 3. MISO 队列用 fetch_add 分配空间

### 3.1 CAS 重试的代价

多个线程同时用 CAS 更新同一个游标时，失败的一方必须重读再重试。线程一多，重试就频繁到不能忽略。MISO 队列在空间分配这条常用路径上改用 `fetch_add`。

### 3.2 fetch_add：先预留，再检查容量

`fetch_add` 原子地增加游标，并返回增加前的值。所有调用对这个变量仍有先后次序，但每个线程都能拿到自己的旧值，程序不必再写一轮“比较失败就重试”的循环。

只看预留动作，伪码可以写成：

```c
unsigned int kfifo_in_concurrent(struct kfifo *fifo, const void *buf, unsigned int len)
{
    // 1. 直接申请内存，申请到的就是[from, from+len)这一段
    unsigned int from = __sync_fetch_add(&fifo->in, len);

    // 2. 这段内存已经归当前线程独占，直接写
    write_data_to(from/*to*/, buf/*from*/, len/*size*/);
    return len;
}
```

假设 `in` 初始值是 0，三个线程 A、B、C 同时申请 10、5、15 字节，执行完后它们拿到的起始位置可能是：

- **线程A**：0，**线程B**：10，**线程C**：15，`in` 最终是 30

也可能是：

- **线程B**：0，**线程C**：5，**线程A**：20，`in` 最终还是 30

看下图：

![fetch_add 的分配结果](img/performance/fetch-add-cases.png)

<p align="center"><em>图 4：fetch_add 为不同线程分配不同区间；容量上限仍需单独检查。</em></p>

顺序取决于线程的实际执行次序，但拿到的区间互不重叠。两种方式的脾气不一样：CAS 是“确认没人动过才更新，失败就重来”；`fetch_add` 是“先把号拿到手，有问题再处理”。图中还画出了容量上限：`fetch_add` 不检查剩余空间，预留之后还要验证拿到的区间是否有效。

### 3.3 越界后的回滚

`fetch_add` 只保证游标增加是原子的，不能保证队列有足够容量。

图中容量上限 `Limit` 为 25，多个线程预留后，`In` 到了 40。C 的末尾和 D 的全部区间都超出当前可写范围，不能直接写入。

消费者把 `Out` 推进到 15 时，可写上限也移到 45。先前越界的区间此时可能重新变得有效；回滚判断要考虑这个变化。

CAS 不会有这个问题，因为最后一次更新时必须确认 `in` 和当初判断时一模一样，中间有人动过就失败重来。

`bq::miso_ring_buffer` 在预留后检查容量，必要时回滚。下面的伪码省略了环回绕等细节：

```c
void* bq::miso_ring_buffer::alloc(size_t len)
{
    // 1. 先用缓存的读游标检查，空间不足时刷新一次
    size_t in = this->in_.load_relaxed();
    if (in + len - this->out_cache_ > this->size_) {
        this->out_cache_ = this->out_.load_acquire();
        if (in + len - this->out_cache_ > this->size_) {
            return nullptr;
        }
    }

    // 2. 原子预留：[from, from + len)
    size_t from = this->in_.fetch_add_relaxed(len);

    // 3. 预检查之后可能又有其他生产者分配，必须检查实际拿到的区间
    while (from + len > this->out_.load_acquire() + this->size_)
    {
        // 4. 确实越界了，只能回滚Rollback
        size_t expected_in = from + len;
        if (__sync_bool_compare_and_swap(&this->in_, expected_in, from))
        {
            // 如果in_的值等于from + len，就把它回滚成from，否则自旋继续尝试回滚
            return nullptr;  // 回滚成功，返回空间不足
        }
        yield(); // 让出时间片，别在这儿空转
    }

    // 5. 内存有效，直接返回（可能申请时就有效，也可能是等待回滚期间消费者腾出了新空间）
    return to_addr(from);
}
```

眼尖的读者会问：回滚里不还藏着一次 CAS 吗？注意它发生的前提——队列已经满了。这时的瓶颈本来就是等消费者腾出空间，一次 CAS 的开销相比之下算不了什么。好钢用在刀刃上：常用路径一次 `fetch_add` 完成分配，冷路径才允许自旋。

为什么不能直接执行 `fetch_add(&this->in_, -len)`？看下面的交错顺序：多个生产者已经预留空间，消费者又在推进 `out`，直接减去自己的长度可能让后续分配与仍有效的区间重叠。

图中给出一个例子：

![Rollback 1](img/rollback_1.png)

<p align="center"><em>图 5：A、B、C 预留空间后，B 和 C 的区间越过当前可写上限。</em></p>

分配前 `in` 为 1000，可写上限为 1012。A、B、C 先后预留后，`in` 到了 1030，B 和 C 的区间越界。如果 B 直接减去自己的长度，可能出现以下顺序：

1. B 执行 `fetch_add(&in, -5)`，把 `in` 减到 1025；
2. 消费者 D 释放空间，可写上限移到 1040；
3. B 再申请 5 字节，得到 1025–1030，此时 `in` 又到了 1030；
4. C 发现自己的 1015–1030 区间已在容量内，也开始写入。

结果如下图：

![Rollback 2](img/rollback_2.png)

<p align="center"><em>图 6：B 直接减去自己的长度并再次申请，结果与 C 的区间重叠。</em></p>

此时 B 与 C 会写到重叠的区间。回滚必须从队尾逆序进行：每个线程只在 CAS 确认 `in == from + len`（也就是自己还占着队尾）时才撤销自己的预留，不是队尾就继续等。若等待期间消费者释放了足够空间，该线程就可以保留区间直接写入。图中的顺序是 C 先把 1030 退回 1015，B 再从 1015 退回 1010。

对应的真实实现可以看这里：  
[miso_ring_buffer.h](https://github.com/Tencent/BqLog/blob/main/src/bq_log/types/buffer/miso_ring_buffer.h)  
[miso_ring_buffer.cpp](https://github.com/Tencent/BqLog/blob/main/src/bq_log/types/buffer/miso_ring_buffer.cpp)

### 3.4 抢到位置不等于写完数据：块状态

预留游标仍不能代表数据已经写完。A 先取得位置但尚未写入，B 随后完成写入时，消费者需要停在 A 的区间前。

BqLog 的 MISO 把环形内存按 **128 字节（C）一个 block** 划分。每条数据（chunk）的第一个 block 有 8 字节头部，其中包含发布状态：

```text
block_num: 3 B | status: 1 B | data_size: 4 B | payload ...
```

`status` 有三个值：`unused` 表示尚无可读数据，`used` 表示写入完成，`invalid` 表示该区间需跳过。生产者拷贝完成后以 release 语义写入 `used`；消费者以 acquire 语义读到 `used`，才读取 payload。它与前面发布 `in` 的作用相同，只是多生产者必须按记录分别报完成。

空间预留只需要保证不同线程取得不同位置，不负责告诉消费者数据已经写好，因此 `fetch_add` 可用 relaxed 序。**预留空间的原子性和发布数据的可见性，是这里分开的两件事。**

![MISO 的内存布局和 chunk 头](img/performance/miso-layout.png)

<p align="center"><em>图 7：MISO 环由 128 字节 block 组成；每条 chunk 的头部占 8 字节，消费者按游标顺序读取。</em></p>

`invalid` 用来处理跨越环尾的预留。普通字节 FIFO 可以把一次拷贝拆成两段，BqLog 上层却需要一块连续内存来填日志。若本次预留跨过环尾，就把整个预留区间标成 `invalid`，让消费者跳过，再重新申请连续空间。

128 字节的分配粒度会产生内部碎片。例如 61 字节的内部日志，加上 LP 路径的 16 字节 context 和 8 字节块头，共 85 字节，仍占一个完整 block。这样分配可让不同生产者的数据落在不同的对齐区域，减少伪共享；代价是小记录占用更多缓冲空间。第 4 节再看共享游标上的竞争。

游标同步还有两处可摊销：

- 写线程把读游标缓存一份在线程本地（TLS），平时用缓存值判断容量，不够了才去读真正的共享 `read_cursor_`。缓存值只会偏旧，偏旧只会低估可用空间，顶多白刷一次，绝不会因此覆盖没消费的数据。
- 消费者也不是每读一条就发布一次 `read_cursor_`，而是攒够四分之一环或者读空了，才批量把状态置 `unused`、发布读游标。同步成本被摊薄到一大批数据上。

下图对照 kFifo 与 MISO 的同步步骤：

![kFifo 与 MISO 的同步流程对比](img/performance/sync-compare.png)

<p align="center"><em>图 8：MISO 除游标更新外，还需发布并读取每条 chunk 的完成状态。</em></p>

单生产者队列主要发布写、读游标。MISO 除了更新游标，还要为每条 chunk 发布完成状态；消费者读取状态后，批量归还空间。同步集中在分配、发布和归还阶段，数据拷贝本身不争用共享游标。

## 4. 共享写游标的硬件代价

MISO 的常用写入路径已经没有锁和 CAS 重试，但线程数拉到几十个时，性能曲线还是会往下弯。这次的瓶颈在 CPU 的硬件结构里。

多核处理器里，每个核都有自己的缓存，核之间以缓存行为单位交换数据，靠缓存一致性协议（比如 MESI）维持对内存的共同视图。多个核反复修改同一缓存行时，这一行的“写所有权”就要在核之间来回转手——你抢过去，我抢回来——每次转手要花几十到上百个时钟周期。早期 CPU 用环形总线（Ring Bus）传递这些消息，核数多了以后换成 2D-Mesh 这类片上网络；互连结构一直在演进，缓存行在核间转移的成本却始终都在。

这个现象有个形象的名字：数据风暴。Linux 内核的 MCS SpinLock 就是为它设计的：每个等待的 CPU 各盯各的变量自旋，而不是所有人反复抢同一个锁变量。

![Ring Bus、2D-Mesh 和 MCS SpinLock](img/performance/cpu-data-storm.png)

<p align="center"><em>图 9：互连结构从 Ring Bus 演进到 2D-Mesh，缓存行转手的成本始终都在；MCS SpinLock 的应对是让每个等待者只管自己的变量。</em></p>

回到 MISO：不管有多少个生产者，`fetch_add` 的目标都是同一个 `write_cursor_`，它所在的缓存行就是全场被抢得最厉害的地方。`fetch_add` 只是在软件层面省掉了重试，底下的缓存行争夺仍随每次写入发生。

可能有人会问：把游标分开布局不就行了？这一步确实做了——MISO 把 `write_cursor_` 和 `read_cursor_` 放进了不同的缓存行，那是为了避免伪共享（False Sharing）：两个不相干的变量恰好挤在同一条缓存行里，互相拖累。但伪共享分得开，真共享分不开：`write_cursor_` 天生就是所有生产者都要修改的同一份数据，加多少 padding 都改变不了这一点。高频生产者需要各自的写游标。

## 5. 高频线程使用专用 SISO 队列

### 5.1 SISO 是什么

`siso_ring_buffer` 只有一个生产者和一个消费者。生产者独占写游标，消费者独占读游标；高频线程各用一条队列，就不必在每条日志上争用同一个写游标。

![SISO 集群的工作方式](img/performance/siso-cluster.png)

<p align="center"><em>图 10：高频线程分别写入专用 SISO，消费者按顺序轮询并批量读取。</em></p>

SISO 头部按 C 字节分成四个区域：

1. 消费者本地状态：缓存的读位置、已知写位置和容量；
2. 生产者本地状态：本地写位置、缓存的读位置；
3. 共享读游标；
4. 共享写游标。

![SISO 头部的四个对齐区与数据区](img/performance/siso-head.png)

<p align="center"><em>图 11：SISO 头部按四个 C 字节区分开读写状态，后面的数据区按 8 字节单位使用。</em></p>

双方平时使用本地缓存的位置，只在空间不足或读空时刷新对方的共享游标。单生产者也无需 MISO 的逐块状态位：chunk 头为 8 字节（`block_num` 和 `data_size` 各 4 字节），分配粒度缩到 8 字节。

消费者在一批开始时读取写游标，以该位置为本批终点。读完并归还空间后，再轮询其他队列。批读减少共享游标访问，也避免一个持续写入的线程占住消费者。

### 5.2 为什么低频线程仍共用 MISO

如果给 1000 个线程各分配 64 KiB，光数据区就占 62.5 MiB，还不包括管理结构。低频线程长期占着专用缓冲，利用率会很低。因此专用 SISO 只分配给高频线程。

BqLog 把两种队列组合起来：

```text
LP（Low Performance）：低频线程共用一条 MISO
HP（High Performance）：每个“线程 × Log”使用自己的 SISO
```

每个线程的 TLS 按 Log 记录写入频率。默认阈值是每秒 1000 条：达到阈值转 HP，频率降低后退回 LP。这个计数只用于决定是否分配专用缓冲，无需逐条维护精细统计。

## 6. SISO 多了以后，怎样管理

每个高频线程都有一条 SISO 后，还得回答三个问题：新线程从哪里领队列，退出时怎样归还，消费者又怎样遍历这些队列。先从最容易想到的数组试起。

### 6.1 为什么不用数组

最先想到的结构可能是数组：把 SISO 放进去，消费者按下标扫描。在所有线程一起启动、一起退出的 benchmark 里，它很好用。真实程序里的线程却会随时加入和离开。

比如线程池随负载伸缩，或者加载线程写完几条日志就退出。数组这时遇到两个麻烦。

第一，数组扩容可能搬动已有元素，而生产者还持有队列地址。预分配足够大的数组可以避开搬迁，却又要提前占用大量内存。

第二，消费者遍历数组时，生产者可能注册或移除队列。若用一把锁保护整个数组，结构变化就会阻塞消费者。也可以设计更细的同步，但数组不再有实现简单的优势。

消费者只有一个。它一旦停住，所有生产者仍可能继续写入，队列逐渐填满。因此这里希望线程增删时只锁住局部结构，让消费者继续处理其他队列。

链表更适合这种局部修改：每个 `next` 指针都可以单独保护，插入和删除不必锁住整个集合。

### 6.2 把链表修改限制在少数位置

链表本身还不够；生产者如果能随意摘除节点，消费者遍历时仍会碰到失效指针。因此规定：

> **生产者只许在头部插；只有消费者能摘。**

生产者只在头部插入，新节点指向原来的头节点；消费者这轮看见或下一轮才看见它都可以。摘除只由消费者执行，它在日常遍历时便不需要为每个节点加锁。

锁只用于结构变化。每个节点用读写自旋锁保护自己的 `next` 指针：

1. 生产者找空闲队列时，手递手地获取读锁：锁住下一个指针后，释放上一个。这样消费者不能在它访问节点时将节点摘除。
2. 新 Group 插在头部，只短暂锁住头指针，不影响消费者对链表中段的遍历。
3. 消费者回收 Group 时，沿链表获取写锁，确认其中的 block 全部归还后再摘除。摘除的 Group 先留在对象池中一秒，短时间内有新线程加入时可以复用。

![Group 链表上的手递手读写锁](img/performance/group-list-rwlock.png)

<p align="center"><em>图 12：Group 3 被摘除时，只锁住其前驱节点的指针；消费者正常遍历不获取这些锁。</em></p>

关键是把会修改链表的时机限制在线程加入与回收时；逐条消费日志无需经过这些锁。

链表节点称为 **Group**，是一块连续内存，通常对应一个 mmap 文件。Group 内有多个 block。每个 block 先放 128 字节管理区，记录链表索引、链表类型和其他状态；后面原地放 SISO 对象及其数据区。一个 block 对应一条高频线程的专用队列。

![log_buffer 全景：Block、Group、Group List](img/performance/log-buffer-structure.png)

<p align="center"><em>图 13：每个 block 的管理区后是 SISO；多个 block 组成 Group，Group 再连接成链。图中标出了 Free/Stage 头部 CAS 与 Group 指针读写锁这两类同步点。</em></p>

### 6.3 Group 内部的三条链

如果 Group 内只有一条带“忙／闲”标志的链，生产者领块和消费者找数据都要扫描、修改它。竞争只是从整个数组缩小到了一个 Group。BqLog 按 block 所处阶段分成三条链：

| 链表 | 生产者 | 消费者 |
|---|---|---|
| **Free（空闲链）** | 领块时 CAS **pop** | 回收块时 CAS **push** |
| **Stage（交接链）** | 初始化完 CAS **push** | 巡场时 CAS **pop** |
| **Used（使用链）** | 只往自己的 SISO 写数据，不修改链表 | 独占遍历、接入和摘除 |

消费者只有一个，可以独占 Used 链的遍历、接入和摘除。生产者在升级到 HP 时，从 Free 取一个 block，初始化后放进 Stage。生产者与消费者同时访问的只有 Free 和 Stage 的头部，CAS 集中在这些低频操作上。

Free 和 Stage 都可能被两个线程访问，因此头部用 CAS 更新。为防 ABA，32 位头部中低 16 位保存 block 索引，高 16 位保存每次操作递增的标记。即使头部索引从 A 变成 B 再变回 A，标记也已改变，旧 CAS 不会误以为链表没有动过。Used 只有消费者修改，使用普通指针操作即可。

为什么要多设一条 Stage？生产者如果直接往 Used 上挂节点，就会撞上正在遍历的消费者。先把初始化好的 block 推到 Stage，消费者巡场时自己取出来、自己接入 Used，Used 链就始终只有消费者一个修改者。

![Group 内部的 Free、Stage、Used 三条链](img/performance/group-three-lists.png)

<p align="center"><em>图 14：生产者从 Free 取 block，初始化后放入 Stage；消费者将它移到 Used，退役后再放回 Free。Free 和 Stage 的头部带 ABA 标记。</em></p>

链表用 **16 位索引**连接 block，`FFFF` 表示空。block 在 Group 中连续排列，索引足以定位。mmap 重新映射后基址可能改变，原指针不能继续用，索引则仍然有效。恢复时会用到这个性质。

### 6.4 Group 的大小

Group 可以只放一个 block，但把多个 block 放在一起有几个实际好处：

- 一次 mmap 可以为多个队列提供内存，不必每个线程单独申请。
- 每个 Group 对应独立的 mmap 文件。文件头保存 Log 配置的校验和，恢复时可逐个检查，单独丢弃损坏文件。
- 空闲 Group 会在对象池中保留一秒。短时间内有新线程加入时，可复用已有映射。

桌面平台一个 Group 装 16 个 block；内存紧张的移动平台装 2 个。

### 6.5 逐条写日志时走哪条路径

上述链表操作发生在线程升级、降级、退出和回收时。高频线程领到 block 后，逐条写日志只访问自己的 SISO；Free、Stage、Used 不参与每条日志的写入。这也是前面愿意增加管理结构的原因。

## 7. 用三个场景串起这套结构

单独看每条链表还不容易看出它们如何配合。下面依次看线程加入、频率切换和崩溃恢复。

### 7.1 场景一：一个新的高频线程加入

假设业务线程在一秒内达到 `high_frequency_threshold_per_second`（默认 1000 条），需要从 LP 升级到 HP。它领取专用 SISO 的过程如下：

![新高频线程加入的流程](img/performance/case-new-thread.png)

<p align="center"><em>图 15：高频线程从 Free 取 block，初始化后放入 Stage，随后由消费者接入 Used。</em></p>

生产者执行四步：

- 1. 在 Group 链表中寻找空闲 block，遍历时手递手地获取读锁，再从 Free 头部用 CAS 取出 block。若所有 Group 都已用满，才创建新 Group 并插入链表头部。
- 2. 原地初始化 block 内的 SISO，在管理区写入 16 字节 context，包括版本、序号和 TLS 身份。
- 3. 用 CAS 把 block 推入 Stage。
- 4. 开始向这条 SISO 写日志。

消费者按原有节奏巡场，遇到该 Group 时从 Stage 取出 block，核验 context，再接到 Used 链尾部。生产者交付后就能继续写入；消费者自行决定何时把新 block 纳入轮询。

### 7.2 场景二：一个线程在高低频之间来回切换

每条队列内部都有序，为什么最终文件还可能乱序？问题出在同一线程切换 LP 和 HP 时。

一个线程先在 LP 写日志 A，升级到 HP 后写 B、C，降级回 LP 后再写 D。

![线程在高低频间切换时的顺序问题](img/performance/case-frequency-switch.png)

<p align="center"><em>图 16：同一线程在 LP、HP、LP 三个阶段写入 A、B、C、D；context 序号用于恢复阶段顺序。</em></p>

LP 与 HP 各自有序，但消费者轮询时可能先碰到 B、C，也可能在处理 B、C 之前就碰到 D。仅靠队列自身顺序，无法还原这个线程写入 A、B、C、D 的先后。

BqLog 为同一线程、同一 Log 的每个阶段分配 16 字节 context：

![context 的逐字节结构与跨路径序号](img/performance/context-order.png)

<p align="center"><em>图 17：16 字节 context 的逐字节结构；同一个生产者的每个阶段各带一个序号。</em></p>

context 里有版本号、线程结束标记、外部引用标记、序号 seq 和 TLS 身份。LP 路径上每条记录带一份；HP 路径上每个 block 存一份，块内所有消息共用。

这里不需要全局序号。消费者要回答的问题只有一个：这个线程下一阶段的数据到了没有？局部序号就够用了。如果给所有线程统一编号，就得再引一个共享计数器——那又回到第 4 节数据风暴的老路上去了。

消费者为每个 TLS 维护期望序号。假设先看到 `seq=1` 的 HP 块，而当前期望是 0，就暂时跳过它，先处理包含 A 的阶段。A 处理完后期望变为 1，才能读取 B、C；待生产者不再使用该块且消费者将它读空，期望才推进到 D 所在的阶段。

这个 HP 块什么时候才算读完？队列暂时为空还不够，生产者可能继续写。降级时，生产者给 block 标记“已移除”；线程退出时还会在 context 中写结束标记。消费者看到标记后读完剩余数据，才把 block 从 Used 摘除并放回 Free。

只使用 LP 的线程没有 HP block 可标记。它退出时会向公共 LP 写一条带 seq 的结束记录；消费者读到后清理该线程对应的顺序状态。

这也解释了第一篇的一个细节：不同线程的日志交错写入文件，时间戳不一定严格递增，因此时间差用 ZigZag 编码来表示负值。

### 7.3 场景三：崩溃了，怎么复盘

BqLog 可以把缓冲区放在内存映射文件（mmap）中。异常退出后，恢复逻辑要重新识别留下的 block，并按线程阶段顺序处理其中的日志。

![崩溃复盘的流程](img/performance/case-recovery.png)

<p align="center"><em>图 18：重启后逐个校验 mmap 文件，跳过未完成的数据，再按 seq 重放。</em></p>

按恢复顺序看：

公共 MISO、各 Group 和超大日志分别使用 mmap 文件。退出时，文件中可能留有 Used/Stage/Free 链、context 和尚未消费的数据。

重启后逐个校验 mmap 文件，损坏或不匹配的文件单独丢弃。恢复逻辑将版本号推进一代（例如从 7 到 8），扫描旧数据，并为每个 TLS 建立最小 seq 表；崩溃时尚未完成的同版本数据作废。可恢复的旧版本范围有限，`MAX_RECOVERY_VERSION_RANGE` 当前为 2。这里也用到了 16 位索引：重新映射后基址可能变化，索引仍可定位 Group 内的 block。context 中保存的 TLS 地址只当作身份 key 用，没人会去访问它指向的内存。

mmap 脏页的落盘顺序不一定等于程序的写入顺序。假设 Used 链中已留下一个 block，但 Stage 链里“摘除它”的修改还没落盘，恢复时就会在两条链中看到同一个 block。恢复代码会识别并去掉 Stage 中的重复节点。

最后按每个线程的 seq 顺序重放旧数据，交给 Appender 的恢复段写入。旧数据处理完后，新版本的实时写入继续进行。

Group 分文件便于隔离损坏数据；索引链和 context 则让重新映射后的 block 与线程顺序仍可识别。这些恢复需求也参与决定了前面的数据结构。

## 8. 超大日志与缓冲区满时的处理

### 8.1 超大日志的载荷单独存放

有些日志会大于普通 ring 的容量，例如一整段内存 dump。BqLog 将载荷放进专门的 oversize buffer，在 LP 中保留一条带 external-ref 标记的父记录。父记录仍参与正常的顺序控制。

提交时先发布外部载荷，再发布父记录。消费者根据父记录中的版本、TLS 身份和序号找到载荷，校验后读取。若载荷分配失败，已预留的父位置要标成“可跳过”，并修正序号，否则后续消息会一直等待缺失的前驱。

### 8.2 消费者追不上的时候

如果生产速度持续超过消费速度，有限容量的缓冲最终会写满。BqLog 提供三种处理方式：

- `discard`：新消息直接写失败，丢掉；
- `block`：唤醒消费者，生产者等着重试；
- `expand`：LP 紧张了转 HP，HP 紧张了加 block，用更多空间换时间。

`expand` 不会原地 realloc 正在使用的 ring，而是通过 LP/HP 切换或增加 block 扩容。因此 `log.buffer_size` 只控制结构中的一部分；估算总内存还要计入 HP 数量、Group、超大日志和 Appender 缓存。

生产者只在空间不足或剩余空间偏低时请求唤醒消费者；`awake_flag` 合并重复请求。消费者有数据时持续处理，空闲时再等待通知或超时。

## 9. 回到最初的问题

开头的问题是：怎样让许多业务线程把日志交给一个消费者，而不让它们总在同一个写游标上排队？各层结构分别解决一部分：

- 连续 ring：省掉了逐条 new 节点的分配；
- 游标缓存和批量归还：省掉了大部分共享变量访问；
- MISO 的 fetch_add + 回滚：空间充足时，预留不需要 CAS 重试；
- HP SISO：高频线程使用各自的写游标，减少共享缓存行争用；
- Group + Free/Stage/Used：把队列的增删放在线程切换路径，方便消费者遍历和崩溃恢复；
- context 序号：用局部顺序接通各条路径，不强求全局序号；
- oversize：把少见的大记录从常驻容量里摘出去。

代价也在对应的位置：LP 的 128 字节粒度会浪费小记录的空间，HP 占用更多内存，Group 的管理操作仍会用锁，满缓冲时还要按配置丢弃、等待或扩容。选哪条路径取决于线程的写入频率。下一篇继续看日志进入总线后，拷贝、哈希、查表和编码怎样减少重复工作。

## 对照源码继续看

- [log_buffer.h](../src/bq_log/types/buffer/log_buffer.h)、[log_buffer.cpp](../src/bq_log/types/buffer/log_buffer.cpp)：TLS、context、切换顺序、大日志与回收。
- [miso_ring_buffer.cpp](../src/bq_log/types/buffer/miso_ring_buffer.cpp)、[siso_ring_buffer.h](../src/bq_log/types/buffer/siso_ring_buffer.h)、[siso_ring_buffer.cpp](../src/bq_log/types/buffer/siso_ring_buffer.cpp)：预留、发布、环尾、批读。
- [group_list.cpp](../src/bq_log/types/buffer/group_list.cpp)、[block_list.h](../src/bq_log/types/buffer/block_list.h)：连续分配和索引链。
- [bq_log_api.cpp](../src/bq_log/api/bq_log_api.cpp)、[log_worker.cpp](../src/bq_log/log/log_worker.cpp)：背压和唤醒。
- [test_log_buffer.h](../test/cpp/test_log_buffer.h)、[test_miso_ring_buffer.h](../test/cpp/test_miso_ring_buffer.h)、[test_siso_ring_buffer.h](../test/cpp/test_siso_ring_buffer.h)：现有对应测试。
