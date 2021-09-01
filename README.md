# pg_limit

Provide some limits on backends.  For now, a POC of a limit for the number of rows returned by a query.  May expand to more in the future.

## Installation

```console
$ git clone git@github.com:pgguru/pg_limit.git
$ cd pg_limit
$ export PG_CONFIG=path/to/pg_config
$ make install
$ psql -c 'CREATE EXTENSION pg_limit' -U <user> -d <database>
```

## Usage

You will need to add `pg_limit` to your `postgresql.conf`'s `shared_preload_libraries`.

You can set the number of rows to return in the `pg_limit.max_rows` variable, which will stop at this number of rows.  At this time, there is no notice that the query result set was truncated, so use carefully.  This is a userset limit at this time, so doesn't serve much to actually limit things, but we could tighten this up, then you could do something like this:

```sql
ALTER ROLE limited SET pg_limit.max_rows = 1000; -- or whatever
```

