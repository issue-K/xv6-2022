# Lab : Multithreading

​	这个实验将使你熟悉多线程。您将在用户级线程包中实现线程之间的切换，使用多线程来加速程序，并实现`barrier`。

> 在开始之前, 阅读xv6 book 的第7章



## Uthread: switching between threads ([moderate](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

> >在这个练习中，你将为用户级线程系统设计上下文切换机制，然后实现它。首先，在xv6中有两个文件`user/uthread.c`和`user/uthread_switch.S`，以及在Makefile中构建`uthread`程序的规则。`uthread.c`包含了用户级线程包的大部分内容，以及三个简单测试线程的代码。threading包缺少一些创建线程和在线程之间切换的代码。
>
> > 你的工作是实现一个方案来创建线程并保存/恢复寄存器以在线程之间切换。完成后，make grade应该显示你的解决方案通过了uthread测试。
>
> 完成后，在xv6上运行uthread时，应该会看到如下输出(三个线程的启动顺序可能不同):
>
> ```shell
> $ make qemu
> ...
> $ uthread
> thread_a started
> thread_b started
> thread_c started
> thread_c 0
> thread_a 0
> thread_b 0
> thread_c 1
> thread_a 1
> thread_b 1
> ...
> thread_c 99
> thread_a 99
> thread_b 99
> thread_c: exit after 100
> thread_a: exit after 100
> thread_b: exit after 100
> thread_schedule: no runnable threads
> $
> ```
>
> 这个输出来自三个测试线程，每个线程都有一个循环，打印一行，然后将CPU让给其他线程。
>
> 不过，在没有上下文切换代码的情况下，你将看不到任何输出。
>
> 你需要将代码添加到`user/uthread.c`中的`thread_create()`和`thread_schedule()`，以及`user/uthread_switch.S`中的`thread_switch`。
>
> 一个目标是确保当`thread_schedule()`第一次运行给定的线程时，线程在自己的栈上执行传递给`thread_create()`的函数。
>
> 另一个目标是确保thread_switch保存被切换的线程的寄存器，恢复被切换的线程的寄存器，并返回到后一个线程指令中最后离开的位置。
>
> 你必须决定在哪里保存/恢复寄存器;修改struct thread来保存寄存器是一个不错的计划。
>
> 你需要在thread_schedule中添加对thread_switch的调用, 你可以向thread_switch传递任何需要的参数，但目的是从线程t切换到next_thread。

**solution**

和内核内的线程切换差不多. 首先, 给thread增加一个context字段, 来保存切换线程时的寄存器.

由于线程切换是调用`thread_switch`函数进行的. 所以只需要在该函数中保存老线程的所有寄存器, 恢复新线程的所有寄存器即可.

但是这里只保存了一部分寄存器, 因为其余寄存器在`ret`指令中会恢复.

```cpp
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

struct thread {
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  struct context ctx;
};
```

创建一个新线程, 这里只需要初始化他的栈和ra寄存器. 因为第一次通过`thread_switch`切换到该线程是通过ret指令.

ret指令会返回到ra寄存器的位置执行, 那么这里初始化为传入函数的首地址即可.

```cpp
void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  // YOUR CODE HERE
  // My Code
  t->ctx.sp =(uint64)&t->stack[STACK_SIZE];
  t->ctx.ra = (uint64)func;
}
```

然后是调度线程的函数

```cpp
void 

thread_schedule(void)
{
  struct thread *t, *next_thread;

  /* Find another runnable thread. */
  next_thread = 0;
  t = current_thread + 1;
  for(int i = 0; i < MAX_THREAD; i++){
    if(t >= all_thread + MAX_THREAD)
      t = all_thread;
    if(t->state == RUNNABLE) {
      next_thread = t;
      break;
    }
    t = t + 1;
  }

  if (next_thread == 0) {
    printf("thread_schedule: no runnable threads\n");
    exit(-1);
  }

  if (current_thread != next_thread) {         /* switch threads?  */
    next_thread->state = RUNNING;
    t = current_thread;
    current_thread = next_thread;
    /* YOUR CODE HERE
     * Invoke thread_switch to switch from t to next_thread:
     * thread_switch(??, ??);
     */
    thread_switch((uint64)(&t->ctx), (uint64)(&current_thread->ctx));
  } else
    next_thread = 0;
}
```




## Using threads ([moderate](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

