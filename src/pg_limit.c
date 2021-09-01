#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/guc.h"

PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

/* stub prototypes */
static void pg_limit_ExecutorStart(QueryDesc *queryDesc, int eflags);
static void pg_limit_ExecutorRun(QueryDesc *queryDesc,
							 ScanDirection direction,
							 uint64 count, bool execute_once);
static void pg_limit_ExecutorFinish(QueryDesc *queryDesc);
static void pg_limit_ExecutorEnd(QueryDesc *queryDesc);
static bool pg_limit_proxy_slot(TupleTableSlot *slot, DestReceiver *self);

/* Saved hook values in case of unload */
static ExecutorStart_hook_type prev_ExecutorStart = NULL;
static ExecutorRun_hook_type prev_ExecutorRun = NULL;
static ExecutorFinish_hook_type prev_ExecutorFinish = NULL;
static ExecutorEnd_hook_type prev_ExecutorEnd = NULL;

/* how many rows to limit our return to; the default 0 means all rows */
static int max_rows = 0;
static int n_tuples = 0;

/* stored "real" DestReceiver receiveSlot function pointer */
bool (*originalSlot) (TupleTableSlot *slot, DestReceiver *self);

void _PG_init(void)
{
	DefineCustomIntVariable("pg_limit.max_rows",
						"Sets the maximum number of rows returned per query (0 means no limit)",
						NULL,
						&max_rows,
						0,
						0,
						INT_MAX,
						PGC_USERSET,
						0,
						NULL,
						NULL,
						NULL);


	/* install hooks */
	prev_ExecutorStart = ExecutorStart_hook;
	ExecutorStart_hook = pg_limit_ExecutorStart;
	prev_ExecutorRun = ExecutorRun_hook;
	ExecutorRun_hook = pg_limit_ExecutorRun;
	prev_ExecutorFinish = ExecutorFinish_hook;
	ExecutorFinish_hook = pg_limit_ExecutorFinish;
	prev_ExecutorEnd = ExecutorEnd_hook;
	ExecutorEnd_hook = pg_limit_ExecutorEnd;
}

void _PG_fini(void)
{
	/* restore hooks */
	ExecutorStart_hook = prev_ExecutorStart;
	ExecutorRun_hook = prev_ExecutorRun;
	ExecutorFinish_hook = prev_ExecutorFinish;
	ExecutorEnd_hook = prev_ExecutorEnd;
}


/*
 * ExecutorStart hook placeholder
 */
static void
pg_limit_ExecutorStart(QueryDesc *queryDesc, int eflags)
{
	if (prev_ExecutorStart)
		prev_ExecutorStart(queryDesc, eflags);
	else
		standard_ExecutorStart(queryDesc, eflags);
}

/*
 * ExecutorRun hook; clamp to max_rows
 */
static void
pg_limit_ExecutorRun(QueryDesc *queryDesc, ScanDirection direction, uint64 count,
				 bool execute_once)
{
	uint64 limit = \
		count == 0 ? max_rows :
		count > max_rows ? max_rows : count;

	/* here we add 1 if we are not unlimited; the proxy slot will make sure we don't exceed the
	   number we want, but we use +1 here so we can tell if we would have consumed more than the
	   limit */
	limit = limit ? limit + 1 : 0;

	/* stash original receiveSlot for proxy use */
	originalSlot = *queryDesc->dest->receiveSlot;

	/* replace with our proxy routine */
	queryDesc->dest->receiveSlot = pg_limit_proxy_slot;

	/* verify we aren't self-recursive; that would be bad */
	Assert(originalSlot != pg_limit_proxy_slot);

	/* reset tuple counter */
	n_tuples = 0;

	/* make sure we clean up the proxy pointer; maybe being overly cautious, but seems like good
	   behavior since we munge it above */
	PG_TRY();
	{
		if (prev_ExecutorRun)
			prev_ExecutorRun(queryDesc, direction, limit, execute_once);
		else
			standard_ExecutorRun(queryDesc, direction, limit, execute_once);
	}
	PG_FINALLY();
	{
		queryDesc->dest->receiveSlot = originalSlot;
		originalSlot = NULL;
	}
	PG_END_TRY();
}

/*
 * ExecutorFinish hook placeholder
 */
static void
pg_limit_ExecutorFinish(QueryDesc *queryDesc)
{
	if (prev_ExecutorFinish)
		prev_ExecutorFinish(queryDesc);
	else
		standard_ExecutorFinish(queryDesc);
}

/*
 * ExecutorEnd hook placeholder
 */
static void
pg_limit_ExecutorEnd(QueryDesc *queryDesc)
{
	if (prev_ExecutorEnd)
		prev_ExecutorEnd(queryDesc);
	else
		standard_ExecutorEnd(queryDesc);
}

/*
 * Proxy function to limit to the max_rows and call original output slot method.  We raise a notice in this case if we hit the limit.
 */
static bool pg_limit_proxy_slot(TupleTableSlot *slot, DestReceiver *self) {
	Assert(originalSlot);

	if (max_rows && n_tuples == max_rows)
	{
		ereport(NOTICE, (errmsg("pg_limit: result set was truncated to the first %d rows", max_rows)));
		return false;
	}

	n_tuples++;

	return originalSlot(slot, self);
}
