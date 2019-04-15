# (c) 2010 The Board of Trustees of the University of Illinois.

TARGET=sgemm

LIB_DEPEND=hooks

LANGUAGE=c
SRCDIR_OBJS=main.o io.o sgemm_kernel.o parboil.o
#APP_CFLAGS= -pthread
#APP_CXXFLAGS= -pthread
APP_CFLAGS= -static -O3
APP_CXXFLAGS= -static -O3
APP_LDFLAGS=-lstdc++

LIBS += -L$(MCSIM_PARENT_DIR)/Apps/hooks/lib -lhooks
LIBS += -L$(MCSIM_PARENT_DIR)/Pthread -lmypthread
INCS += -I$(MCSIM_PARENT_DIR)/Apps/hooks/include

CUR_DIR=$(shell pwd)

all: $(TARGET) $(LIB_DEPEND)

$(TARGET): $(SRCDIR_OBJS) $(LIB_DEPEND)
	$(CXX) $(APP_CFLAGS) $(APP_LDFLAGS) -o $@ $(SRCDIR_OBJS) $(LIBS) $(INCS)

$(LIB_DEPEND):
	(cd $(MCSIM_PARENT_DIR)/Apps/hooks; $(MAKE); cd $(CUR_DIR))

%.o: %.c
	$(CC) $(APP_CFLAGS) $(APP_LDFLAGS) -c $< -o $@ $(LIBS) $(INCS)

%.o: %.cc
	$(CXX) $(APP_CXXFLAGS) $(APP_LDFLAGS) -c $< -o $@ $(LIBS) $(INCS)

clean:
	rm -f sgemm *.o
