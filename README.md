## Lab: Copy-on-Write Fork for xv6

​	虚拟内存提供了一种间接级别:内核可以通过标记页表项无效或只读来拦截内存引用，从而导致缺页异常，还可以通过修改页表项来改变地址的含义。在计算机系统中有这样一句话:任何系统问题都可以在一定程度上间接地解决。本实验探讨了一个例子:写时复制fork。

### The problem

​	xv6中的fork()系统调用将父进程的所有用户空间内存复制到子进程中。如果父节点很大，复制可能需要很长时间。更糟糕的是，这些工作通常都被浪费了:  fork()之后通常是exec()，它会丢弃复制的内存，通常不会使用大部分内存。另一方面，如果父进程和子进程都使用复制的页，并且其中一人或双方都写它，那么确实需要复制。

### The solution

​	实现写时复制(COW) fork() 的目的是推迟物理内存页的分配和复制，直到真正需要复制时才分配和复制。

​	COW fork() 仅为子进程创建一个分页表，其中用于用户内存的PTEs指向父进程的物理内存页。COW fork()将父节点和子节点中的所有user pte标记为只读。当任何一个进程试图写入其中一个COW页时，CPU将强制发生缺页异常。内核的页异常处理程序检测到这种情况，为发生异常的进程分配一页物理内存，将原始页复制到新页，并修改发生异常的进程中的相关PTE，使其引用新页，这一次将PTE标记为可写。当缺页异常处理程序返回时，用户进程将能够编写该页的副本。

COW fork()使得释放实现用户内存的物理内存页变得有点棘手。给定的物理内存页可以由多个进程的页表引用，只有在最后一次引用消失时才应该释放。在像xv6这样的简单内核中，记录的工作相当简单，但在用于生产的内核中，可能很难做好

## Implement copy-on-write fork([hard](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

> 你的任务是在xv6内核中实现写时复制(copy-on-write) fork。如果修改后的内核成功执行了cowtest和` usertests -q `程序，那么就完成了。
>
> 为了帮助你测试自己的实现，我们提供了一个xv6程序，名为cowtest(源代码在user/cowtest.c中)。Cowtest运行各种测试，但在未修改的xv6上，即使是第一个测试也会失败。因此，一开始，你会看到:
>
> ```shell
> $ cowtest
> simple: fork() failed
> $
> ```
>
> “simple”测试分配超过一半的可用物理内存，然后fork()。fork失败是因为没有足够的空闲物理内存来给子进程一个父进程内存的完整拷贝。
>
> 完成后，内核应该通过cowtest和usertests -q中的所有测试。那就是:
>
> ```shell
> $ cowtest
> simple: ok
> simple: ok
> three: zombie!
> ok
> three: zombie!
> ok
> three: zombie!
> ok
> file: ok
> ALL COW TESTS PASSED
> $ usertests -q
> ...
> ALL TESTS PASSED
> $
> ```
>
> 这里有一个合理的计划。
>
> 1、修改uvmcopy()，将父进程的物理内存页映射到子进程中，而不是分配新页。如果设置了PTE_W，则清除子页和父页的页表项中的PTE_W。
>
> 2、修改usertrap()函数，使其能够识别缺页异常。在原来可写的COW页发生写页异常时，使用kalloc()分配一个新页，将旧页复制到新页，并使用设置了PTE_W的PTE将新页安装到PTE中。原来只读的页(未映射PTE_W，如text段中的页)应该保持只读，并在父子之间共享。试图写入这样一页的进程应该被终止。
>
> 3、确保每个物理内存页在最后一个PTE引用消失时释放，但不是在它消失之前。一种好的方法是对每个物理内存页保存一个“引用计数”，其中包含引用该页的用户页表的数目。当kalloc()分配页面时，将其引用计数设置为1。在fork导致子进程共享该页时，将该页的引用计数加1;在任何进程从其页表删除该页时，将该页的引用计数减1。只有当Kfree()的引用计数为0时，它才会将页放回到未使用内存块的链表上。可以将这些计数保存在一个固定长度的整数数组中。你必须制定一个方案，如何索引数组以及如何选择其大小。例如，你可以用页的物理地址除以4096来索引数组，并将kalloc .c中kinit()放置在空闲链表上的任何页的最高物理地址赋值给数组。你可以随意修改kalloc.c。 (例如，kalloc()和kfree())来维护引用计数。
>
> 4、修改copyout()，使其在遇到COW页时使用与缺页异常相同的方案
>
> 还有一些提示
>
> - 有一种方法可以记录每个PTE是否为COW映射，这可能很有用。为此，可以使用RISC-V PTE中的RSW(专为软件而设)位。
> - `usertests -q`探索cowtest不测试的场景，因此不要忘记检查是否所有测试都通过了。
> - kernel/riscv.h的末尾有一些有用的宏和页表标志的定义。
> - 如果发生COW缺页异常，并且没有空闲内存，则应该终止进程。
>
> **几个注意点**
>
> 不管如何, 内核应该进来避免panic, 比较只要杀掉进程就好了.
>
> 还有, 在对物理页引用计数时需要使用锁, 测试有多线程的情况..........



