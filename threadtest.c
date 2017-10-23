#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "syscall.h"

void
run(){
	int id = kthread_id();
	int pid = getpid();
	int i, j;
	printf(1, "my id: %d\n", id);
	printf(1,"my pid: %d\n", pid);
	for(i=0; i<100000;i++)
		for(j=0; j<400;j++)
			id++;
	printf(1,"hey");
	kthread_exit();
}

int
main(int argc, char *argv[])
{
	void* stack = (void*)malloc(4000);
	void*(*start_func)();
	start_func = (void*)&run;
	int pid = getpid();
	printf(1, "%d\n",pid);
	int tid = kthread_create(start_func, stack, 4000);
	 int rest = kthread_join(tid);
	//int rest = 0;
	printf(1, "result: %d, tid: %d\n", rest, tid);
	exit();
}