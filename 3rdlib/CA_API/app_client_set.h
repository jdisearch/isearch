#ifndef CONFIG_CENTER_CLIENT_H_INCLUDED_
#define CONFIG_CENTER_CLIENT_H_INCLUDED_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <memory.h>

#define IP_LEN 16

typedef struct ip_node
{
	int bid;
	int port;
	int status;
	int weight;
	char ip[IP_LEN];
}IP_NODE;

typedef struct ip_route
{
	int ip_num;
	IP_NODE* ip_list;
}IP_ROUTE;

int get_version(uint64_t *version);

int dump_ca_shm();

int get_ip_route(int bid, IP_ROUTE* ip_route);

int free_ip_route(IP_ROUTE* ip_route);

#endif
