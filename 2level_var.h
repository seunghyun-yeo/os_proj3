/* signal test */
/* sigaction */
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <unistd.h> 
#include <time.h>
#include <stdbool.h>


#define sec 0
#define msec 4000
#define time_quantum 2
#define maxproc 1
#define maxcpuburst 4
#define gendtik 200
#define pmemsize 0x40000000
#define kernelmemsize 0x10000000
#define l1mask 0xffc00000
#define l1shift 22
#define l2shift 12
#define l2mask 0x003ff000
#define offsetmask 0xfff
#define checksc 0b10
#define checkvalid 0b1
#define valid 0b1
#define swapcondition 1048550
//#define swapcondition 1048500

#include "queues.h"
#include "fs.h"
int virtualaddress=0;
int* kernel;
int* usr;
int ttbr;
int offset;
int l1index;
int l2index;
int l2result;
int l1result;
int pa;
queue* hdd;
double highest=0;

//clock_t start,end;
struct timeval start,end;
double operatingtime;

void signal_handler(int signo);
void child_handler(int signo);
void tiktok(int, int);
void reduceall();
void searchqueue(queue* ,queuenode**,queuenode**,int);
void insertqueue(queue* ,queuenode*,queuenode*,queuenode*);
void mymovqueue(queue*,queue*,int,int);
void memrequest_handler();

pid_t pid;
int tq;
int dio_time;
int dcpu_time;
int cpu_time;
int io_time;
int ret;
int key;
int globaltik=0;
pid_t gpid[maxproc];

int msgq;
struct sigaction old_sa;
struct sigaction new_sa;
struct itimerval new_itimer, old_itimer;

queue* rqueue;
queue* ioqueue;
queue* fusrqueue;
queue* fkernelqueue;

typedef struct{
	long mtype;
	pid_t pid;
	int io_time;
} msgbuf;

typedef struct{
	long mtype;
	pid_t pid;
	int va[10];
	bool write[10];
	int data[10];
} msgbuf2;

typedef struct{
	pid_t pid;
	int io_time;
	int cpu_time;
	int tq;
	int ttbr;
	queue* kqueue;
	queue* uqueue;
} pcb;

typedef struct{
	pid_t pid;
	int va;
	int motherptr;
	int pgfnum;
} pgfinfo;

typedef struct{
	pid_t pid;
	int va;
	int motherptr;
	int data[1024];
} bckstr;

msgbuf msg;
msgbuf2 memrequest;
pcb* pcbdata;
pgfinfo* fpf;
bckstr* block;

void tiktok(int a, int b)
{
	new_itimer.it_interval.tv_sec = a;
	new_itimer.it_interval.tv_usec = b;
	new_itimer.it_value.tv_sec = a;
	new_itimer.it_value.tv_usec = b;
	setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
}
void reduceall()
{
	pcb* pcbptr;
	queuenode *traverse;
	for(traverse = ioqueue->front;traverse!=NULL;traverse=traverse->next)
	{
		pcbptr=traverse->dataptr;
		pcbptr->io_time--;
	}
}
void mymovqueue(queue* sourceq, queue* destq, int pid, int iotime)
{
	if(sourceq->count==0) return;
	queuenode *ppre=NULL;
	queuenode *ploc=NULL;
	queuenode *pploc= NULL;
	pcb* pcbptr;
	for(ppre=NULL,ploc=sourceq->front; ploc!=NULL;ppre=ploc,ploc=ploc->next){
		pcbptr = ploc->dataptr;

		if(pcbptr->pid == pid){
			if(ppre != NULL)
				ppre->next = ploc->next;
			else if(ppre ==NULL)
				sourceq->front = ploc->next;

			if(ploc->next==NULL)
			{
				sourceq->rear=ppre;
			}

			ploc->next = NULL;
			sourceq->count--;
			pcbptr->io_time = iotime;
			break;
		}
	}
	searchqueue(destq,&ppre,&pploc,iotime);
	insertqueue(destq,ppre,ploc,pploc);
}
void insertqueue(queue* targetqueue, queuenode *ppre, queuenode *ploc, queuenode* pploc)
{
	if(ppre==NULL)//ploc is the first
	{
		if(!emptyqueue(targetqueue))
		{
			ploc->next = targetqueue->front;
			targetqueue->front=ploc;
		}
		else
		{
			targetqueue->front=ploc;
			targetqueue->rear=ploc;
		}
	}
	else
	{
		if(pploc == NULL){//ploc is the end
			ppre->next = ploc;
			targetqueue->rear = ploc;
			targetqueue->count++;
			return;
		}
		ploc->next=ppre->next;
		ppre->next= ploc;
	}
	targetqueue->count++;
}
void searchqueue(queue* targetqueue, queuenode **ppre ,queuenode **pploc, int iotime)
{
	pcb * pcbptr;
	for(*ppre=NULL,*pploc=targetqueue->front;*pploc!=NULL;*ppre=*pploc,*pploc=(*pploc)->next)
	{
		pcbptr=(*pploc)->dataptr;
		if(pcbptr->io_time>iotime)
			break;
	}
}
void overthanos()
{
		for(int i=0;i<maxproc;i++)
		{
			kill(gpid[i],SIGKILL);
		}
}

