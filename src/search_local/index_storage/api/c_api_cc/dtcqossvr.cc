#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>  
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>

#include "protocol.h"

#include "dtcapi.h"
#include "dtcint.h"
#include "dtcqossvr.h"

using namespace DTC;

/* linux-2.6.38.8/include/linux/compiler.h */
//# define likely(x)	__builtin_expect(!!(x), 1)
//# define unlikely(x)	__builtin_expect(!!(x), 0)

#define random(x) (rand()%x)

const uint32_t ACCESS_KEY_LEN = 40;

DTCQosServer::DTCQosServer():m_Status(0), m_Weight(0), m_RemoveTimeStamp(0), 
								m_LastRemoveTime(0), m_MaxTimeStamp(0), m_Server(NULL)
{
}

DTCQosServer::~DTCQosServer()
{
	if (this->m_Server != NULL){
		delete this->m_Server;
		this->m_Server = NULL;
	}
} 

DTCServers::DTCServers()
	:m_TimeOut(50), m_AgentTime(0), m_KeyType(0), m_TableName(NULL),
	m_SetRoute(false), m_ConstructedBySetIPs(false), m_Bid(0),
	m_IDCNo(0), m_BucketsPos(0), m_BalanceBucketSize(0), m_BidVersion(0),
	m_LastGetCaTime(0), m_RefreshBucketsTime(0), m_RemoveBucketsTime(0),
	m_LoadBalanceBuckets(NULL), m_QOSSevers(NULL)
{
}

DTCServers::~DTCServers()
{
	if(this->m_TableName)
	{
		free(this->m_TableName);
		this->m_TableName = NULL;
	}

	if(this->m_LoadBalanceBuckets)
	{
		free(this->m_LoadBalanceBuckets);
		this->m_LoadBalanceBuckets = NULL;
	}

	if(this->m_QOSSevers)
	{
		delete[] this->m_QOSSevers;
		this->m_QOSSevers = NULL;
	}
}

void PrintIPNode(ROUTE_NODE RouteNode)
{
	printf("\n");
	printf("\t bid = %d \n", RouteNode.bid);
	printf("\t ip = %s \n", RouteNode.ip);
	printf("\t port = %d \n", RouteNode.port);
	printf("\t weight = %d \n", RouteNode.weight);
	printf("\t status = %d \n", RouteNode.status);
	printf("\n");
}

void DTCServers::SetErrorMsg(int err, std::string from, std::string msg)
{
	char buffer[1000];
	memset(buffer, 0, 1000);
	sprintf(buffer, "ERROR CODE %d, FROM %s ERRMSG %s", err, from.c_str(), msg.c_str());
	std::string ErrMsg(buffer);
	this->m_ErrMsg.clear();
	this->m_ErrMsg = ErrMsg;
}

std::string DTCServers::GetErrorMsg()
{
	return this->m_ErrMsg;
}

int InvalidIpaddr(char *str)
{
   if(str == NULL || *str == '\0')
      return 1;

   union
   {
      struct sockaddr addr;
      struct sockaddr_in6 addr6;
      struct sockaddr_in addr4;
   } a;
   memset(&a, 0, sizeof(a));
   if(1 == inet_pton(AF_INET, str, &a.addr4.sin_addr))
      return 0;
   else if(1 == inet_pton(AF_INET6, str, &a.addr6.sin6_addr))
      return 0;
   return 1;
}

