#include<stdio.h>
#include<stdlib.h>
#include<string>
#include<stdint.h>
#include<unistd.h>

#include "dtcapi.h"

using namespace DTC;

int main(int argc,char* argv[])
{
	int ret = 0;
	int uid = 0;
	int age = 0;
	int salary = 0;
	std::string name;
	std::string city;
	std::string descr;

	uid = atoi(argv[1]);
	name = std::string(argv[2]);
	city = std::string(argv[3]);
	descr = std::string(argv[4]);
	age = atoi(argv[5]);
	salary = atoi(argv[6]);

	ServerPool svp(5, 5);
	Server sv1, sv2, sv3, sv4, sv5;

	/* server-request-1 */
	sv1.IntKey();
	sv1.SetTableName("tp1");
	sv1.SetAddress("192.168.214.62", "10201");
	sv1.SetAccessKey("0000020184d9cfc2f395ce883a41d7ffc1bbcf4e");
	sv1.SetTimeout(5);

	InsertRequest req1(&sv1);
	req1.SetKey(uid);
	//req1.Set("uid", 100001);
	req1.Set("name", name.c_str());
	req1.Set("city", city.c_str());
	req1.Set("descr", descr.c_str());
	req1.Set("age", age);
	req1.Set("salary", salary);

	/* server-request-2 */
	sv2.IntKey();
	sv2.SetTableName("tp2");
	sv2.SetAddress("192.168.214.62", "10202");
	sv2.SetAccessKey("0000020284d9cfc2f395ce883a41d7ffc1bbcf4e");
	sv2.SetTimeout(5);

	InsertRequest req2(&sv2);
	req2.SetKey(uid);
	//req2.Set("uid", 100001);
	req2.Set("name", name.c_str());
	req2.Set("city", city.c_str());
	req2.Set("descr", descr.c_str());
	req2.Set("age", age);
	req2.Set("salary", salary);
	/* server-request-3 */
	sv3.IntKey();
	sv3.SetTableName("tp3");
	sv3.SetAddress("192.168.214.62", "10203");
	sv3.SetAccessKey("0000020384d9cfc2f395ce883a41d7ffc1bbcf4e");
	sv3.SetTimeout(5);

	InsertRequest req3(&sv3);
	req3.SetKey(uid);
	//req2.Set("uid", 100001);
	req3.Set("name", name.c_str());
	req3.Set("city", city.c_str());
	req3.Set("descr", descr.c_str());
	req3.Set("age", age);
	req3.Set("salary", salary);
	/* server-request-4 */
	sv4.IntKey();
	sv4.SetTableName("tp4");
	sv4.SetAddress("192.168.214.62", "10204");
	sv4.SetAccessKey("0000020484d9cfc2f395ce883a41d7ffc1bbcf4e");
	sv4.SetTimeout(5);

	InsertRequest req4(&sv4);
	req4.SetKey(uid);
	//req4.Set("uid", 100001);
	req4.Set("name", name.c_str());
	req4.Set("city", city.c_str());
	req4.Set("descr", descr.c_str());
	req4.Set("age", age);
	req4.Set("salary", salary);
	/* server-request-5 */
	sv5.IntKey();
	sv5.SetTableName("tp5");
	sv5.SetAddress("192.168.214.62", "10205");
	sv5.SetAccessKey("0000020584d9cfc2f395ce883a41d7ffc1bbcf4e");
	sv5.SetTimeout(5);

	InsertRequest req5(&sv5);
	req5.SetKey(uid);
	//req5.Set("uid", 100001);
	req5.Set("name", name.c_str());
	req5.Set("city", city.c_str());
	req5.Set("descr", descr.c_str());
	req5.Set("age", age);
	req5.Set("salary", salary);

	svp.AddRequest(&req1, 1);
	svp.AddRequest(&req2, 2);
	svp.AddRequest(&req3, 3);
	svp.AddRequest(&req4, 4);
	svp.AddRequest(&req5, 5);

	ret = svp.Execute(120000);
	printf("ret:%d\n", ret);

	return 0;
}
