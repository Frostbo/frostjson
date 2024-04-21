#include "frostjson.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int main_ret = 0;
static int test_count = 0;
static int test_pass = 0;

//测试宏
#define EXPECT_EQ_BASE(equality, expect, actual)   do {\
        test_count++;\
        if (equality)\
            test_pass++;\
        else {\
            fprintf(stderr, "%s:%d:\nexpect: %d actual: %d\n", __FILE__, __LINE__, expect, actual);\
            main_ret = 1;\
        }\
    } while(0)

#define EXPECT_EQ_INT(expect, actual)  EXPECT_EQ_BASE((expect) == (actual), expect, actual)

static void test_parse_null() {
    frost_value val;
    val.type = FROST_FALSE;
    EXPECT_EQ_INT(FROST_PARSE_OK, frost_parse(&val, "null"));
    EXPECT_EQ_INT(FROST_NULL, frost_get_type(&val));
}

static void test_parse_true() {
    frost_value val;
    val.type = FROST_FALSE;
    EXPECT_EQ_INT(FROST_PARSE_OK, frost_parse(&val, "true"));
    EXPECT_EQ_INT(FROST_TRUE, frost_get_type(&val));
}

static void test_parse_false() {
    frost_value val;
    val.type = FROST_TRUE;
    EXPECT_EQ_INT(FROST_PARSE_OK, frost_parse(&val, "false"));
    EXPECT_EQ_INT(FROST_FALSE, frost_get_type(&val));
}
/* ... */

static void test_parse() {
    test_parse_null();
    test_parse_true();
    test_parse_false();
    /* ... */
}

auto main() -> int {
    test_parse();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    return main_ret;
}