int DTCServers::SetRouteList(std::vector<ROUTE_NODE>& IPList)
{
	if(IPList.empty())
	{
		return -ER_SET_IPLIST_NULL;
	}

	std::vector<ROUTE_NODE>().swap(this->m_IPList);	
	std::vector<ROUTE_NODE>::iterator it = IPList.begin();
	for(; it != IPList.end(); ++it)
	{
		ROUTE_NODE ip;
		ip.bid = it->bid;
		if(it->port > 0 && it->port <= 65535){
			ip.port = it->port;
		}else{
			SetErrorMsg(ER_PORT_OUT_RANGE, "DTCServers::SetRouteList", "port is out of range!");
			return -ER_PORT_OUT_RANGE;
		}
		if(it->status == 0 || it->status == 1){
			ip.status = it->status;
		}else{
			SetErrorMsg(ER_STATUS_ERROR_VALUE, "DTCServers::SetRouteList", "status is error value!");
			return -ER_STATUS_ERROR_VALUE;
		}
		if(it->weight > 0){
			ip.weight = it->weight;
		}else{
			SetErrorMsg(ER_WEIGHT_ERROR_VALUE, "DTCServers::SetRouteList", "weight is error value!");
			return -ER_WEIGHT_ERROR_VALUE;
		}
		if(InvalidIpaddr(it->ip) == 0){
			memcpy(ip.ip, it->ip, IP_LEN);
		}else{
			SetErrorMsg(ER_IP_ERROR_VALUE, "DTCServers::SetAccessKey", "ip is error value!");
			return -ER_IP_ERROR_VALUE;
		}
#ifdef DEBUG_INFO
		PrintIPNode(ip);	
#endif
		this->m_IPList.push_back(ip);
	}
	this->m_SetRoute = true;

	return 0;
}

void DTCServers::SetIDCNo(int IDCNo)
{
	this->m_IDCNo = IDCNo;
}

int DTCServers::SetAccessKey(const char *token)
{
	std::string str;
	if(token == NULL)
		return -EC_BAD_ACCESS_KEY;
	else
		str = token;                                                     
	if(str.length() != ACCESS_KEY_LEN)
	{
		log_error("Invalid accessKey!");
		this->m_AccessToken = "";
		SetErrorMsg(EC_BAD_ACCESS_KEY, "DTCServers::SetAccessKey", "Invalid accessKey!");
		return -EC_BAD_ACCESS_KEY;
	}
	else
		this->m_AccessToken = str;

	std::string stemp = str.substr(0, 8);
	sscanf(stemp.c_str(), "%d", &(this->m_Bid));
	return 0;
}

int DTCServers::SetTableName(const char *tableName)
{
	if(tableName==NULL) return -DTC::EC_BAD_TABLE_NAME;

	if(this->m_TableName)
		return mystrcmp(tableName, this->m_TableName, 256)==0 ? 0 : -DTC::EC_BAD_TABLE_NAME; 

	this->m_TableName = STRDUP(tableName);

	return 0;
}

int DTCServers::SetKeyType(int type)
{
	switch(type)
	{
		case DField::Signed:
		case DField::Unsigned:
		case DField::Float:
		case DField::String:
		case DField::Binary:
			this->m_KeyType = type;
			break;
		default:
			return -DTC::EC_BAD_KEY_TYPE;
			break; 
	}
	return 0;
}

/*
int DTCServers::AddKey(const char* name, uint8_t type)
{
}
*/

void DTCServers::SetAgentTime(int t)
{
	this->m_AgentTime = t;
}

void DTCServers::SetMTimeout(int n)
{
	this->m_TimeOut = n<=0 ? 5000 : n;
}

