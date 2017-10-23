#include "types.h"
#include "stat.h"
#include "user.h"
#include "kthread.h"
void * print_id(void)
{
	printf(1,"Thread id is: %d\n",kthread_id());
	
	sleep(100);
	printf(1,"Thread is exiting.\n");
	kthread_exit();
	printf(1,"lalalalalalalalalalalalalalalala\n" );
	return (void *) 1;
}
int main()
{
	int tid1,tid2;
	char stack1[1024];
	char stack2[1024];
		printf(1,"thread in main %d,process %d\n", kthread_id(),getpid());

	tid1=kthread_create(&print_id,stack1,1024);
	
	tid2=kthread_create(&print_id,stack2,1024);
	kthread_join(tid1);
	printf(1,"Got id : %d \n",tid1);
	kthread_join(tid2);
	printf(1,"Got id : %d \n",tid2);
	printf(1,"Finished.\n");
	sleep(500);
	kthread_exit();
	return 1;
}