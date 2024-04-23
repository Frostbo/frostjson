#include "frostjson.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int main_ret = 0;
static int test_count = 0;
static int test_pass = 0;

//测试宏
#define EXPECT_EQ_BASE(equality, expect, actual, format)\
    do {\
        test_count++;\
        if (equality)\
            test_pass++;\
        else {\
            fprintf(stderr, "%s:%d:\nexpect: " format " actual: " format "\n", __FILE__, __LINE__, expect, actual);\
            main_ret = 1;\
        }\
    } while(0)

#define EXPECT_EQ_INT(expect, actual)  EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%d")
#define EXPECT_EQ_DOUBLE(expect, actual)  EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%.17g")

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

#define TEST_NUMBER(expect, json)\
    do {\
        frost_value v;\
        EXPECT_EQ_INT(FROST_PARSE_OK, frost_parse(&v, json));\
        EXPECT_EQ_INT(FROST_NUMBER, frost_get_type(&v));\
        EXPECT_EQ_DOUBLE(expect, frost_get_number(&v));\
    } while(0)

static void test_parse_number(){
    TEST_NUMBER(0.0, "0");
    TEST_NUMBER(0.0, "-0");
    TEST_NUMBER(0.0, "-0.0");
    TEST_NUMBER(1.0, "1");
    TEST_NUMBER(-1.0, "-1");
    TEST_NUMBER(1.5, "1.5");
    TEST_NUMBER(-1.5, "-1.5");
    TEST_NUMBER(3.1416, "3.1416");
    TEST_NUMBER(1E10, "1E10");
    TEST_NUMBER(1e10, "1e10");
    TEST_NUMBER(1E+10, "1E+10");
    TEST_NUMBER(1E-10, "1E-10");
    TEST_NUMBER(-1E10, "-1E10");
    TEST_NUMBER(-1e10, "-1e10");
    TEST_NUMBER(-1E+10, "-1E+10");
    TEST_NUMBER(-1E-10, "-1E-10");
    TEST_NUMBER(1.234E+10, "1.234E+10");
    TEST_NUMBER(1.234E-10, "1.234E-10");
    TEST_NUMBER(0.0, "1e-10000"); 

    /*极限情况检测*/
    TEST_NUMBER(1.0000000000000002, "1.0000000000000002"); 
    TEST_NUMBER( 4.9406564584124654e-324, "4.9406564584124654e-324"); 
    TEST_NUMBER(-4.9406564584124654e-324, "-4.9406564584124654e-324");
    TEST_NUMBER( 2.2250738585072009e-308, "2.2250738585072009e-308");  
    TEST_NUMBER(-2.2250738585072009e-308, "-2.2250738585072009e-308");
    TEST_NUMBER( 2.2250738585072014e-308, "2.2250738585072014e-308");  
    TEST_NUMBER(-2.2250738585072014e-308, "-2.2250738585072014e-308");
    TEST_NUMBER( 1.7976931348623157e+308, "1.7976931348623157e+308");  
    TEST_NUMBER(-1.7976931348623157e+308, "-1.7976931348623157e+308");
}

#define TEST_ERROR(error, json)\
    do {\
        frost_value v;\
        v.type = FROST_FALSE;\
        EXPECT_EQ_INT(error, frost_parse(&v, json));\
        EXPECT_EQ_INT(FROST_NULL, frost_get_type(&v));\
    } while(0)

static void test_parse_expect_value() {
    TEST_ERROR(FROST_PARSE_EXPECT_VALUE, "");
    TEST_ERROR(FROST_PARSE_EXPECT_VALUE, " ");
}

static void test_parse_invalid_value() {
    /* 无效类型 */
    TEST_ERROR(FROST_PARSE_INVALID_VALUE, "nul");
    TEST_ERROR(FROST_PARSE_INVALID_VALUE, "?");

    /* 无效数字 */
    TEST_ERROR(FROST_PARSE_INVALID_VALUE, "+0");
    TEST_ERROR(FROST_PARSE_INVALID_VALUE, "+1");
    TEST_ERROR(FROST_PARSE_INVALID_VALUE, ".123"); /* '.'前面至少有一位 */
    TEST_ERROR(FROST_PARSE_INVALID_VALUE, "1.");   /* '.'后面至少有一位 */
    TEST_ERROR(FROST_PARSE_INVALID_VALUE, "INF");
    TEST_ERROR(FROST_PARSE_INVALID_VALUE, "inf");
    TEST_ERROR(FROST_PARSE_INVALID_VALUE, "NAN");
    TEST_ERROR(FROST_PARSE_INVALID_VALUE, "nan");
}

static void test_parse_root_not_singular() {
    TEST_ERROR(FORST_PARSE_ROOT_NOT_SINGULAR, "null x");

    TEST_ERROR(FORST_PARSE_ROOT_NOT_SINGULAR, "0123"); /* 0开头的特殊情况 */
    TEST_ERROR(FORST_PARSE_ROOT_NOT_SINGULAR, "0x0");
    TEST_ERROR(FORST_PARSE_ROOT_NOT_SINGULAR, "0x123");
}

static void test_parse_number_too_big() {
    TEST_ERROR(FROST_PARSE_NUMBER_TOO_BIG, "1e309");
    TEST_ERROR(FROST_PARSE_NUMBER_TOO_BIG, "-1e309");
}

static void test_parse() {
    test_parse_null();
    test_parse_true();
    test_parse_false();
    test_parse_number();
    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();
    test_parse_number_too_big();
}

auto main() -> int {
    test_parse();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    return main_ret;
}