int DTCServers::ConstructServers()
{
	if(this->m_IPList.empty())
	{
		log_error("ip list is empty!");
		SetErrorMsg(ER_SET_IPLIST_NULL, "DTCServers::ConstructServers", "ip list is empty!");
		return -ER_SET_IPLIST_NULL;
	}
	
	if(this->m_AccessToken.empty() || (this->m_TableName == NULL) || (this->m_KeyType == 0))
	{
		log_error("m_AccessToken m_TableName or m_KeyType is unset");
		SetErrorMsg(ER_SET_INSTANCE_PROPERTIES_ERR, "DTCServers::ConstructServers", "m_AccessToken m_TableName or m_KeyType is unset!");
		return -ER_SET_INSTANCE_PROPERTIES_ERR;
	}

	int i = 0;
	int ret = 0;
	char tmpPort[7];
	memset(tmpPort, 0, sizeof(tmpPort));	
	
	if(this->m_QOSSevers)
	{
		delete[] this->m_QOSSevers;
		this->m_QOSSevers = NULL;
	}
 	int IPCount = this->m_IPList.size();
	this->m_QOSSevers = new DTCQosServer[IPCount];
	for( ; i < IPCount; ++i)
	{
		this->m_QOSSevers[i].CreateDTCServer();
		Server *server = this->m_QOSSevers[i].GetDTCServer();
		sprintf(tmpPort, "%d", this->m_IPList[i].port);
		ret = server->SetAddress(this->m_IPList[i].ip, tmpPort);
		if(ret < 0)
		{
			if(this->m_QOSSevers)
			{
				delete[] this->m_QOSSevers;
				this->m_QOSSevers = NULL;  
			}                      

			return -ret;
		}
		
		this->m_QOSSevers[i].setWeight(this->m_IPList[i].weight);
		this->m_QOSSevers[i].SetStatus(this->m_IPList[i].status);
		ret = server->SetTableName(this->m_TableName);
		if(ret < 0)
		{
			if(this->m_QOSSevers)
			{
				delete[] this->m_QOSSevers;
				this->m_QOSSevers = NULL;  
			}                      
			return -ret;
		}

		server->SetAccessKey(this->m_AccessToken.c_str());	
		
		switch(this->m_KeyType)
		{
			case DField::Signed:
			case DField::Unsigned:
				server->IntKey();
				break;
			case DField::String:
				server->StringKey();
				break;
			case DField::Binary:
				server->StringKey();
				break;
			case DField::Float:
			default:
				{
					log_error("key type is wrong!");
					if(this->m_QOSSevers)
					{
						delete[] this->m_QOSSevers;
						this->m_QOSSevers = NULL;  
					}                      
					SetErrorMsg(ER_KEY_TYPE, "DTCServers::ConstructServers", "key type is wrong!");
					return -ER_KEY_TYPE;
				}
				break;
		}
		
		if(this->m_TimeOut > 0)
		{
			server->SetMTimeout(this->m_TimeOut);
		}
	}
	return 0;
}

int DTCServers::IsServerHasExisted(ROUTE_NODE& ip)
{
	int i;
	for (i = 0; i < this->m_IPList.size(); i++)
	{
		if((ip.port == this->m_IPList[i].port) && (strncmp(ip.ip, this->m_IPList[i].ip, IP_LENGHT) == 0))
			return i;
	}
	return -1;
}

