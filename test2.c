/*
 * author : Erez Fremder
 */

#include "types.h"
#include "stat.h"
#include "user.h"

int mutex;
int test;

void printer (){
	int input;
	input = kthread_mutex_lock(mutex);
	if(input<0)
		printf(1,"Error: thread mutex didnt lock!");
	printf(1,"thread %d said hi\n",kthread_id());
	test=1;
	input = kthread_mutex_unlock(mutex);
	if(input<0)
		printf(1,"Error: thread mutex didnt unlock!");
	kthread_exit();
	printf(1,"Error: returned from exit !!");
}

int main(int argc, char **argv)
{
	printf(1,"~~~~~~~~~~~~~~~~~~\ntest starts\nIf it ends without Errors you win! : )\n~~~~~~~~~~~~~~~~~~\n");
	int input,i;
	mutex = kthread_mutex_alloc();
	if(mutex<0)
		printf(1,"Error: mutex didnt alloc! (%d)\n",mutex);
	for(i = 0; i<5; i++){
		test=0;
		input = kthread_mutex_lock(mutex);
		if(input<0)
			printf(1,"Error: mutex didnt lock! (%d)\n",input);
		char * stack = malloc (1024);
		int tid = kthread_create ((void *)printer, stack, 1024);
		if(tid<0) printf(1,"Thread wasnt created correctly! (%d)\n",tid);
		printf(1,"joining on thread %d\n",tid);
		if(test)printf(1,"Error: mutex didnt prevent writing!\n");
		input = kthread_mutex_unlock(mutex);
		if(input<0) printf(1,"Error: mutex didnt unlock!\n");
		kthread_join(tid);
		if(!test) printf(1,"Error: thread didnt run!\n");
		printf(1,"finished join\n");
	}
	printf(1,"Exiting\n");
	input = kthread_mutex_dealloc(mutex);
	if(input<0)
		printf(1,"Error: mutex didnt dealloc!\n");
	exit();
	printf(1,"Error: returned from exit !!\n");
}
