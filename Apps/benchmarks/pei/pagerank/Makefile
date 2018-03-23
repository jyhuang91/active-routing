
TRGS = pagerank pagerank_lock

LIB_DEPEND = hooks

#LIBS += -lpthread -lrt
LIBS += -lrt
LIBS += -L$(MCSIM_PARENT_DIR)/Apps/hooks/lib -lhooks
LIBS += -L$(MCSIM_PARENT_DIR)/Pthread -lmypthread
INCS += -I$(MCSIM_PARENT_DIR)/Apps/hooks

CXXFLAGS = -g --std=c++0x -static #-O3 -static
CXXFLAGS += -Wall -Werror

CUR_DIR = $(shell pwd)

all: $(TRGS) $(LIB_DEPEND)

$(LIB_DEPEND):
	(cd $(MCSIM_PARENT_DIR)/Apps/hooks; $(MAKE); cd $(CUR_DIR))

pagerank: pagerank.cc
pagerank_lock: pagerank_lock.cc

clean:
	rm -f $(TRGS) *.o *.dot
	rm -f file.txt

$(TRGS): $(LIB_DEPEND)

%: %.cc ;
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS) $(INCS)
