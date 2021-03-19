#include<stdio.h>
#include<stdlib.h>
#include<string>
#include<stdint.h>
#include<unistd.h>

#include "dtcapi.h"

int main(int argc,char* argv[])
{
	int retCode = 0;
	unsigned int uid;

	DTC::Server stServer; // 只要server不析构，后台会保持长连接
	stServer.IntKey(); // 声明key类型
	stServer.SetTableName("t_dtc_example");//设置dtc的表名，与table.conf中tablename应该一样
	stServer.SetAddress("192.168.214.62", "10009");//设置的dtc的ip和端口
	stServer.SetTimeout(5); // 设置网络超时时间
	stServer.SetAccessKey("0000010284d9cfc2f395ce883a41d7ffc1bbcf4e"); // 设置访问码 AccessToken，在申请dtc实例的时候网站端会生成

	DTC::DeleteRequest deleteReq(&stServer);
	uid = atoi(argv[1]);
	deleteReq.SetKey(uid);

	DTC::Result stResult;
	retCode = deleteReq.Execute(stResult);
	printf("retCode:%d\n", retCode);
	if(retCode == 0)
	{
		printf("delete success!\n");
	}

	return 0;  
}  
