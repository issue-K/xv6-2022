# Lab 3: page tables

你将在这个实验学习页表并修改它们, 以加快某些系统调用的速度，并检测哪些页被访问过。

> 在开始前, 阅读xv6 book的第三章, 以及下列有关文件
>
> - `kernel/memlayout.h`,  了解内存布局
> - `kernel/vm.c`, 包括了许多virtual memory代码
> - `kernel/kalloc.c`, 包括分配和释放物理内存的代码



## Preview code

### 页表结构/虚拟地址结构/物理地址结构

在xv6中, 虚拟地址由三张页表解析(三级页表), 故虚拟地址的结构分为三级

```cpp
vm 地址结构
offset: [0,11]
L0: [12,20] 12 + 9 * 0
L1: [21,29] 12 + 9 * 1
L2: [30,38] 12 + 9 * 2
```

而页表中的页表项PTE, 包含一个44位的**PPN**`(physical page node)`和10位的flag。

页表硬件会生成一个56位物理地址(这是由RISC-V的设计人员确定的)，其前44位来自PTE中的PPN，后12位直接复制原始虚拟地址的低12位即可。当然, 如果是一二级页表, 物理地址就不需要加offset.

所以如果根据PTE的到对应的值, 应该右移10位再左移12位.

---

### 宏定义分析

那么以下宏就不难理解

```c
#define PGSHIFT 12 // offset
#define PXMASK          0x1FF // 9 bits, 方便取出对应级别页表的具体项
#define PXSHIFT(level) (PGSHIFT+(9*level)))  // level级页表的位置
#define PX(level, va) ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK) // level级页表项索引

// 以下是pa和pte的相互转换
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)
#define PTE2PA(pte) (((pte) >> 10) << 12)

// 对sz上取整为最接近PGSIZE的整数
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
// 对a下取整为最接近PGSIZE的整数
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))
```

### vm.c分析(虚拟地址映射)

所以可以看一下`walk`函数, 通过虚拟地址找到对应的PTE

```cpp
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}
```

而`walkaddr`直接根据虚拟地址的到物理地址(不过似乎只是找到所在的页的物理地址, 没算偏移)

```cpp
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}
```

接下来看一下`main.c`中初始化页表的步骤, 首先是`kvminit`函数

它使用`kvmmap`函数创建 虚拟地址到物理地址的映射关系

```cpp
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// 为从va开始引用的虚拟地址创建pte
// 物理地址从pa开始。Va和size可能不会页面对齐
// 成功时返回0，如果walk()不成功则返回-1
// 分配所需的页表页。
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if(size == 0)
    panic("mappages: size");
  
  a = PGROUNDDOWN(va);  // 映射begin: 下取整, 映射完整的页
  last = PGROUNDDOWN(va + size - 1); // 映射end: 上取整.和上面同理
  for(;;){
    // 使用walk函数创建这个虚拟地址所在的PTE
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}
```

在设置好内核页表好, 注意此时页表还没启用, 然后`main.c`调用`kvminithart`来启用页表

它将第一级根页表page的物理地址写入寄存器`satp`。此后，CPU将使用内核页表转换地址。由于内核使用**直接映射**，下一条指令的当前虚拟地址将映射到正确的物理内存地址。

```cpp
void
kvminithart()
{
  // wait for any previous writes to the page table memory to finish.
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  // flush stale entries from the TLB.
  sfence_vma();
}
```

### kalloc.c(物理内存分配器)

无论页表怎么映射, 还是需要先向物理内存请求才行.

可分配的物理内存有`end`(内核末尾处)到`PHYSTOP`

然后, 使用一条链表把这些空闲页串在一起

```cpp
struct run {
  struct run *next;
};
```

大概就是这样, 接下来就只是对空闲链表增删改查了.

### 进程地址空间

每个进程都有一个单独的页表，当xv6在进程之间切换时，也会改变页表。如图2.3所示，进程的用户内存从虚拟地址0开始，可以增长到`MAXVA`(kernel/riscv.h:348)，原则上允许进程寻址256GB的内存。

<img src="https://img-blog.csdnimg.cn/6225a4819cc34508b01691c7f5690b00.png" width=50%>

**进程申请用户内存**
当一个进程向xv6请求更多的用户内存时，xv6首先使用kalloc来分配物理page。然后它将PTE添加到进程的页表中，指向新的物理page。xv6在这些PTE中设置PTE_W、PTE_X、PTE_R、PTE_U和PTE_V标志。大多数进程不使用整个用户地址空间，xv6将未使用的PTE中的PTE_V清除。

