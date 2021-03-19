LIB_PATH = ../../../..

include ../../Make.conf

GIT_VERSION=$(shell git log . | head -n 4 | grep "commit" | cut -d " " -f 2 | cut -c 1-7)
SVN_REVISION = $(shell test -d .svn && (svn info | grep "Last Changed Rev" | cut -d " " -f 4))
ifeq "$(SVN_REVISION)a" "a"
	SVN_REVISION = "(unknown)"
endif

VPATH = ../../common
################compile#############
target = libdtc.a container_api.pic.o version.pic.o somain.pic.o libdtc.so libdtc.pic.a 

# CLIENTAPI macro use for scope test only
CFLAGS += -DCLIENTAPI
CFLAGS += -pthread -I../../common -I../../stat -I../../../../3rdlib/CA_API/ $(ZINC)
LIBS = ${Z_LIB} -ldl -lpthread $(CA_API_LIB)

filelist := dtcreq dtcsvr dtcpool dtcwrap dtcwrapp dtcqossvr \
	    poller timerlist keylist table_def \
	    log_client memcheck md5 value \
	    section decode encode field_api \
	    packet_base packet_client \
	    task_base task_const sockaddr udppool compress buffer thread

#liddtc.so: container_api.pic.o version.pic.o

# for auto ln -sf
#libdtc_objs := $(patsubst %,%.o,$(filelist))
#libdtc.so: LDFLAGS += -Wl,--version-script,dtcapi.lst -e _so_start container_api.pic.o version.pic.o somain.pic.o
#libdtc_filename := libdtc-gcc-$(GCCVER)-r$(GIT_VERSION).so
#libdtc_soname := libdtc.so.1

###############install##############
target_install = libdtc.a libdtc.pic.a libdtc.so dtcapi.h 
install_dir = ../../../bin
%.a: install_dir = ../../../lib
%.h: install_dir = ../../../include

include ../../Make.rules

