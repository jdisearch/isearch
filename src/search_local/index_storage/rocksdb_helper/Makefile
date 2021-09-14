#LIBNAME = dtcMonitor 

TARGETEXE = rocksdb_helper

CC = g++

.SUFFIXES: .cc .h

FLAGS = -g -O2 -std=c++11 -DROCKSDB_COMPILER=1 -DOMN_PLATFORM_UNIX $(Optimize) -Wl,--no-undefined -Xlinker -zmuldefs
#FLAGS = -O3 -DOMN_PLATFORM_UNIX $(Optimize)

INCDIRS = -I.. -I../common -I../watchdog -I../cache -I../stat -I../../../3rdlib/rocksdb/include -I../../../3rdlib/rocksdb/logging -I../../../3rdlib/attr_api -I../../../comm

LIBDIR = -L../common  -L../cache -L../../../3rdlib/rocksdb/lib
#EXLIBDIR = ../../../Lib

OBJDIR = .

CREATEOBJDIR = $(shell if [ -d $(OBJDIR) ]; then echo ""; else mkdir $(OBJDIR); fi)

OUTPUTDIR = .

LIBS = -lnsl -lpthread -lstdc++ -ldl -lz -lrocksdb -L../bin

DLIBS = ../cache/libdtcd.a \
		../common/libcommon.a \
		../stat/libstat.a \
		../watchdog/libwatchdog.a \
		../../../3rdlib/attr_api/libattr_api_64.a \

#
# objects and targets
#
#OBJECTS = $(patsubst %.cc,$(OBJDIR)/%.o,$(wildcard *.cc))
OBJECTS = $(wildcard *.cc)
OBJECTS:= $(filter-out comm_main.cc db_process_gauss.cc gauss_db_conn.cc, $(OBJECTS))
OBJECTS:= $(patsubst %.cc,$(OBJDIR)/%.o,$(OBJECTS))

#
# Rules for normal comile and link 
#
all:: $(TARGETEXE) 

$(TARGETEXE): $(OBJECTS)
	$(CC) -rdynamic -Wl,--no-as-needed $(FLAGS) -o $(OBJDIR)/$(TARGETEXE) $^  $(LIBDIR) $(DLIBS) $(LIBS) $(DLIBS) 
$(OBJECTS): $(OBJDIR)/%.o: %.cc
	$(CREATEOBJDIR)
	$(CC) -c -static  $(FLAGS) $< $(INCDIRS) -o $@

clean:
	rm -rf $(OBJDIR)/*.o $(OBJDIR)/$(TARGETEXE)
	rm -rf ./tester/rocksdata/*
