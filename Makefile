EXTENSION = pg_limit
MODULE_big = pg_limit
DATA = sql/*.sql
OBJS = src/pg_limit.o 
PG_CONFIG ?= pg_config
PG_CFLAGS := -Wno-missing-prototypes -Wno-deprecated-declarations -Wno-unused-result
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