void signal_handler(int signo)
{
	
	globaltik++;
	pcb *pcbptr = NULL;
	/*queuenode * ppre;
	printf("================================================\nat %d\n\n",globaltik);
	printf("ready queue : ");
	for(ppre=rqueue->front;ppre!=NULL;ppre=ppre->next){
		pcbptr=ppre->dataptr;
		printf("%d\t",pcbptr->pid);
	}
	printf("\n\t");
	for(ppre=rqueue->front;ppre!=NULL;ppre=ppre->next){
		pcbptr=ppre->dataptr;
		printf("\t%d",pcbptr->tq);
	}
	printf("\n");
	printf("io queue :    ");
	for(ppre=ioqueue->front;ppre!=NULL;ppre=ppre->next){
		pcbptr=ppre->dataptr;
		printf("%d\t",pcbptr->pid);
	}
	printf("\n\t");
	for(ppre=ioqueue->front;ppre!=NULL;ppre=ppre->next){
		pcbptr=ppre->dataptr;
		printf("\t%d",pcbptr->io_time);
	}
	printf("\n");*/
	if(globaltik==gendtik)
	{
		overthanos();
		exit(0);
	}
	/*
	if(!emptyqueue(ioqueue))
	{
		reduceall();
		while(1)
		{
			if(emptyqueue(ioqueue)) break;

			queuefront(ioqueue,(void**)&pcbptr);

			if(pcbptr->io_time<0)
			{
				ppre=ioqueue->front;
				ioqueue->front=ppre->next;
				ppre->next=NULL;
				ioqueue->count--;
				if(rqueue->front==NULL)
				{
					rqueue->front=ppre;
					rqueue->rear=ppre;
					rqueue->count++;
				}
				else
				{
					rqueue->rear->next=ppre;
					rqueue->rear = ppre;
					rqueue->count++;
				}
			}
			else
			{
				break;
			}
		}
	}
	*/
	if(!emptyqueue(rqueue))
	{
		queuefront(rqueue,(void**)&pcbptr);
	//	pcbptr->tq--;
	//	pcbptr->cpu_time++;
		kill(pcbptr->pid,SIGUSR1);
	//	if(pcbptr->tq==0)
	//	{
	//		pcbptr->tq=time_quantum;
	//		if(queuecount(rqueue)>1)requeue(rqueue);
	//	}
	}

}
void memrequest_handler()
{
	printf("pid : %d\n",memrequest.pid);
	for(int k=0; k<10;k++)
	{
		if(memrequest.write[k]) printf("write  at : ");
		else printf("read from : ");
		printf("%08x",memrequest.va[k]);
		if(memrequest.write[k]) printf(": %d\n",memrequest.data[k]);
		else printf("\n");
	}
	queuenode * ppre=NULL;
	queuenode * ploc=NULL;
	pcb * pcbptr;
	pgfinfo* fpf2;
	for(ppre=NULL,ploc=rqueue->front;ploc!=NULL;ppre=ploc,ploc=ploc->next)
	{
		pcbptr=ploc->dataptr;
		if(pcbptr->pid==memrequest.pid)
		{
			break;
		}
	}
	ttbr = pcbptr->ttbr;

	for(int k=0; k<10;k++)
	{

		printf("-------------------------------------------------\n%d\n",k);
		offset= memrequest.va[k] & offsetmask;
		printf("ttbr : 0x%08x \n",ttbr);
		printf("va : 0x%08x\n",memrequest.va[k]);
		l1index = (ttbr>>2) + ((memrequest.va[k]&l1mask)>>l1shift);
		printf("L1 index : %08x\n",l1index);
		if((usr[l1index]&0b1)==0)
		{
			printf("make map between l1 and l2\n");
			dequeue(fusrqueue,(void**)&fpf);
			fpf->pid=pcbptr->pid;
			fpf->va=memrequest.va[k]&l1mask;
			enqueue(pcbptr->kqueue,fpf);
			usr[l1index]=fpf->pgfnum;
			l1result=usr[l1index];
			usr[l1index]=usr[l1index]|0b1;
			l2index=(l1result>>2)+((memrequest.va[k]&l2mask)>>l2shift);
		}
		else
		{
			l1result=usr[l1index];
			l2index=(l1result>>2)+((memrequest.va[k]&l2mask)>>l2shift);
		}
		printf("L2 index : %d\n",l2index);
		if((usr[l2index]&0b1)==0)
		{
			printf("make map between l2 and page frame\n");
				dequeue(fusrqueue,(void**)&fpf);
				fpf->pid=pcbptr->pid;
				fpf->va=memrequest.va[k]&(l1mask|l2mask);
				fpf->motherptr=l2index;
				enqueue(pcbptr->uqueue,fpf);
				usr[l2index]=fpf->pgfnum;
				l2result=usr[l2index];
				usr[l2index]=usr[l2index]|0x3;
			printf("L2 index : %d : 0x%08x\n",l2index,l2result);
		}
		else
		{
			usr[l2index]=usr[l2index]|0x2;
			l2result=usr[l2index];
			l2result=l2result&(l1mask|l2mask);
		}
		pa=l2result|offset;
		printf("va : 0x%08x  pa : 0x%08x\n",memrequest.va[k],pa);
		if(memrequest.write[k])
		{
			printf("write data : %d at 0x%08x\n",memrequest.data[k],pa);
			usr[pa/4]=memrequest.data[k];
		}
		else
		{
			if(memrequest.data[k-5]!=usr[pa/4]) printf("wrong\n");
			printf("load data : %d from 0x%08x\n",usr[pa/4],pa);
		}
	}
}


