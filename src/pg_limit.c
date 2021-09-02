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
static bool notify_total_count = false;

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

	DefineCustomBoolVariable("pg_limit.notify_total_count",
							 "Show total original row count if hit limit",
							 NULL,
							 &notify_total_count,
							 false,
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
	if (!max_rows)
	{
		/* short-circuit if we are not doing limit on rows */

		if (prev_ExecutorRun)
			prev_ExecutorRun(queryDesc, direction, count, execute_once);
		else
			standard_ExecutorRun(queryDesc, direction, count, execute_once);
	}
	else
    {
		/*
		 * Calculate the upper bound for the number of rows we want to actually inspect.  If we are
		 * notifying the total count we just use the count we were given, otherwise we clamp to
		 * max_rows + 1 so we can tell if we would have consumed more than the limit.
		 */

		uint64 limit = \
			notify_total_count ? count :
			count == 0 ? max_rows + 1 :
			count > max_rows ? max_rows + 1 : count;

		/* stash original receiveSlot for proxy use */
		originalSlot = *queryDesc->dest->receiveSlot;

		/* replace with our proxy routine */
		queryDesc->dest->receiveSlot = pg_limit_proxy_slot;

		/* verify we aren't self-recursive; that would be bad */
		Assert(originalSlot != pg_limit_proxy_slot);

		/* reset tuple counter */
		n_tuples = 0;

		PG_TRY();
		{
			if (prev_ExecutorRun)
				prev_ExecutorRun(queryDesc, direction, limit, execute_once);
			else
				standard_ExecutorRun(queryDesc, direction, limit, execute_once);
		}
		PG_FINALLY();
		{
			/* make sure we clean up the proxy pointer; maybe being overly cautious, but seems like good
			 * behavior since we munge it above */

			queryDesc->dest->receiveSlot = originalSlot;
			originalSlot = NULL;
		}
		PG_END_TRY();

		if (notify_total_count && n_tuples > max_rows)
			ereport(NOTICE, (errmsg("pg_limit: result set was truncated to the first %d rows (had %d rows total)", max_rows, n_tuples)));
	}
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

	/* if no tuple is returned, we stop now */
	if (!slot)
		return false;

	/* if we are limiting the output to a fixed number and we exceed the max and we want to notify on first exceeding */
	if (max_rows && n_tuples >= max_rows && !notify_total_count)
	{
		ereport(NOTICE, (errmsg("pg_limit: result set was truncated to the first %d rows", max_rows)));
		return false;
	}

	/* increment counter unconditionally */
	n_tuples++;

	/* do original output if unlimited or if we are less than the boundary */
	if (!max_rows || n_tuples <= max_rows)
		return originalSlot(slot, self);

	/* we hit this code path if we would have consumed a tuple but we are dropping it; do nothing but increment the counter */
	return true;
}
