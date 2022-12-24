# Notes

Sqlite uses [flexible typing](https://www3.sqlite.org/flextypegood.html).  
It uses integers, floating point values, strings, BLOBs, or NULL 
[behind the scenes](https://www3.sqlite.org/c3ref/c_blob.html).  
These correspond to `SQLITE_INTEGER`, `SQLITE_FLOAT`, `SQLITE_TEXT`, `SQLITE_BLOB`,
and `SQLITE_NULL` respectively. A column can hold data of any of these types.  
This makes it easy to use sqlite as a key-value store, but not so convenient
for getting data in and out of sqlite while preserving the original type.

`SQLITE_BOOLEAN` and `SQLITE_DATETIME` are defined in the platform
independent `fms_sqlite.h` header to preserve Excel and JSON/BSON fidelity.
Excel does not have a datetime type and JSON has neither. BSON has both.

When creating a sqlite table it is possible to specify any of the usual
SQL data types specified in the 
[Affinity Name Examples](https://www.sqlite.org/datatype3.html#affinity_name_examples).
The function `sqlite3_column_decltype` returns the character string used
and `int sqlite::stmt::sqltype(int i)` returns the extended sqlite type.

Getting data in and out of sqlite is just copying an iterable.

A SELECT statement creates an output iterable `sqlite::stmt`.

An INSERT statement creates an input iterable `sqlite::stmt`.

`copy(i, o) { while (i and o) { *o = *i} }`

Override `operator*` to return proxies.