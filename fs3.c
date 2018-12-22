
#include "2level_var.h"

partition part1;
char * dentry;
char * ddentry;
int child_global_file =1;

int showpartiction()
{
	return part1.s.first_inode;
}

int showinode(int i,int* datablock)
{
	for(int j=0;j<6;j++)
	{
		datablock[j]=part1.inode_table[i].blocks[j];
	}
	if(part1.inode_table[i].mode==0x20777)
	{
		dentry=malloc(part1.inode_table[i].size);
		ddentry=dentry;
	}
	int k=part1.inode_table[i].size/1024;
	int j=part1.inode_table[i].size%1024;
	if(j==0) return k;
	else return (k+1);

}

void showdatablock(int i, int* b,int k)
{
	char* file=(char*)malloc(1024);
	char* dfile = file;
	for(int round =0; round<k;round++){
		memcpy(file,&(part1.data_blocks[part1.inode_table[i].blocks[round]]),1024);
		for(int i=0; i<32; i++)
		{
			memcpy(dentry,file,4);
			memcpy(dentry+4,file+4,4);
			memcpy(dentry+8,file+8,4);
			memcpy(dentry+12,file+12,4);
			memcpy(dentry+16,file+16,16);
			file=file+32;
			dentry=dentry+32;
		}
		file=dfile;
	}

}
void lsdentry()
{
	printf("ls\n");
	dentry=ddentry;
	for(int i=0; i<0xcc0; i=i+32)
	{
		printf("%s   ",dentry+16+i);
	}
	dentry=ddentry;
	printf("\n");
}

void ishowdatablock(int i, int* b,int k,int pa,int size, int nmemb, int offset)
{
	for(int round =0; round<k;round++)
	{
		memcpy(&usr[pa],&(part1.data_blocks[part1.inode_table[i].blocks[round]]),1024);
		printf("-------------------data------------------\n");
		printf("%s",&usr[pa]);
		pa=pa+(1024/4);
	}

}

int mount()
{
	int inode;
	int datablock[6];
	int howmanyblock;

	inode=showpartiction();
	howmanyblock=showinode(inode,datablock);
	showdatablock(inode,datablock,howmanyblock);
	printf("MOUNTING DONE\n");
}

int makepartition()
{
	unsigned int memory[0x100000];
	int i =0;
	FILE *stream;
	unsigned int line;
	size_t read=1;
	stream = fopen("disk.img","rb");
	if(stream ==NULL)
	{
		exit(EXIT_FAILURE);
	}
	while(read)
	{
		read=fread(&line,4,1,stream);
		memory[i]=line;
		if(read==0)break;
		i++;
	}
	fclose(stream);
	memcpy(&part1,&memory[0],0x400000);

}

typedef struct {
	long mtype;
	pid_t pid;
	char path[16];
	char mode[16];
	int inodenum;
}fopenmsg;

typedef struct{
	long mtype;
	pid_t pid;
	int inodenum;
	int ptr;
	int size;
	int nmemb;
	int offset;
}freadmsg;

typedef struct{
	long mtype;
	pid_t pid;
	int inodenum;
}fclosemsg;

fopenmsg msgfopen;
freadmsg msgfread;
fclosemsg msgfclose;

#define fopenrequest 3
#define freadrequest 4
#define fcloserequest 5

int myfopen(char* path, char* mode)
{
	memset(&msgfopen,0,sizeof(fopenmsg));
	msgfopen.mtype=fopenrequest;
	msgfopen.pid = getpid();
	memcpy(msgfopen.path,path,16);
	memcpy(msgfopen.mode,mode,16);
	ret=msgsnd(msgq,&msgfopen,sizeof(msgfopen),NULL);
}
int myfread(int ptr, int size, int nmemb,int stream)
{
	memset(&msgfread,0,sizeof(freadmsg));
	msgfread.mtype=freadrequest;
	msgfread.pid = pid;
	msgfread.inodenum = stream;
	msgfread.ptr = ptr;
	msgfread.size = size;
	msgfread.nmemb = nmemb;
	ret=msgsnd(msgq,&msgfread,sizeof(msgfread),NULL);
}
int myfclose(int* stream)
{
	free(stream);
}

