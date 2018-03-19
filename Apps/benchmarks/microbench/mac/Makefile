
TRGS = mac ar_mac

LIB_DEPEND = hooks

#LIBS += -lpthread -lrt
LIBS += -lrt
LIBS += -L$(MCSIM_PARENT_DIR)/Apps/hooks/lib -lhooks
LIBS += -L$(MCSIM_PARENT_DIR)/Pthread -lmypthread
INCS += -I${MCSIM_PARENT_DIR}/Apps/hooks
LDFLAGS = $(MCSIM_PARENT_DIR)/Pthread/libmypthread.a

CXXFLAGS = -g --std=c++0x -O3 -static
CXXFLAGS += -Wall #-Werror -static

CUR_DIR = $(shell pwd)

all: $(TRGS) $(LIB_DEPEND)

$(LIB_DEPEND):
	(cd $(MCSIM_PARENT_DIR)/Apps/hooks; $(MAKE); cd $(CUR_DIR))

mac: main.cc
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS) $(INCS) $(LDFLAGS)
ar_mac: main_ar.cc
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS) $(INCS) $(LDFLAGS)

clean:
	rm -f $(TRGS) *.o *.dot
	rm -f file.txt

$(TRGS): $(LIB_DEPEND)

#%: %.cc ;
#	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS) $(INCS) $(LDFLAGS)
