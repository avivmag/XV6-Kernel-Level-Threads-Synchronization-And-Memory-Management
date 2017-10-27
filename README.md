# XV6-Kernel-Level-Threads-Synchronization-And-Memory-Management
The famous XV6 operating system with extension to , synchronization primitives and Copy On Write (COW) optimization for the fork system call.

This repository extends the famous [mit-pdos xv6 repository](https://github.com/mit-pdos/xv6-public).
It adds three improvements to the original implementation of xv6:
1.	Kernel level threads.
2.	Synchronization primitives.
3.	Copy On Write optimization.

The development of this OS improvements was done as part of an assignment in "Operating system" course at Ben-Gurion University in the summer of 2016.

A fully detailed description of the implementation can be found in the assignment description attached.

## Kernel level threads And Synchronization primitives

Threads are now supported, meaning multiple tasks sharing the same memory can now be ran simultaneously. Synchronization primitives are also supported as part of the improvements.
Using them will assure a mutual exclusion (mutex) principle will be kept. Using threads and mutex are available via system calls. An fssp program can be run which tests the threads and the new primitives.

## Memory management

Copy on write trick is a great improvement for most operating systems. basically what it does is reducing the amount of page faults by decreasing the number of pages. that is achieved by adding 'writable' flag for every physical page and maintaining a mechanism in which new pages will not be assigned when forking a process. instead of assinging new pages for a forked process, the new forked process will point to the same pages the father of it is pointing to and those shared pages 'writable' flag will be set to off (meaning they are not writable). once one of the processes decides to make some changes, a copy of the page will be assigned for that process, its flag will be set to true and the flag of the old page will be set according to the number of processes pointing to it (if bigger than one, it should not be writable).</br>
This mechanism is highly recommended because most of times a calling to fork is made, most of the pages do not change until the process terminates. for example sections .text or .rodata never changes.

## Getting Started
### Prerequisites

1. Kubuntu - this program was tested only on kubuntu, but it probably can be ran on any other known qemu and gcc compatible operating systems.
	https://kubuntu.org/getkubuntu/</br>
2. QEMU 
	via ```sudo apt-get install qemu-system``` on ubuntu based os (kubuntu included).
3. GNU make
	https://www.gnu.org/software/make/
4. gcc compiler
	via ```sudo apt-get install gcc-4.8``` on ubuntu based os (kubuntu included).

### Simulating the process

1. open terminal and navigate to the program directory
2. type `make qemu` and press enter, the operating system should now boot on.
3. when shell is available, you can try the followings: fssp program with `fssp <number_of_soldiers>`, sanity test for cow with `cowtest` command, sanity test for fork with `forktest` command, many more thread tests (for better view of the available user commands use `ls` command).
4. enjoy :).

## Built With

* [GNU make](https://www.gnu.org/software/make/) - A framework used for simple code compilation.
* [gcc](https://gcc.gnu.org/)
* [QEMU](https://www.qemu.org/)

## Useful links

* The original source of the assignment: https://www.cs.bgu.ac.il/~os163/wiki.files/Assignment2.pdf.
* https://en.wikipedia.org/wiki/Operating_system
* https://en.wikipedia.org/wiki/Xv6
* https://pdos.csail.mit.edu/6.828/2014/xv6/book-rev8.pdf
* https://en.wikipedia.org/wiki/System_call
* http://www.programmerinterview.com/index.php/operating-systems/thread-vs-process/
* https://en.wikipedia.org/wiki/Firing_squad_synchronization_problem
* https://en.wikipedia.org/wiki/Copy-on-write
* https://en.wikipedia.org/wiki/Page_table
* https://en.wikipedia.org/wiki/Page_fault
