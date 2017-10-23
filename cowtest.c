#include "types.h"
#include "stat.h"
#include "user.h"
#include "x86.h"

void testCase1();
void testCase2();
void testCase3();

int main()
{
	int pid, i;
	// make sure we we'll change two pages.
	int SIZE = 4096 + 1;
	char *space = (char *)malloc(SIZE);
	printf(1,"cow test.\n");

	procdump();
    // son
	if((pid = fork())==0)
	{
		printf(1,"\nChild process before changing values\n\n");
		procdump();
		for(i = 0; i < SIZE; i++)
		{
			// writing to heap
			space[i]++;
		}
		printf(1,"\nChild process after changing values\n\n");
		procdump();
		exit();	
	}
	// parent
	else
		wait();

	free(space);
	exit();

return 1;
}