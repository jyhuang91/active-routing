#CC = gcc
AR = ar
RANLIB = ranlib
LIBDIR = lib

INCS = -I${PIN_HOME}/extras/xed2-intel64/include

default: all

all: $(LIBDIR)/libhooks.a

$(LIBDIR)/libhooks.a : hooks.c
	$(CC) -O0 -fPIC -g -c $(INCS) -o hooks.o hooks.c
	$(AR) ru $(LIBDIR)/libhooks.a hooks.o
	$(RANLIB) $(LIBDIR)/libhooks.a
	$(CC) -shared hooks.o -o $(LIBDIR)/libhooks.so 


clean:
	rm -f *.o *.f *~ $(LIBDIR)/libhooks.a $(LIBDIR)/libhooks.so

.PHONY: all clean
