#include "types.h"
#include "stat.h"
#include "user.h"
#include "kthread.h"
#define STACK_SIZE  1024

int soldiersNum;
//                                 NON
enum soldierState { Q, P, R, Z, M, N, F };
enum soldierState algo[5][6][6] =
{
  {
    {Q,P,Q,Q,N,Q},
    {P,P,N,N,N,P},
    {Q,N,Q,N,N,N},
    {Q,N,N,Q,N,Q},
    {N,N,N,N,N,N},
    {Q,P,Q,Q,N,N}
  },
  {
    {Z,Z,R,R,N,N},
    {Z,N,Z,Z,N,N},
    {R,Z,Z,N,N,Z},
    {R,Z,N,Z,N,Z},
    {N,N,N,N,N,N},
    {Z,N,Z,Z,N,N}
  },
  {
    {N,N,R,P,Z,N},
    {N,N,M,R,M,N},
    {R,M,N,N,M,N},
    {P,R,N,N,R,N},
    {Z,M,M,R,M,N},
    {N,N,N,N,N,N}
  },
  {
    {N,N,Q,P,Q,N},
    {N,Z,N,Z,N,N},
    {Q,N,Q,Q,N,Q},
    {P,Z,Q,F,Q,F},
    {Q,N,N,Q,Q,Q},
    {N,Z,Q,F,Q,N}
  },
  {
    {N,N,N,N,N,N},
    {N,N,N,N,N,N},
    {N,N,R,Z,N,N},
    {N,N,Z,N,N,N},
    {N,N,N,N,N,N},
    {N,N,N,N,N,N}
  }
};
char getCharacterFromState(enum soldierState state)
{
  switch(state)
  {
    case Q:
      return 'Q';
    case P:
      return 'P';
    case R:
      return 'R';
    case Z:
      return 'Z';
    case M:
      return 'M';
    case N:
      return 'N';
    case F:
      return 'F';
  }
  return 0;
}

int **stacks;
int mainLock;
int *soldiersTid;

struct soldierStruct {
  enum soldierState nextState;
  int mid;
};

struct soldierStruct *soldiers;
enum soldierState *stateArray;

int functionsLock;
int updatedState;
void* soldierFunc(int pos)
{
  struct soldierStruct * s = &soldiers[pos];
  
  while(s->nextState != F)
  {
    while(kthread_mutex_lock(s->mid) == -1);
  
    s->nextState = algo[stateArray[pos]]
                       [pos == 0 ? N : stateArray[pos - 1]]
                       [pos == soldiersNum - 1 ? N : stateArray[pos + 1]];

    // when a soldier is finished generating a state, it calls this function
    while(kthread_mutex_lock(functionsLock) == -1);

    updatedState++;
    if(updatedState == soldiersNum)
    {
      while(kthread_mutex_unlock(mainLock) == -1);
      updatedState = 0;
    }

    while(kthread_mutex_unlock(functionsLock) == -1);
    // between this last row and the first on the while it can be very problematic if the unlock the lock 
    // before the lock actually happens, the answer to that lies
    // in the implementation of the unlock mechanism, if no thread
    // is waiting on it, it will fail and the main will try to unlock it in a loop.
  }

  kthread_exit();
  return (void*)-1;
}

void initializeSoldiers(int soldiersNum)
{
  int i;
  for(i = 0; i < soldiersNum; i++)
  {
    //for(j = 256; j < STACK_SIZE; j++)
    stacks[i][STACK_SIZE/4] = i;

    soldiersTid[i] = -1;
    while(soldiersTid[i] == -1)
      soldiersTid[i] = kthread_create(soldierFunc, stacks[i], STACK_SIZE-4);
  }
}

int main(int argc, char **argv)
{
  int i;
  struct soldierStruct *s;

  if(argc != 2){
    printf(1,"Missing arguments.\n");
    exit();
  }
  soldiersNum = atoi(argv[1]);

  if(soldiersNum > NTHREAD - 1){
    printf(1,"There are too many soldiers.\n ");
    exit();
  }

  if(soldiersNum <= 2){
    printf(1,"The number of soldier must be above 2.\n");
    exit();
  }
  soldiersTid = malloc(soldiersNum * sizeof(int));
  soldiers = malloc(soldiersNum * sizeof(struct soldierStruct));
  stateArray = malloc(soldiersNum * sizeof(int));
  
  functionsLock = -1;
  while(functionsLock == -1)
    functionsLock = kthread_mutex_alloc();
  
  // build lock and take it so we can stop main thread and make him wait for all other threads later on.
  mainLock = -1;
  while(mainLock == -1)
    mainLock = kthread_mutex_alloc();
  
  while(kthread_mutex_lock(mainLock) == -1);

  // init soldiers
  for(s = soldiers; s < &soldiers[soldiersNum]; s++)
  {
    s->nextState = Q;
    s->mid = -1;
    while(s->mid  == -1)
      s->mid = kthread_mutex_alloc();
  }

  stacks = malloc(4*soldiersNum);


  stateArray[0] = P;
  stacks[0] = malloc(4*STACK_SIZE);
  printf(1, "%c", getCharacterFromState(stateArray[0]));
  for(i = 1; i < soldiersNum; i++)
  {
    stacks[i] = malloc(4*STACK_SIZE);
    stateArray[i] = Q;
    printf(1, "%c", getCharacterFromState(stateArray[i]));
  }
  printf(1, "\n");

  initializeSoldiers(soldiersNum);

  char notFinished = 1;
  while(notFinished)
  {
    while(kthread_mutex_lock(mainLock) == -1);
    
    notFinished = 0;
    // print row
    for(i = 0; i < soldiersNum; i++)
    {
      stateArray[i] = soldiers[i].nextState;
      // figure if we have finished and everyone has now fired.
      if(stateArray[i] != F)
        notFinished = 1;
      printf(1, "%c", getCharacterFromState(stateArray[i]));
    }
    printf(1, "\n");


    // // make sure other processes finished are in waiting state
    // kthread_mutex_lock(functionsLock);


    // free other threads to run freely
    for(s = soldiers; s < &soldiers[soldiersNum]; s++)
      // we will try to unlock it again and again until some one locks it
      while(kthread_mutex_unlock(s->mid) == -1);


    // kthread_mutex_unlock(functionsLock);
  }

  for(s = soldiers; s < &soldiers[soldiersNum]; s++)
    while(kthread_mutex_dealloc(s->mid) == -1);

  for(i = 0; i < soldiersNum; i++)
    while(kthread_join(soldiersTid[i]) == -1);

  while(kthread_mutex_dealloc(functionsLock) == -1);
  while(kthread_mutex_unlock(mainLock) == -1);
  while(kthread_mutex_dealloc(mainLock) == -1);

  
  for(i = 0; i < soldiersNum; i++)
    free(stacks[i]);
  free(stacks);
  free(soldiersTid);
  free(soldiers);
  free(stateArray);
  exit();
  return 0;
}



