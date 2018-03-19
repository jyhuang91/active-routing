##################################################################################
#  CasHMC v1.2 - 2016.09.27
#  A Cycle-accurate Simulator for Hybrid Memory Cube
#
#  Copyright (c) 2016, Dong-Ik Jeon
#                      Ki-Seok Chung
#                      Hanyang University
#                      estwings57 [at] gmail [dot] com
#  All rights reserved.
##################################################################################

all: cashmc.mk obj
	@$(MAKE) -C . -f cashmc.mk

obj:
	mkdir $@

clean: 
	@$(MAKE) clean -f cashmc.mk 
	-rm -f ${REBUILDABLES} $(SRCDIR)/*.dep 
