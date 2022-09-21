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
	
	//2.删除指定文件的meta info.
	uint64_t file_id = 0;
	cout<<"want to delete file's id:"<<endl;
	cin>>file_id;
	
	//如果blockid不合法
	if(file_id<1)
	{
		cerr<<"invalid file_id,exit."<<endl;
		exit(-3);
	}

	//删除
	ret = index_handle->delete_segment_meta(file_id);	//内部已经对索引信息进行了更新
	if(ret != largefile::TFS_SUCCESS)
	{
		fprintf(stderr,"delete_segment_meta error. file_id:%ld, ret:%d\n",file_id,ret);
		exit(-4);
	}
	
	//3.注意索引文件meta info删除文件，主块文件中不会真的删除，而是先打个标记不管，等到一定条件了再一次性删除
	
	//4.更新到磁盘文件
	ret = index_handle->flush();
	if(ret != largefile::TFS_SUCCESS)
	{
		fprintf(stderr,"flush index error.block_id：%d, file_id:%ld, ret:%d\n",block_id,file_id,ret);
		exit(-5);
	}
	
	printf("delete successfully\n");
	
	delete index_handle;
	
	return 0;
	
}

