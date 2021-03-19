#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <alloca.h>
#include "udppool.h"
#include "log.h"

static unsigned int bindip = 0xFFFFFFFF;

unsigned int GetBindIp(void)
{
    const char *name = getenv("DTCAPI_UDP_INTERFACE");

    if (name == NULL || name[0] == 0 || strcmp(name, "*") == 0)
    {
	    return 0;
    }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct ifconf ifc;
    struct ifreq *ifr = NULL;
    int n = 0;
    int i;

    if (fd < 0)
        return 0;

    ifc.ifc_len = 0;
    ifc.ifc_req = NULL;

    if (ioctl(fd, SIOCGIFCONF, &ifc) == 0)
    {
        ifr = (struct ifreq *) alloca(ifc.ifc_len > 128 ? ifc.ifc_len : 128);
        ifc.ifc_req = ifr;

        if (ioctl(fd, SIOCGIFCONF, &ifc) == 0)
            n = ifc.ifc_len / sizeof(struct ifreq);
    }

    close(fd);

    for (i = 0; i < n; i++)
    {
        if (strncmp(ifr[i].ifr_name, name, sizeof(ifr[i].ifr_name)) != 0)
            continue;

        if (ifr[i].ifr_addr.sa_family == AF_INET)
            return ((struct sockaddr_in *)&ifr[i].ifr_addr)->sin_addr.s_addr;
    }

    return 0;
}

static int CreatePortIpv4(void)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (bindip == 0xFFFFFFFF)
        bindip = GetBindIp();

    if (bindip != 0)
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = bindip;

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            close(fd);
            return -1;
        }
    }

    return fd;
}

static int CreatePortIpv6(void)
{
    int fd = socket(AF_INET6, SOCK_DGRAM, 0);

    return fd;
}

static int CreatePortUnix(void)
{
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path),
		    "@dtcapi-global-%d-%d-%d", getpid(), fd, (int)time(NULL));
    socklen_t len = SUN_LEN(&addr);
    addr.sun_path[0] = 0;
    if (bind(fd, (const struct sockaddr *)&addr, len) < 0)
    {
	    close(fd);
	    return -1;
    }

    return fd;
}


struct NCUdpPortList {
public:
	int stopped;
	int family;
	int (*newport)(void);
	pthread_mutex_t lock;
	NCUdpPort *list;
public:
	NCUdpPort *Get(void);
	void Put(NCUdpPort *);
public:
	~NCUdpPortList(void);
};

// this destructor only called when unloading libdtc.so
NCUdpPortList::~NCUdpPortList(void)
{
	if (pthread_mutex_lock(&lock) == 0)
	{
		stopped = 1;
		while (list != NULL)
		{
			NCUdpPort *port = list;
			list = port->next;
			delete port;
		}
		pthread_mutex_unlock(&lock);
	}
}

NCUdpPort *NCUdpPortList::Get(void)
{
    NCUdpPort *port = NULL;

    if (pthread_mutex_lock(&lock) == 0)
    {
        if (list != NULL)
        {
            port = list;
            list = port->next;
        }
	pthread_mutex_unlock(&lock);
    }
    else
    {
        log_error("api mutex_lock error,may have fd leak");
    }

    if (port != NULL)
    {
        if (getpid() == port->pid)
        {
            port->sn++;
        }
        else
        {
            delete port;
            port = NULL;
        }
    }

    if (port == NULL)
    {
        int fd = newport();

        if (fd > 0)
        {
            port = new NCUdpPort;
            port->fd = fd;
            unsigned int seed = fd + (long)port + (long) & port + (long)pthread_self() + (long)port;
            port->sn = rand_r(&seed);
            port->pid = getpid();
            port->timeout = -1;
	    port->family = family;
        }
    }

    return port;
}

void NCUdpPortList::Put(NCUdpPort *port)
{
    if (this != NULL && pthread_mutex_lock(&lock) == 0)
    {
	if(stopped) {
	    // always delete port after unloading process
            port->Eat();
	} else {
            port->next = list;
            list = port;
	}
	pthread_mutex_unlock(&lock);
    }
    else
    {
        port->Eat();
    }
}

static NCUdpPortList ipv4List = { 0, AF_INET, CreatePortIpv4, PTHREAD_MUTEX_INITIALIZER, NULL };
static NCUdpPortList ipv6List = { 0, AF_INET6, CreatePortIpv6, PTHREAD_MUTEX_INITIALIZER, NULL };
static NCUdpPortList unixList = { 0, AF_UNIX, CreatePortUnix, PTHREAD_MUTEX_INITIALIZER, NULL };

struct NCUdpPortList *GetPortList(int family) {
	switch(family) {
	case AF_INET:
		return &ipv4List;
	case AF_INET6:
		return &ipv6List;
	case AF_UNIX:
		return &unixList;
	}
	return NULL;
}

NCUdpPort *NCUdpPort::Get(int family)
{
    NCUdpPortList *portList = GetPortList(family);
    if(portList == NULL) {
	    return NULL;
    }

    return portList->Get();
}

void NCUdpPort::Put()
{
    NCUdpPortList *portList = GetPortList(family);
    portList->Put(this);
}
