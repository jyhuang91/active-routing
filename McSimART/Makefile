TARGET = mcsim

.PHONY: dbg opt depend clean clean_dbg clean_opt rb clean_rb

opt: $(TARGET).mk obj_opt
	@$(MAKE) -C ../CasHMC/sources
	@$(MAKE) TAG=opt -C . -f $(TARGET).mk

rb: $(TARGET).mk obj_rb
	@$(MAKE) -C ../CasHMC/sources
	@$(MAKE) TAG=rb -C . -f $(TARGET).mk

all: dbg opt

dbg: $(TARGET).mk obj_dbg
	@$(MAKE) -C ../CasHMC/sources
	@$(MAKE) TAG=dbg -C . -f $(TARGET).mk

obj_dbg:
	mkdir $@

obj_opt:
	mkdir $@

obj_rb:
	mkdir $@

clean: clean_dbg clean_opt clean_rb

clean_dbg: obj_dbg
	@$(MAKE) -C ../CasHMC/sources clean
	@$(MAKE) TAG=dbg -C . -f $(TARGET).mk clean
	rm -rf $<

clean_opt: obj_opt
	@$(MAKE) -C ../CasHMC/sources clean
	@$(MAKE) TAG=opt -C . -f $(TARGET).mk clean
	rm -rf $<

clean_rb: obj_rb
	@$(MAKE) -C ../CasHMC/sources clean
	@$(MAKE) TAG=rb -C . -f $(TARGET).mk clean
	rm -rf $<

clean_mcsim: clean_mcsim_dbg clean_mcsim_opt

clean_mcsim_dbg: obj_dbg
	@$(MAKE) TAG=dbg -C . -f $(TARGET).mk clean
	rm -rf $<

clean_mcsim_opt: obj_opt
	@$(MAKE) TAG=opt -C . -f $(TARGET).mk clean
	rm -rf $<

clean_cashmc:
	@$(MAKE) -C ../CasHMC/sources clean
