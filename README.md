# Lab 4: traps

本实验探讨如何使用traps实现系统调用。你首先要做一个栈的热身练习，然后实现一个用户级trap处理的例子。

> 在开始之前, 阅读xv6 book的第四章, 以及以下文件
>
> - `kernel/trampoline.S`: 从用户空间切换到内核空间所涉及的程序集
> - `kernel/trap.c`: 处理所有中断的代码

## Preview

> ## Traps(前置知识)
>
> 
>
> ###  一、从C程序到汇编程序
>
> ​	处理器并不能理解C语言，处理器能够理解的是[汇编语言。或者更具体的说，处理器能够理解的是二进制编码后的汇编代码。
>
> **RISC-V处理器**
>
> 处理器的厂家非常多, 本课程主要讨论RISC-V处理器, 
>
> **ISA**
>
> 任何一个处理器都有一个与之关联的ISA(指令集). 每条指令都有一个对应的二进制编码或一个操作码Opcode. 当处理器运行时, 如果看到对应的编码，处理器就做出对应的操作.
>
> 比如, RISC-V处理器能理解RISC-V汇编语言.
>
> **.o文件**
>
> C文件编译为汇编, 之后汇编语言被翻译成二进制文件, 也就是.o文件. 这是处理器能理解的目标文件.
>
> ### 二、RISC-V vs x86
>
> > 汇编语言有很多种(因为不同的处理器指令集不一样，而汇编语言中都是一条条指令，所以不同处理器对应的汇编语言必然不一样)。如果你使用RISC-V处理器，你不太能将Linux运行在上面。相应的，大多数现代计算机都运行在x86和x86-64处理器上，通常你们的个人电脑上运行的处理器是x86，Intel和AMD的CPU都实现了x86。x86拥有一套不同的指令集，看起来与RISC-V非常相似。
> > **RISC-V vs x86**
> > RISC-V中的RISC是精简指令集(Reduced Instruction Set Computer)。
> > x86通常被称为CISC，复杂指令集(Complex Instruction Set Computer)。
> > **区别**
> > ①指令的数量
> > 	创造RISC-V的一个非常大的初衷就是因为Intel手册中指令数量太多了。
> > 	x86-64指令介绍由3个文档组成，并且新的指令以每个月3条的速度在增加。因为x86-64是在1970年代发布的，所以现在有多于15000条指令。
> > 	RISC-V指令介绍由两个文档组成。这两个文档包含了RISC-V的指令集的所有信息，分别是240页和135页，相比x86的指令集文档要小得多的多。这是有关RISC-V比较好的一个方面。所以在RISC-V中，我们有更少的指令数量。
> > ②指令复杂度
> > 	在x86-64中，很多指令都做了不止一件事情，它们会执行一系列复杂的操作并返回结果。RISC-V的指令趋向于完成更简单的工作，相应的也消耗更少的CPU执行时间。
> > 	这其实是设计人员的在底层设计时的权衡，并没有一些非常确定的原因说RISC比CISC更好，它们各自有各自的使用场景。
> > ③开源程度
> > 	相比x86来说，RISC是市场上唯一的一款开源指令集，这意味着任何人都可以为RISC-V开发主板。RISC-V来自于UC-Berkly的一个研究项目，之后被大量的公司选中并做了支持。
> > ④应用广泛度
> > 	在你们的日常生活中，你们可能已经在完全不知情的情况下使用了精简指令集。比如说ARM也是一个精简指令集，高通的Snapdragon(骁龙)处理器就是基于ARM。如果你使用一个Android手机，那么大概率你的手机运行在精简指令集上。如果你使用IOS，苹果公司也实现某种版本的ARM处理器，这些处理器运行在iPad、iPhone和大多数苹果移动设备上(甚至包括Mac)。苹果公司也在尝试向ARM做迁移(如刚刚发布的Macbook)。所以精简指令集出现在各种各样的地方。如果你想在现实世界中找到RISC-V处理器，除了QEMU，你可以在一些嵌入式设备中找到。所以RISC-V也是有应用的，当然它可能没有x86那么广泛。
> > ⑤向后兼容性
> > 在最近几年，由于Intel的指令集是在是太大了，精简指令集的使用越来越多。Intel的指令集之所以这么大，是因为Intel对于向后兼容非常看重。Intel并没有下线任何指令，所以一个现代的Intel处理器还可以运行三四十年前的指令。而RISC-V提出的更晚，所以不存在历史包袱的问题。
> > ⑥指令分类
> > 如果查看RISC-V的文档，可以发现RISC-V的特殊之处在于：它区分了Base Integer Instruction Set 和Standard Extension Instruction Set。
> >
> > - Base Integer Instruction Set包含了所有的常用指令，比如add、mult等。
> > - 除此之外，处理器还可以选择性的支持Standard Extension Instruction Set。例如，一个处理器可以选择支持单精度浮点型支持，就可以包含相应的扩展指令集。这种模式使得RISC-V更容易支持向后兼容，每一个RISC-V处理器可以声明支持了哪些扩展指令集，然后编译器可以根据支持的指令集来编译代码。
>
> ### 四、RISC-V寄存器
>
> <img src="https://img-blog.csdnimg.cn/54d0b5e803c242eaac908eb6ca485b1c.png" width = 60%>
>
> ①	汇编代码并不是在内存上执行，而是在寄存器上执行。也就是说，当我们在做add、sub等操作时，是对寄存器进行操作。所以你们通常看到的汇编代码中使用寄存器的模式是：
>
> - 将数据加载进寄w存器中(这里的数据可以来自内存，也可以来自另一个寄存器)
> - 在寄存器上执行一些操作。
> - 如果对操作的结果关心的话，将操作的结果存储在某个地方。可能是内存中的某个地址，也可能是另一个寄存器。
>
> ②寄存器是用来进行任何运算和数据读取的最快的方式。因此，我们更喜欢使用寄存器而不是内存。
>
> **ABI**
>
> 通常我们谈到寄存器的时候, 我们会用ABI名词代替它们. 因为在写汇编代码时也是使用ABI名字.
>
> 实际上上述图表中第一列的名字并不重要, 它唯一重要的场景是在RISC-V指令的压缩版本
>
> **压缩指令**
>
> RISC-V中通常的指令是64bit，但是在压缩指令是16bit。在压缩指令中我们使用更少的寄存器，也就是`x8-x15`寄存器。我猜你们可能会有疑问，为什么s1(x9)寄存器和其他的s寄存器是分开的。这是因为s1对压缩指令是有效的，而s2-s11却是无效的，所以我们认为s1是个压缩指令寄存器。除了压缩指令，寄存器都是通过它们的ABI名字来引用。
>
> **函数参数寄存器**
>
> `a0`到`a7`寄存器是用来作为函数的参数。如果一个函数有超过8个参数，我们就需要用内存了。从这里也可以看出，当可以使用寄存器的时候，我们不会使用内存，我们只在不得不使用内存的场景才使用它。
>
> **Saver**
>
> Saver有两个可能的值：Caller调用者，Callee被调用者。
>
> - Caller Saved寄存器在函数调用的时候不会保存。
> - Callee Saved寄存器在函数调用的时候会保存。
>
> 这里的意思是：一个Caller Saved寄存器可能被其他函数重写。假设我们在函数a中调用函数b，任何被函数a使用的并且是Caller Saved寄存器，调用函数b可能重写这些寄存器。例如Return address寄存器(保存的是函数返回的地址)是Caller Saved，因为每个函数都需要使用返回地址 ，因此它导致了当函数a调用函数b的时侯，b会重写Return address。
>
> 总结：对于任何一个Caller Saved寄存器，作为调用方的函数要小心数据的变化。对于任何一个Callee Saved寄存器，作为被调用方的函数要小心寄存器的值不会相应的变化。
>
> **数据改造**
> 所有的寄存器都是64bit，各种各样的数据类型都会被改造使得可以放进这64bit中。比如说我们有一个32bit的整数，取决于整数是不是有符号的，会通过在前面补32个0或者1来使得这个整数变成64bit并存在这些寄存器中。
>
> 
>
> ### 五、栈
>
> > 栈之所以很重要的原因是，它使得我们的函数变得有组织，且能够正常返回。
> >
> > 试想一下, 当你执行一个函数时, 需要将pc执行函数代码. 当函数执行时, 需要将pc返回到之前的代码. 而且函数之间还能嵌套递归, 所以必然有一个位置保存着每个函数的返回地址等信息.
> >
> > 下面是一个非常简单的栈的布局图, 每个区域都是一个栈帧，每执行一次函数调用就会产生一个stack frame.
> >
> > <img src="https://img-blog.csdnimg.cn/f7efb4edc5e6497c8f8eb94ef351cb21.png" width=70%>
> >
> > **栈的工作流程**
> >
> > ​	每一次我们调用一个函数，函数都会为自己创建一个Stack Frame，并且只给自己用。函数通过移动Stack Pointer(sp)来完成Stack Frame的空间分配。对于Stack来说，是从高地址开始向低地址使用，所以栈总是向下扩展。当我们想要创建一个新的Stack Frame的时候，总是对当前的Stack Pointer做减法。
> > ​	一个函数的Stack Frame包含了保存的寄存器、局部变量。由于如果函数的参数多于8个，额外的参数会出现在Stack中。此外，不同的函数有不同数量的局部变量、不同的寄存器等。所以Stack Frame大小并不总是一样，即使在这个图里面看起来是一样大的。
> >
> > **Stack Frame跳转/回溯逻辑**
> >
> > ①Return address总是会出现在Stack Frame的第一位。
> >
> > ②指向前一个Stack Frame的指针也会出现在栈中的固定位置。有关Stack Frame中有两个重要的寄存器，第一个是SP(Stack Pointer)，它指向Stack的底部并代表了当前Stack Frame的位置。第二个是FP(Frame Pointer)，它指向当前Stack Frame的顶部。因为Return address和指向前一个Stack Frame的的指针都在当前Stack Frame的固定位置，所以可以通过当前的帧指针fp寻址到这两个数据。我们保存前一个Stack Frame的指针的原因是为了让我们能跳转回去。所以当前函数返回时，我们可以将前一个栈帧的Frame Pointer存储到FP寄存器中。所以我们使用Frame Pointer来操纵我们的Stack Frames，并确保我们总是指向正确的函数。
> >
> > **汇编函数**
> >
> > Stack Frame必须要被汇编代码创建，所以是编译器遵循调用约定生成了汇编代码，进而创建了Stack Frame。
> >
> > 所以通常，在汇编代码中，函数的最开始你们可以看到函数序言(Function prologue)，之后是函数体，最后是函数尾声(Epollgue)。这就是一个汇编函数通常的样子。
>
> 
>
> ### 六、结构体
>
> > **struct在内存中是一段连续的内存区域**。假设现在如果我们有一个struct，并且有f1，f2，f3三个字段。
> >
> > 当我们创建这样一个struct时，内存中相应的字段会彼此相邻。你可以认为struct像是一个数组，但是里面的不同字段的类型可以不一样。这里有一个名字是Person的struct，它有两个字段。我将这个struct作为参数传递给printPerson并打印相关的信息。
> >
> > ```cpp
> > struct Person {
> > int id;
> > int age;
> > };
> > void printPerson() {
> > printf("Person %d (%d)\n", p->id, p->page);
> > }
> > ```
> >
> > 
>
> >**问题**
> >
> >是谁创建了编译器来将C代码转换成各种各样的汇编代码，是不同的指令集创建者，还是第三方？
> >
> >**回答**
> >
> >我认为不是指令集的创建者，通常是第三方创建的。你们常见的两大编译器，一个是gcc，这是由GNU基金会维护的。一个是Clang llvm，这个是开源的，你可以查到相应的代码。当一个新的指令集(例如RISC-V)发布之后，我认为会指令集的创建者和编译器的设计者之间会有一些高度合作。简单来说我认为是第三方配合指令集的创建者完成的编译器。RISC-V或许是个例外，因为它是来自于一个研究项目，相应的团队或许自己写了编译器，但是我不认为Intel对于gcc或者llvm有任何投入。
>
> 下面是一些常用的RISC-V指令
>
> ```asm
> ld a, b # 将b加载到a
> sd a, b # 将a存储到b
> li a, b # 将立即数b加载到a
> 
> 
> 
> 
> auipc x5, 0x12345	# x5 = PC + (0x12345 << 12)
> auipc x6, 0		# x6 = PC, to obtain the current PC
> 
> 
> jalr 1562(ra)
> # jalr 指令设置pc到值. 使用 12 位立即数(有符号数)作为偏移量,与操作数寄存器 ra中的值相加,然后将结果的最低有效位置0。jalr指令将其下一条指令的 PC(即当前指令PC+4)的值写入ra。
> ```
>
> 
>
> ----
>
> 
>
> ### 一、trap机制
>
> **trap定义**
>
> > 掌握程序运行时如何完成用户空间和内核空间的切换是非常必要的。以下三种情况会发生这样的切换.这里用户空间和内核空间的切换通常被称为trap。
> >
> > ①syscall(系统调用)
> >
> > 当用户程序执行ecall指令以要求内核为其做一些事情时。
> >
> > ②exception(异常)
> >
> > 一条用户或内核指令做了一些非法的事情时，例如除以0或使用无效的虚拟地址。
> >
> > ③interrupt(设备中断)
> >
> > 当设备发出需要注意的信号时，例如时钟芯片产生的中断, 磁盘硬件完成读取或写入请求时中断。
> >
> > ---
> >
> > trap涉及了许多精心的设计和重要的细节，这些细节对于实现安全隔离和性能来说非常重要。很多应用程序因为系统调用、page fault等原因会频繁地切换到内核中，所以trap机制还需要要尽可能的简单。
>
> **工作流程**
>
> > 作为xv6启动的第一个用户程序shell运行在用户空间, 它需要执行一些系统调用, 比如write.
> >
> > **寄存器状态**
> >
> > 有一些寄存器有着很特殊的作用, 这些寄存器表明了执行系统调用时计算机的状态。
> >
> > ①栈指针(Stack Pointer)，也叫做栈寄存器，保存栈帧的地址。
> > ②程序计数器(Program Counter Register)
> > ③模式(Mode)，表明当前模式的标志位。这个标志位表明了当前是supervisor mode还是user mode。当我们在运行Shell的时候，自然是在user mode。
> > 还有一堆控制CPU工作方式的寄存器：
> > ④SATP(Supervisor Address Translation and Protection)寄存器，它包含了指向页表的物理内存地址。
> > 还有一些对于今天讨论非常重要的寄存器：
> > ⑤STVEC(Supervisor Trap Vector Base Address Register)寄存器，它指向了内核中处理trap的指令的起始地址。
> > ⑥SEPC(Supervisor Exception Program Counter)寄存器，在trap的过程中保存程序计数器的值。
> > ⑦SSCRATCH(Supervisor Scratch Register)寄存器。
> >
> > **寄存器状态修改**
> >
> > 可以肯定的是，在trap的最开始，CPU的所有状态都设置成运行用户代码而不是内核代码。在trap处理的过程中，我们需要更改一些这里的状态，或者对状态做一些操作，这样我们才可以运行系统内核中普通的C程序。接下来我们预览一下需要做的操作：
> >
> > ①首先，我们需要保存32个用户寄存器。因为很显然我们需要恢复用户应用程序的执行，尤其是当用户程序随机地被设备中断所打断时，我们希望内核能够响应中断，之后在用户程序完全无感知的情况下再恢复用户代码的执行。所以这意味着32个用户寄存器不能被内核弄乱。但是这些寄存器又要被内核代码所使用，所以在trap之前，你必须先在某处保存这32个用户寄存器。
> >
> > ②程序计数器也需要在某个地方保存，它几乎跟一个用户寄存器的地位是一样的，我们需要能够在用户程序运行中断的位置继续执行用户程序。
> >
> > ③需要将mode改成supervisor mode，因为我们想要使用内核中的各种各样的特权指令。
> >
> > ④satp寄存器现在正指向用户页表，而用户页表只包含了用户程序所需要的内存映射和一两个其他的映射，它并没有包含整个内核数据的内存映射。所以在运行内核代码之前，我们需要将satp指向内核页表。
> >
> > ⑤需要将栈寄存器指向位于内核的一个地址，因为我们需要一个栈来调用内核的C函数。
> >
> > 一旦我们设置好了，并且所有的硬件状态都适合在内核中使用， 我们需要跳入内核的C代码。一旦我们运行在内核的C代码中，那就跟平常的C代码是一样的。
>
> **trap机制的一些注意点**
>
> > 之后我们会讨论内核通过C代码做了什么工作，但是今天讨论的是如何将将程序执行从用户空间切换到内核的一个位置，这样我们才能运行内核的C代码。
> >
> > ①操作系统的一些high-level的目标限制了这里的实现，其中一个目标是安全和隔离。我们不想让用户代码干扰到这里的user/kernel切换，否则有可能会破坏安全性。所以这意味着，trap中涉及到的硬件和内核机制不能依赖任何来自用户空间东西。比如说我们不能依赖32个用户寄存器，它们可能保存的是恶意的数据。所以，XV6的trap机制不会查看这些寄存器，而只是将它们保存起来。
> >
> > ②在操作系统的trap机制中，我们想保留隔离性并防御来自用户代码的可能的恶意攻击。但另一方面，我们想要让trap机制对用户代码是透明的。也就是说，我们想要执行trap，然后在内核中执行代码，之后再恢复代码到用户空间。这个过程中，用户代码并不会注意到发生了什么，这样也就更容易编写用户代码。
> >
> > ③需要注意的是，虽然我们这里关心隔离和安全，但是今天我们只会讨论从用户空间切换到内核空间相关的安全问题。当然，系统调用的具体实现(如write在内核的具体实现)以及内核中任何的代码也必须小心并安全地写好。因此，即使从用户空间到内核空间的切换十分安全，整个内核的其他部分也必须非常安全，并时刻小心用户代码可能会尝试欺骗它。
> >
> > ④在前面介绍的寄存器中，保存mode标志的寄存器需要讨论一下。当我们在用户空间时，这个标志位对应的是user mode，当我们在内核空间时，这个标志位对应supervisor mode。但是有一点很重要：当这个标志位从user mode变更到supervisor mode时，我们能得到什么样的权限。实际上，这里获得的额外权限实在是有限。也就是说，并不是像你可以在supervisor mode完成但是不能在user mode完成一些工作那么有特权。我们接下来看看supervisor mode可以控制什么。
> >
> > - 可以读写控制寄存器。比如说，当你在supervisor mode时，你可以：读写SATP寄存器，也就是页表的指针；STVEC，也就是处理trap的内核指令地址；SEPC，保存当发生trap时的程序计数器；SSCRATCH等等。在supervisor mode你可以读写这些寄存器，而用户代码不能做这样的操作。
> >
> > - 可以使用PTE_U标志位为0的PTE。当PTE_U标志位为1时，表明用户代码可以使用这个页表。如果这个标志位为0，则只有supervisor mode可以使用这个页表。
> >
> >   这两点就是supervisor mode可以做的事情，除此之外就不能再干别的事情了。
>
> 
>
> ### 二、trap代码的执行流程
>
> ​	让我们跟踪shell中调用write系统调用的过程. 从shell的角度来说, 这只是一个shell代码中的c函数而已. 但实际上, write通过执行`ecall`指令来执行系统调用, `ecall`指令会切换到具有supervisor mode的内核中。
>
> ​	在这个过程中，内核中执行的第一个指令是一个由汇编语言写的函数，叫做`uservec`。这个函数是内核代码**trampoline.s**文件的一部分。
>
> ​	之后，在这个汇编函数中，代码执行跳转到了由C语言实现的函数`usertrap`中，这个函数在**trap.c**中。
>
> ​	现在代码运行在C中，所以代码更加容易理解。在`usertrap`这个C函数中，我们执行了一个叫做`syscall`的函数。
>
> ​	这个函数会在一个表单中，根据传入的代表系统调用的数字进行查找，并在内核中执行具体实现了系统调用功能的函数。对于我们来说，这个函数就是`sys_write`。
>
> ​	`	sys_write`会将要显示数据输出到console上。当它完成了之后，它会返回到`syscall`函数，然后`syscall`函数返回到用户空间。
>
> ​	因为我们现在相当于在`ecall`之后中断了用户代码的执行，为了用户空间的代码恢复执行，需要做一系列的事情。在`syscall`函数中，会调用一个函数叫做`usertrapret`，它也位于**trap.c**中，这个函数完成了部分在C代码中实现的返回到用户空间的工作。
>
> ​	除此之外，最终还有一些工作只能在汇编语言中完成。这部分工作通过汇编语言实现，并且存在于**trampoline.s**文件中的`userret`函数中。
>
> ​	最终，在这个汇编函数中会调用机器指令返回到用户空间，并且恢复`ecall`之后的用户程序的执行。
>
> <img src="https://img-blog.csdnimg.cn/76f6a4ae4a62493fabb75d0db8d277b9.png" width=60%>
>
> 
>
> ### 三、ecall指令之前的状态
>
> 当执行系统调用`write`时, 实际上调用的是关联到Shell到一个库函数(参见`usys.s`)
>
> ```asm
> write:
> 	li a7, SYS_write
> 	ecall
> 	ret
> ```
>
> 上面这几行代码就是实际被调用的write函数的实现，这是个非常短的函数。
>
> ①它首先将SYS_write加载到a7寄存器，SYS_write是常量16。这里告诉内核，我想要运行第16个系统调用，而这个系统调用正好是write。
>
> ②之后这个函数中执行了ecall指令，从这里开始代码执行跳转到了内核。
>
> ③内核完成它的工作之后，代码执行会返回到用户空间，继续执行ecall之后的指令，也就是ret，最终返回到Shell中，所以ret从write库函数返回到了Shell中。
>
> 
>
> ### 四、ecall指令之后的状态
>
> 在执行ecall后, pc的值改变为一个大得多的地址, 而页表仍然不变.
>
> 那么根据页表, pc的值指向trampoline page的开始位置. 接下来的几条指令也是在trap机制中最开始执行的几条指令. 此时寄存器的值还没有变化.
>
> ecall不会切换页表, 这是ecall指令很重要的一个特点. 这意味着trap处理代码需要存在于每个用户页表中, 我们需要在用户页表的某个位置来执行最初的内核代码.
>
> **stvec寄存器状态**
>
> 这里的控制是通过`stvec`寄存器完成的，这是一个只能在supervisor mode下读写的特权寄存器。在从内核空间进入到用户空间之前，内核会设置好`stvec`寄存器指向内核希望trap代码运行的位置。
>
> 因此, stvec寄存器的内容就是ecall指令执行后, 我们会在这个特定地址执行指令的原因.
>
> **进入trampoline page**
>
> 即使trampoline page是在用户地址空间的用户页表完成的映射，用户代码也不能写它, 因为它没有设置PTE_U标志位. 这就是为什么trap是安全的.
>
> 而此时我们进入了trampoline page执行命令, 说明我们已经处于supervisor mode了.
>
> **ecall干了什么**
>
> 相信你已经可以猜到ecall做了什么. 它的工作只有两点
>
> - 将代码从user mode修改为supervisor mode
> - 将pc的值保存在spec寄存器, 将pc的值修改为stvec寄存器的值
>
> **距离执行内核的C代码还需要什么工作?**
>
> 1、我们需要保存32个用户寄存器. 这样稍后从内核返回时, 才能恢复状态.
>
> 2、需要切换到内核的页表
>
> 3、需要创建或找到一个内核栈, 并将Stack Pointer寄存器的内容指向那个内核栈, 以供C代码使用
>
> 完成这些工作并不简单, 我们需要编写复杂的代码. 为什么ecall不完成这些工作？ 因为硬件的设计者希望提供最大的灵活性, 所以只做最少的事, 把选择的权利交给软件.
>
> 举个例子, 某些操作系统可以在不切换页表的前提下执行部分系统调用, 那ecall封装了切换页表的过程就显得很不合理了.
>
> 
>
> ### 五、uservec函数
>
> #### 保存寄存器
>
> > ​	回到XV6和RISC-V，现在程序位于trampoline page的起始处，也是uservec函数的起始处。在RISC-V上如果不能使用寄存器基本上不能做任何事情，所以我们现在需要做的第一件事情就是保存寄存器的内容。对于保存这些寄存器，我们有什么样的选择呢？
> >
> > ​	在一些其他的机器中，我们或许直接就将32个寄存器中的内容写到物理内存中某些合适的位置。但是我们不能在RISC-V中这样做，因为在RISC-V中，supervisor mode下的代码不允许直接访问物理内存。所以我们只能使用页表中的内容，但是从前面的输出来看，页表中也没有多少内容。
> >
> > ​	虽然XV6并没有使用，但是另一种可能的操作是直接将satp寄存器指向内核页表，之后我们就可以直接使用所有的内核映射来帮助我们存储用户寄存器。这是合法的，因为supervisor mode可以更改satp寄存器。但是在trap代码当前的位置，也就是trap机制的最开始，我们并不知道内核页表的地址。并且更改satp寄存器的指令，要求写入satp寄存器的内容来自于另一个寄存器。所以，为了能执行更新页表的指令，我们需要一些空闲的寄存器，这样我们才能先将页表的地址存在这些寄存器中，然后再执行修改satp寄存器的指令。
> >
> > **所以最终对于保存用户寄存器的，XV6在RISC-V上的实现包括了两个部分：**
> >
> > ① 第一个部分
> >
> > ​	XV6在每个用户页表映射了trapframe page，这样每个进程都有自己的trapframe page。这个page包含了很多有趣的不同类型的数据，但是现在最重要的数据是用来保存用户寄存器的32个空槽位。所以，在trap处理代码中，现在的好消息是我们在用户页表中有一个之前由内核设置好的映射关系，这个映射关系指向了一个可以用来存放这个进程的用户寄存器的内存位置。这个位置的虚拟地址总是0x3ffffffe000。
> > ​	如果你想查看XV6在trapframe page中存放了什么，这部分代码在proc.h中的trapframe结构体中。比如, 第一个数据保存了内核页表的地址, 这将是trap处理代码将要加载到satp寄存器的数值. 所以对于如何保存用户寄存器的一半答案是: 内核将一个trapframe page映射到了用户页表.
> >
> > ②另一部分
> > 	RISC-V提供的sscratch寄存器，就是为接下来的目的而创建的。在进入到用户空间之前，内核会将trapframe page的地址保存在这个寄存器中，也就是0x3fffffe000这个地址。更重要的是，RISC-V有一个指令允许交换任意两个寄存器的值。而sscratch寄存器的作用就是保存另一个寄存器的值，并将自己的值加载给另一个寄存器。如果我查看trampoline.S代码。
> >
> > 第一条指令就是
> >
> > ```asm
> > csrrw a0,sscratch, a0
> > ```
> >
> > 这个指令交换了a0和sscratch两个寄存器的内容. 这样a0将保存trapframe page的虚拟地址. 接下来30多个奇怪指令执行`sd`，将每个寄存器保存在`trapframe`中的不同偏移位置。
> >
> > 所以这就是第二个答案. sscratch可以临时保存寄存器, 方便我们进行操作.
>
> **问答**
>
> > 寄存器保存在了trapframe page，但是这些寄存器用户程序也能访问，为什么我们要使用内存中一个新的区域(指的是trapframe page)，而不是使用程序的栈？
> >
> > **solution**
> >
> > ​	这里或许有两个问题。
> >
> > ​	第一个是为什么我们要保存寄存器？为什么内核要保存寄存器的原因，是因为内核即将要运行会覆盖这些寄存器的C代码。如果我们想正确的恢复用户程序，我们需要将这些寄存器恢复成它们在ecall调用之前的数值，所以我们需要将所有的寄存器都保存在trapframe中，这样才能在之后恢复寄存器的值。
> > ​	另一个问题是，为什么这些寄存器保存在trapframe，而不是用户代码的栈中？这个问题的答案是，我们不确定用户程序是否有栈，必然有一些编程语言没有栈，对于这些编程语言的程序，Stack Pointer不指向任何地址。当然，也有一些编程语言有栈，但是或许它的格式很奇怪，内核并不能理解。比如，编程语言从堆中以小块来分配栈，编程语言的运行时知道如何使用这些小块的内存来作为栈，但是内核并不知道。所以，如果我们想要运行任意编程语言实现的用户程序，内核就不能假设用户内存的哪部分可以访问，哪部分有效，哪部分存在。所以内核需要自己管理这些寄存器的保存，这就是为什么内核将这些内容保存在属于内核内存的trapframe中，而不是用户内存。
>
> #### 切换页表
>
> > 程序现在仍然在trampoline的最开始，也就是`uservec`函数的最开始，我们基本上还没有执行任何内容。在保存玩所有寄存器后, 执行以下几条指令
> >
> > ```asm
> > # initialize kernel stack pointer, from p->trapframe->kernel_sp
> > ld sp, 8(a0)
> > 
> > # make tp hold the current hartid, from p->trapframe->kernel_hartid
> > ld tp, 32(a0)
> > 
> > # load the address of usertrap(), from p->trapframe->kernel_trap
> > ld t0, 16(a0
> > 
> > # fetch the kernel page table address, from p->trapframe->kernel_satp.
> > ld t1, 0(a0)
> > 
> > # install the kernel page table.
> > csrw satp, t1
> > ```
> >
> > ​	第一条指令正在将a0指向的内存地址往后数的第8个字节开始的数据加载到Stack Pointer寄存器。a0的内容现在是trapframe page的地址，从trapframe的格式可以看出，第8个字节开始的数据是内核的Stack Pointer(kernel_sp)。trapframe中的kernel_sp是由kernel在进入用户空间之前就设置好的，它的值是这个进程的kernel stack。所以这条指令的作用是初始化Stack Pointer指向这个进程的kernel stack的最顶端
> >
> > ​	下一条指令是向tp寄存器写入数据。因为在RISC-V中，没有一个直接的方法来确认当前运行在多核处理器的哪个核上，XV6会将CPU核的编号也就是hartid保存在tp寄存器。在内核中好几个地方都会使用了这个值，例如内核可以通过这个值确定某个CPU核上运行了哪些进程。
> >
> > ​	下一条指令是向`t0`寄存器写入数据。这里写入的是我们将要执行的第一个C函数的指针，也就是函数`usertrap`的指针。我们在后面会使用这个指针。
> >
> > ​	下一条指令是向`t1`寄存器写入数据。这里写入的是内核页表的地址. 实际上严格来说，`t1`的内容并不是内核页表的地址，这是你需要向`satp`寄存器写入的数据。它包含了内核页表的地址，但是移位了，并且包含了各种标志位。
> >
> > ​	下一条指令是交换`satp`和`t1`寄存器。这条指令执行完成之后，当前程序会从用户页表切换到内核页表。
>
> **上面有一个值得思考的问题. 当我们从用户页表修改为内核页表时,  pc存储的是虚拟地址. 为什么这个虚拟地址也能被内核页表使用?**
>
> 因为内核页表也映射了trampoline页且地址一样.
>
> 所以接下来的指令尽管还在`trampoline page`中,  还是能正常执行
>
> ```asm
> jr t0
> ```
>
> 这条指令跳转到内核的C代码中(usertrap函数).
>
> 
>
> ### 六、usertrap函数
>
> ​	终于, 从ecall修改user mode为supervisor mode, 跳转到trampoline page执行代码.
>
> ​	然后将所有用户寄存器保存在trapframe page中. 
>
> ​	恢复一些寄存器, 比如satp(切换页表), 调用内核的usertrap函数.
>
> > `usertrap`函数是位于**trap.c**文件的一个函数。
> >
> > 我们正在运行C代码，应该会更容易理解。我们仍然会读写一些有趣的控制寄存器，但是环境比起汇编语言来说会少了很多晦涩。现在我们在usertrap，有很多原因都可以让程序运行进入到usertrap函数中来，比如系统调用，运算时除以0，使用了一个未被映射的虚拟地址，或者是设备中断。usertrap某种程度上存储并恢复硬件状态，但是它也需要检查触发trap的原因，以确定相应的处理方式，我们在接下来执行usertrap的过程中会同时看到这两个行为。
> >
> > **具体流程**
> >
> > 1、更改stvec寄存器. 
> >
> > ​	对于trap是来自用户空间还是内核空间, xv6的处理方式是不一样的. 目前为止, 我们只讨论过trap由用户空间发器. 如果trap从内核空间发起是一个不同的处理流程. 因为从内核发起的话, 程序已经在使用内核页表, 许多处理都不必要. 
> >
> > ​	`usertrap`首先将stvec指向了`kernelvec`变量. 这是内核空间处理trap的处理代码, 类似于用户空间下的trampoline page. 因为在trap时是跳转到stvec的地址, 所以这里首先设置它, 而不是仍然保留用户空间的处理入口.
> >
> > 2、保存用户程序计数器
> >
> > ```cpp
> > struct proc *p = myproc();
> > 
> > // save user program counter.
> > p->trapframe->epc = r_sepc();
> > ```
> >
> > ​	首先, 我们通过myproc()找到当前运行的是什么进程.接下来需要保存用户程序计数器.
> >
> > 它仍然保存在spec寄存器， 但是可能当程序还在内核执行时切换到另一个进程, 进入到那个程序的用户空间, 导致sepc寄存器内容被覆盖.
> >
> > 3、确定trap原因
> >
> > ​	RISC-V的`scause`寄存器会有不同的数字。数字8表明，我们现在在`trap`代码中是因为系统调用。
> >
> > ​	在RISC-V中，存储在sepc寄存器中的程序计数器，是用户程序中触发trap的指令的地址。但是当我们恢复用户程序时，我们希望在下一条指令恢复，也就是ecall之后的一条指令。所以对于系统调用，我们对于保存的用户程序计数器加4，这样我们会在ecall的下一条指令恢复，而不是重新执行ecall指令。
> >
> > ```cpp
> > if(r_scause() == 8){
> >  // system call
> > 
> >  if(killed(p))
> >    exit(-1);
> > 
> >  // sepc points to the ecall instruction,
> >  // but we want to return to the next instruction.
> >  p->trapframe->epc += 4;
> > 
> >  // an interrupt will change sepc, scause, and sstatus,
> >  // so enable only now that we're done with those registers.
> >  intr_on();
> > 
> >  syscall();
> > }
> > ```
> >
> > 在进入syscall()之前, 会开中断, 不至于让其他用户程序等待太久. 中断总是会被RISC-V的trap硬件关闭，所以在这个时间点，我们需要显式的打开中断。
> >
> > 4、进入syscall
> >
> > ​	进入syscall后, 需要确定具体要执行哪个系统调用. 答案在trapframe中a7的数字. 这个值我们已经保存过了, 这样就能通过索引执行sys_write函数. 再往后的代码就很复杂了, 不过到这里为止, 也基本完成了进入和跳出内核的过程.
> >
> > ​	系统调用需要找到它们的参数. 在这里write函数, 需要从a0,a1,a2分别获取三个参数. 当sys_write返回时, 我们将返回值赋值给p->trapframe->a0. 原因在于RISC-V上的C代码的习惯是函数的返回值存储于寄存器`a0`。所以为了模拟函数的返回，我们将返回值存储在trapframe的`a0`中。之后，当我们返回到用户空间，trapframe中的`a0`槽位的数值会写到实际的`a0`寄存器，Shell会认为`a0`寄存器中的数值是`write`系统调用的返回值。
> >
> > ​	似乎trap就和普通的函数一样, 这多亏了我们的代码.
> >
> > ```cpp
> > p->trapframe->a0 = syscalls[num]();
> > ```
> >
> > 5、结束syscall
> >
> > ​	执行完syscall后, 我们返回到函数`usertrap`中执行剩下的语句
> >
> > ```cpp
> > if(killed(p)) // 不要恢复一个被杀掉的进程
> >  exit(-1);
> > 
> > // give up the CPU if this is a timer interrupt.
> > if(which_dev == 2)
> >  yield();
> > 
> > usertrapret();
> > ```
>
> 
>
> ### 七、usertrapret函数
>
> 在处理trap的最后, 调用了usertrapret这个函数.
>
> 这是在返回到用户空间之前内核要做的工作.
>
> **工作流程**
>
> > 一、修改stvec寄存器(修改为用户空间trap处理函数)
> >
> > ​	它首先关闭了中断。我们之前在系统调用的过程中是打开了中断的，这里关闭中断是因为我们将要更新stvec寄存器来指向用户空间的trap处理代码。而之前在内核中的时候，我们指向的是内核空间的trap处理代码。我们关闭中断因为当我们将stvec更新到指向用户空间的trap处理代码时，我们仍然在内核中执行代码。如果这时发生了一个中断，那么程序执行会走向用户空间的trap处理代码，即便我们现在仍然在内核中，出于各种各样具体细节的原因，这会导致内核出错。所以我们这里关闭中断。
> >
> > ```cpp
> > // we're about to switch the destination of traps from
> > // kerneltrap() to usertrap(), so turn off interrupts until
> > // we're back in user space, where usertrap() is correct.
> > intr_off();
> > 
> > // send syscalls, interrupts, and exceptions to uservec in trampoline.S
> > uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
> > w_stvec(trampoline_uservec);
> > ```
> >
> > 二、填入trapframe内容
> >
> > ```cpp
> > // set up trapframe values that uservec will need when
> > // the process next traps into the kernel.
> > p->trapframe->kernel_satp = r_satp();         // kernel page table
> > p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
> > p->trapframe->kernel_trap = (uint64)usertrap;
> > p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()
> > ```
> >
> > 这些内容对trampoline代码非常有用. 这里的代码是
> >
> > - 存储内核页表指针
> > - 存储当前用户进程的内核栈
> > - 存储`usertrap`函数的指针. 这样trampoline代码才能跳转到这个函数
> > - 从`tp`寄存器读取cpu编号存储在trapframe中, 这样trampoline代码才能恢复这个数
> >
> > 现在我们在`usertrapret`函数中，我们正在设置trapframe中的数据，这样下一次从用户空间转换到内核空间时可以用到这些数据。
> >
> > 接下来我们要设置sstatus寄存器，这是一个控制寄存器。这个寄存器的SPP bit位控制了sret指令的行为，该bit为0表示下次执行sret的时候，我们想要返回user mode而不是supervisor mode。这个寄存器的SPIE bit位控制了在执行完sret之后，是否打开中断。因为我们在返回到用户空间之后，我们的确希望打开中断，所以这里将`SPIE` bit位设置为1。修改完这些bit位之后，我们会把新的值写回到`sstatus`寄存器。
> >
> > ```cpp
> >   // set S Previous Privilege mode to User.
> >   unsigned long x = r_sstatus();
> >   x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
> >   x |= SSTATUS_SPIE; // enable interrupts in user mode
> >   w_sstatus(x);
> > ```
> >
> > 
> >
> > 我们在trampoline代码的最后执行了`sret`指令。这条指令会将程序计数器设置成`sepc`寄存器的值，所以现在我们将`sepc`寄存器的值设置成之前保存的用户程序计数器的值。在不久之前，我们在`usertrap`函数中将用户程序计数器保存在trapframe中的`epc`字段。
> >
> > ```cpp
> >   // set S Exception Program Counter to the saved user pc.
> >   w_sepc(p->trapframe->epc);
> > ```
> >
> > ​	根据用户页表地址生成相应的satp值，这样我们在返回到用户空间的时候才能完成页表的切换。实际上，我们会在汇编代码trampoline中完成页表的切换，并且也只能在trampoline中完成切换，因为只有trampoline中代码是同时在用户和内核空间中映射。但是我们现在还没有在trampoline代码中，我们现在还在一个普通的C函数中，所以这里我们将页表指针准备好，并将这个指针作为参数传递给汇编代码，这个参数会出现在a0寄存器。
> > ​	trampoline_userret： 倒数第二行的作用是计算出我们将要跳转到汇编代码的地址。我们期望跳转的地址是tampoline中的userret函数，这个函数包含了所有能将我们带回到用户空间的指令。所以这里我们计算出了userret函数的地址。倒数第一行，将trampoline_userret指针作为一个函数指针，执行相应的函数(也就是userret函数)并传入参数
> >
> > ```cpp
> >   // tell trampoline.S the user page table to switch to.
> >   uint64 satp = MAKE_SATP(p->pagetable);
> > 
> >   // jump to userret in trampoline.S at the top of memory, which 
> >   // switches to the user page table, restores user registers,
> >   // and switches to user mode with sret.
> >   uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
> >   ((void (*)(uint64))trampoline_userret)(satp);
> > ```
> >
> > 
>
> 
>
> ### 八、userret函数
>
> ​	现在程序执行又到了trampoline代码。
>
> ​	第一步是切换页表。在执行`csrw satp, a0`之前，页表应该还是巨大的内核页表。这条指令会将用户页表指针(在usertrapret中作为第二个参数传递给了这里的userret函数，所以存在a0寄存器中)存储在satp寄存器中。执行完这条指令之后，页表就变成了小得多的用户页表。
>
> ```asm
>         csrw satp, a0
>         sfence.vma zero, zero #清空页表缓存
> ```
>
>  接着将a0赋值为trapframe, 并恢复一系列用户寄存器
>
> ```asm
>         li a0, TRAPFRAME
> 
>         # restore all but a0 from TRAPFRAME
>         ld ra, 40(a0)
>         ld sp, 48(a0)
>         ld gp, 56(a0)
>         ld tp, 64(a0)
>         ld t0, 72(a0)
>         ld t1, 80(a0)
>         ............
> ```
>
> 最后, 恢复a0
>
> ```cpp
> ld a0, 112(a0)
> ```
>
> `sret`是我们在kernel中的最后一条指令，当我执行完这条指令：
>
> ①程序会切换回user mode
> ②`sepc`寄存器的数值会被拷贝到PC寄存器(程序计数器)
> ③重新打开中断
>
> ```cpp
>         # return to user mode and user pc.
>         # usertrapret() set up sstatus and sepc.
>         sret
> ```
>
> 



#### 函数调用

sp寄存器指示着栈空间, 每个函数都占据一段连续的栈空间, 名为栈帧.

当为一个函数分配栈空间时, 只需要将sp指针往下挪(减小即可).

ra寄存器指示着函数的返回地址. 也就是当本函数执行完, 需要返回到之前到指令地址. ra就是存储着这样一个值.

fp寄存器指示着上一个栈帧的头地址, 这样当本函数执行完时, sp也能返回到之前到位置.

**为了实现函数的嵌套调用, 必须考虑到ra, fp寄存器到值是会被覆盖的**

举个例子, main调用level1函数, 进入level1函数时, ra确实指向main的某条指令地址.

当level1返回时, 会将pc修改到ra到位置去. 前提是在level1函数期间ra不改变.

但如果level1执行期间调用level2函数, 那么为了让level2函数返回, 需要将ra, fp修改

- ra修改为自己执行level2函数之后到那条指令地址(这样从level2返回后能接着执行)
- fp修改为自己栈帧的开始位置. 因为level2函数也会分配一个栈帧改变sp的位置. 当level2返回时,  需要有信息提供level1的栈帧开始处, 这样才能复原sp指针.

说的简单点, 就是因为level2需要使用ra, fp寄存器, 需要有个地方临时存一下ra, fp的值.

索性存储在本函数的栈帧中去。又由于每个函数返回时会根据当前fp指针的值返回上个函数的栈帧, 所以相当于所有值都被恢复了.




## Lab

### RISC-V assembly ([easy](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

了解一点RISC-V汇编是很重要的，您在6.1910(6.004)中已经接触过。在xv6仓库中有一个文件`user/call.c`。`make fs.img`对它进行编译，并在`user/call.asm`中生成可读的程序汇编版本。

请阅读`call.asm`中的代码中的函数`g`、`f`和`main`。RISC-V的使用手册参见参考页面。下面是一些需要回答的问题(将答案存储在文件answers-traps.txt中):

> 哪些寄存器包含函数的参数? 例如，在main调用printf时，哪个寄存器保存了13 ?
>
> **solution**
>
> 首先s0寄存器又叫fp寄存器, 指向前一个栈帧的地址.  
>
> 可以看到, 在f函数的开始位置, 是一个长达三条指令的函数头.
>
> 先为f的栈帧分配16字节大小, 此时fp存储到是上一个栈帧的位置. 所以将s0存储在8(sp)位置.
>
> 由于后续可能还需要调用其他函数, 为了维护fp的性质, 将fp修改为当前栈帧地址(对于子函数来说，父函数的栈帧就是上一个栈帧). 可以发现这里的栈帧没存返回地址, 也没关系, 因为这是leaf函数(在汇编中是如此, 尽管C代码不是), ra寄存器不会被修改了.
>
> ```asm
> addi	sp,sp,-16
> sd	s0,8(sp)
> addi	s0,sp,16
> ```
>
> 接下来的指令是函数体. 容易看出函数参数在a0寄存器中
>
> ```cpp
> addiw	a0,a0,3  
> ```
>
> 再接下来几条指令, 是函数尾
>
> ```asm
> ld	s0,8(sp)  // 现在需要返回, 复原fp
> addi	sp,sp,16 // 复原sp
> ret
> ```
>
> 这就是f的全过程. 
>
> 然后main函数中， a2寄存器保存了13.



> main的汇编代码中对函数f的调用在哪里?呼叫g在哪里?(提示:编译器可以内联函数。)
>
> **solution**
>
> 没有调用。`g`被内联到`f`中，然后`f`又被内联到main中。由汇编代码中main函数中的`li a1,12`可以看出，直接将最后的结果12传递到了`a1`。



> printf函数位于什么地址?
>
> **solution**
>
> 根据这两句, 首先设置ra为当前pc到值0x30
>
> 然后加上1562(0x61a), 所以实际的跳转地址是0x64a
>
> ```asm
>   30:	00000097          	auipc	ra,0x0
>   34:	61a080e7          	jalr	1562(ra) # 64a <printf>
> ```





> 在main中的jalr到printf之后，寄存器ra中的值是什么?
>
> **solution**
>
> jalr将pc设置为printf的首地址, 将ra设置为当前指令的下一条指令地址. 就是0x38.



> 运行下面的代码。
>
> ```cpp
> 	unsigned int i = 0x00646c72;
> 	printf("H%x Wo%s", 57616, &i);
> ```
>
> 输出是什么? 下面是一个将字节映射到字符的[ASCII表](https://www.asciitable.com/)。
> 输出结果依赖于RISC-V是小端序的。如果RISC-V是大端序的，为了得到相同的输出，应该把i设置为什么呢? 是否需要将57616更改为其他值?
>
> 这是对[小端和大端的描述](http://www.webopedia.com/TERM/b/big_endian.html)还有一个更[古怪的描述](https://www.rfc-editor.org/ien/ien137.txt)。
>
> **solution**
>
> 输出是`He110 World`. 
>
> 首先57616的十六进制是0xe110.
>
> RISC-V是小端序(小/低位在前), 在内存的存储形式为0x726c6400, 也就是rld， 末尾一个'\0'.
>
> 如果RISC-V是大端序, 只需将i修改为i=0x726c6400.



> 在下面的代码中，` y= `后面会打印什么?(注意:答案不是一个特定的值。)为什么会这样?
>
> ```cpp
> printf("x=%d y=%d", 3);
> ```
>
> **solution**
>
> printf的两个参数分别放在a1和a2中. 这里会直接输出a2里的值.



## Backtrace ([moderate](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

>在调试时，backtrace通常很有用: 在错误发生点以上的堆栈上的函数调用列表。为帮助进行回溯，编译器生成机器代码，在栈上维护一个栈帧，对应于当前调用链中的每个函数。每个栈帧由返回地址和指向调用方栈帧的“帧指针”组成。寄存器s0包含一个指向当前栈帧的指针(它实际上指向栈中保存的返回地址的地址加上8)。你的回溯应该使用栈指针向上遍历栈，并打印出每个栈帧中保存的返回地址。
>
>实现一个backtrace()函数(在kernel.printf.c中). 在sys_sleep中插入对它的调用, 然后运行`bttest`. 它会调用sys_sleep. 你的输出应该形如下面这个列表
>
>```cpp
>backtrace:
>0x0000000080002cda
>0x0000000080002bb6
>0x0000000080002898
>```
>
>在`bttest`终止qemu后, 在终端窗运行`addr2line -e kernel/kernel`,  然后复制你的输出地址, 如下所示
>
>```shell
>    $ addr2line -e kernel/kernel
>    0x0000000080002de2
>    0x0000000080002f4a
>    0x0000000080002bfc
>    Ctrl-D
>```
>
>你应该看到以下输出
>
>```shell
>    kernel/sysproc.c:74
>    kernel/syscall.c:224
>    kernel/trap.c:85
>```

- 当前栈帧地址保存在fp当中, 添加以下函数到kernel/riscv.h中. 你会用得到它.

```cpp
static inline uint64
r_fp()
{
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x) );
  return x;
}
```

- 请注意，返回地址与stackframe的帧头部的偏移量是固定的(-8)，而保存的帧指针与帧头部的偏移量是固定的(-16)。
- 你的backtrace()需要一种方法来识别它已经看到了最后一个栈帧，并且应该停止。一个有用的事实是，分配给每个内核栈的内存由一个页对齐的页组成，因此给定栈的所有栈帧都在同一页上。大家可以使用PGROUNDDOWN(fp)(参见kernel/riscv.h)来确定帧指针所指的页。
  回溯开始工作后，在kernel/printf.c中从panic调用它，这样您就可以在内核发生紧急情况时看到它的回溯。

```cpp
// My Code ===============================
// fp为当前的帧头部地址
uint64 ex(uint64 addr) {
  uint64 *vm = (uint64 *)addr;
  return *vm;
}

void dfs(uint64 fp) {
  uint64 ra = ex(fp - 8);
  uint64 lasFp = ex(fp - 16);
  printf("%p\n", ra);
  if (PGROUNDDOWN(fp) != PGROUNDDOWN(lasFp)) {
    return;
  }
  dfs(lasFp);
}

void backtrace() {
  uint64 fp = r_fp();
  dfs(fp);
}
```



## Alarm ([hard](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

> 在这个练习中，您将为xv6添加一个特性，该特性会在进程使用CPU时间时周期性地发出警报。这对于计算受限的进程想要限制它们占用的CPU时间，或者想要进行计算但又想采取一些周期性操作的进程可能很有用。更一般地说，你将实现用户级中断/错误处理程序的基本形式; 例如，您可以使用类似的东西来处理应用程序中的缺页异常。如果通过了`alarmtest`和` usertests -q `，那么你的解决方案就是正确的。
>
> 您应该添加一个新的`sigalarm(interval, handler)`系统调用。如果应用程序调用了`sigalarm(n, fn)`，那么在该程序消耗的CPU时间的每n个“tick”之后，内核就应该调用应用程序函数fn。当fn返回时，应用程序应该从中断的位置恢复。在xv6中，时钟是一个相当随意的时间单位，由硬件定时器产生中断的频率决定。如果应用程序调用sigalarm(0,0)，内核应该停止产生周期性的报警调用。
>
> 你会在xv6库中找到文件user/alarmtest.c。将其添加到Makefile中。除非添加sigalarm和sigreturn系统调用(见下文)，否则它将无法正确编译。
>
> alarmtest调用test0中的sigalarm(2, periodic)，要求内核强制每2个时钟周期调用periodic()一次，然后旋转一段时间。你可以在user/alarmtest中看到alarmtest的汇编代码。当alarmtest产生如下输出时，你的解决方案是正确的，usertests -q也能正确运行:
>
> ```shell
> $ alarmtest
> test0 start
> ........alarm!
> test0 passed
> test1 start
> ...alarm!
> ..alarm!
> ...alarm!
> ..alarm!
> ...alarm!
> ..alarm!
> ...alarm!
> ..alarm!
> ...alarm!
> ..alarm!
> test1 passed
> test2 start
> ................alarm!
> test2 passed
> test3 start
> test3 passed
> $ usertest -q
> ...
> ALL TESTS PASSED
> $
> ```
>
> 当你完成时，你的解决方案将只有几行代码，但可能很难把它做好。我们将使用原始存储库中的alarmtest.c版本来测试代码。可以修改alarmtest.c以帮助调试，但要确保原来的alarmtest说所有测试都通过了。

#### test0: invoke handler

首先，修改内核，使其跳转到用户空间的`alarm handler`，这将导致test0打印"alarm!"。现在不用担心输出“alarm!”之后会发生什么;如果你的程序在打印"alarm!"后崩溃了，暂时没有问题。这里有一些提示:

- 你需要修改Makefile，将alarmtest.c编译为xv6用户程序。

- `user/user.h`中的定义应该如下所示

  ```cpp
  int sigalarm(int ticks, void (*handler)());
  int sigreturn(void);
  ```

- 更新`user/usys.pl`(它生成`user/usys.S`)、`kernel/syscall.h`和`kernel/syscall.c`，以允许`alarmtest`调用`sigalarm`和`sigreturn`系统调用。

- `sys_sigreturn`应该返回0

- `sys_sigalarm()`应该将alarm interval(警报间隔)和指向处理程序函数的指针存储在proc结构体的新字段中(在`kernel/proc.h`中)。

- 你需要跟踪从上一次调用(或直到下一次调用)进程的告警处理程序以来已经传递了多少个时钟周期;为此，你也需要在struct proc中添加一个新字段。你可以在proc.c的allocproc()中初始化proc字段。

- 每个时钟周期，硬件时钟强制执行一个中断，由kernel/trap.c中的usertrap()处理。

- 只有在出现定时器中断的情况下，你才需要操作进程的警报时钟;你想要的是

	```cpp
	   if(which_dev == 2) ...

- 只有在进程有定时器未完成时，才调用alarm函数。请注意，用户的alarm函数的地址可能是0(例如，在user/alarmtest.asm，`periodic`位于地址0)。
- 你需要修改usertrap()，以便在进程的alarm interval过期时，用户进程执行处理程序函数。当RISC-V上的trap返回到用户空间时，什么决定用户空间代码恢复执行的指令地址?

#### test1/test2()/test3(): resume interrupted code

你现在只能通过test0. 要解决这个问题，必须确保在alarm处理程序完成时，控制返回到用户程序最初被定时器中断时的指令。必须确保寄存器内容恢复到中断时的值，以便用户程序在alarm后可以继续不受干扰。最后，应该在每次报警计数器发出警报后“重新武装”它，以便定期调用处理程序。

作为一个起点，我们已经为您做出了一个设计决策: 用户alarm处理程序在完成时需要调用sigreturn系统调用。请查看alarmtest.c中的periodic作为示例。这意味着，可以向usertrap和sys_sigreturn添加代码，它们协同工作，使用户进程在处理完告警后正常恢复。

一些提示

- 你的解答需要保存并存储寄存器.(需要保存和恢复哪些寄存器才能正确地恢复中断的代码?(提示:会有很多))
- 在定时器结束时，让usertrap在struct proc中保存足够的状态，使sigreturn能够正确地返回到被中断的用户代码。
- 防止对处理程序的可重入调用----如果处理程序尚未返回，内核不应再次调用它。Test2对此进行测试。
- 确保恢复a0。sigreturn是一个系统调用，它的返回值存储在a0中。

一旦通过了test0、test1、test2和test3，就运行usertests -q，以确保没有破坏内核的任何其他部分。

---

**solution**

首先看一下我的进程结构体中新增字段.

```cpp
  uint64 need_interval;  // 每need_interval触发一次
  uint64 now_interval;		// 已经过去了now_interval的间隔
  void (*handler)(void);  // 处理函数	
  int on;	// 当前是否在启动sigalarm
  int inhandler;  // 当前是否还在处理上一个handler. 如果是, 即使到了时间间隔也不应该再次调用
	struct trapframe trapframe2;  // 备份的用户寄存器状态
```

梳理整个过程, 用户调用sigalarm让内核监视, 每经过need_interval次tick调用一次handler.

于是每次trap且是时钟中断, 就让now_interval自增. 当达到时间间隔, 我们尝试调用用户的处理函数。

由于该函数在用户空间内, 只能让epc寄存器指向该地址. 这样我们可以通过`trampoline.S/userret`函数返回到用户空间的处理函数.

但是此时应该在内核保存原来的所有用户寄存器. 因为当执行用户空间的处理函数这些状态都将不复存在.

最后用户空间的处理函数末尾会调用系统调用sigreturn, 于是再次回到内核, 此时内核将先前保存的用户寄存器恢复.

在`trap.c/usertrap`函数中增加以下实现

```cpp
    if(which_dev == 2) {
      if (p->on && (++p->now_interval) >= p->need_interval) {
        if (!p->inhandler) {
          p->inhandler = 1;
          p->now_interval = 0;
          p->trapframe2 = *p->trapframe;
          p->trapframe->epc = (uint64)p->handler;
        }
      }
    }
  }
```

两个系统调用实现

```cpp
uint64 sys_sigalarm(void) {
  uint64 addr;
  int interval;
  argint(0, &interval);
  argaddr(1, &addr);
  struct proc *p = myproc();
  p->now_interval = 0;
  p->need_interval = interval;
  p->handler = (void (*)(void))addr;
  p->on = 1;
  if (interval == 0 && addr ==0) {
    p->on = 0;
  }
  return 0;
}

uint64 sys_sigreturn(void) {
  struct proc *p = myproc();
  
  *p->trapframe = p->trapframe2;
  p->inhandler = 0;
  return 0;
}
```


