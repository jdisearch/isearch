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
	unsigned int lv = 0;
	std::string name;
	std::string city;
	std::string descr;
	unsigned int salary;

	unsigned int uid1;
	unsigned int age1;
	std::string name1;
	std::string city1;
	std::string descr1;
	unsigned int salary1;

	lv = atoi(argv[1]);
    DTC::init_log("dtc_client");
    DTC::set_log_level(lv);
	DTC::Server stServer; // 只要server不析构，后台会保持长连接
	stServer.IntKey(); // 声明key类型
	stServer.SetTableName("t_dtc_example");//设置dtc的表名，与table.conf中tablename应该一样
	stServer.SetAddress("192.168.214.62", "10901");//设置的dtc的ip和端口
	stServer.SetTimeout(5); // 设置网络超时时间
	stServer.SetAccessKey("0000090184d9cfc2f395ce883a41d7ffc1bbcf4e"); // 设置访问码 AccessToken，在申请dtc实例的时候网站端会生成

	DTC::InsertRequest insertReq(&stServer);
	//retCode = insertReq.SetKey(key);

	uid = atoi(argv[2]);
	name = std::string(argv[3]);
	city = std::string(argv[4]);
	descr = std::string(argv[5]);
	age = atoi(argv[6]);
	salary = atoi(argv[7]);

	insertReq.SetKey(uid);
	//insertReq.Set("key", 100003);
	insertReq.Set("uid", uid);
	insertReq.Set("name", name.c_str());
	insertReq.Set("city", city.c_str());
	insertReq.Set("descr", descr.c_str());
	insertReq.Set("age", age);
	insertReq.Set("salary", salary);

	DTC::Result stResult;
	retCode = insertReq.Execute(stResult);
	printf("retCode:%d, errmsg:%s, errfrom:%s\n", retCode, stResult.ErrorMessage(), stResult.ErrorFrom());
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
		if(retCode == 0)
			retCode = getReq.Need("salary");
		if(retCode != 0)
		{
			printf("get-req set key or need error: %d", retCode);
			fflush(stdout);
			return(-1);
		}

		// execute & get result
		stResult.Reset();
		retCode = getReq.Execute(stResult);
		printf("retCode:%d, errmsg:%s, errfrom:%s\n", retCode, stResult.ErrorMessage(), stResult.ErrorFrom());
		retCode = stResult.FetchRow();//开始获取数据
		printf("uid:%lu\n", stResult.IntValue("uid"));
	    printf("name: %s\n", stResult.StringValue("name"));//输出binary类型的数据
		printf("city: %s\n", stResult.StringValue("city"));
		printf("descr: %s\n", stResult.BinaryValue("descr"));
	    printf("age: %lu\n", stResult.IntValue("age"));//输出int类型的数据
	    printf("salary: %lu\n", stResult.IntValue("salary"));//输出int类型的数据
	}

	printf("-------------first request end -----------------\n");
	printf("-------------second request begin---------------\n");

	DTC::Server stServer1; // 只要server不析构，后台会保持长连接
	stServer1.IntKey(); // 声明key类型
	stServer1.SetTableName("tp1");//设置dtc的表名，与table.conf中tablename应该一样
	stServer1.SetAddress("192.168.214.62", "10201");//设置的dtc的ip和端口
	stServer1.SetTimeout(5); // 设置网络超时时间
	stServer1.SetAccessKey("0000020184d9cfc2f395ce883a41d7ffc1bbcf4e"); // 设置访问码 AccessToken，在申请dtc实例的时候网站端会生成

	DTC::InsertRequest insertReq1(&stServer1);
	//retCode = insertReq.SetKey(key);

	uid1 = atoi(argv[8]);
	name1 = std::string(argv[9]);
	city1 = std::string(argv[10]);
	descr1 = std::string(argv[11]);
	age1 = atoi(argv[12]);
	salary1 = atoi(argv[13]);

	insertReq1.SetKey(uid1);
	//insertReq.Set("key", 100003);
	insertReq1.Set("uid", uid1);
	insertReq1.Set("name", name1.c_str());
	insertReq1.Set("city", city1.c_str());
	insertReq1.Set("descr", descr1.c_str());
	insertReq1.Set("age", age1);
	insertReq1.Set("salary", salary1);

	DTC::Result stResult1;
	retCode = insertReq1.Execute(stResult1);
	printf("retCode:%d, errmsg:%s, errfrom:%s\n", retCode, stResult1.ErrorMessage(), stResult1.ErrorFrom());
	if(retCode == 0)
	{
		DTC::GetRequest getReq1(&stServer1);
		getReq1.SetKey(uid);
		if(retCode == 0)
			retCode = getReq1.Need("uid");//设置需要select的字段，注意第一个key字段不能在这里出现
		if(retCode == 0)
			retCode = getReq1.Need("name");
		if(retCode == 0)
			retCode = getReq1.Need("city");
		if(retCode == 0)
			retCode = getReq1.Need("descr");
		if(retCode == 0)
			retCode = getReq1.Need("age");
		if(retCode == 0)
			retCode = getReq1.Need("salary");
		if(retCode != 0)
		{
			printf("get-req set key or need error: %d", retCode);
			fflush(stdout);
			return(-1);
		}

		// execute & get result
		stResult1.Reset();
		retCode = getReq1.Execute(stResult1);
		printf("retCode:%d, errmsg:%s, errfrom:%s\n", retCode, stResult1.ErrorMessage(), stResult1.ErrorFrom());
		retCode = stResult1.FetchRow();//开始获取数据
		printf("uid:%lu\n", stResult1.IntValue("uid"));
	    printf("name: %s\n", stResult1.StringValue("name"));//输出binary类型的数据
		printf("city: %s\n", stResult1.StringValue("city"));
		printf("descr: %s\n", stResult1.BinaryValue("descr"));
	    printf("age: %lu\n", stResult1.IntValue("age"));//输出int类型的数据
	    printf("salary: %lu\n", stResult1.IntValue("salary"));//输出int类型的数据
	}

	return 0;  
}  
