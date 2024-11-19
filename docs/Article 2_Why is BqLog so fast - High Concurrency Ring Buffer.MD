# Why is  BqLog So Fast - Part 2: High Concurrency Ring Buffer

In systems that pursue extreme performance, reducing any unnecessary computation is key to optimization. In the case of mobile games, frame rate and smoothness are the foundation of the basic experience, but the release version of a game is often caught in an "impossible triangle":

1. Performance is good enough (logs are written less frequently)
2. Easy to trace issues (logs should be written as much as possible)
3. Saving storage space (logs should preferably not be written at all)

Since globally deployed software faces challenges like time zone differences, language, and privacy concerns, it’s difficult to directly communicate with users when facing tricky issues. At such times, we need a product that helps us "have it all," breaking this impossible triangle. BqLog was created in this context.

## Foreword: About the Performance of BqLog

Currently, the following log components are known in the industry for their performance:

#### Java
- [Log4j2](https://github.com/apache/logging-log4j2)
- [Logback](https://github.com/qos-ch/logback)

#### C#
- [NLog](https://github.com/NLog/NLog)
- log4net

#### C++
- [SpdLog](https://github.com/gabime/spdlog)

#### Multi-language
- [mars-xlog](https://github.com/Tencent/mars)

No matter what language the logging component is written in, we use the same scenarios and test cases for performance comparisons. Each component is configured to use its highest-performance mode (such as asynchronous mode). Ultimately, Log4j2 outperforms all in terms of raw performance. Now, what if we compare BqLog with Log4j2's asynchronous mode? The chart below gives an intuitive comparison:

#### Total time for logging each log with 4 parameters (in milliseconds)

|                         | 1 Thread | 2 Threads | 3 Threads | 4 Threads | 5 Threads | 6 Threads | 7 Threads | 8 Threads | 9 Threads | 10 Threads |
|-------------------------|----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|------------|
| BqLog Compress (C++)     | 155      | 250       | 310       | 406       | 515       | 622       | 761       | 885       | 972       | 1007       |
| BqLog Text (C++)         | 384      | 768       | 1136      | 1716      | 2020      | 2783      | 3578      | 3883      | 4032      | 4383       |
| BqLog Compress (Java)    | 664      | 782       | 931       | 911       | 989       | 1055      | 1107      | 1229      | 1288      | 1336       |
| BqLog Text (Java)        | 706      | 993       | 1165      | 1582      | 1912      | 2572      | 2779      | 3275      | 4249      | 4591       |
| Log4J2 Text              | 1065     | 2583      | 4249      | 4843      | 5068      | 6195      | 6424      | 7943      | 8794      | 9254       |

![Benchmark](img/benchmark_4_params.png)

From the results, we can see that with 2 million logs written per thread (each log contains 4 formatted parameters, including log level, timestamp, and thread information), the C++ version of `BqLog` outperforms `Log4j2` by approximately ***9 times***. The Java version of `BqLog` is about ***7 times*** faster than Log4j2. For the test cases and code configurations used, please refer to this link: [Benchmark](https://github.com/Tencent/BqLog/blob/main/README.md#5-benchmark-results).

So, what are the key factors behind the performance improvement of `BqLog`?

### Key Performance Improvement Measures of BqLog
- 1. Cache line isolation to avoid `False Sharing`
- 2. Efficient use of memory models (`Memory Order`), utilizing tools like AMD uProf to locate memory access bottlenecks
- 3. IO operation merging
- 4. Avoiding runtime formatting
- 5. Improve cache hit rate
- 6. And so on...


The performance optimization of `BqLog` has reached a temporary limit, with most of the overhead now concentrated on the operating system's IO operations. Even whether container functions are inlined can have a significant impact on the final performance.  
To detail each of the performance optimization points would likely require many articles. For seasoned experts, these technical details might not offer anything new, and for beginners, there are plenty of resources available online. Even by directly reading the `BqLog` source code, there are many comments and explanations. So, there’s no need to waste space here. This article focuses on the innovations of `BqLog`'s self-implemented high-concurrency queue, exploring how it discards the traditional `CAS (Compare And Swap)` operation found in conventional concurrent queues to achieve more efficient concurrent processing. This approach has been patented before `BqLog` was open-sourced.


## Prerequisite Knowledge: Message Queue

1. ***This chapter is aimed at beginners and primarily introduces Linux's `kFifo` and `LMAX Disruptor` solutions. Those already familiar with these topics can skip directly to [BqLog Ring Buffer](#3-bqlog-ring-buffer).***
2. ***This article focuses on alternatives to `CAS (Compare And Swap)`, so details regarding memory barriers (`Memory Barrier`), memory models (`Memory Order`), etc., will be intentionally glossed over. Those interested in these topics can search for relevant articles online.***

### 1. Linux kFifo

`kFifo` is a circular queue (FIFO) implementation provided by the Linux kernel, widely used in kernel modules, driver development, and efficient communication between devices. It manages data through a ring buffer to ensure efficient interaction between producers and consumers, while maintaining data safety in multithreaded scenarios. ***The classic kFifo is only suitable for one thread reading and one thread writing*** and does not support higher concurrency.

#### Working Principle
![Basic Structure](img/kfifo_buffer.png)  
`kFifo` is based on a ring buffer design and uses two pointers, `in` and `out`, to control writing and reading data. When writing, the `in` pointer moves forward, and when reading, the `out` pointer moves forward. When either pointer reaches the end of the buffer, it goes back to the beginning, allowing data to flow in a circular way.

The pseudocode is as follows:

```c
/// <summary>
/// Called by the writing thread
/// </summary>
/// <param name="fifo">The pre-created fifo object</param>
/// <param name="buf">The source of the data to write</param>
/// <param name="len">The size of the data to write</param>
/// <returns>The actual size of the data written</returns>
unsigned int kfifo_in(struct kfifo *fifo, const void *buf, unsigned int len)
{
    unsigned int l;

    l = min(len, fifo->size - fifo->in + fifo->out);  // First, check how much space is left for writing
    memcpy(fifo->buffer + (fifo->in & (fifo->size - 1)), buf, l);  // Write the data
    memory_barrier();  // Memory barrier to ensure that when the read thread sees the latest fifo->in, the preceding data has been written successfully and can be read
    fifo->in += l; // Update fifo->in so that the read thread knows where the new data has been written
    return l;  // Return the actual size of the data successfully written
}

/// <summary>
/// Called by the reading thread
/// </summary>
/// <param name="fifo">The pre-created fifo object</param>
/// <param name="buf">The target buffer for the read data</param>
/// <param name="len">The size of the data to read</param>
/// <returns>The actual size of the data read</returns>
unsigned int kfifo_out(struct kfifo *fifo, void *buf, unsigned int len)
{
    unsigned int l;

    l = min(len, fifo->in - fifo->out); // First, check how much data is left in the queue
    memcpy(buf, fifo->buffer + (fifo->out & (fifo->size - 1)), l);  // Read the data
    memory_barrier();  // Memory barrier to ensure that when the write thread sees the latest fifo->out, all preceding data has been read and the space is free for new writes
    fifo->out += l;  // Update fifo->out so the write thread knows that all previous data has been read and can be overwritten
    return l;  // Return the actual size of the data read
}
```

#### kFifo Analysis

`kFifo` achieves efficient read and write operations through its ring buffer design and simple pointer management. The producer (writing thread) advances the `in` pointer to write data into the buffer, while the consumer (reading thread) moves the `out` pointer to read data. When both pointers reach the end of the buffer, they automatically wrap around to the beginning, creating a circular structure. The core advantage of this design lies in simplifying read and write operations: the writing thread only needs to update the `in` pointer to indicate the write location, and the reading thread only updates the `out` pointer to indicate the read location, facilitating effective coordination between producer and consumer.

This minimalist design avoids race conditions that could occur if both parties were modifying the same variables simultaneously. In multithreaded scenarios, the synchronization mechanism used by `kFifo` is extremely lightweight, relying solely on memory barriers to ensure ordering without requiring complex locks or `CAS (Compare And Swap)` operations. This simple synchronization method is one of the reasons for `kFifo`'s high performance.

##### Memory Barrier

In multi-core systems, the execution order of instructions may differ from the programming order. Memory barriers ensure that read and write operations are executed in the expected order to maintain data integrity. Although memory barriers are crucial for ensuring data consistency, they are a lower-level concept, and `kFifo`'s logic does not rely on complex synchronization mechanisms. Aside from the memory barriers, all operations are quite straightforward.

##### Limitations

While `kFifo`'s "producer-consumer" model is efficient, it also has limitations. It is best suited for scenarios with a single producer and a single consumer. In such cases, the producer only needs to focus on the `in` pointer, and the consumer only needs to care about the `out` pointer. However, if multiple producers or consumers compete simultaneously, this simplified design becomes inadequate. `kFifo` does not scale well in highly concurrent scenarios.

For example, if two threads attempt to write simultaneously, consider the following code:

```c
unsigned int kfifo_in(struct kfifo *fifo, const void *buf, unsigned int len)
{
    unsigned int l;

    l = min(len, fifo->size - fifo->in + fifo->out);  
    memcpy(fifo->buffer + (fifo->in & (fifo->size - 1)), buf, l);  // The issue: if two threads write at the same time, both could perform memcpy on the same address
    memory_barrier();  
    fifo->in += l; 
    return l; 
}
```

### 2. LMAX Disruptor

`LMAX Disruptor` is one of the most popular high-performance concurrent frameworks in recent years. Developed by the UK-based LMAX Exchange, it was created to handle the challenges of high-throughput, low-latency scenarios in financial trading systems. Its outstanding performance has made it a go-to framework for many high-concurrency systems. It is also the default message queue implementation for Log4j2. The code and documentation are open-sourced on Github: [LMAX-Exchange-disruptor](https://github.com/LMAX-Exchange/disruptor).

#### Why is `Disruptor` so good?

In scenarios where millions of messages need to be processed, `Disruptor` shows extremely low latency and high throughput. Traditional queues in multi-producer, multi-consumer environments usually suffer from performance drops due to lock contention, memory allocation, and synchronization mechanisms. `Disruptor` solves these issues with a lock-free concurrency model, greatly improving performance.  
In concurrent environments, `Disruptor` uses two main mechanisms to achieve efficient multi-producer concurrency: the `CAS (Compare-And-Swap)` operation and a memory marking mechanism. These two features work together to make it high-performance, ensuring data correctness and safety in high-concurrency scenarios.

#### A. CAS (Compare-And-Swap) for Concurrent Writes
`CAS` is a synchronization method used to solve shared data update problems in concurrent programming without locks. It ensures that only one thread can successfully update a variable by comparing and swapping, preventing multiple threads from modifying the same data at the same time.

The basic operation of `CAS` is as follows:
1. **Compare**: Check if the current value at a memory address matches the expected value.
2. **Swap**: If they match, update the value at this address; if not, it means another thread has modified the value, so the current thread fails and must retry.

`CAS` is atomic, meaning the operation either fully succeeds or fully fails, with no partial states. This method is ideal for updating variables in multi-threaded environments, especially for avoiding "race conditions" (where multiple threads compete to modify the same data).

Now, let's go back to the earlier `kFifo` example. We'll try to modify the `kfifo_in` function to support concurrent writing.

Suppose we want to modify the `kFifo_in` function to allow multi-threaded writes. The main problem we need to solve is: how do we let multiple threads safely claim space in the buffer without overwriting each other?

In a single-threaded situation, the `in` pointer can be directly increased to represent the writing area. However, in a multi-threaded environment, multiple threads may try to change this pointer at the same time, leading to data conflicts. We can use the `CAS` operation to solve this problem. Each thread will use `CAS` to claim space before writing data, ensuring that the pointer is correctly updated.

Here’s an example of modifying the `kFifo_in` to support concurrent writing:

```c
unsigned int kfifo_in_concurrent(struct kfifo *fifo, const void *buf, unsigned int len)
{
    unsigned int old_in, new_in, free_space;

    do {
        // 1. Get the current position of the in pointer
        old_in = fifo->in;

        // 2. Calculate the remaining available space in the ring buffer
        free_space = fifo->size - (fifo->in - fifo->out);
        if (len > free_space) {
            return 0;  // If there isn’t enough space, fail the write
        }

        // 3. Calculate the new position of the in pointer
        new_in = old_in + len;

        // 4. Try to update the in pointer with CAS
        // If the current in value is still old_in, successfully update it to new_in, otherwise retry
    } while (!__sync_bool_compare_and_swap(&fifo->in, old_in, new_in));

    // 5. Now the space is safely claimed, start writing the data. The current thread owns the len bytes from fifo->in.
    write_data_to(fifo->in/*to*/, buf/*from*/, len/*size*/);

    return len;
}
```

##### Why does CAS solve concurrent writing problems?

In a multi-threaded write scenario, several threads might try to modify the `in` pointer at the same time, causing write conflicts. `CAS` ensures that only one thread can successfully update the pointer, and the failed threads can retry to claim free space.  

This design ensures that during concurrent writes, each producer thread will not write to the same memory area. Through `CAS`, each producer thread can claim its own safe space to write data. Compared to traditional locking mechanisms, `CAS` avoids lock contention and overhead, greatly improving concurrent performance.

#### B. Memory Marking for Correct Reading
In `Disruptor`, the producer uses a `CAS` operation to claim memory space before writing data. This is different from `kFifo`, where memory is written first and then claimed.

The advantage of this design is that producers can ensure they have exclusive memory space before writing, avoiding the risk of data being overwritten in a multi-producer environment. Also, in concurrent scenarios, `CAS` reduces contention and avoids the performance loss associated with traditional locks.

However, this also introduces a new problem. In `kFifo`, since memory is written first and then the index is updated, `in` indicates the latest position of the data. But in `Disruptor`, where space is claimed before writing, the `in` pointer no longer serves this purpose, as shown in the diagram below.
![Disruptor Alloc](img/Disruptor_Alloc.png)

The diagram shows the memory of the `Disruptor` message queue, where `out` represents the position that has been read, but the meaning of `in` has changed. We can see that threads A, B, and C have each claimed three blocks of space in the message queue. Threads A and C have successfully written data, but Thread B has not finished yet. In this case, if we still use `in` as the final read position, it would be incorrect. The only role of `in` now is to mark the starting position for the next producer to write. So, we need a new mechanism to know where the consumer can read up to.

To solve this problem, `Disruptor` introduces a new memory section to mark whether each block of memory (or `Slot`) has been fully written. When reading, the consumer checks these markers to verify if the data in the corresponding block is complete. Since Java objects are references, each marker can be represented by a pointer-sized data unit. However, in environments where data is directly copied onto the message queue, the solution would need some adjustments. Of course, this is beyond the scope of this article. If you're interested, you can explore `Disruptor`'s source code and documentation.




### 3. BqLog Ring Buffer

The `CAS` operation in `Disruptor` has become a standard for concurrent programming, especially in high-concurrency scenarios. While `CAS` brings performance improvements, it’s not a perfect solution and has problems in certain situations.

#### Why isn’t `CAS` as good as it seems?

The core idea of `High-Concurrency` design is to avoid threads being blocked by locks. By using atomic operations like `CAS`, multiple threads can compete to update shared data without waiting for a lock. This approach reduces context switching and lock contention, which makes it perform well in high-concurrency environments.

"While `CAS` operations eliminate the need for locks, they do not guarantee efficient execution across all threads. `CAS` inherently involves competitive access, requiring threads that fail in their `CAS` attempts to retry. This process can introduce delays and variability in performance. In scenarios with high contention, frequent `CAS` failures can lead to significant inefficiencies, preventing threads from completing their tasks within predictable or optimal time frames and thus impacting the overall performance of the system." 

For example, in a highly concurrent environment, one thread might keep failing and never update its data. While the system doesn’t deadlock, some threads will experience significant delays, leading to poor overall throughput and latency.

#### BqLog’s Optimized Implementation

The message queue `bq::miso_ring_buffer` in `BqLog` implements memory allocation with fixed overhead using a proprietary algorithm that replaces `CAS` with `fetch_add`. It also features a rollback mechanism for scenarios where there is insufficient space, ensuring that both producers and consumers can complete logging write and read operations within a fixed number of steps under high concurrency. The code implementation can be referenced.
[https://github.com/Tencent/BqLog/blob/main/src/bq_log/types/miso_ring_buffer.h](https://github.com/Tencent/BqLog/blob/main/src/bq_log/types/miso_ring_buffer.h)  
[https://github.com/Tencent/BqLog/blob/main/src/bq_log/types/miso_ring_buffer.cpp](https://github.com/Tencent/BqLog/blob/main/src/bq_log/types/miso_ring_buffer.cpp)  

#### What is `fetch_add` and why is it better?

`fetch_add` is another atomic operation that’s important in concurrent programming. It works in two steps:

1. **Get the current value**: Read the current value of a variable.
2. **Add and update**: Add a specified number to the value and update it.

`fetch_add` guarantees that the operation will always succeed, so even when multiple threads operate at the same time, each thread can safely update the variable without needing to retry or wait.

Unlike `CAS`, `fetch_add` doesn’t require retries because it can’t fail. Each thread gets a unique value and performs the addition, updating the variable in one step. This means every thread can complete its operation without being blocked by competition.

Let’s see an example where we modify the `kfifo_in_concurrent` function to use `fetch_add`:

```c
unsigned int kfifo_in_concurrent(struct kfifo *fifo, const void *buf, unsigned int len)
{
    // 1. Calculate the remaining space in the ring buffer
    unsigned int free_space = fifo->size - (fifo->in - fifo->out);
    if (len > free_space) {
        return 0;  // If there’s not enough space, the write fails
    }

    // 2. Claim memory. The memory claimed starts from 'from' for a length of 'len'
    unsigned int from = __sync_fetch_add(fifo->in, len);
    
    // 3. Now that the space is safely claimed, start writing data. The current thread owns the 'len' bytes from 'fifo->in'.
    write_data_to(from/*to*/, buf/*from*/, len/*size*/);
}

unsigned int allocated_A = kfifo_in_concurrent(&fifo, buf, 10);  // Called by Thread A
unsigned int allocated_B = kfifo_in_concurrent(&fifo, buf, 5);   // Called by Thread B
unsigned int allocated_C = kfifo_in_concurrent(&fifo, buf, 15);  // Called by Thread C
```

If `fifo->in` starts at 0, after these three threads execute, the memory locations could be:

- **Thread A**: 0
- **Thread B**: 10
- **Thread C**: 15
- **fifo->in**: 30

Or:
- **Thread A**: 20
- **Thread B**: 0
- **Thread C**: 5
- **fifo->in**: 30

See the diagram below:

![BqLog FetchAdd](img/BqLog_Fetch_Add_Demo.png)

In this way, the three threads each get their own memory segment without any lock or waiting.

By using `fetch_add`, each thread can claim its own space atomically, without check whether memory allocating is successed or not.

#### The limitations of `fetch_add` and the rollback mechanism
If `fetch_add` makes better performance so easy, why do most message queues still use `CAS`? The problem with `fetch_add` is that it has one critical flaw. Let’s go back to the previous example. Imagine the buffer has a maximum size of 25, and threads A, B, and C all execute `kfifo_in_concurrent` at the same time. They each check the remaining space and see 25, which seems enough for their writes. But when they all perform `fetch_add`, each thread believes it successfully claimed memory, but in reality, the last thread's claim is invalid.

In contrast, `CAS` avoids this problem because the final claim only succeeds if no other thread has changed the memory, and `in` matches what was checked earlier.

To solve this problem, ensuring both a fixed number of steps for memory allocation and accurate results, `bq::miso_ring_buffer` has developed a rollback mechanism. This mechanism performs a rollback when there is insufficient space, and then returns an error indicating insufficient space. The pseudocode for memory allocation is as follows:

```c
void* bq::miso_ring_buffer::alloc(size_t len)
{
    // 1. Calculate the remaining space in the ring buffer
    size_t free_space = this->size_ - (this->in_ - this->out_);
    if (len > free_space) {
        return nullptr;  // If there’s not enough space, the write fails
    }

    // 2. Claim memory, starting from 'from' for a length of 'len'
    size_t from = __sync_fetch_add(this->in_, len);

    // 3. Check if the claimed memory is valid
    while(from + len > this->out_ + this->size_)
    {
        // 4. If space is insufficient, perform a rollback
        size_t expected_in = from + len;
        if(__sync_bool_compare_and_swap(expected_in, this->in_, from))  // Rollback if `in_` equals `from + len`, otherwise spin and retry
        {
            // Rollback successful
            return nullptr;  // Return insufficient space
        }
        yield();  // Yield CPU time to avoid wasting resources
    }
    
    // 5. Memory is valid, either it was valid when claimed, or new space was freed during rollback
    return to_addr(from);
}
```

This code not only demonstrates how `bq::miso_ring_buffer` allocates memory using `fetch_add`, but also shows the rollback algorithm when there is insufficient space. Some might question the performance of this rollback algorithm, but it’s important to understand that when a rollback occurs, it indicates that the message queue is running out of space. At this point, the system's performance bottleneck becomes the need to expand the message queue or block until consumer threads retrieve data and free up space. Under such circumstances, the performance cost of the `CAS` operation becomes negligible.

Now, let’s explain why the rollback algorithm needs to use `CAS` rather than simply doing `fetch_add(this->in_, -len)` to subtract the claimed length. The challenge with rollback is that after `in` exceeds the limit, each producer doesn’t know how much to roll back without causing issues.

Take a look at the example below:

![Rollback 1](img/rollback_1.png)

Before this round of allocations, `in` was 1000, and there were 12 bytes left, meaning allocations up to 1012 are valid. But because threads A, B, and C all requested memory at the same time, `in` was pushed to 1030. Threads B and C realize their allocations exceeded the limit and prepare to roll back. If thread B rolls back first, here’s what could happen:

1. Thread B reduces `in` to 1025 using `fetch_add(this->in_, -5)`.
2. The consumer thread D frees up space, allowing `in` to go up to 1040.
3. Thread B tries to allocate 5 bytes again and gets the memory from 1025 to 1030, pushing `in` to 1030.
4. Thread C starts its rollback, finds its allocation is now valid, and writes data.

This leads to the following problem:

![Rollback 2](img/rollback_2.png)

As shown, data allocated to threads B and C overlaps.

The core principle of the `CAS` rollback algorithm is to have `in` roll back step by step, with each thread responsible for rolling back its own allocation. If space is freed during rollback, it can stop rolling back.

#### Solution Summary
The combination of `fetch_add` and rollback in `BqLog` has created a optimized "High-Concurrency" queue model. Based on the final benchmark results, in terms of throughput and latency, this approach has outperformed `LMAX Disruptor` in multi-concurrent scenarios. While this optimization doesn’t have much impact on client applications, it shows significant value in server-side or other high-concurrency environments.








