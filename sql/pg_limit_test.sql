SELECT generate_series(1,10);
SET pg_limit.max_rows = 5;
SELECT generate_series(1,10);
SET pg_limit.notify_total_count = true;
SELECT generate_series(1,10);
