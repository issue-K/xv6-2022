# Lab 2: system calls

在上一个实验中，你使用系统调用编写了一些实用程序。在这个实验中，大家将向xv6添加一些新的系统调用，这将帮助大家理解系统调用的工作原理，并向大家展示xv6内核的一些内部结构。你将在以后的实验中添加更多的系统调用。

> 在开始之前, 阅读一下xv6书读第二章, 和4.3,4.4节, 和一些相关文件
>
> - 将系统调用路由到内核的用户空间“存根”位于user/usys.S中，它是由user/usys.pl在运行make时生成的。定义保存在user/user.h中
> - 将系统调用路由到实现该调用的内核函数的内核空间代码在kernel/syscall.c和kernel/syscall.h中。
> - 与进程相关的代码是kernel/proc.h和kernel/proc.c。



## First Syscall

这个实验的内容是添加两个系统调用函数, 那么先研究一下系统调用函数是如何启动的.

首先, 让我们不要关注太多细节,  先看内核模式代码的入口点`kernel/main.c`

首先它执行一系列初始化工作

```c
kinit：设置好页表分配器(page allocator)
kvminit：设置好虚拟内存，这是下节课的内容
kvminithart：打开页表，也是下节课的内容
processinit：设置好初始进程或者说设置好进程表单
trapinit/trapinithart：设置好user/kernel mode转换代码
plicinit/plicinithart：设置好中断控制器PLIC（Platform Level Interrupt Controller），我们后面在介绍中断的时候会详细的介绍这部分，这是我们用来与磁盘和console交互方式
binit：分配buffer cache
iinit：初始化inode缓存
fileinit：初始化文件系统
virtio_disk_init：初始化磁盘
userinit：初始化第一个用户进程
```

这个`userinit`是一个小程序, 定义在`kernel/proc.c`的`initcode`中, 它是程序的二进制形式, 对应的汇编代码为`user/initcode.S`

```c
// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};
```

观察汇编代码`user/initcode.S`, 它将init中的地址加载到a0(la a0, init)，argv中的地址加载到a1(la a1, argv)，exec系统调用对应的数字加载到a7(li a7, SYS_exec)，最后调用ecall。所以这里执行了3条指令，之后在第4条指令将控制权交给了操作系统。

```asm
# 以下代码截自user/initcode.S
0000000000000000 <start>:
#include "syscall.h"

# exec(init, argv)
.globl start
start:
        la a0, init
   0:	00000517          	auipc	a0,0x0
   4:	00050513          	mv	a0,a0
        la a1, argv
   8:	00000597          	auipc	a1,0x0
   c:	00058593          	mv	a1,a1
        li a7, SYS_exec
  10:	00700893          	li	a7,7
        ecall
  14:	00000073          	ecall
```

userinit会创建初始进程，返回到用户空间，执行刚刚介绍的3条指令，再回到内核空间, 执行`SYS_exec`系统调用, 加载`user/init.c`程序

在`init.c`程序中会启动`shell`.



是的, 暂时还不需要了解太多, 不过有几点是需要知道的......

- a0-a6寄存器存储参数(例如上面的exec系统调用, a0存程序地址, a1存其余参数), a7寄存器存储系统调用号
- 通过ecall从用户空间返回内核空间, 执行syscall。内核通过检查a7的值判断调用哪个系统调用

所以添加一个系统调用, 需要

- 在`kernel/syscall.h`中添加新的系统调用号
- 在`kernel/syscall.c`中的`syscalls`数组添加对应的系统调用函数指针, 并`extern`该函数(该函数另外的文件实现)
- 在对应文件中实现该系统调用
- 为`user/user.h`添加一个系统调用的原型, 为`user/usys.pl`添加一个存根, 然后`Makefile`会利用它生成`user/usys.S`, 这是用户系统调用的原型实现，使用`ecall`指令调用内核的`syscall`函数.

## System call tracing ([moderate](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

> 在这个任务中，您将添加一个系统调用tracing功能，这可能有助于您在调试以后的实验。您将创建一个新的trace系统调用来控制跟踪。它应该接受一个参数，一个整数“掩码”，其比特位指定要跟踪的系统调用。例如，要跟踪fork系统调用，一个程序调用了trace(1 << SYS_fork)，其中SYS_fork是一个系统调用编号，来自kernel/syscall.h。你必须修改xv6内核，使其在每个系统调用即将返回(如果掩码中设置了系统调用的编号)时打印出一行。这一行应该包含进程id、系统调用的名称和返回值;你不需要打印系统调用参数。trace系统调用应该启用对调用它的进程及其后续分支的任何子进程的跟踪，但不应该影响其他进程。

我们提供了一个跟踪用户级程序，它可以运行另一个启用了跟踪的程序(参见user/trace.c)。完成后，应该可以看到类似下面的输出:

```shell
$ trace 32 grep hello README
3: syscall read -> 1023
3: syscall read -> 966
3: syscall read -> 70
3: syscall read -> 0
$
$ trace 2147483647 grep hello README
4: syscall trace -> 0
4: syscall exec -> 3
4: syscall open -> 3
4: syscall read -> 1023
4: syscall read -> 966
4: syscall read -> 70
4: syscall read -> 0
4: syscall close -> 0
$
$ grep hello README
$
$ trace 2 usertests forkforkfork
usertests starting
test forkforkfork: 407: syscall fork -> 408
408: syscall fork -> 409
409: syscall fork -> 410
410: syscall fork -> 411
409: syscall fork -> 412
410: syscall fork -> 413
409: syscall fork -> 414
411: syscall fork -> 415
...
$
```

