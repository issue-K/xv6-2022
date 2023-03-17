# Lab: Xv6 and Unix utilities

本实验将使您熟悉xv6及其系统调用。

## sleep

> 为xv6实现UNIX程序sleep. 您的sleep应该暂停用户指定的ticks数。tick是由xv6内核定义的时间概念，即来自计时器芯片的两次中断之间的时间。您的解决方案应该在文件user/sleep.c中。

一些提示

- 在开始之前, 先阅读[xv6 book](https://pdos.csail.mit.edu/6.828/2022/xv6/book-riscv-rev3.pdf)的第一章
- 查看`user/`中的其他一些程序(例如，`user/echo.c`, `user/grep.c`和`user/rm.c`)，了解如何获取传递给程序的命令行参数。
- 如果用户忘记传递参数，sleep应该打印一条错误消息。
- 命令行参数作为字符串传递; 您可以使用`atoi`将其转换为整数(参见`user/ulib.c`)。
- 使用系统调用`sleep`
- 参见`kernel/sysproc.c`获得实现sleep系统调用的xv6内核代码(请查找`sys_sleep`)，参见`user/user.h`获得从用户程序调用sleep的C定义，参见`user/usys.S`表示从用户代码跳转到内核休眠的汇编程序代码。

- `main`应该调用`exit(0)`来结束
- 将`sleep`程序添加到Makefile中的`UPROGS`. 完成此操作后，`make qemu`将编译您的程序，您将能够从xv6 shell运行它。

完成后你可以运行程序

```shell
$ make qemu
...
init: starting sh
$ sleep 10
(nothing happens for a little while)
$
```

如果您的程序在如上所示运行时暂停，那么您的解决方案就是正确的。运行make grade，看看你是否真的通过了sleep测试。

```shell
$ ./grade-lab-util sleep
```

或者, 可以(`make grade`会运行所有测试)

```shell
make GRADEFLAGS=sleep grade
```



## pingpong ([easy](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

> 编写一个程序，利用UNIX系统调用，通过一对管道在两个进程之间“ping-pong”一个字节。父进程应该向子进程发送一个字节; 子进程应该打印“<pid>: received ping”，其中<pid>是它的进程ID，将该字节写入管道到父进程，然后退出;父进程应该从子进程读取字节，打印"<pid>: received pong"，然后退出。你的解决方案应该在文件user/pingpong.c中。

一些提示

- 使用pipe创建一个管道
- 使用fork创建一个儿子进程
- 使用read从管道读, 使用write写入一个管道
- 使用getpid获取进程的id号
- 同样添加到Makefile
- xv6上的用户程序只能使用一组有限的库函数。你可以在user/user.h中看到这个列表;源(除了系统调用)在user/ulib.c、user/printf.c和user/umalloc.c中。



## primes ([moderate](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))/([hard](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

> 使用管道(pipe)编写一个prime sieve的并发版本。这个想法要归功于Unix管道的发明者Doug McIlroy。[this page](https://swtch.com/~rsc/thread/)中间的图片和周围的文字解释了如何做到这一点。你的解决方案应该在文件user/primes.c中。
>
> 你的目标是使用pipe和fork来设置管道。第一个进程将数字2 ~ 35输入到管道中。对于每个质数，你可以安排创建一个进程，通过一个管道从它的左邻居读取数据，并通过另一个管道向它的右邻居写入数据。由于xv6的文件描述符和进程数量有限，第一个进程可以在35处停止。



## find ([moderate](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

> 编写一个Unix find的简单版本. 查找目录树中具有特定名称的所有文件。你的解决方案应该在文件user/find. d中。

- 查看user/ls.c了解如何读取目录
- 你需要用到递归来遍历目录树，但是不要对"."和”..“进行递归
- 对文件系统的更改会在qemu运行时持续保存;要得到一个干净的文件系统，运行make clean，然后make qemu。

- 你需要用到C字符串。但是请使用strcmp()来比较字符串
- 将程序添加到Makefile中的UPROGS中。

```shell
[input]
echo > b
mkdir a
echo > a/b
mkdir a/aa
echo > a/aa/b
find . b
[output]
./b
./a/b
./a/aa/b
```



## xargs ([moderate](https://pdos.csail.mit.edu/6.828/2022/labs/guidance.html))

> 编写一个简单版本的UNIX xargs程序: 它的参数描述了要运行的命令，它从标准输入中读取行，然后针对每一行执行该命令，并将该行添加到命令的参数后面。你的解决方案应该在文件user/xargs.c中。
>
> 例如, 下面是一个例子
>
> ```shell
> $ echo hello too | xargs echo bye
> bye hello too
> $
> ```
>
> 注意，这里的命令是"echo bye"，附加的参数是"hello too"，所以命令是"echo bye hello too"，输出结果是"bye hello too"。
> 请注意，UNIX上的xargs进行了优化，它将一次向命令提供更多的参数。我们不期望你进行这种优化。为了让xargs在UNIX上按照我们希望的方式运行，请将-n选项设置为1。例如
>
> ```shell
> $ (echo 1 ; echo 2) | xargs -n 1 echo
> 1
> 2
> $
> ```

- 使用fork和exec来对每一行输入调用该命令。在父进程中使用wait等待子进程完成命令。
- 要读取单个输入行，每次读取一个字符，直到出现换行符('\n')。
- kernel/param.h声明了MAXARG，如果你需要声明argv数组，它可能会很有用。
- 将程序添加到Makefile中的UPROGS中。

这里有一个xargs, find, grep组合的例子

```shell
$ find . b | xargs grep hello
```

将对“.”目录下的每个名为b的文件运行“grep hello”。

请运行shell脚本xargstest.sh。如果你的解决方案产生以下输出，说明它是正确的:

```shell
$ make qemu
...
init: starting sh
$ sh < xargstest.sh
$ $ $ $ $ $ hello
hello
hello
$ $
```

你可能需要返回去修复find程序中的bug。输出中有很多\$, 因为xv6 shell没有意识到它处理的是文件中的命令，而不是控制台中的命令，因此文件中的每个命令都会打印一个$。