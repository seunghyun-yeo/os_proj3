#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "fs.h"

unsigned int memory[0x100000];
partition part1;

void loadprogram()
{
	int i =0;
	FILE *stream;
	unsigned int line;
	size_t read=1;
	stream = fopen("disk.img","rb");
	if(stream ==NULL) exit(EXIT_FAILURE);
	while(read)
	{
		read=fread(&line,4,1,stream);
		//line=(unsigned int)ntohl((uint32_t)line);
		memory[i]=line;
		if(read==0)break;
		i++;
	}
	fclose(stream);
}

int showpartiction()
{
	printf("-----------partition------------\n");
	printf(" partiction_type : 0x%08x\n",part1.s.partition_type);
	printf("      block_size : 0x%08x\n",part1.s.block_size);
	printf("      inode_size : 0x%08x\n",part1.s.inode_size);
	printf("     first_inode : 0x%08x\n",part1.s.first_inode);
	printf("      num_inodes : 0x%08x\n",part1.s.num_inodes);
	printf("num_inodes_block : 0x%08x\n",part1.s.num_inode_blocks);
	printf(" num_free_indoes : 0x%08x\n",part1.s.num_free_inodes);
	printf("      num_blocks : 0x%08x\n",part1.s.num_blocks);
	printf(" num_free_blocks : 0x%08x\n",part1.s.num_free_blocks);
	printf("first_data_block : 0x%08x\n",part1.s.first_data_block);
	printf("     volume_name : %s\n",part1.s.volume_name);
	return part1.s.first_inode;
}
int showinode(int i,int* datablock)
{
	printf("-----------inode----------------\n");

	printf("            mode : 0x%08x\n",part1.inode_table[i].mode);
	printf("          locked : 0x%08x\n",part1.inode_table[i].locked);
	printf("            date : 0x%08x\n",part1.inode_table[i].date);
	printf("            size : 0x%08x\n",part1.inode_table[i].size);
	printf("  indirect_block : 0x%08x\n",part1.inode_table[i].indirect_block);	
	printf("        block[0] : 0x%04x\n",part1.inode_table[i].blocks[0]);
	printf("        block[1] : 0x%04x\n",part1.inode_table[i].blocks[1]);
	printf("        block[2] : 0x%04x\n",part1.inode_table[i].blocks[2]);
	printf("        block[3] : 0x%04x\n",part1.inode_table[i].blocks[3]);
	printf("        block[4] : 0x%04x\n",part1.inode_table[i].blocks[4]);
	printf("        block[5] : 0x%04x\n",part1.inode_table[i].blocks[5]);
	for(int j=0;j<6;j++)
	{
		datablock[j]=part1.inode_table[i].blocks[j];
	}
	int k=part1.inode_table[i].size/1024;
	int j=part1.inode_table[i].size%1024;
	if(j==0) return k;
	else return (k+1);

}

void showdatablock(int i, int* b,int k)
{
	printf("-----------datablock------------\n");
	char* file=(char*)malloc(1024);
	for(int round =0; round<k;round++){
		printf("-----------datablock[%d]---------\n",round);
		memcpy(file,&(part1.data_blocks[part1.inode_table[i].blocks[round]]),1024);
		for(int i=0; i<32; i++)
		{
			printf("           inode : 0x%08x\n",*file);
			printf("      dir_length : 0x%08x\n",*(file+4));
			printf("        name_len : 0x%08x\n",*(file+8));
			printf("       file_type : 0x%08x\n",*(file+12));
			printf("       file_name : %s\n\n",file+16);
			file=file+32;
		}
	}
	//free(file);

}
void ishowdatablock(int i, int* b,int k)
{
	printf("-----------datablock------------\n");
	char* file=(char*)malloc(1024);
	for(int round =0; round<k;round++){
		printf("-----------datablock[%d]---------\n",round);
		memcpy(file,&(part1.data_blocks[part1.inode_table[i].blocks[round]]),1024);
		printf("-------------------data------------------\n");
		printf("%s",file);
	}
	//free(file);

}
int main()
{
	int arm =0;
	int inode;
	int datablock[6];
	int howmanyblock;
	loadprogram();
	memcpy(&part1,&memory[0],0x400000);

	inode=showpartiction();
	howmanyblock=showinode(inode,datablock);
	showdatablock(inode,datablock,howmanyblock);

	howmanyblock=showinode(3,datablock);
	ishowdatablock(3,datablock,howmanyblock);
}

