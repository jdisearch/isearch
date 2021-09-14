CC := g++
AR := ar rc
LIBS := -lrt -lpthread -lcommon -ljsoncpp -lcurl64 -libmysqlclient.a

INCLUDE := -I../ -I../../ -I../../common -I../../3rdparty/jsoncpp/include/ -I../../3rdparty/dtc/include/ -I../../3rdparty/mysql64/include/mysql/ -I../../3rdparty/curl/include64/

LIBPATH := -L../../common -L../../3rdparty/jsoncpp/lib/  -L../../3rdparty/mysql64/lib/mysql/ -L../../3rdparty/curl/lib

SRCS := $(wildcard *.cpp)

OBJECTS := $(patsubst %.cpp,%.o,$(SRCS))

CFLAGS := -g -fPIC -Wall -O3 -c $(INCLUDE) $(LIBPATH) 

TARGETLIB := libDtcMonitor.a

all:
	$(CC) $(CFLAGS) $(SRCS) $(LIBS)
	$(AR) $(TARGETLIB) $(OBJECTS) 

clean:
	rm -f *.o
	rm -f $(TARGETLIB)