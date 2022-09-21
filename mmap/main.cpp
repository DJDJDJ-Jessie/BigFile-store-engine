#include"common.h"
#include"mmap_file.h"

using namespace std;
using namespace program;

static const mode_t OPEN_MODE = 0644;	//无符号整数，打开权限参数
static const largefile::MMapSizeOption mmapfile_size_option = {10240000,4096,4096};//内存映射大小参数 10M 4K 4K

int open_file(string file_name,int open_flags)
{
	int fd = open(file_name.c_str(),open_flags,OPEN_MODE);	//打开方式参数open_flags
	if(fd < 0)
	{
		return -errno;	//返回负数
	}
	return fd;
}

int main()
{
	const char* filename = "./mapfile_test.txt";
	//1.打开/创建一个文件，取得文件的句柄open
	int fd = open_file(filename,O_RDWR|O_CREAT|O_LARGEFILE);
	if(fd < 0)
	{
		//这里不用errno的原因：怕被覆盖，比如这里前面还有个read，出错则会重置errno
		fprintf(stderr,"open file failed. filename:%s,error desc:%s\n",filename,strerror(-fd));//负负地正
		return -1;
	}
	puts("创建fd成功");
	//进行内存初始化
	largefile::MMapFile* mmapfile = new largefile::MMapFile(mmapfile_size_option,fd);
	puts("内存初始化成功");
	
	//进行内存映射，设为可写
	bool is_mapped = mmapfile->map_file(true);
	puts("内存映射成功");
	//映射成功
	if(is_mapped)
	{
		puts("准备去分配内存空间");
		//测试重新映射
		mmapfile->remap_file();	 //本来4K变成8K
		//分配空间
		memset(mmapfile->get_data(),'8',mmapfile->get_size());
		//同步内容
		mmapfile->sync_file();
		//解除映射
		mmapfile->munmap_file();
	}
	else
	{
		fprintf(stderr,"map file failed\n");
	}
	puts("成功");
	close(fd);
	
	return 0;
}