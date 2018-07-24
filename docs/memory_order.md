# 内存序与原子操作
- Memory_order
``` C++
    typedef enum memory_order {
    memory_order_relaxed,
    memory_order_consume,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
} memory_order;
```

    * std::memory_order 指定常规的非原子内存访问如何围绕原子操作排序。在没有任何制约的多处理器系统上，多个线程同时读或写数个变量时，一个线程能观测到变量值更改的顺序不同于另一个线程写它们的顺序。其实，更改的顺序甚至能在多个读取线程间相异。一些类似的效果还能在单处理器系统上出现，因为内存模型允许编译器变换。
    * 线程间同步和内存顺序决定表达式的`求值`和`副效应`在程序执行的不同线程之间如何排序：
        1. `先序于`
        2. `携带依赖`：大致可以理解为求值A的值被求值B使用，比如A的值被用作B的运算数，或者是A写入某个标量M，而B读取这个标量M，或者是A将携带依赖带入另一个求值X，而X将依赖携带入B，也就是依赖传递。
        3. `修改顺序`：大致可以理解为前者对一个对象的修改，必须出现在后者对该对象的修改之前，比如`写写连贯`， `读读连贯`，`读写连贯`，`写读连贯`。
        4. `释放序列`
        5. `依赖先序于`
        6. `线程间先发生于`
        7. `先发生于`
        8. `可见副效应`
        9. `memory_order_consume`
        10. `memory_order_release`
        11. `memory_order_acquire`
    就是很多的不同的内存序和编程语句之间的依赖，导致了程序中的代码的执行顺序，大致可以理解为要么是天生的规则排序，比如先序于规则，导致一些操作必定在另一些操作之前完成；要么是同一个线程之间的语句之间存在依赖关系，比如读写依赖，那么注定这些语句是不能够乱序的，否则得不到合乎逻辑的结果；要么就是线程之间的原子变量的释放和获取导致的同步操作，典型的就有`release-acquire`操作，对照到比较高层的语法，就有`mutex_lock`和`mutex_unlock`等操作。

- 举例
    * 宽松顺序：memory_order_relaxed。
        1. 带有这种标签的原子操作基本上没有同步操作，它们不会在共时的内存访问间强加顺序。它们只保证原子性和修改顺序一致性。这种模式最典型的使用是计数器自增，例如std::shared_ptr的引用计数器，因为只要求原子性，但是不要求顺序性或同步。
    * 释放获得顺序：memory_order_release和memory_order_acquire.
        1. 这种顺序大致可以这样认为：若线程 A 中的一个原子存储带标签 memory_order_release ，而线程 B 中来自同一变量的原子加载带标签 memory_order_acquire ，则从线程 A 的视角先发生于原子存储的所有内存写入（非原子及宽松原子的），在线程 B 中成为可见副效应，即一旦原子加载完成，则保证线程 B 能观察到线程 A 写入内存的所有内容。
        2. 这种模式最典型的应用场景就是互斥锁或者原子自旋锁：锁被线程A释放且被线程B获得时，发生于线程A环境的临界区中的所有事件，必须对于执行统一临界区的线程B可见。其实在大部分的强顺序系统中，释放获取顺序对于多数操作是自动进行的，不需要为此同步操作添加额外的CPU指令。
    ``` C++
    #include <thread>
    #include <atomic>
    #include <cassert>
    #include <string>
 
    std::atomic<std::string*> ptr;
    int data;
 
    void producer() {
        std::string* p  = new std::string("Hello");
        data = 42;
        ptr.store(p, std::memory_order_release);
    }
 
    void consumer() {
        std::string* p2;
        //这个while循环是至关重要的，就是通过这个while循环，让acquire看到release操作store的值。
        while (!(p2 = ptr.load(std::memory_order_acquire))); 
        assert(*p2 == "Hello"); // 绝无问题
        assert(data == 42); // 绝无问题
    }
 
    int main()
    {
        std::thread t1(producer);
        std::thread t2(consumer);
        t1.join(); t2.join();
    }
    ```
    * 释放消费顺序，memory_order_consume。
        1. 若线程 A 中的原子存储带标签 memory_order_release 而线程 B 中来自同一原子对象的加载带标签 memory_order_consume ，则线程 A 视角中依赖先序于原子存储的所有内存写入（非原子和宽松原子的），会在线程 B 中该加载操作所携带依赖进入的操作中变成可见副效应，即一旦完成原子加载，则保证线程B中，使用从该加载获得的值的运算符和函数，能见到线程 A 写入内存的内容。
        2. 可以看到和memory_order_acquire的区别在于只对新厂B中该加载操作所携带依赖进入的操作具有可见性，而不是对说有的store都具有可见性，可以说比acquire具有更高的自由度，更弱的顺序约束性
        3. 最常见的应用场景， 设计很少被写入的数据结构(安排表、配置、安全策略、防火墙规则等)的共时读取，和有指针中介发布的发布者-订阅者情形：无需生产者写入内存的所有其它内容对消费者可见。比如rcu解引用。
    ``` C++
    #include <thread>
    #include <atomic>
    #include <cassert>
    #include <string>
 
    std::atomic<std::string*> ptr;
    int data;
 
    void producer() {
        std::string* p  = new std::string("Hello");
        data = 42;
        ptr.store(p, std::memory_order_release);
    }
 
    void consumer() {
        std::string* p2;
        while (!(p2 = ptr.load(std::memory_order_consume)));
        assert(*p2 == "Hello"); // 绝无出错： *p2 从 ptr 携带依赖
        assert(data == 42); // 可能也可能不会出错： data 不从 ptr 携带依赖
    }
 
    int main()
    {
        std::thread t1(producer);
        std::thread t2(consumer);
        t1.join(); t2.join();
    }
    ```
    * 序列一致顺序，memory_order_seq_cst。
        1. 带标签 memory_order_seq_cst 的原子操作不仅以与释放/获得顺序相同的方式排序内存（在一个线程中先发生于存储的任何结果都变成做加载的线程中的可见副效应），还对所有拥有此标签的内存操作建立一个单独全序。
        2. 可以理解为，使用这种模式，从多线程的角度来看，多线程是以某种指定的顺序交叉执行的，这种指定的顺序以输出结果的结果来看是相同的。
    ``` C++
    #include <thread>
    #include <atomic>
    #include <cassert>
 
    std::atomic<bool> x = {false};
    std::atomic<bool> y = {false};
    std::atomic<int> z = {0};
 
    void write_x() {
        x.store(true, std::memory_order_seq_cst);
    }
 
    void write_y() {
        y.store(true, std::memory_order_seq_cst);
    }
 
    void read_x_then_y() {
        while (!x.load(std::memory_order_seq_cst));
        if (y.load(std::memory_order_seq_cst)) {
            ++z;
        }
    }
 
    void read_y_then_x() {
        while (!y.load(std::memory_order_seq_cst));
        if (x.load(std::memory_order_seq_cst)) {
            ++z;
        }
    }
 
    int main()
    {
        std::thread a(write_x);
        std::thread b(write_y);
        std::thread c(read_x_then_y);
        std::thread d(read_y_then_x);
        a.join(); b.join(); c.join(); d.join();
        assert(z.load() != 0);  // 决不发生
    }
    ```