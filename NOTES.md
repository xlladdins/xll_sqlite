# fms_sqlite

C++ wrapper for the sqlite C API.

## Type

Sqlite has [flexible typing](https://www3.sqlite.org/flextypegood.html).  
It uses 64-bit signed integers, 64-bit IEEE floating point values, character strings, 
BLOBs, or NULL 
[internally](https://www3.sqlite.org/c3ref/c_blob.html).  
These correspond to `SQLITE_INTEGER`, `SQLITE_FLOAT`, `SQLITE_TEXT`, `SQLITE_BLOB`,
and `SQLITE_NULL` respectively. 
The function `sqlite3_column_type` returns the internal type currently being used.
Note that database operatons might change the internal representation.

Flexible typing makes it easy to use sqlite as a key-value store, but not so convenient
for getting data in and out of sqlite while preserving their original types.

When creating a sqlite table it is possible to specify any of the usual
SQL data types specified in the 
[Affinity Name Examples](https://www.sqlite.org/datatype3.html#affinity_name_examples).
The function `sqlite3_column_decltype` returns the character string used
and `int sqlite::stmt::sqltype(int i)` returns the **extended sqlite type**
based on string used when creating a table.

The extended types `SQLITE_BOOLEAN` and `SQLITE_DATETIME` are defined 
in the `fms_sqlite.h` header to preserve Excel and JSON/BSON fidelity.
Excel does not have a datetime type, JSON has neither, and BSON has both.
A sqlite boolean is stored as an integer. The datetime type is a union
that can contain a floating point Gegorian date, integer `time_t` seconds
since Unix epoch, or an ISO 8601 format string.

```
stmt.prepare("SELECT * from table");
iterator iter(stmt);
while(i) {
	const stmt& row = *i;
	*r = *row;
	++i;
}
```

Getting data in and out of sqlite is just copying an iterable.

A SELECT statement creates an output iterable `sqlite::stmt`.

An INSERT statement creates an input iterable `sqlite::stmt`.

`copy(I i, O o) { while (i and o) { *o++ = *i++} }`

Override `operator*` to return proxies.