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
	std::string mainblock_path;
	std::string index_path;
	int32_t ret = largefile::TFS_SUCCESS;
	
	cout<<"Type your block_id:"<<endl;
	cin >> block_id;
	
	if(block_id<1)
	{
		cerr<<"Invalid blockid,exit."<<endl;
		exit(-1);
	}
	
	//加载索引文件,写入文件到主块文件
	//1.加载索引文件（内部自动进行文件映射）
	largefile::IndexHandle* index_handle = new largefile::IndexHandle(".",block_id);	//索引文件句柄
	
	if(debug) printf("load index...\n");

	ret = index_handle->load(block_id,bucket_size,mmap_option);
	
	if(ret!=largefile::TFS_SUCCESS)
	{
		fprintf(stderr, "load index %d failed.\n", block_id);
		//delete mainblock;			//删除主块指针
		delete index_handle;		//删除索引指针
		exit(-2);
	}

	printf("load successfully:%d\n",block_id);
	delete index_handle;
	
	return 0;
	
}