int DTCServers::ConstructServers2(std::vector<ROUTE_NODE>& IPList)
{
	if(IPList.empty())
	{
		log_error("ip list is empty!");
		SetErrorMsg(ER_SET_IPLIST_NULL, "DTCServers::ConstructServers2", "ip list is empty!");
		return -ER_SET_IPLIST_NULL;
	}
	
	if(this->m_AccessToken.empty() || (this->m_TableName == NULL) || (this->m_KeyType == 0))
	{
		log_error("m_AccessToken m_TableName or m_KeyType is unset");
		SetErrorMsg(ER_SET_INSTANCE_PROPERTIES_ERR, "DTCServers::ConstructServers2", "m_AccessToken m_TableName or m_KeyType is unset!");
		return -ER_SET_INSTANCE_PROPERTIES_ERR;
	}

	int i = 0;
	int ret = 0;
	char tmpPort[7];
	memset(tmpPort, 0, sizeof(tmpPort));

	int IPCount = IPList.size();
	DTCQosServer* tmpQosServer = new DTCQosServer[IPCount];
	for ( ; i < IPCount; i++)
	{
		int idx = IsServerHasExisted(IPList[i]);
		if (idx >= 0){
			tmpQosServer[i].setWeight(IPList[i].weight);
			tmpQosServer[i].SetStatus(IPList[i].status);
			tmpQosServer[i].SetRemoveTimeStamp(this->m_QOSSevers[idx].GetRemoveTimeStamp());
			tmpQosServer[i].SetLastRemoveTime(this->m_QOSSevers[idx].GetLastRemoveTime());
			tmpQosServer[i].SetMaxTimeStamp(this->m_QOSSevers[idx].GetMaxTimeStamp());
			tmpQosServer[i].SetDTCServer(this->m_QOSSevers[idx].GetDTCServer());
			this->m_QOSSevers[idx].ResetDTCServer();
		} else {
			tmpQosServer[i].CreateDTCServer();
			Server *server = tmpQosServer[i].GetDTCServer();
			sprintf(tmpPort, "%d", IPList[i].port);
			ret = server->SetAddress(IPList[i].ip, tmpPort);
			if(ret < 0)
			{
				if(this->m_QOSSevers)
				{
					delete[] this->m_QOSSevers;
					this->m_QOSSevers = NULL;  
				}   
				delete[] tmpQosServer;                  
				return -ret;
			}

			tmpQosServer[i].setWeight(IPList[i].weight);
			tmpQosServer[i].SetStatus(IPList[i].status);
			ret = server->SetTableName(this->m_TableName);
			if(ret < 0)
			{
				if(this->m_QOSSevers)
				{
					delete[] this->m_QOSSevers;
					this->m_QOSSevers = NULL;  
				}
				delete[] tmpQosServer;                 
				return -ret;
			}
			server->SetAccessKey(this->m_AccessToken.c_str());

			switch(this->m_KeyType)
			{
				case DField::Signed:
				case DField::Unsigned:
					server->IntKey();
					break;
				case DField::String:
					server->StringKey();
					break;
				case DField::Binary:
					server->StringKey();
					break;
				case DField::Float:
				default:
					{
						log_error("key type is wrong!");
						if(this->m_QOSSevers)
						{
							delete[] this->m_QOSSevers;
							this->m_QOSSevers = NULL;  
						}
						delete[] tmpQosServer;
						SetErrorMsg(ER_KEY_TYPE, "DTCServers::ConstructServers2", "key type is wrong!");
						return -ER_KEY_TYPE;
					}
					break;
			}
		
			if(this->m_TimeOut > 0)
			{
				server->SetMTimeout(this->m_TimeOut);
			}
		}
	}

	std::vector<ROUTE_NODE>().swap(this->m_IPList);
	if(this->m_QOSSevers)
	{
		delete[] this->m_QOSSevers;
		this->m_QOSSevers = NULL;
	}
	this->m_QOSSevers = tmpQosServer;
	tmpQosServer = NULL;
	
	return 0;
}

void init_random()
{
	uint64_t ticks;
	struct timeval tv;
	int fd;
	gettimeofday(&tv, NULL);
	ticks = tv.tv_sec + tv.tv_usec;
	fd = open("/dev/urandom", O_RDONLY);
	if (fd > 0)
	{
		uint64_t r;
		int i;
		for (i = 0; i <100 ; i++)
		{
			read(fd, &r, sizeof(r));
			ticks += r;
		}
		close(fd);
	}
	srand(ticks);
}

unsigned int new_rand()
{
	int fd;
	unsigned int n = 0;
	fd = open("/dev/urandom", O_RDONLY);
	if(fd > 0)
	{
		read(fd, &n, sizeof(n));
	}
	close (fd);
	return n;
}

void DTCServers::DisorderList(int *tmpBuckets, int size)
{
    int randCount = 0;// 索引
    unsigned int position = 0;// 位置
    int k = 0;
//    srand((int)time(0));
    do{
        int r = size - randCount;
//        position = random(r);
		init_random();
		unsigned int tmp = new_rand();
        position = tmp%r;
        this->m_LoadBalanceBuckets[k++] = tmpBuckets[position];
        randCount++; 
		 // 将最后一位数值赋值给已经被使用的position
        tmpBuckets[position] = tmpBuckets[r - 1];   
    }while(randCount < size);  
    return ; 
}