**页表使用案例**
我们在这里看到了一些使用页表的好例子。首先，不同进程的页表将用户地址转换为物理内存不同的page，这样每个进程都有私有的用户内存。其次，每个进程将其内存视为具从0开始的连续虚拟地址，而进程的物理内存可以是不连续的。最后，内核在用户地址空间的顶部映射一个带有trampoline code的页面，这样所有地址空间都可以看到一个单独的物理内存页面。

**xv6进程的用户内存布局**
图3.4更详细地显示了xv6中执行进程的用户内存布局。栈是一个单独的page，显示的是exec创建后的初始内容。包含命令行参数的字符串以及指向它们的指针数组位于堆栈的最顶部。再往下是允许程序从main开始的值，(即main的地址、argc、argv)，这些值产生的效果就像刚刚调用了main(argc, argv)一样。

<img src="https://img-blog.csdnimg.cn/ed2dd5a8fadf4f21a65ae55635a378d1.png" width=80%>

**guard page**
为了检测用户栈溢出分配的栈内存，xv6在栈正下方放置了一个无效的保护page。如果用户栈溢出并且进程试图使用栈下方的地址，由于映射无效，硬件将生成页面错误异常。实际上，操作系统可能会在用户栈溢出时自动为其分配更多内存。

### copyout函数分析

这个函数中在本章的实验用的非常多, 所以具体看一下它的实现。

```cpp
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len);
```

它将内核`src`开始的`len`字节复制到用户页表的目的地址`dstva`处.

不难想到首先需要遍历`pagetable`获得物理地址, 然后将物理地址映射到内核的虚拟地址(由于内核使用直接映射, 省略这步)

接下来, 将`src`开始的`len`字节复制过去即可.

```cpp
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}
```



### 杂项分析

> **TLB**
>
> TLB来源： 如果我们回想一下页表的结构，你可以发现：当处理器从内存加载或存储数据时，基本上都要做3次内存查找。第一次在最高级的page directory，第二次在中间级的page directory，最后一次在最低级的page directory。所以对于一个虚拟地址的寻址，需要读三次内存，代价有点高。所以实际中，几乎所有的处理器都会对最近使用过的虚拟地址的映射结果有缓存，这个缓存被称为：页表缓存(Translation Lookside Buffer, TLB)。基本上来说，这就是Page Table Entry(PTE)的缓存。
>
> 页表条目会缓存在TLB(Translation Look Aside Buffer)中，当xv6更改页表时，它必须告诉CPU使相应的缓存TLB条目无效。如果没有，那么稍后TLB可能会使用旧的缓存映射，指向同时已分配给另一个进程的物理page。结果，一个进程可能占用一些其他进程的内存。RISC-V 有一条指令`sfence.vma`刷新当前CPU的TLB。xv6在重新加载satp寄存器后在kvminithart中执行`sfence.vma`，在返回用户空间之前在切换到用户页表的 trampoline 代码中执行 `sfence.vma`(kernel/trampoline.S:79)。
>
> **procinit**
>
> `main.c`还会调用`procinit`函数. 它为每个进程指定一个内核堆栈的虚拟地址. 
>
> 然后内核为其映射具体一个PTE, 作为进程的无效堆栈保护页
>
> 也就是在`kvmmap`中会调用`proc_mapstacks`来映射一个页.
>
> 你可以看到每个进程的内核堆栈虚拟地址都不一样(差了2个PGSIZE), 因为内核要将这些虚拟地址作为自己的虚拟地址映射到自己的页表, 为其分配物理内存
>
> ```cpp
> // map kernel stacks beneath the trampoline,
> // each surrounded by invalid guard pages.
> #define KSTACK(p) (TRAMPOLINE - (p)*2*PGSIZE - 3*PGSIZE)
> 
> void
> proc_mapstacks(pagetable_t kpgtbl)
> {
>   struct proc *p;
>   
>   for(p = proc; p < &proc[NPROC]; p++) {
>     char *pa = kalloc();
>     if(pa == 0)
>       panic("kalloc");
>     uint64 va = KSTACK((int) (p - proc));
>     kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
>   }
> }
> 
> void
> procinit(void)
> {
>   struct proc *p;
>   
>   initlock(&pid_lock, "nextpid");
>   initlock(&wait_lock, "wait_lock");
>   for(p = proc; p < &proc[NPROC]; p++) {
>       initlock(&p->lock, "proc");
>       p->state = UNUSED;
>       p->kstack = KSTACK((int) (p - proc));
>   }
> }
> ```
>
> 





## Speed up system calls ([easy](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

有些操作系统(例如，Linux)通过在用户空间和内核之间的只读区域共享数据来加速某些系统调用。这消除了在执行这些系统调用时穿越内核的需要。为了帮助大家学习如何将映射插入到页表中，大家的第一个任务是对xv6中的`getpid()`系统调用实现这种优化。

