TARGET = mcsim

.PHONY: dbg opt depend clean clean_dbg clean_opt

opt: $(TARGET).mk obj_opt
	@$(MAKE) -C ../CasHMC/sources
	@$(MAKE) TAG=opt -C . -f $(TARGET).mk

all: dbg opt

dbg: $(TARGET).mk obj_dbg
	@$(MAKE) -C ../CasHMC/sources
	@$(MAKE) TAG=dbg -C . -f $(TARGET).mk

obj_dbg:
	mkdir $@

obj_opt:
	mkdir $@

clean: clean_dbg clean_opt

clean_dbg: obj_dbg
	@$(MAKE) -C ../CasHMC/sources clean
	@$(MAKE) TAG=dbg -C . -f $(TARGET).mk clean
	rm -rf $<

clean_opt: obj_opt
	@$(MAKE) -C ../CasHMC/sources clean
	@$(MAKE) TAG=opt -C . -f $(TARGET).mk clean
	rm -rf $<

