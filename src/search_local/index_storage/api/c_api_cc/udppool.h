#include <stdlib.h>

class NCPort
{
public:
    uint64_t sn;
    int fd;
    int timeout;//the timeout this socket is.

public:
    NCPort()
    {
        sn = 0;
        fd = -1;
        timeout = -1;
    }

    NCPort(const NCPort &that)
    {
        sn = that.sn;
        fd = that.fd;
        timeout = that.timeout;
    }

    ~NCPort()
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }
};

class NCUdpPortList;
class NCUdpPort: public NCPort
{
public:
    int family;
    pid_t pid;

private:
    NCUdpPort *next;

private:
    NCUdpPort()
    {
        pid = -1;
    };
    ~NCUdpPort()
    {
    };

public:
    friend class NCUdpPortList;
    static NCUdpPort *Get(int family); // get from cache
    void Put(); // put back cache
    void Eat()
    {
        delete this;
    } // eat and delete
};

