# File: makefile
#
# This is a makefile for use with g++
#
LIBNAME = TransPool

VERSION=`git log -l --pretty=%h`

CC = g++

.SUFFIXES: .cpp .cc .h

#FLAGS = -std=c++0x -gdwarf-2 -g3 -Wall -DACTIVE_GIT_VERSION="${VERSION}" -DOMN_PLATFORM_UNIX $(Optimize) -DAOS_USERLAND
FLAGS = -gdwarf-2 -g3 -Wall -DACTIVE_GIT_VERSION="${VERSION}" -DOMN_PLATFORM_UNIX $(Optimize) -DAOS_USERLAND

INCDIRS = -I../../ -I../../../common -I../../../ -I../../../3rdparty/jsoncpp/include/ -I../../../api/ -I../../../3rdparty/curl/include64/ -I../../../3rdparty/mysql64/include/mysql/

OBJDIR = ./$(LIBNAME)
$(warning $(OBJDIR))

CREATEDIR = $(shell if [ -d $(OBJDIR) ]; then echo ""; else mkdir $(OBJDIR); fi)

OUTPUTDIR = ./$(OBJDIR)

LIBS = -L/usr/local/scws/lib -lnsl -lpthread -lstdc++ -lscws

#
# objects and targets
#
OBJECTS = $(patsubst %.cpp,$(OBJDIR)/%.o,$(wildcard *.cpp))
$(warning $(OBJECTS))

TARGETLIB = lib$(LIBNAME).a

#
# Rules for normal comile and link
#
all:: lib

lib: $(OBJECTS)
	ar rvu $(TARGETLIB) $^
	mv -f $(TARGETLIB) $(OUTPUTDIR)

$(OBJECTS): $(OBJDIR)/%.o: %.cpp
	@echo "do create dir begin"
	$(CREATEDIR)
	@echo "do create dir end"
	$(CC) -c $(FLAGS) $< $(INCDIRS) -o $@
	#$(CC) -c $(FLAGS) $< -o $@

.PHONY: clean
clean:
	rm -rf $(OBJECTS) $(OBJDIR) $(OUTPUTDIR)/$(TARGETLIB) 