int DTCServers::ConstructBalanceBuckets()
{
	if(!this->m_QOSSevers || this->m_IPList.empty())
	{
		log_error("QOSSevers is null or ip count <0 ");
		SetErrorMsg(ER_ROUTE_INFO_NULL, "DTCServers::ConstructBalanceBuckets", "QOSSevers is null or ip count <0!");
		return -ER_ROUTE_INFO_NULL;
	}

	int i = 0;
	int totalCount = 0;
	int IPCount = this->m_IPList.size();
	for( ; i < IPCount; ++i)
	{
		totalCount += this->m_QOSSevers[i].m_Weight;	
	}
	FREE_IF(this->m_LoadBalanceBuckets);
	this->m_LoadBalanceBuckets = (int *)malloc(sizeof(int) * totalCount);
	this->m_BalanceBucketSize = totalCount;
	int* tmpBuckets = (int *)malloc(sizeof(int) * totalCount);
	int j = 0;
	int pos = 0;
	for( ; j < IPCount; ++j)
	{
		int k = 0;
		for( ; k < this->m_QOSSevers[j].m_Weight; ++k)
		{
			tmpBuckets[pos] = j; 
			pos ++;
		}
	}
	this->m_BucketsPos = 0;
	DisorderList(tmpBuckets, totalCount);
//	FREE_IF(tmpBuckets);
	FREE_CLEAR(tmpBuckets);
	return 0;
} 

void DTCServers::RemoveServerFromBuckets(uint64_t now)
{
	int i = 0;
	int IPCount = this->m_IPList.size();
	for( ; i < IPCount; ++i)
	{
		if(0 == this->m_QOSSevers[i].GetStatus())
		{
			continue;
		}
		uint64_t errorCount = this->m_QOSSevers[i].GetDTCServer()->GetErrCount();
		if(errorCount >= DEFAULT_REMOVE_ERROR_COUNT)
		{
			this->m_QOSSevers[i].GetDTCServer()->IncRemoveCount();
			uint64_t removeTime = this->m_QOSSevers[i].GetDTCServer()->GetRemoveCount() * DEFAULT_ROUTE_INTERVAL_TIME;
			if(removeTime >= DEFAULT_MAX_REMOVE_THRESHOLD_TIME)
			{
				removeTime = DEFAULT_MAX_REMOVE_THRESHOLD_TIME;
			}
			this->m_QOSSevers[i].SetRemoveTimeStamp(removeTime);
			this->m_QOSSevers[i].SetLastRemoveTime(now);
			this->m_QOSSevers[i].SetStatus(0);
			log_debug("bid=[%d], remove time=[%lu], now=[%lu], address=[%s], pos=[%d]!", \
					this->m_Bid, removeTime, now, this->m_QOSSevers[i].GetDTCServer()->GetAddress(), i);
#ifdef DEBUG_INFO 
			printf("remove bid=[%d],now=[%lu],remvoe timeStamp=[%lu],remove count=[%d],address=[%s]!\n", \
					this->m_Bid, now, removeTime, this->m_QOSSevers[i].GetDTCServer()->GetRemoveCount(), \
					this->m_QOSSevers[i].GetDTCServer()->GetAddress()); 
#endif
		}
	}
}

int DTCServers::RefreshBalanceBuckets(uint64_t now)
{
	int i = 0;
	int IPCount = this->m_IPList.size();
	for( ; i < IPCount; ++i)
	{
		if(this->m_QOSSevers[i].GetStatus() == 0)
		{
			if(this->m_QOSSevers[i].GetLastRemoveTime() != 0)
			{
				if(now - this->m_QOSSevers[i].GetLastRemoveTime() > this->m_QOSSevers[i].GetRemoveTimeStamp())
				{
					this->m_QOSSevers[i].SetStatus(1);
					this->m_QOSSevers[i].SetRemoveTimeStamp(0);
					this->m_QOSSevers[i].SetLastRemoveTime(0);
					this->m_QOSSevers[i].GetDTCServer()->ClearErrCount();
					log_debug("bid=[%d], address=[%s], pos=[%d]!", \
							this->m_Bid, this->m_QOSSevers[i].GetDTCServer()->GetAddress(), i); 
#ifdef DEBUG_INFO 
					printf("refresh bid=[%d],address=[%s],now=[%lu]!\n", \
							this->m_Bid, this->m_QOSSevers[i].GetDTCServer()->GetAddress(), now);
#endif 
				}
			}
		}
	}

	return 0;
}

