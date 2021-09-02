MODULE_big = pg_limit
OBJS = src/pg_limit.o 
PG_CONFIG ?= pg_config
PG_CFLAGS := -Wno-missing-prototypes -Wno-deprecated-declarations -Wno-unused-result
REGRESS = pg_limit_test
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