> 在创建每个进程时，在`USYSCALL `(memlayout.h中定义的虚拟地址)映射一个只读页。在该页的起始处，存储一个结构体`usyscall`(也定义在memlayout.h中)，并将其初始化为存储当前进程的PID。对于这个实验，`ugetpid()`已经在用户空间端提供，并将自动使用`USYSCALL`映射。如果运行`pgtbltest`时ugetpid测试用例通过，你将获得这部分实验的全部学分。

- 通过`proc_pagetable()`来执行映射(参见`kernel/proc.c`)
- 选择合适的比特位, 让用户空间只读该页
- `mappages()`可能可以帮到你
- 不要忘记在`allocproc()`中分配和初始化该页
- 确保在`freeproc()`中释放该页

---

首先在`allocproc`函数中， 先找到一个未被使用的`proc`槽位, 调用`proc_pagetable()`来为该进程分配页表. 因此, 首先就应该在这个函数中为进程分配一个`USYCALL`页

```cpp
pagetable_t
proc_pagetable(struct proc *p)
{
.......
  // My Code : 分配一个只读页, 存储该进程的pid
  uint64 pa = (uint64)kalloc();
  if(pa == 0)
    panic("kalloc");
  if (mappages(pagetable, USYSCALL, PGSIZE, (uint64)pa, PTE_R | PTE_U) < 0) {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmunmap(pagetable, TRAPFRAME, 1, 0);
    uvmfree(pagetable, 0);
    kfree((void *)pa);
  }
.......
  return pagetable;
}
```

当然还需要初始化这个页, 接下来在`allocproc()`中完成初始化.(当然, 初始化在执行映射后完成)

```cpp
static struct proc*
allocproc(void) 
{  
  .............
// 为用户空间的页赋值
  struct usyscall src;
  src.pid = p->pid; 
  copyout(p->pagetable, USYSCALL, (char *)(&src), sizeof(src));
  .........
  return p;
}
```

最后, `freeproc()`需要释放掉该页,  在`proc_freepagetable()`中取消映射即可.

```cpp
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmunmap(pagetable, USYSCALL, 1, 1);
  //vmprint(pagetable);
  uvmfree(pagetable, sz);
}
```



## Print a page table ([easy](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

为了帮助您可视化RISC-V页表，或者帮助将来的调试，您的第二个任务是编写一个打印页表内容的函数。

> 定义一个名为vmprint()的函数。它应该接受一个pagetable_t参数，并以下面描述的格式打印该pagetable。在exec.c中return argc之前插入`if(p->pid==1) vmprint(p->pagetable)`，以打印第一个进程的页表。如果你通过了make grade的`pte printout`，你将在这部分的实验中获得满分。

现在，当你启动xv6时，它应该打印如下输出，描述第一个进程刚刚执行完`exec()` init命令时的页表:

```shell
page table 0x0000000087f6b000
 ..0: pte 0x0000000021fd9c01 pa 0x0000000087f67000
 .. ..0: pte 0x0000000021fd9801 pa 0x0000000087f66000
 .. .. ..0: pte 0x0000000021fda01b pa 0x0000000087f68000
 .. .. ..1: pte 0x0000000021fd9417 pa 0x0000000087f65000
 .. .. ..2: pte 0x0000000021fd9007 pa 0x0000000087f64000
 .. .. ..3: pte 0x0000000021fd8c17 pa 0x0000000087f63000
 ..255: pte 0x0000000021fda801 pa 0x0000000087f6a000
 .. ..511: pte 0x0000000021fda401 pa 0x0000000087f69000
 .. .. ..509: pte 0x0000000021fdcc13 pa 0x0000000087f73000
 .. .. ..510: pte 0x0000000021fdd007 pa 0x0000000087f74000
 .. .. ..511: pte 0x0000000020001c0b pa 0x0000000080007000
init: starting sh
```

第一行显示vmprint对参数。之后，每个页表项对应一行，包括引用树中更深层次页表项的页表项。每一行PTE都缩进了一个`" .."`，表示它在树中的深度。

每个PTE行显示其页表页中的PTE索引、PTE位和从PTE中提取的物理地址。在上面的例子中，顶级页表页对应于表项0和255。表项0的下一层只映射了索引0，而索引0的最底层映射了索引0、1和2。

你的代码可能会发出与上面显示的不同的物理地址, 但表项数和虚拟地址数应该相同。

- 把`vmprint()`实现在`kernel/vm.c`中
- 使用`kernel/riscv.h`末尾的宏
- `freewalk`函数一定能帮到你
- 在`kernel/defs.h`中定义`vmprint`的原型，以便可以从`exec.c`中调用它。
- 在`printf`调用中使用`%p`打印出完整的64位十六进制`pte`和地址，如示例所示。

