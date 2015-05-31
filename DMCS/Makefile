######################################################################
## 
## Filename: Makefile
## Description: 
## Author: Jujj
## Created: 周五 十月  4 22:49:11 2013 (+0800)
## Version: 1.0
## 
######################################################################
## 
# my Makefile for sdu dmcs
include ../../Makefile.inc

# all source and header files  
SRCS = mystdio.cpp cc_file.cpp common.cpp dm.cpp dmmaster.cpp dmslave.cpp rdma.cpp

INCLUDE = -I. -I$(INCDIR)

# convert crlf to lf
DOS2UNIX = dos2unix

VER_MAJOR = 1
VER_MINOR = 0.0

LIBRARIES = -lpthread
MODULES = $(SRCS:.c=.o)
MODULES := $(MODULES:.cpp=.o)
CFLAGS = -O3 -fPIC $(OTHER_FLAGS)
CFLAGS += $(INCLUDE)
CXXFLAGS = -O3 -fPIC -fexceptions $(OTHER_FLAGS)
CXXFLAGS += $(INCLUDE)
LDFLAGS=-L$(LIBDIR) -Wl,--rpath,$(LIBDIR)

TARGET = sdudmcs
STATICLIB = lib$(TARGET).a
SHAREDLIB = lib$(TARGET)-$(VER_MAJOR).$(VER_MINOR).so
LIBNAME = lib$(TARGET).so
VERLIBNAME = $(LIBNAME).$(VER_MAJOR)
HEADER = mystdio.h


default:all

all:dist

dist:sdudmcs

dos2unix:
	@$(DOS2UNIX) $(SRCS) $(INCLS)

sdudmcs: $(SHAREDLIB)

.c.o:
	$(MPICC) $(CFLAGS) -c $< -o $@

.cpp.o:
	$(MPICXX) $(CXXFLAGS) -c $< -o $@

$(STATICLIB): $(MODULES)
	$(AR) r $@ $(MODULES)

$(SHAREDLIB): $(MODULES)
	$(MPICXX) -shared -Wl,-soname,$(VERLIBNAME) $(LDFLAGS) -o $@ $(MODULES) $(LIBRARIES)

install:
	install -d $(INCDIR) $(LIBDIR)
	install -m 644 $(HEADER) $(INCDIR)
	install -m 644 $(STATICLIB) $(LIBDIR)
	install -m 755 $(SHAREDLIB) $(LIBDIR)
	ln -sf $(SHAREDLIB) $(LIBDIR)/$(VERLIBNAME)
	ln -sf $(VERLIBNAME) $(LIBDIR)/$(LIBNAME)

clean:
	rm -f $(MODULES) $(STATICLIB) $(SHAREDLIB) $(LIBNAME) test/*.o

test1: test/test1.o
	$(MPICXX) -o test/run1 $(LDFLAGS) $< -lsdudmcs

test2: test/test2.o
	$(MPICXX) -o test/run2 $(LDFLAGS) $< -lsdudmcs
