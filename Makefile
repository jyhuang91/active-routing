.PHONY: dbg opt rb clean

opt: mcsim_opt pthread_opt benchmarks_opt

dbg: mcsim_dbg pthread_dbg benchmarks_dbg

apps: hooks benchmarks

all: dbg opt

mcsim_opt:
	@$(MAKE) -C McSimART opt

mcsim_dbg:
	@$(MAKE) -C McSimART dbg

pthread_opt: hooks
	@$(MAKE) -C Pthread opt

pthread_dbg: hooks
	@$(MAKE) -C Pthread dbg

hooks:
	@$(MAKE) -C Apps/hooks

benchmarks_opt: pthread_opt hooks
	@$(MAKE) -C Apps/benchmarks

benchmarks_dbg: pthread_dbg hooks
	@$(MAKE) -C Apps/benchmarks

clean:
	@$(MAKE) -C McSimART clean
	@$(MAKE) -C Pthread clean
	@$(MAKE) -C Apps/hooks clean
	@$(MAKE) -C Apps/benchmarks clean
