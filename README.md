# xll_sqlite

Use [sqlite](https://www.sqlite.org/) from Excel.

Open and/or create a sqlite database using `\SQL.DB(file, options)`.
If no arguments are specified a temporary in-memory database is created.
[`SQLITE_OPEN_XXX()`](https://www.sqlite.org/c3ref/c_open_autoproxy.html) 
enumerations are provided for `options`.

Get the big picture with [`SQL.SCHEMA(db)`](https://www.sqlite.org/schematab.html).
The common pragmas [`SQL.TABLE_LIST(db)`](https://www.sqlite.org/pragma.html#pragma_table_list)
and [`SQL.TABLE_INFO(db)`](https://www.sqlite.org/pragma.html#pragma_table_info)
are provided in addition to [`SQL.PRAGMA(db, pragma)`](https://www.sqlite.org/pragma.html)
that calls `PRAGMA pragma`.

Call [`SQL.QUERY(db, sql)`](https://www.sqlite.org/c3ref/query.html) to return
the result of executing `sql`. 

You can create a sqlite statement with `SQL.STMT(db)`
and use the result as the first argument to 
[`SQL.PREPARE(stmt, sql)`](https://www.sqlite.org/c3ref/prepare.html).
Bind parameter values with [`SQL.BIND(stmd, range)`](https://www.sqlite.org/c3ref/bind_blob.html)
where `range` is one-dimensional to specify positional parameters or a two columns
range of key-value pairs to bind based on the key name. The type of binding is
based on each value's Excel type.
Statements are executed with [`SQL.EXEC(stmt)`](https://www.sqlite.org/c3ref/exec.html).

Sqlite tables are created from 
[`SQL.TABLE(db, name, data, _columns, _types)`](https://www.sqlite.org/lang_createtable.html).
The `_columns` and `_types` arguments are optional. If `_columns` are not specified then
the first row of `data` is used for column names. The allowed `_types` are those specified
in the [Affinity Name Examples](https://www.sqlite.org/datatype3.html#affinity_name_examples).
If table `name` exists it is dropped before being recreated.

It is also possible to create tables from a query using 
[`SQL.CREATE_TABLE_AS(db, name, stmt)`](https://www.sqlite.org/lang_createtable.html).
The new table will contain the result of executing the statement.

## FAQ

<dl>

<dt>Do I have to do the sweep out ranges, F2, Ctrl-Shift-Enter, rinse and repeat to
get the output of range-valued functions?</dt>
<dd>Not if you use a version of Excel with 
[dynamic arrays](https://techcommunity.microsoft.com/t5/excel-blog/preview-of-dynamic-arrays-in-excel/ba-p/252944)
</dd>

<dt>What are those funny numbers coming out of commands that start with a backslash (`\`)?</dt>
<dd>They are  64-bit
floating point numbers with the same bits as the address in memory of the underlying C++ object.
See [handles](https://github.com/xlladdins/xll#handle).
</dd>

<dt>My SQL queries are too big to put in one cell.</dt>
<dd>You can spread the query over a range of cells. The values are concatenated
using a space character before being sent to sqlite.</dd>

<dt>How did you create this add-in?</dt>
<dd>Using my [xll](https://github.com/xlladdins/xll) library.</dd>
</dl>