**修改uvmcopy()**

这是fork系统调用使用的函数, 将父进程的物理页复制到子进程, 这里我们改成COW的形式, 指示简单的指向同一个物理地址, 但子父进程的PTE都标记为COW.

```cpp
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  for(i = 0; i < sz; i += PGSIZE) {
    if ((pte = walk(old, i , 0)) == 0) {
      panic("uvmcopy: pte should exist");
    }
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    if ((*pte) & PTE_W) {
      (*pte) |= PTE_CW;
      *pte &= (~PTE_W);
    }
    *pte = (*pte) | PTE_COW;
    flags = PTE_FLAGS(*pte);
    refpage(pa); // 子进程引用父进程页
    if (mappages(new, i, PGSIZE, pa, flags)) {
      kfree((void *)pa);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```

**修改usertrap()**

这是trap的逻辑, 在其中判断页错误异常处理.

当然只对cow页处理, 复制一份数据重新映射那个虚拟地址.

当然, `usertrap`函数添加的代码很简单

```cpp
else {
    // 页错误
    uint64 sca = r_scause();
    if ((sca == 0xf || sca == 0xd) && cow(p->pagetable, r_stval())) {
      ;
    } else {
      printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
      printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
      setkilled(p);
    }
  }
```

主要是`cow`函数, 它复制用到的页面, 被我是现在`vm.c`中

```cpp
// 若va是一个cow页, 返回1，否则返回0
// 将va映射到新的页, 内容为pa. 当然是
int cow(pagetable_t pagetable, uint64 va) {
  va = PGROUNDDOWN(va);
  if(va >= MAXVA) {
    return 0;
  }
  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0) {
    return 0;
  }
  uint64 flags = PTE_FLAGS(*pte);
  // 这是一个可写的COW页
  if ((*pte) & PTE_COW) {
    uint64 pa = PTE2PA(*pte);
    flags ^= PTE_COW;
    if ((*pte) & PTE_CW) {
      flags ^= PTE_CW;
      flags |= PTE_W;
    }
    // 为r_stval()的地址 重新映射一个物理地址, 并修改它的权限
    uint64 cp_pa = (uint64)kalloc();
    if (!cp_pa) {
      // memory not enough.....
      return 0;
    }
    memmove((void *)cp_pa, (void *)pa, PGSIZE);
    uvmunmap(pagetable, va, 1, 1);
    mappages(pagetable, va, PGSIZE, cp_pa, flags);
    return 1;
  }
  return 0;
}
```

**修改copyout**

也就是内核将数据复制到用户空间的函数.

其实很简单, 在这里只需要简单的在复制数据前, 将用户page重新映射一下就好了(如果是cow page)

这里有个比较坑的点, 由于我们捕获了所有页异常, 所以当va >= MAXVA时也会进来.

但是此时调用walk会panic,  这是我们不希望的, 我们只需要杀掉该进程就好了, 这里手动判断掉就好了.

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
    // 如果是cow页, 先拷贝一份
    if (cow(pagetable, va0)) {
      ;
    }
    pa0 = walkaddr(pagetable, va0);
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}
```

**记录页的引用次数**

由于COW页的存在, 释放页需要记录他的被引用次数。

```cpp
int page_count[PHYSTOP/PGSIZE + 1];
#define INDEX(pa) ((uint64)pa/PGSIZE)

void refpage(uint64 pa) {
  acquire(&kmem.lock);
  page_count[INDEX(pa)]++;
  release(&kmem.lock);
}

void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  acquire(&kmem.lock);
  if ((--page_count[INDEX(pa)]) > 0) {
    release(&kmem.lock);
    return;
  }
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  
  page_count[INDEX(r)] = 1;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
```

