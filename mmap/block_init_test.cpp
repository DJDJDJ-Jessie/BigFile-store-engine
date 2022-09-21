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
	
	//生成主块文件、生成索引文件并初始化
	
	
	//1.创建索引文件（内部自动进行文件映射）
	largefile::IndexHandle* index_handle = new largefile::IndexHandle(".",block_id);	//索引文件句柄
	
	if(debug) printf("init index...\n");

	ret = index_handle->create(block_id,bucket_size,mmap_option);
	
	if(ret!=largefile::TFS_SUCCESS)
	{
		fprintf(stderr, "create index %d failed.\n", block_id);
		//delete mainblock;			//删除主块指针
		delete index_handle;		//删除索引指针
		exit(-3);
	}

	//2.生成主块文件
	std::stringstream tmp_stream;
	tmp_stream<<"."<<largefile::MAINBLOCK_DIR_PREFIX<<block_id;
	tmp_stream>>mainblock_path;
	
	//初始化文件路径和打开方式
	largefile::FileOperation* mainblock = new largefile::FileOperation(mainblock_path,O_RDWR|O_LARGEFILE|O_CREAT);
	//初始化文件大小
	ret = mainblock->ftruncate_file(main_blocksize);
	if(ret!=0)
	{
		fprintf(stderr, "create main block %s failed. reason:%s\n", mainblock_path.c_str(), strerror(errno));
		delete mainblock;
		index_handle->remove(block_id);	//主块文件如果出错，那么之前生成的索引文件也要删除
		exit(-2);
	}
	
	mainblock->close_file();
	index_handle->flush();
	
	delete mainblock;
	delete index_handle;
	
	return 0;
	
}