> 在本作业中，你将探索使用哈希表进行线程和锁的并行编程。您应该在具有多核的真正Linux或MacOS计算机(不是xv6，也不是qemu)上执行此任务。大多数最新的笔记本电脑都有多核处理器。
>
> 这个作业使用UNIX pthread线程库。你可以从手册页找到相关信息，用man pthreads，你也可以在网上找，比如这里，这里，还有这里。
>
> 文件notxv6/ph.c包含一个简单的哈希表，如果从单个线程使用该哈希表是正确的，但如果从多个线程使用该哈希表则不正确。在您的主xv6目录(可能是~/xv6-labs-2021)中，键入以下内容:
>
> ```shell
> $ make ph
> $ ./ph 1
> ```
>
> 注意，使用你的os的gcc构建ph，而不是6.S081工具。
> 给ph的参数指定在哈希表上执行put和get操作的线程数。运行一段时间后，ph 1将产生类似这样的输出:
>
> ```cpp
> 100000 puts, 3.991 seconds, 25056 puts/second
> 0: 0 keys missing
> 100000 gets, 3.981 seconds, 25118 gets/second
> ```
>
> 您看到的数字可能与这个示例输出相差两倍或更多，这取决于您的计算机有多快，它是否有多个核心，以及它是否忙着做其他事情。
>
> ph运行两个基准。首先，它通过调用put()向哈希表添加大量的键，并以每秒的put数为单位打印实现的速率。它用get()从哈希表中获取键。它打印由于put操作而在哈希表中丢失的数字键(在本例中为零)，并打印每秒获得的数字键。
>
> 你可以通过给ph一个大于1的参数，让它同时在多个线程中使用它的哈希表。试试ph 2:
>
> ```cpp
> $ ./ph 2
> 100000 puts, 1.885 seconds, 53044 puts/second
> 1: 16579 keys missing
> 0: 16579 keys missing
> 200000 gets, 4.322 seconds, 46274 gets/second
> ```
>
> 这个ph 2输出的第一行表示，当两个线程并发地向哈希表添加条目时，它们实现的总插入速率为每秒53,044个。这大约是运行ph 1时单线程速度的两倍。这是一个大约2倍的优秀“并行加速”，就像人们可能希望的那样(即两倍的内核在单位时间内产生两倍的工作)。
>
> 然而，缺少16579个键的两行表示哈希表中本应存在的大量键不在那里。也就是说，put应该把这些键添加到哈希表中，但是出了问题。看一下notxv6/ph.c，特别是put()和insert()。
>
> 在answers-thread.txt中解释这个现象, 为什么两个线程就会出现问题.
>
> 要避免这一系列事件，请在notxv6/ph.c中的put和get中插入lock和unlock语句，以便在两个线程中缺少的键数始终为0。相关的pthread调用有:
>
> ```cpp
> pthread_mutex_t lock;            // declare a lock
> pthread_mutex_init(&lock, NULL); // initialize the lock
> pthread_mutex_lock(&lock);       // acquire lock
> pthread_mutex_unlock(&lock);     // release lock
> ```
>
> 当make grade说您的代码通过了ph_safe测试时，您就完成了，该测试要求两个线程中没有缺失键。在这一点上，ph_fast测试失败是可以的。
>
> 不要忘记调用pthread_mutex_init()。先用一个线程测试代码，然后用两个线程测试。它是正确的吗(例如，你已经消除了丢失的键吗?)? 双线程版本相对于单线程版本是否实现了并行加速(即单位时间内更多的总工作)?
>
> 在某些情况下，并发put()在哈希表中读写的内存没有重叠，因此不需要锁来相互保护。你能改变ph.c来利用这种情况来获得一些put()的并行加速吗?提示:每个哈希桶一个锁怎么样?
>
> 修改代码，使一些put操作在保持正确性的同时并行运行。当make grade说你的代码同时通过了ph_safe和ph_fast测试时，你就完成了。ph_fast测试要求两个线程每秒产生的put次数至少是一个线程的1.25倍。



## Barrier([moderate](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

> 在这个作业中，你将实现一个barrier: 应用程序中的一个点，所有参与的线程都必须等待，直到所有其他参与的线程也到达这个点。您将使用pthread条件变量，这是一种序列协调技术，类似于xv6的sleep和wakeup。
>
> 您应该在一台真正的计算机(不是xv6，也不是qemu)上执行此分配。
>
> 文件notxv6/barrier.c包含一个不完整的barrier。
>
> ```cpp
> $ make barrier
> $ ./barrier 2
> barrier: notxv6/barrier.c:42: thread: Assertion `i == t' failed.
> ```
>
> 参数2指定在barrier上同步的线程数(barrier.c中的nthread)。每个线程执行一个循环。在每次循环迭代中，线程调用barrier()，然后随机休眠微秒数。assert触发，因为一个线程在另一个线程到达barrier之前离开了barrier。理想的行为是每个线程都阻塞在barrier()中，直到它们中的所有nthread个线程都调用barrier()。
>
> > 你的目标是达到预期的阻塞行为。除了在ph程序中看到的锁原语之外，您还需要以下新的pthread原语
> >
> > ```cpp
> > pthread_cond_wait(&cond, &mutex);  // go to sleep on cond, releasing lock mutex, acquiring upon wake up
> > pthread_cond_broadcast(&cond);     // wake up every thread sleeping on cond
> > ```
>
> pthread_cond_wait在被调用时释放mutex，并在返回之前重新获取mutex。
> 我们为您提供了barrier_init()。您的工作是实现barrier()，这样就不会发生panic。我们已经为你定义了`struct barrier`; 它的字段供您使用。
>
> 有两个问题会让你的任务变得复杂:
>
> - 首先，屏障的同步是在一系列 barrier() 函数调用中进行的，每次调用 barrier() 时，所有线程必须到达屏障点才能继续执行后续的操作。每个这样的 barrier() 调用称为一轮（round），屏障的当前轮次用 bstate.round 记录。在每个轮次结束后，需要将 bstate.round 加 1，以便下一轮的同步可以开始。
> - 其次，需要处理一个线程在其他线程退出屏障前绕了一圈的情况。具体来说，由于 bstate.nthread 记录了当前轮次到达屏障的线程数，当一个线程在退出屏障后继续执行，会将 bstate.nthread 加 1，如果其他线程还没有退出屏障，那么这个值就会被下一轮使用，导致同步错误。为了避免这种情况，需要在屏障同步期间禁止其他线程修改 bstate.nthread。可以通过加锁来实现这个机制，具体地，在每个轮次开始时，需要将 bstate.nthread 设为 0，然后使用互斥锁保护它，直到所有线程都退出屏障后才能释放锁。这样就可以保证每个轮次使用的是正确的 bstate.nthread 值。

实现很简单......

```cpp
static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  //
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread++;
  int round = bstate.round;
  if (bstate.nthread == nthread) {
    bstate.round++;
    bstate.nthread = 0;
    pthread_cond_broadcast(&bstate.barrier_cond);
  } else {
    while (bstate.round == round) {
      pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
    }
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
}
```


