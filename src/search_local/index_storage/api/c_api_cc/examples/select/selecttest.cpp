#include<stdio.h>
#include<stdlib.h>
#include<string>
#include<stdint.h>
#include<unistd.h>
 
#include "dtcapi.h"

int main(int argc,char* argv[])
{
	int iRet;
	unsigned int uid;
	DTC::Server stServer; // 只要server不析构，后台会保持长连接
	stServer.IntKey(); // 声明key类型
	stServer.SetTableName("t_dtc_example");//设置dtc的表名，与table.conf中tablename应该一样
	stServer.SetAddress("192.168.214.62", "10109");//设置的dtc的ip和端口
	stServer.SetTimeout(5); // 设置网络超时时间
	stServer.SetAccessKey("0000010284d9cfc2f395ce883a41d7ffc1bbcf4e"); // 设置访问码 AccessToken，在申请dtc实例的时候网站端会生成
	//下面三行代码是Get数据
	unsigned int  age;
	std::string name;
	std::string city;
	std::string descr;
 
    DTC::GetRequest stGetReq(&stServer);
    uid = atoi(argv[1]);
    iRet = stGetReq.SetKey(uid);
 
	if(iRet == 0)
		iRet = stGetReq.Need("uid");//设置需要select的字段，注意第一个key字段不能在这里出现
    if(iRet == 0)
    	iRet = stGetReq.Need("name");
    if(iRet == 0)
    	iRet = stGetReq.Need("city");
    if(iRet == 0)
    	iRet = stGetReq.Need("descr");
    if(iRet == 0)
    	iRet = stGetReq.Need("age");
    if(iRet == 0)
    	iRet = stGetReq.GT("age", 24);
		//iRet = stGetReq.OR("age", 27);

    if(iRet != 0)
    { 
		printf("get-req set key or need error: %d", iRet);
		fflush(stdout); 
		return(-1); 
    } 
 
    // execute & get result
    DTC::Result stResult; 
    iRet = stGetReq.Execute(stResult);
    if(iRet != 0)
	//如果出错,则输出错误码、错误阶段，错误信息，stResult.ErrorFrom(), stResult.ErrorMessage() 这两个错误信息很重要，一定要打印出来，方便定位问题
    { 
		printf ("uin[%u] dtc execute get error: %d, error_from:%s, msg:%s\n",
			uid,//出错的key是多少
			iRet,//错误码为多少
			stResult.ErrorFrom(),//返回错误阶段
			stResult.ErrorMessage()//返回错误信息
			);
		fflush(stdout);
		return(-2); 
    } 
 
    if(stResult.NumRows() <= 0)
	{ 
		// 数据不存在
		printf("uin[%u] data not exist.\n", uid);
		return(0); 
    } 
 
	printf("nubrow:%d\n", stResult.NumRows());
	for(int i=0;i<=stResult.NumRows();++i)
	{
		iRet = stResult.FetchRow();//开始获取数据
		if(iRet < 0)
		{
			printf ("uid[%lu] dtc fetch row error: %d\n", uid, iRet);
			fflush(stdout);
			return(-3);
		}
		//如果一切正确，则可以输出数据了
		printf("uid: %d\n", stResult.IntValue("uid"));//输出int类型的数据
		printf("name: %s\n", stResult.StringValue("name"));//输出string类型的数据
		printf("city: %s\n", stResult.StringValue("city"));//输出string类型的数据
		printf("descr: %s\n", stResult.BinaryValue("descr"));//输出binary类型的数据
		printf("age:%d\n",stResult.IntValue("age"));
	}
    return(0);
}  
