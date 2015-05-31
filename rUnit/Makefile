######################################################################
## 
## Filename: Makefile
## Description: 
## Author: Jujj
## Created: 周五 十月  4 22:51:46 2013 (+0800)
## Version: 1.0
## 
######################################################################
## 
# makefile for rendering unit
include ../Makefile.inc

INCLUDES=-I. -I$(INCDIR)
LDFLAGS=-L$(LIBDIR) -Wl,-rpath,$(DESTDIR)/lib -lbleman $(OTHER_LDFLAGS)
all:rUnit
rUnit:rUnit.cpp taskMaster.cpp taskServer.cpp taskSlave.cpp threadPool.cpp common.cpp dmwrapper.cpp
	$(MPICXX) $(OTHER_CXXFLAGS) $(INCLUDES) $(LDFLAGS) -o rUnit $+ -lpciaccess -lpthread
	@echo "================= Test Program using following command ==============="
	@echo "    You need to set : export MV2_ENABLE_AFFINITY=0  MVAPICH2 will set processor affinity automatically"
	@echo "    You need to set : export PIXIEHOME=/gos4/user39/863_project/pixie_withDM or pixie will use the wrong version library"
	@echo "    LD_LIBRARY_PATH=/gos4/user39/863_project/pixie_withDM/lib:/gos4/user39/863_project/rUnit-withDM/hust_dmcs/lib mpirun -n 2 ./rUnit -t 8 -d /gos4/user39/863_project/test_scenes/building -s 192.168.0.100"

install:
	install -d $(BINDIR)
	install -m 755 rUnit $(BINDIR)

clean:
	rm rUnit -f
