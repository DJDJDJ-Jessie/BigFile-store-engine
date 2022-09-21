#include"common.h"
#include"file_op.h"
#include"index_handle.h"
#include<sstream>

using namespace program;
using namespace std;

//内存映射参数
const static largefile::MMapSizeOption mmap_option = {1024000,4096,4096};	//索引文件1M、4K、4K
const static uint32_t main_blocksize = 1024*1024*64;	//主块文件的大小64M
const static uint32_t bucket_size = 1000;	//哈希桶的大小
static int32_t block_id = 1;	//默认块id为1

static int debug = 1;

int main(int argc, char** argv)
{
	std::string mainblock_path;	//主块文件路径
	std::string index_path;		//索引文件路径
	int32_t ret = largefile::TFS_SUCCESS;
	
	cout<<"Type block id:"<<endl;
	cin>>block_id;
	
	//如果blockid不合法
	if(block_id<1)
	{
		cerr<<"invalid block_id,exit."<<endl;
		exit(-1);
	}
	//1.如果合法，那么就初始化索引操作对象，准备载入索引文件
	largefile::IndexHandle* index_handle = new largefile::IndexHandle(".",block_id);
	if(debug) printf("load index...\n");
	
	ret = index_handle->load(block_id,bucket_size,mmap_option);
	if(ret!=largefile::TFS_SUCCESS)
	{
		fprintf(stderr, "load index %d failed.\n", block_id);
		delete index_handle;		//删除索引指针
		exit(-2);
	}
	
	//2.读取文件的meta info.
	uint64_t file_id = 0;
	cout<<"Type file id:"<<endl;
	cin>>file_id;
	
	//如果blockid不合法
	if(file_id<1)
	{
		cerr<<"invalid file_id,exit."<<endl;
		exit(-3);
	}

	largefile::MetaInfo meta;
	ret = index_handle->read_segment_meta(file_id,meta);
	if(ret != largefile::TFS_SUCCESS)
	{
		fprintf(stderr,"read_segment_meta error. file_id:%ld, ret:%d\n",file_id,ret);
		exit(-4);
	}
	
	//3.根据meta info读取文件，首先要得到主块文件才行
	std::stringstream tmp_stream;
	tmp_stream<<"."<<largefile::MAINBLOCK_DIR_PREFIX<<block_id;
	tmp_stream>>mainblock_path;
	
	//打开主块文件，进行读
	largefile::FileOperation* mainblock = new largefile::FileOperation(mainblock_path,O_RDWR);
	//char buffer[4096];
	char* buffer = new char[meta.get_size()+1];	//使用动态内存分配(这里其实可以做一下处理，比如太大了，就每次读4096，直到被读完)
	ret = mainblock->pread_file(buffer,meta.get_size(),meta.get_offset());
	if(ret != largefile::TFS_SUCCESS)
	{
		fprintf(stderr,"read from main block failed. ret:%d, reason:%s\n",ret,strerror(errno));
		mainblock->close_file();
		
		delete mainblock;
		delete index_handle;
		exit(-5);
	}
	
	//4.打印输出结果
	buffer[meta.get_size()] = '\0';
	printf("read:%s, read size:%d\n",buffer,meta.get_size());
	
	mainblock->close_file();	
	delete mainblock;
	delete index_handle;
	
	return 0;
	
}

