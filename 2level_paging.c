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
#define maxproc 10
#define maxcpuburst 4
#define gendtik 10000
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
#include "2level_swap_var.h"
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

int main()
{
	memset(&new_sa, 0, sizeof(new_sa));
	new_sa.sa_handler = &signal_handler;
	sigaction(SIGALRM, &new_sa, &old_sa);

	rqueue = createqueue();
	ioqueue = createqueue();
	hdd = createqueue();

	usr = (int*)malloc(sizeof(int)*pmemsize);
	for(int j =0; j<pmemsize;j++)
	{
		usr[j]=0;
	}

	fusrqueue=createqueue();

	for(int j=0; j<pmemsize;j=j+1024)
	{
		fpf=(pgfinfo*)malloc(sizeof(pgfinfo));
		fpf->pgfnum=j<<2;
		fpf->pid =-1;
		fpf->va = -1;
		enqueue(fusrqueue,fpf);
	}

	printf("total queue count : %d\n",fusrqueue->count);

	for(int i=0; i<maxproc; i++)
	{
		pid=fork();
		if(pid==0)
		{
			key=0x142735;
			msgq = msgget(key,IPC_CREAT|0666);
			srand(getpid());
			dcpu_time = rand()%6;
			dio_time =7;//rand()%6;
			cpu_time=dcpu_time;
			msg.mtype=1;
			msg.pid=getpid();
			msg.io_time=dio_time;
			new_sa.sa_handler = &child_handler;
			sigaction(SIGUSR1,&new_sa,&old_sa);
			while(1);
		}
		else if(pid>0)
		{
			gpid[i]=pid;
			pcbdata = (pcb*)malloc(sizeof(pcb));
			pcbdata->pid=pid;
			pcbdata->io_time=-1;
			pcbdata->cpu_time=0;
			pcbdata->kqueue=createqueue();
			pcbdata->uqueue=createqueue();
			dequeue(fusrqueue,(void**)&fpf);
			fpf->pid=pcbdata->pid;
			pcbdata->ttbr=fpf->pgfnum;
			pcbdata->tq=time_quantum;
			enqueue(rqueue,pcbdata);
			enqueue(pcbdata->kqueue,fpf);
		}
		else printf("fork error\n");
	}

	tiktok(sec,msec);
	key=0x142735;
	msgq = msgget(key,IPC_CREAT|0666);

	while (1)
	{
		bool flag=0;
		gettimeofday(&start,NULL);
		ret=msgrcv(msgq, &msg, sizeof(msgbuf),1,IPC_NOWAIT);
		if(ret != -1)
		{
			flag=1;
			mymovqueue(rqueue,ioqueue,msg.pid,msg.io_time);
		}

		ret=msgrcv(msgq, &memrequest,sizeof(msgbuf2),2,IPC_NOWAIT);
		if(ret != -1)
		{
			flag=1;
			memrequest_handler();
		}
		gettimeofday(&end,NULL);
		operatingtime=(end.tv_usec-start.tv_usec);
		if(highest<operatingtime)highest=operatingtime;
		if(flag)printf("main execution time : %f\n",highest);
	}
	return 0;
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


void child_handler(int signo)
{
	cpu_time--;
	memrequest.mtype=2;
	memrequest.pid = getpid();
	for(int k=0; k<5;k++)
	{
		//		printf("%d",k);
		memrequest.va[k]=virtualaddress;
		virtualaddress+=4096;
		memrequest.va[k]=memrequest.va[k]&0xfffffffc;
		memrequest.va[k+5]=memrequest.va[k];
		memrequest.write[k]=1;
		memrequest.write[k+5]=0;
		memrequest.data[k]=rand();
	}

	ret=msgsnd(msgq,&memrequest,sizeof(memrequest),NULL);
	//	printf("msgsnd done");
	if(cpu_time<1)
	{
		printf("wait request");
		msg.io_time=dio_time;
		ret = msgsnd(msgq, &msg,sizeof(msg),NULL);
		cpu_time=dcpu_time;

		return;
	}

}

void adjustqueue(queue * queue, int va)
{
	queuenode* ppre;
	queuenode* ploc;
	pgfinfo * fpf;
	if(queue->count==0) return;
	for(ppre=NULL, ploc=queue->front;ploc!=NULL;ppre=ploc,ploc=ploc->next)
	{
		fpf=ploc->dataptr;
		if(fpf->va==va)
		{
			break;
		}
	}
	if(ploc==queue->rear) return;
	if(ppre!=NULL)
	{
		printf("in when ppre is not NULL\n");
		ppre->next=ploc->next;
		ploc->next=NULL;
		if(queue->rear==NULL)printf("err\n");
		queue->rear->next=ploc;
		queue->rear=ploc;
	}
	else
	{
		printf("in when ppre is NULL\n");
		queue->front=ploc->next;
		queue->rear->next=ploc;
		ploc->next=NULL;
		queue->rear=ploc;
	}
	printf("adjustqueue end");
}

