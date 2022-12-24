// fms_sqlite.cpp - test platform independent sqlite
#include <cassert>
#include "../fms_sqlite.h"

int test_sqlite = sqlite::stmt::test();
int test_value = sqlite::value::test();

int main()
{
	return 0;
}