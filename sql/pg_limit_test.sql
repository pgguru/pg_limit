-- basic sanity check that we show all rows
SELECT generate_series(1,10);

-- test basic max_rows functionality
SET pg_limit.max_rows = 5;

-- less than threshold; no notify
SELECT generate_series(1,4);

-- boundary condition; no notify
SELECT generate_series(1,5);

-- this should be truncated to 5 rows; notify
SELECT generate_series(1,10);

-- now check the notify_total_count flag handling
SET pg_limit.notify_total_count = true;

-- less than threshold; no notify
SELECT generate_series(1,4);

-- boundary condition; no notify
SELECT generate_series(1,5);

-- this should be truncated to 5 rows; notify with total
SELECT generate_series(1,10);
