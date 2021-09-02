# pg_limit

Provide some limits on backends.  For now, a POC of a limit for the number of rows returned by a query.  May expand to more in the future.

## Installation

```console
$ git clone git@github.com:pgguru/pg_limit.git
$ cd pg_limit
$ export PG_CONFIG=path/to/pg_config
$ make install
$ # add to shared_preload_libraries
```

## Usage

You will need to add `pg_limit` to your `postgresql.conf`'s `shared_preload_libraries`.

You can set the number of rows to return in the `pg_limit.max_rows` variable, which will stop returning results at this number of rows.  Currently the `pg_limit.max_rows` is limited to an integer, so you can only effectively set this parameter to <= `INT_MAX`.

If the result set was truncated, a `NOTICE` will be set to that effect.  If the `pg_limit.notify_total_count` variable is true, this notice will include the total record count that *would* have been returned.


## Examples

```sql
ALTER ROLE limited SET pg_limit.max_rows = 1000; -- or whatever
```

## TODO

- This currently limits all queries, including ones which would access system tables, COPY statements, etc.  Adjust to limit to only queries that return directly to backends, or at least make this handling selective as to which queries it affects.

## Testing

Uses PGXS-standard `installcheck` action:

```console
$ export PG_CONFIG=path/to/pg_config
$ make installcheck
```

This will create a new temporary instance, add the shared library to `shared_preload_libraries`, then run the regression tests.
