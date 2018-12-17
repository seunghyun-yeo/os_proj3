
#include "2level_var.h"


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

