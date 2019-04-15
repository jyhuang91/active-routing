# C compiler
CC = gcc
#CC_FLAGS = -g -fopenmp  -O2
# for host machine
#CC_FLAGS = -pthread -O2 -static
# for mcsim
CC_FLAGS = -O2 -static

TARGET=backprop
OBJS=backprop.o facetrain.o imagenet.o backprop_kernel.o

LIB_DEP=hooks

LIBS += -lrt
LIBS += -L$(MCSIM_PARENT_DIR)/Apps/hooks/lib -lhooks
LIBS += -L$(MCSIM_PARENT_DIR)/Pthread -lmypthread
INCS += -I$(MCSIM_PARENT_DIR)/Apps/hooks/include
  
all: $(OBJS)
	$(CC) $(CC_FLAGS) $(OBJS) $(LIBS) $(INCS) -o $(TARGET) -lm

$(LIB_DEP): (cd $(MCSIM_PARENT_DIR)/Apps/hooks; $(MAKE); cd $(CUR_DIR))

$(TARGET): $(LIB_DEP)

%.o: %.[ch]
	$(CC) $(CC_FLAGS) -c $< -o $@ $(LIBS) $(INCS)

facetrain.o: facetrain.c backprop.h
	$(CC) $(CC_FLAGS) facetrain.c -c
	
backprop.o: backprop.c backprop.h
	$(CC) $(CC_FLAGS) $(INCS) backprop.c -c

backprop_kernel.o: backprop_kernel.c backprop.h
	$(CC) $(CC_FLAGS) backprop_kernel.c -c

imagenet.o: imagenet.c backprop.h
	$(CC) $(CC_FLAGS) imagenet.c -c


clean:
	rm -f *.o *~ backprop backprop_cuda.linkinfo