DTC::Server* DTCServers::GetOneServerFromBuckets()
{
	int serverPos = 0;
	int lastBucketsPos = this->m_BucketsPos;
	do{
		++this->m_BucketsPos;
		if(this->m_BucketsPos >= this->m_BalanceBucketSize)
			this->m_BucketsPos = 0;
		
		if(unlikely(!this->m_LoadBalanceBuckets))
			return NULL;
		serverPos = this->m_LoadBalanceBuckets[this->m_BucketsPos];
		if(this->m_QOSSevers[serverPos].GetStatus())
		{
#ifdef DEBUG_INFO 
			printf("get server address=[%s]\n", this->m_QOSSevers[serverPos].GetDTCServer()->GetAddress());
#endif
			return this->m_QOSSevers[serverPos].GetDTCServer();
		}else{
			//整租服务皆不可用，返回默认位置的server
			if(lastBucketsPos == this->m_BucketsPos)
			{
				int tmpPos = this->m_LoadBalanceBuckets[DEFAULT_SERVER_POS];
				return this->m_QOSSevers[tmpPos].GetDTCServer();
			}
		}
	}while(1);
}

DTC::Server* DTCServers::GetServer()
{
	int ret = 0;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	uint64_t now = tv.tv_sec; 
	if(this->m_SetRoute && (!this->m_IPList.empty()) && (!this->m_ConstructedBySetIPs))
	{
#ifdef DEBUG_INFO 
		printf("construct server by set route\n");
#endif
		ret = ConstructServers();
		if(ret < 0)
		{
			log_error("construct servers by set route failed! error code=[%d]", ret);
			SetErrorMsg(ret, "DTCServers::GetServer", "construct servers by set route failed!");
			return NULL;
		}
		ret = ConstructBalanceBuckets();
		if(ret < 0)
		{
			log_error("construct balance bucket failed! error code=[%d]", ret);
			SetErrorMsg(ret, "DTCServers::GetServer", "construct balance bucket failed");
			return NULL;
		}
		this->m_ConstructedBySetIPs = true;
#ifdef DEBUG_INFO 
		printf("get one server from set route\n");
#endif
		goto GetOneServer;
	}
	
	if(this->m_SetRoute && (!this->m_IPList.empty()) && this->m_ConstructedBySetIPs)
	{
#ifdef DEBUG_INFO 
		printf("get one server from set route\n");
#endif
		goto GetOneServer;
	}

	if(0 == this->m_BidVersion)
	{
		uint64_t BidVersion = 0;
		int ret = get_version(&BidVersion);
		if(BidVersion <= 0)
		{
			log_info("get version from CC error!");
//			SetErrorMsg(ER_BID_VERSION_ERR, "DTCServers::GetServer", "get version from CC error!");
			SetErrorMsg(ret, "DTCServers::GetServer", "get version from CC error!");
			return NULL;
		}
		IP_ROUTE IPRoute;
		memset(&IPRoute, 0, sizeof(IP_ROUTE));
		ret = get_ip_route(this->m_Bid, &IPRoute);
		if(ret < 0 || IPRoute.ip_num <= 0 || IPRoute.ip_list == NULL)
		{
			log_error("get ip list by bid failed! ip list is null!");
			SetErrorMsg(0, "DTCServers::GetServer", "get ip list by bid failed! ip list is null!");
			free_ip_route(&IPRoute);
			return NULL;
		}

		int i = 0;
		std::vector<ROUTE_NODE>().swap(this->m_IPList);
		for(; i < IPRoute.ip_num; ++i)
		{
			ROUTE_NODE RouteNode;
			RouteNode.bid = IPRoute.ip_list[i].bid;
			memcpy(RouteNode.ip, IPRoute.ip_list[i].ip, IP_LEN);
			RouteNode.port = IPRoute.ip_list[i].port;
			RouteNode.status = IPRoute.ip_list[i].status;
			RouteNode.weight = IPRoute.ip_list[i].weight;
#ifdef DEBUG_INFO
			PrintIPNode(RouteNode);
#endif
			this->m_IPList.push_back(RouteNode);	
		}
		free_ip_route(&IPRoute);

#ifdef DEBUG_INFO 
		printf("construct server by cc\n");
#endif
		ret = ConstructServers();
		if(ret < 0)
		{
			log_error("construct servers failed!");
			SetErrorMsg(ret, "DTCServers::GetServer", "construct servers failed!!");
			return NULL;
		}
		//TODO cc version管理
		
		ret = ConstructBalanceBuckets();
		if(ret < 0)
		{
			log_error("construct balance bucket failed! error code=[%d]", ret);
			SetErrorMsg(ret, "DTCServers::GetServer", "construct balance bucket failed!");
			return NULL;
		}
#ifdef DEBUG_INFO 
		printf("get one server from cc\n");
#endif
		this->m_LastGetCaTime = now; 
		this->m_BidVersion = BidVersion;
		goto GetOneServer;
	}
	
	if(now - this->m_LastGetCaTime >= DEFAULT_ROUTE_EXPIRE_TIME)
	{
		uint64_t BidVersion = 0;
		get_version(&BidVersion);
		this->m_LastGetCaTime = now;
		if(this->m_BidVersion >= BidVersion)
		{
			log_info("the version is lastest!");
#ifdef DEBUG_INFO 
			printf("get one server from cc\n");
#endif
			goto GetOneServer;
		}
		IP_ROUTE IPRoute;
		memset(&IPRoute, 0, sizeof(IP_ROUTE));
		ret = get_ip_route(this->m_Bid, &IPRoute); 
		if(ret < 0 || IPRoute.ip_num <= 0 || IPRoute.ip_list == NULL)
		{
			log_error("get ip list by bid failed! ip list is null!");
			SetErrorMsg(0, "DTCServers::GetServer", "get ip list by bid failed! ip list is null!");
			free_ip_route(&IPRoute);
			return NULL;
		}

		int i = 0;
		std::vector<ROUTE_NODE> IPList;

		for(; i < IPRoute.ip_num; ++i)
		{
			ROUTE_NODE RouteNode;
			RouteNode.bid = IPRoute.ip_list[i].bid;
			memcpy(RouteNode.ip, IPRoute.ip_list[i].ip, IP_LEN);
			RouteNode.port = IPRoute.ip_list[i].port;
			RouteNode.status = IPRoute.ip_list[i].status;
			RouteNode.weight = IPRoute.ip_list[i].weight;
#ifdef DEBUG_INFO
			PrintIPNode(RouteNode);
#endif
			IPList.push_back(RouteNode);	
		}
		free_ip_route(&IPRoute);

#ifdef DEBUG_INFO
		printf("construct server by cc\n");
#endif
		ret = ConstructServers2(IPList);
		if(ret < 0)
		{
			std::vector<ROUTE_NODE>().swap(this->m_IPList);
			this->m_IPList = IPList;
			log_error("construct servers failed!");
			SetErrorMsg(ret, "DTCServers::GetServer", "construct servers failed!");
			return NULL;
		}
		this->m_IPList = IPList;
		ret = ConstructBalanceBuckets();
		if(ret < 0)
		{
			log_error("construct balance bucket failed! error code=[%d]", ret);
			SetErrorMsg(ret, "DTCServers::GetServer", "construct balance bucket failed!");
			return NULL;
		}
		this->m_BidVersion = BidVersion;
#ifdef DEBUG_INFO
		printf("get one server from cc\n");
#endif
		goto GetOneServer;
	}else{
#ifdef DEBUG_INFO
		printf("get one server from cc\n");
#endif
		goto GetOneServer;
	}
	log_error("Exception process!");
	SetErrorMsg(0, "DTCServers::GetServer", "Exception process!");
	return NULL;

GetOneServer:
/*	if(now - this->m_RefreshBucketsTime >= DEFAULT_REFRESH_BUCKETS_TIME)
	{
		RefreshBalanceBuckets(now);
		this->m_RefreshBucketsTime = now;
	}
	if(now - this->m_RemoveBucketsTime >= DEFAULT_REMOVE_BUCKETS_TIME)
	{
		RemoveServerFromBuckets(now);
		this->m_RemoveBucketsTime = now;
	}
*/
	RefreshBalanceBuckets(now);
	RemoveServerFromBuckets(now);
	return GetOneServerFromBuckets();
}
