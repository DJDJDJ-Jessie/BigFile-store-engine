#include"mmap_file_op.h"
#include"common.h"

using namespace std;
using namespace program;

const static largefile::MMapSizeOption mmap_size_option = {10240000,4096,4096};

int main()
{
	int ret=0;
	const char* filename = "mmap_file_op.txt";
	//largefile::MMapFileOperation* mmfo = new largefile::MMapFileOperation(filename);
	largefile::MMapFileOperation mmfo(filename);
	
	printf("ready to open file\n");
	int fd = mmfo.open_file();
	if(fd < 0)
	{
		fprintf(stderr,"open file failed. filename:%s,error desc:%s\n",filename,strerror(-fd));//负负得正
		exit(-1);
	}
	
	printf("ready to mmap_file\n");
	ret = mmfo.mmap_file(mmap_size_option);
	if(ret == largefile::TFS_ERROR)
	{
		fprintf(stderr,"mmap_file failed. reason:%s\n",strerror(errno));
		mmfo.close_file();
		exit(-2);
	}
	
	printf("ready to pwrite_file\n");
	char buf[128+1];
	memset(buf,'6',128);
	ret = mmfo.pwrite_file(buf,128,8);
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
	
	printf("ready to pread_file,read 128bytes,offset 8bytes\n");
	memset(buf,0,128);
	ret = mmfo.pread_file(buf,128,8);
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
		buf[128] = '\0';
		printf("read:%s\n",buf);
	}
	
	printf("ready to flush_file\n");
	ret = mmfo.flush_file();
	if(ret ==  largefile::TFS_ERROR)
	{
		fprintf(stderr,"flush file failed.reason:%s\n",strerror(errno));
	}
	
	mmfo.munmap_file();
	
	mmfo.close_file();
	
	return 0;
}