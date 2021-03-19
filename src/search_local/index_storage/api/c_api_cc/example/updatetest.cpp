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
	unsigned int age;
	std::string name;
	std::string city;
	std::string descr;

	DTC::Server stServer; // 只要server不析构，后台会保持长连接
	stServer.IntKey(); // 声明key类型
	stServer.SetTableName("t_dtc_example");//设置dtc的表名，与table.conf中tablename应该一样
	stServer.SetAddress("192.168.214.62", "10009");//设置的dtc的ip和端口
	stServer.SetTimeout(5); // 设置网络超时时间
	stServer.SetAccessKey("0000010284d9cfc2f395ce883a41d7ffc1bbcf4e"); // 设置访问码 AccessToken，在申请dtc实例的时候网站端会生成
	
	uid = atoi(argv[1]);
	name = std::string(argv[2]);
	city = std::string(argv[3]);
	descr = std::string(argv[4]);
	age = atoi(argv[5]);
	DTC::UpdateRequest UpdateReq(&stServer);
	retCode = UpdateReq.SetKey(uid);
	if(retCode != 0)
	{
		printf("update-req set key error: %d", retCode);
		fflush(stdout);
		return(-1);
	}
	retCode = UpdateReq.Set("name", name.c_str());
	retCode = UpdateReq.Set("city", city.c_str());
	retCode = UpdateReq.Set("descr", descr.c_str());
	retCode = UpdateReq.Set("age", age);
	if(retCode != 0)
	{
		printf("update-req set field error: %d", retCode);
		fflush(stdout);
		return(-1);
	}

	// execute & get result
	DTC::Result stResult;
	retCode = UpdateReq.Execute(stResult);
	printf("retCode:%d\n", retCode);
	if(retCode == 0)
	{
		DTC::GetRequest getReq(&stServer);
		getReq.SetKey(uid);
		if(retCode == 0)
			retCode = getReq.Need("uid");//设置需要select的字段，注意第一个key字段不能在这里出现
		if(retCode == 0)
			retCode = getReq.Need("name");
		if(retCode == 0)
			retCode = getReq.Need("city");
		if(retCode == 0)
			retCode = getReq.Need("descr");
		if(retCode == 0)
			retCode = getReq.Need("age");
		if(retCode != 0)
		{
			printf("get-req set key or need error: %d", retCode);
			fflush(stdout);
			return(-1);
		}

		// execute & get result
		retCode = getReq.Execute(stResult);
		retCode = stResult.FetchRow();//开始获取数据
		printf("uid:%d\n", stResult.IntValue("uid"));
	    printf("name: %s\n", stResult.StringValue("name"));//输出binary类型的数据
		printf("city: %s\n", stResult.StringValue("city"));
		printf("descr: %s\n", stResult.BinaryValue("descr"));
	    printf("age: %d\n", stResult.IntValue("age"));//输出int类型的数据
	}
	return 0;
}  
