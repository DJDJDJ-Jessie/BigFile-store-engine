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

	//2.写入文件到主块文件
	std::stringstream tmp_stream;
	tmp_stream<<"."<<largefile::MAINBLOCK_DIR_PREFIX<<block_id;
	tmp_stream>>mainblock_path;
	
	//初始化文件路径和打开方式
	largefile::FileOperation* mainblock = new largefile::FileOperation(mainblock_path,O_RDWR|O_LARGEFILE|O_CREAT);
	
	//写文件
	char buf[4096];
	memset(buf,'6',sizeof(buf));
	
	int32_t data_offset = index_handle->get_block_data_offset();	//拿到未使用数据的偏移位置
	uint32_t file_no = index_handle->block_info()->seq_no_;			//拿到下一个可分配的文件编号
	
	//如果写入失败
	if((ret = mainblock->pwrite_file(buf,sizeof(buf),data_offset))!=largefile::TFS_SUCCESS)
	{
		fprintf(stderr,"write to main block failed.ret:%d,reason:%s\n",ret,strerror(errno));
		mainblock->close_file();
		
		delete mainblock;
		delete index_handle;
		exit(-3);
	}
	

	//3.将主块文件信息写入MetaInfo.成功，则需要更新块内偏移信息和文件哈希索引块MetaInfo
	largefile::MetaInfo meta;
	meta.set_file_id(file_no);		//下一个可分配的文件编号
	meta.set_offset(data_offset);	//设置文件在块内部的偏移量就是第一个可以使用的数据的偏移位置
	meta.set_size(sizeof(buf));		//主块文件大小
	
	//4.MetaInfo写到哈希链表
	ret = index_handle->write_segment_meta(meta.get_key(),meta);
	if(ret == largefile::TFS_SUCCESS)
	{
		//写成功后
		//1.更新索引头部信息
		index_handle->commit_block_data_offset(sizeof(buf));
		
		//2.更新块信息，比如更新文件大小，块数据偏移等
		index_handle->update_block_info(largefile::C_OP_INSERT,sizeof(buf));
		
		//写到磁盘
		ret = index_handle->flush();
		if(ret!=largefile::TFS_SUCCESS)
		{
			fprintf(stderr,"flush mainblock %d failed. file no: %u\n",block_id,file_no);
		}	
	}
	else
	{
		fprintf(stderr,"write_segment_meta mainblock %d failed. file no: %u\n",block_id,file_no);
	}
	
	if(ret != largefile::TFS_SUCCESS)
	{
		fprintf(stderr,"write to mainblock %d failed. file no: %u\n",block_id,file_no);
	}
	else 
	{
		if(debug) printf("write successfully. file no:%u, block_id:%d\n",file_no,block_id);
	}
	
	mainblock->close_file();
	
	delete mainblock;
	delete index_handle;
	
	return 0;
	
}