int myfopen_handler(pid_t lpid, char* path)
{
	bool find =0;
	int i=0;
	dentry=ddentry;
	for(i=0;i<0xcc0;i=i+32)
	{
		if((strncmp(path,(dentry+i+16),strlen(path)))==0)
		{
			find=1;
			break;
		}
	}
	if(find)
	{
		msgfopen.mtype = lpid;
		msgfopen.pid = lpid;
		msgfopen.inodenum = dentry[i];
		ret=msgsnd(msgq,&msgfopen,sizeof(fopenmsg),NULL);
	}
	else
	{
		printf("NO_SUCH_FILE\n");
		overthanos();
		exit(EXIT_FAILURE);
	}
}
int checkmapping(int ptr,int tpid,pcb** pcbloc ,int* pa,int* al1index,int* al2index)
{
	queuenode * ppre=NULL;
	queuenode * ploc=NULL;
	pcb * pcbptr;
	pgfinfo* fpf2;
	for(ppre=NULL,ploc=rqueue->front;ploc!=NULL;ppre=ploc,ploc=ploc->next)
	{
		pcbptr=ploc->dataptr;
		if(pcbptr->pid==tpid)
		{
			*pcbloc=pcbptr;
			break;
		}
	}
	ttbr = pcbptr->ttbr;
	offset= ptr & offsetmask;
	l1index = (ttbr>>2) + ((ptr&l1mask)>>l1shift);
	*al1index=l1index;
	if((usr[l1index]&0b1)==0)
	{
		return 1;
	}
	else
	{
		l1result=usr[l1index];
		l2index=(l1result>>2)+((ptr&l2mask)>>l2shift);
		*al2index=l2index;
	}
	if((usr[l2index]&0b1)==0)
	{
		return 2;
	}
	else
	{
		usr[l2index]=usr[l2index]|0x2;
		l2result=usr[l2index];
		l2result=l2result&(l1mask|l2mask);
	}
	*pa=l2result|offset;
	return 0;
}

void l2mappmem(int ptr,int pid,pcb* pcbloc,int* pa,int al2index)
{
	pcb* lpcbptr=pcbloc;
	dequeue(fusrqueue,(void**)&fpf);
	fpf->pid=lpcbptr->pid;
	fpf->va=ptr&(l1mask|l2mask);
	fpf->motherptr=al2index;
	enqueue(lpcbptr->uqueue,fpf);
	usr[l2index]=fpf->pgfnum;
	l2result=usr[l2index];
	usr[l2index]=usr[l2index]|0x3;
	*pa = l2result|(ptr&offsetmask);
}

void l1mapl2(int ptr,int pid, pcb* pcbloc,int* pa, int al1index)
{
	pcb* lpcbptr=pcbloc;
	dequeue(fusrqueue,(void**)&fpf);
	fpf->pid=lpcbptr->pid;
	fpf->va=ptr&l1mask;
	enqueue(lpcbptr->kqueue,fpf);
	usr[l1index]=fpf->pgfnum;
	l1result=usr[l1index];
	usr[l1index]=usr[l1index]|0b1;
	l2index=(l1result>>2)+((ptr&l2mask)>>l2shift);

	l2mappmem(ptr, pid, pcbloc, pa, l2index);
}

int myfread_handler()
{
	int inodenum=msgfread.inodenum;
	int ptr = msgfread.ptr;
	int size = msgfread.size;
	int nmemb = msgfread.nmemb;
	int offset = msgfread.offset;
	int ldatablock[6];
	int lhowmanyblock;
	int pa=0;
	int al1index=0;
	int al2index=0;
	int mappingcheck=0;
	pcb* pcbloc=0;

	lhowmanyblock=showinode(inodenum,ldatablock);
	if((mappingcheck=checkmapping(ptr,msgfread.pid,&pcbloc,&pa,&al1index,&al2index))!=0)
	{
		if(mappingcheck==1)
		{
			l1mapl2(ptr,pid,pcbloc,&pa,al1index);
		}
		else if(mappingcheck==2)
		{
			l2mappmem(ptr,pid,pcbloc,&pa,al2index);
		}
	}
	ishowdatablock(inodenum,ldatablock,lhowmanyblock,pa,size,nmemb,offset);
	msgfread.mtype=msgfread.pid;
	ret=msgsnd(msgq,&msgfread,sizeof(msgfread),NULL);
}
int myfclose_handler()
{
}

int main()
{
	memset(&new_sa, 0, sizeof(new_sa));
	new_sa.sa_handler = &signal_handler;
	sigaction(SIGALRM, &new_sa, &old_sa);

	rqueue = createqueue();
	ioqueue = createqueue();
	hdd = createqueue();
	makepartition();
	mount();
	lsdentry();
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
			pid=getpid();
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
		ret=msgrcv(msgq, &msgfopen,sizeof(fopenmsg),fopenrequest,IPC_NOWAIT);
		if(ret != -1)
		{
			flag=1;
			myfopen_handler(msgfopen.pid,msgfopen.path);
		}
		ret=msgrcv(msgq, &msgfread,sizeof(freadmsg),freadrequest,IPC_NOWAIT);
		if(ret != -1)
		{
			flag=1;
			myfread_handler();
		}
		//for fclose
		gettimeofday(&end,NULL);
		operatingtime=(end.tv_usec-start.tv_usec);
		if(highest<operatingtime)highest=operatingtime;
	}
	return 0;
}


void child_handler(int signo)
{
	char *filename=(char*)malloc(9);
	sprintf(filename,"file_%d",child_global_file);
	child_global_file++;
	myfopen(filename,"rb");
	ret=msgrcv(msgq,&msgfopen,sizeof(fopenmsg),pid,NULL);
	int * stream = (int*)malloc(sizeof (int));
	*stream = msgfopen.inodenum;
	myfread(0x00000400,1024,1,*stream);
	ret=msgrcv(msgq,&msgfread,sizeof(freadmsg),pid,NULL);
	myfclose(stream);
	free(filename);
}

