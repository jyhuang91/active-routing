SHELL = /bin/sh
.PHONY: all depend clean
.SUFFIXES: .cc .o

default: all

LIBS = ../../../extras/xed2-intel64/lib/libxed.a
INCS = -I${PIN_HOME}/extras/xed2-intel64/include -I${MCSIM_PARENT_DIR}/CasHMC/sources

# add hooks
LIBS += -L$(MCSIM_PARENT_DIR)/Apps/hooks/lib -lhooks
INCS += -I$(MCSIM_PARENT_DIR)/Apps/hooks/include

ifeq ($(TAG),dbg)
	DBG = -Wall
	OPT = -ggdb -g -O0
else ifeq ($(TAG),rb)
	DBG += -DRUNTIME_KNOB
	OPT = -O3 -g
else
  #DBG = -DNDEBUG
  DBG =
  OPT = -O3 -g
endif

#DBG += -DDEBUG_CACHE
#DBG += -DDEBUG_GATHER
DBG += -DDEBUG_VAULT
#OPT = -O3 -DNDEBUG -axS -march=core2 -mtune=core2
#OPT = -O3 -DNDEBUG -msse2 -march=pentium-m -mfpmath=sse
#CXXFLAGS = -Wall -Wno-unknown-pragmas -Winline $(DBG) $(OPT) 
#CXXFLAGS = -fPIC -Wno-unknown-pragmas $(DBG) $(OPT) 
CXXFLAGS = -Wno-unknown-pragmas $(DBG) $(OPT) 
CXX = g++ -DTARGET_IA32E
CC  = gcc -DTARGET_IA32E
#CXX = icpc -DTARGET_IA32E
#CC = icc -DTARGET_IA32E
PINFLAGS = 

SRCS = PTSCache.cc \
	PTSComponent.cc \
	PTSCore.cc \
	PTSO3Core.cc \
	PTSDirectory.cc \
	PTSRBoL.cc \
	PTSMemoryController.cc \
	PTSHMCController.cc \
	PTSTLB.cc \
	PTSXbar.cc \
	McSim.cc \
	PTS.cc

OBJS = $(patsubst %.cc,obj_$(TAG)/%.o,$(SRCS))
HMCOBJS = $(wildcard ../CasHMC/sources/obj/*.o)

all: remove obj_$(TAG)/mcsim
	cp -f obj_$(TAG)/mcsim mcsim

remove:
	rm -f obj_$(TAG)/mcsim mcsim

obj_$(TAG)/mcsim : $(OBJS) main.cc
	$(CXX) $(CXXFLAGS) $(INCS) -o obj_$(TAG)/mcsim $(OBJS) $(HMCOBJS) main.cc

obj_$(TAG)/%.o : %.cc
	$(CXX) -c $(CXXFLAGS) $(PIN_CXXFLAGS) $(INCS) -o $@ $<

clean:
	-rm -f *.o pin.log mcsim 