**实现**

- 在Makefile的UPROGS中添加`$U/_trace`
- 为`user/user.h`添加它的用户系统调用原型,  并在`user/usy.pl`添加对应的模块, 然后`Makefile`会根据它生成用户系统调用实现.(也就是调用ecall)
- 修改`kernel/syscall.c`中添加对应函数指针, 在`kernel/sysproc.c`中添加一个`sys_trace()`函数来具体实现该系统调用
- 为了实现该系统调用, 你需要向`proc`结构体(参见`kernel/proc.h`)添加属性. 比如, 保存该进程被追踪的掩码号
- 修改`fork() `(参见`kernel/proc.c`)，你需要把父进程的掩码复制到子进程
- 最后修改`kernel/syscall.c`中的`syscall()`函数. 它是具体的路由函数, 路由到各个系统调用, 所以这里可以很轻松输出该进程正在执行什么系统调用.(如果它的掩码正在追踪此系统调用)

这是修改后的`proc`结构体

```cpp
// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // wait_lock must be held when using this:
  struct proc *parent;         // Parent process

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  // My Code
  int mask; // for sys_trace
};
```

然后, `trace`系统调用的具体实现如下所示

```cpp
// kernel/sysproc.c
uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);  // 从a0寄存器解析出参数
  struct proc *p = myproc();
  p->mask |= mask;
  return 0;
}

// kernel/proc.c
int
fork(void)
{
...........
  // My Code
  np->mask = p->mask;  // 子进程复制父进程的mask
  return pid;
}
```



## Sysinfo ([moderate](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

> 在这个任务中，您将添加一个系统调用sysinfo，它收集关于正在运行的系统的信息。该系统调用有一个参数: 一个指向结构体sysinfo的指针(参见kernel/sysinfo.h)。内核应该填写该结构体的各个字段: freemem字段应该设置为空闲内存的字节数，nproc字段应该设置为状态被使用的进程的数目。我们提供了一个测试程序sysinfotest;如果你通过了这个任务, 它会打印出"sysinfotest: OK"

**实现**

- 在Makefile的UPROGS添加`$U/_sysinfotest`

- 添加用户系统调用声明.在`user/user.h`添加定义

  ```cpp
  int sysinfo(struct sysinfo *);
  ```

  添加系统调用指针方便`syscall`路由, 在`kernel/syscall.c`添加

  ```cpp
  extern uint64 sys_sysinfo(void);
  static uint64 (*syscalls[])(void) = {
  .......
  [SYS_sysinfo] sys_sysinfo,
  };
  ```

- 实现`sysinfo`系统调用. 需要注意这里的参数是个指针, 那么只需要把地址解析出来就好了.

  直接转换为指针是不对的, 比较现在使用的页表是内核的而不是用户空间的.

  这里我把这两个函数实现在了`kernel/sysproc.c`

  这里需要注意的是, 我们需要把``sysinfo`的内容复制回用户空间. 毫无疑问这需要用户空间的页表, `copyout()`函数可以做到这一点

  ```cpp
  uint64 sysinfo(uint64 addr) {
    struct proc *p = myproc();
    struct sysinfo info;
    // 空闲内存的数量, kernel/kalloc.c中实现
    info.freemem = GetFreeMem();
    // 进程的数量，kernel/proc.c中实现
    info.nproc = GetNproc();
    if(copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0)
      return -1;
    return 0;
  }
  
  uint64
  sys_sysinfo(void)
  {
    uint64 addr;
    argaddr(0, &addr);
    return sysinfo(addr);
  }
  ```

- 首先获取当前进程数量, `GetNproc()`函数实现在`kernel/proc.c`中

	只需要遍历进程数组, 检查`state`属性即可知道该进程是否在使用
	
	```cpp
	uint64 
	GetNproc(void) 
	{
	  struct proc *p;
	  uint64 num = 0;
	  for(p = proc; p < &proc[NPROC]; p++) {
	    acquire(&p->lock);
	    if (p->state != UNUSED) {
	      num++;
	    }
	    release(&p->lock);
	  }
	  return num;
	}

- 然后获取当前空闲内存的数量,  `GetFreeMem()`函数实现在`kernel/kalloc.c`

  这里需要分析一下这个文件的管理内存逻辑. 首先, 内核通过下面这条链表结构管理内存

  ```cpp
  struct run {
    struct run *next;
  };
  ```

  所有空闲链表串起来, 链表末尾的地址值是0, 链表开头则是`freelist`

  ```cpp
  struct {
    struct spinlock lock;
    struct run *freelist;
  } kmem;
  ```

  链表的相邻两个节点的地址值刚好相差`PGSIZE`的大小, 这个可以从`freerange()`函数看出来

  ```cpp
  void
  freerange(void *pa_start, void *pa_end)
  {
    char *p;
    p = (char*)PGROUNDUP((uint64)pa_start);
    for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
      kfree(p);
  }
  ```

  于是获得空闲内存数目就很简单了, 直接遍历`freelist`, 节点数乘以`PGSIZE`就是答案

  ```cpp
  uint64 
  GetFreeMem(void) 
  {
    acquire(&kmem.lock);
    struct run *r = kmem.freelist;
    uint64 freemem = 0;
    while (r) {
      freemem += PGSIZE;
      r = r->next;
    }
    release(&kmem.lock);
    return freemem;
  }
  ```