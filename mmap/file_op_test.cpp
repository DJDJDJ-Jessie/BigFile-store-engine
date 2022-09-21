#include "file_op.h"
#include "common.h"

using namespace std;
using namespace program;

int main()
{
	const char* filename = "file_op.txt\n";
	largefile::FileOperation* fileOP = new largefile::FileOperation(filename,O_CREAT|O_RDWR|O_LARGEFILE);
	
	printf("ready to open file");
	int fd = fileOP->open_file();
	if(fd < 0)
	{
		fprintf(stderr,"open file failed. filename:%s,error desc:%s\n",filename,strerror(-fd));//负负得正
		exit(-1);
	}
	
	printf("ready to pwrite_file,write 64bytes at 1024 position\n");
	char buf[65];
	memset(buf,'6',64);
	int ret = fileOP->pwrite_file(buf,64,1024);
	if(ret < 0)
	{
		if(ret == largefile::EXIT_DISK_OPEN_INCOMPLETE)
		{
			fprintf(stderr,"read or write length is less than required\n");
		}
		else
		{
			fprintf(stderr,"pwrite file %s failed. error desc:%s\n",filename,strerror(-ret));
		}
	}
	
	printf("ready to pread_file,read 64bytes at 1024 position\n");
	memset(buf,0,64);
	ret = fileOP->pread_file(buf,64,1024);
	if(ret < 0)
	{
		if(ret == largefile::EXIT_DISK_OPEN_INCOMPLETE)
		{
			fprintf(stderr,"read or write length is less than required\n");
		}
		else
		{
			fprintf(stderr,"pread file %s failed. error desc:%s\n",filename,strerror(-ret));
		}
	}
	else
	{
		buf[64] = '\0';
		printf("read:%s\n",buf);
	}
	
	
	printf("ready to write_file,write 64bytes in head\n");
	memset(buf,'9',64);
	ret = fileOP->write_file(buf,64);	//覆盖
	if(ret < 0)
	{
		if(ret == largefile::EXIT_DISK_OPEN_INCOMPLETE)
		{
			fprintf(stderr,"read or write length is less than required\n");
		}
		else
		{
			fprintf(stderr,"write_file %s failed. error desc:%s\n",filename,strerror(-ret));
		}		
	}
	
	
	printf("close_file\n");
	fileOP->close_file();
	
	return 0;
}
