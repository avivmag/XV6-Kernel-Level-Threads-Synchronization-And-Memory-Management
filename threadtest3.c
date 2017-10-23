#include "types.h"
#include "stat.h"
#include "user.h"
#include "kthread.h"
void * hey()
{
	sleep(10);
	printf(1,"Thread %d is finished.",kthread_id());
	kthread_exit();
	exit();
}
void * hey_bad()
{
	int i=1;
	while(i)
	{
		printf(1,"HAHA.\n");
	}
	exit();
}
int main(void)
{
	char * a[3] = {"ls",0};
	char stack1[1024];
	int tid;
	tid=kthread_create(&hey,stack1,1024);
	kthread_join(tid);
	printf(1,"Creating the second thread.\n");
	tid=kthread_create(&hey_bad,stack1,1024);
	sleep(500);
	printf(1,"                             Execing.\n");
	if(exec("ls",a)<0)
	{
		printf(1,"HIIIIIIIIIIIIIIIIIIIIIII.\n");
	}
	printf(1,"Before Exit!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	exit();
}