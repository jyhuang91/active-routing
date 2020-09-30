TARGET = mypthreadtool

.PHONY: dbg opt depend clean clean_dbg clean_opt rb clean_rb

opt: $(TARGET).mk obj_opt
	@$(MAKE) TAG=opt -C . -f $(TARGET).mk

rb: $(TARGET).mk obj_rb
	@$(MAKE) TAG=rb -C . -f $(TARGET).mk

all: dbg opt

dbg: $(TARGET).mk obj_dbg
	@$(MAKE) TAG=dbg -C . -f $(TARGET).mk

obj_dbg:
	mkdir $@

obj_opt:
	mkdir $@

obj_rb:
	mkdir $@

clean: clean_dbg clean_opt clean_rb

clean_dbg: obj_dbg
	@$(MAKE) TAG=dbg -C . -f $(TARGET).mk clean
	rm -rf $<

clean_opt: obj_opt
	@$(MAKE) TAG=opt -C . -f $(TARGET).mk clean
	rm -rf $<

clean_rb: obj_rb
	@$(MAKE) TAG=rb -C . -f $(TARGET).mk clean
	rm -rf $<