> 根据文本中的图3-4解释vmprint的输出。第0页包含什么?第二页是什么?在用户模式运行时，进程能否读写第1页映射的内存?第三到最后一页包含什么?

```cpp
// My Code =============================
#define EXIST(pte)  ((pte & PTE_V))  
void helprint(int level, pte_t pte, int index) {
  printf("..");
  for (int i = 0; i < level; i++) {
    printf(" ..");
  }
  printf("%d: pte %p pa %p\n", index, pte, PTE2PA(pte));
}

void vmprint(pagetable_t page0) {
  printf("page table %p\n", page0);
  for (int i = 0; i <= PXMASK; i++) {
    if (!EXIST(page0[i])) continue;
    helprint(0, page0[i], i);
    pagetable_t page1 = (pagetable_t)PTE2PA(page0[i]);
    for (int j = 0; j <= PXMASK; j++) {
      if (!EXIST(page1[j])) continue;
      helprint(1, page1[j], j);
      pagetable_t page2 = (pagetable_t)PTE2PA(page1[j]);
      for (int q = 0; q <= PXMASK; q++) {
        if (!EXIST(page2[q])) continue;
        helprint(2, page2[q], q);
      }
    }
  }
}
```

## Detect which pages have been accessed ([hard](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

> 某些垃圾收集器(自动内存管理的一种形式)可以从哪些页被访问(读或写)的信息中获益。在本部分的实验中，你将向xv6添加一个新特性，通过检查RISC-V页表中的访问位来检测并向用户空间报告这些信息。RISC-V硬件的`page walker`在解决TLB缺失时，会在页表项中标记这些比特位。
>
> 你的任务是实现`pgaccess()`，这是一个报告哪些页面被访问的系统调用。该系统调用接受3个参数。首先，它需要检查第一个用户页的起始虚拟地址。其次，需要检查页数。最后，将用户地址存入缓冲区，将结果存储到bitmask(一种每页使用一个比特位的数据结构，第一页对应最低有效位)中。如果pgaccess测试用例在运行pgtbltest时通过，你将获得这部分实验的全部学分。

- 阅读`pgaccess_test()`（参见`user/pgtlbtest.c`）了解`pgaccess`如何使用
- 在`kernel/sysproc.c`中实现`sys_pgaccess()`
- 你需要解析参数(使用`argaddr()`和`argint()`)
- 为了输出bitmask， 在内核中存储一个临时缓冲区并将其复制给用户(通过`copyout()`)，在填充正确的位之后，这样做更容易。

- `kernel/vm.c`中的`walk()`对于查找正确的pte非常有用。
- 你需要在`kernel/riscv.h`定义一个`PTE_A`权限位, 请参考RISC-V特权体系结构手册以确定其值。
- 在检查PTE_A是否设置后，务必清除它。否则，将无法确定自上次调用`pgaccess()`以来是否访问过该页(即，该比特位将永远被设置)。
- `vmprint()`也许可以帮助你debug

![image-20230322130350801](/Users/cl/Library/Application Support/typora-user-images/image-20230322130350801.png)

这个联系还是比较简单的, 实现系统调用, 解析一下参数.

然后在内核中遍历用户空间需要检查的虚拟地址, 检查`PTE_A`标志并清空. 位图用一个数组实现.

美中不足的是这里用了一个`MAX_CHECK`表示检查的最大页, 这才能定义一个数组. (没办法, 内核还没实现动态分配空间的函数.)

```cpp
#ifdef LAB_PGTBL
#define MAX_CHECK 64

uint64 pgaccess(uint64 base, int len, uint64 mask) {
  struct proc *p = myproc();
  uint64 bitmap[MAX_CHECK / 64] = {0};  // 分配位图空间并初始化为0
  for (int i = 0; i < len; i++) {
    pte_t *pte = walk(p->pagetable, base, 0);
    if ((*pte) & PTE_A) {
      bitmap[i / 64] |= 1ULL << (i & 63);
      *pte = (*pte) ^ PTE_A;
    }
    base += PGSIZE;
  }
  copyout(p->pagetable, mask, (char *)&bitmap, sizeof(bitmap));
  return 0;
}

int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  int len;
  uint64 base, mask;
  argaddr(0, &base);
  argint(1, &len);
  if (len > MAX_CHECK) {
    return -1;
  }
  argaddr(2, &mask);
  // 检查addr开始的len页是否被访问过， 将结果放入mask
  return pgaccess(base, len, mask);
  return 0;
}
#endif
```


