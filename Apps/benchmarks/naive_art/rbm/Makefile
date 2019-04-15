CC = gcc
LDFLAGS = -lm -L$(MCSIM_PARENT_DIR)/Apps/hooks/lib -lhooks -L$(MCSIM_PARENT_DIR)/Pthread -lmypthread

INCLUDES = -I$(CURDIR)/includes -I$(CURDIR) -I$(MCSIM_PARENT_DIR)/Apps/hooks/include
COMPILE_C = $(CC) $(CFLAGS) -O2 $(INCLUDES)
COMMON_SRC := $(wildcard $(CURDIR)/*.c)
C_SRC := $(wildcard $(CURDIR)/includes/*.c)

compile: rbm #small medium large

run-all: run-small run-medium run-large

rbm: $(C_SRC)
	$(COMPILE_C) $(COMMON_SRC) $(C_SRC) -o rbm $(LDFLAGS)

#small: $(C_SRC)
#	$(COMPILE_C) $(COMMON_SRC) $(C_SRC) -DUSERS=10 -DTEST_USERS=10 -DMOVIES=10 -DLOOPS=20 -o rbm-small $(LDFLAGS)
#
#medium: $(C_SRC)
#	$(COMPILE_C) $(COMMON_SRC) $(C_SRC) -DUSERS=100 -DTEST_USERS=100 -DMOVIES=100 -DLOOPS=20 -o rbm-medium $(LDFLAGS)
#
#large: $(C_SRC)
#	$(COMPILE_C) $(COMMON_SRC) $(C_SRC) -DUSERS=100 -DTEST_USERS=100 -DMOVIES=100 -DLOOPS=200 -o rbm-large $(LDFLAGS)

run-small:
	-./rbm -u 10 -t 10 -m 10 -l 20 | tee ../results/rbm.small

run-medium:
	-./rbm -u 100 -t 100 -m 100 -l 20 | tee ../results/rbm.medium

run-large:
	-./rbm -u 100 -t 100 -m 100 -l 200 | tee ../results/rbm.large

clean:
	-rm rbm #rbm-small rbm-medium rbm-large out.txt
