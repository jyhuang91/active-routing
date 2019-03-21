TRGS = spmv

#LIBS += -lpthread -lrt

LIB_DEPEND = hooks
LIBS += -lrt
LIBS += -L$(MCSIM_PARENT_DIR)/Apps/hooks/lib -lhooks
LIBS += -L$(MCSIM_PARENT_DIR)/Pthread -lmypthread
INCS += -I$(MCSIM_PARENT_DIR)/Apps/hooks/include
LDFLAGS = $(MCSIM_PARENT_DIR)/Pthread/libmypthread.a

CXXFLAGS = -g --std=c++0x -O3 -static
CXXFLAGS += -Wall ##-Werror

CUR_DIR = $(shell pwd)


all: $(TRGS) $(LIB_DEPEND)

$(LIB_DEPEND):
	(cd $(MCSIM_PARENT_DIR)/Apps/hooks; $(MAKE); cd $(CUR_DIR))

ge: ge.cc

##boost_dijk_real: boost_dijk_real.cc

clean:
	rm -f $(TRGS) *.o *.dot

$(TRGS): $(LIB_DEPEND)

%: %.cc ;
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS) $(INCS)
