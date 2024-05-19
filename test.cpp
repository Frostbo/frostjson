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
#define EXPECT_EQ_STRING(expect, actual, alength) \
    EXPECT_EQ_BASE(sizeof(expect) - 1 == alength && memcmp(expect, actual, alength) == 0, expect, actual, "%s")
#define EXPECT_TRUE(actual) EXPECT_EQ_BASE((actual) != 0, "true", "false", "%s")
#define EXPECT_FALSE(actual) EXPECT_EQ_BASE((actual) == 0, "false", "true", "%s")


#if defined(_MSC_VER)
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ_BASE((expect) == (actual), (size_t)expect, (size_t)actual, "%Iu")
#else
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ_BASE((expect) == (actual), (size_t)expect, (size_t)actual, "%zu")
#endif

static void test_parse_null() {
    frost_value val;
    val.type = FROST_FALSE;
    frost_set_boolean(&val, 0);
    EXPECT_EQ_INT(FROST_PARSE_OK, frost_parse(&val, "null"));
    EXPECT_EQ_INT(FROST_NULL, frost_get_type(&val));
    frost_free(&val);
}

static void test_parse_true() {
    frost_value val;
    val.type = FROST_FALSE;
    frost_set_boolean(&val, 0);
    EXPECT_EQ_INT(FROST_PARSE_OK, frost_parse(&val, "true"));
    EXPECT_EQ_INT(FROST_TRUE, frost_get_type(&val));
    frost_free(&val);
}

static void test_parse_false() {
    frost_value val;
    val.type = FROST_TRUE;
    frost_set_boolean(&val, 1);
    EXPECT_EQ_INT(FROST_PARSE_OK, frost_parse(&val, "false"));
    EXPECT_EQ_INT(FROST_FALSE, frost_get_type(&val));
    frost_free(&val);
}

#define TEST_NUMBER(expect, json)\
    do {\
        frost_value v;\
        frost_init(&v);\
        EXPECT_EQ_INT(FROST_PARSE_OK, frost_parse(&v, json));\
        EXPECT_EQ_INT(FROST_NUMBER, frost_get_type(&v));\
        EXPECT_EQ_DOUBLE(expect, frost_get_number(&v));\
        frost_free(&v);\
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

#define TEST_STRING(expect, json)\
    do {\
        frost_value v;\
        frost_init(&v);\
        EXPECT_EQ_INT(FROST_PARSE_OK, frost_parse(&v, json));\
        EXPECT_EQ_INT(FROST_STRING, frost_get_type(&v));\
        EXPECT_EQ_STRING(expect, frost_get_string(&v), frost_get_string_length(&v));\
        frost_free(&v);\
    } while(0)

static void test_parse_string() {
    TEST_STRING("", "\"\"");
    TEST_STRING("Hello", "\"Hello\"");
    TEST_STRING("Hello\nWorld", "\"Hello\\nWorld\"");
    TEST_STRING("\" \\ / \b \f \n \r \t", "\"\\\" \\\\ \\/ \\b \\f \\n \\r \\t\"");
    TEST_STRING("Hello\0World", "\"Hello\\u0000World\"");
    TEST_STRING("\x24", "\"\\u0024\"");         /* 测试美元符号 */
    TEST_STRING("\xC2\xA2", "\"\\u00A2\"");     /* 测试分符号 */
    TEST_STRING("\xE2\x82\xAC", "\"\\u20AC\""); /* 测试欧元符号 */
    TEST_STRING("\xF0\x9D\x84\x9E", "\"\\uD834\\uDD1E\"");  /* 测试G clef符号 */
    TEST_STRING("\xF0\x9D\x84\x9E", "\"\\ud834\\udd1e\"");  
}

static void test_parse_array() {
    size_t i, j;
    frost_value val;

    frost_init(&val);
    EXPECT_EQ_INT(FROST_PARSE_OK, frost_parse(&val, "[ ]"));
    EXPECT_EQ_INT(FROST_ARRAY, frost_get_type(&val));
    EXPECT_EQ_SIZE_T(0, frost_get_array_size(&val));
    frost_free(&val);

    frost_init(&val);
    EXPECT_EQ_INT(FROST_PARSE_OK, frost_parse(&val, "[ null , false , true , 123 , \"abc\" ]"));
    EXPECT_EQ_INT(FROST_ARRAY, frost_get_type(&val));
    EXPECT_EQ_SIZE_T(5, frost_get_array_size(&val));
    EXPECT_EQ_INT(FROST_NULL,  frost_get_type(frost_get_array_element(&val, 0)));
    EXPECT_EQ_INT(FROST_FALSE,  frost_get_type(frost_get_array_element(&val, 1)));
    EXPECT_EQ_INT(FROST_TRUE,   frost_get_type(frost_get_array_element(&val, 2)));
    EXPECT_EQ_INT(FROST_NUMBER, frost_get_type(frost_get_array_element(&val, 3)));
    EXPECT_EQ_INT(FROST_STRING, frost_get_type(frost_get_array_element(&val, 4)));
    EXPECT_EQ_DOUBLE(123.0, frost_get_number(frost_get_array_element(&val, 3)));
    EXPECT_EQ_STRING("abc", frost_get_string(frost_get_array_element(&val, 4)), frost_get_string_length(frost_get_array_element(&val, 4)));
    frost_free(&val);

    frost_init(&val);
    EXPECT_EQ_INT(FROST_PARSE_OK, frost_parse(&val, "[ [ ] , [ 0 ] , [ 0 , 1 ] , [ 0 , 1 , 2 ] ]"));
    EXPECT_EQ_INT(FROST_ARRAY, frost_get_type(&val));
    EXPECT_EQ_SIZE_T(4, frost_get_array_size(&val));
    for (i = 0; i < 4; i++) {
        frost_value* a = frost_get_array_element(&val, i);
        EXPECT_EQ_INT(FROST_ARRAY, frost_get_type(a));
        EXPECT_EQ_SIZE_T(i, frost_get_array_size(a));
        for (j = 0; j < i; j++) {
            frost_value* e = frost_get_array_element(a, j);
            EXPECT_EQ_INT(FROST_NUMBER, frost_get_type(e));
            EXPECT_EQ_DOUBLE((double)j, frost_get_number(e));
        }
    }
    frost_free(&val);
}

static void test_parse_object() {
    frost_value v;
    size_t i;

    frost_init(&v);
    EXPECT_EQ_INT(FROST_PARSE_OK, frost_parse(&v, " { } "));
    EXPECT_EQ_INT(FROST_OBJECT, frost_get_type(&v));
    EXPECT_EQ_SIZE_T(0, frost_get_object_size(&v));
    frost_free(&v);

    frost_init(&v);
    EXPECT_EQ_INT(FROST_PARSE_OK, frost_parse(&v,
        " { "
        "\"n\" : null , "
        "\"f\" : false , "
        "\"t\" : true , "
        "\"i\" : 123 , "
        "\"s\" : \"abc\", "
        "\"a\" : [ 1, 2, 3 ],"
        "\"o\" : { \"1\" : 1, \"2\" : 2, \"3\" : 3 }"
        " } "
    ));
    EXPECT_EQ_INT(FROST_OBJECT, frost_get_type(&v));
    EXPECT_EQ_SIZE_T(7, frost_get_object_size(&v));
    EXPECT_EQ_STRING("n", frost_get_object_key(&v, 0), frost_get_object_key_length(&v, 0));
    EXPECT_EQ_INT(FROST_NULL,   frost_get_type(frost_get_object_value(&v, 0)));
    EXPECT_EQ_STRING("f", frost_get_object_key(&v, 1), frost_get_object_key_length(&v, 1));
    EXPECT_EQ_INT(FROST_FALSE,  frost_get_type(frost_get_object_value(&v, 1)));
    EXPECT_EQ_STRING("t", frost_get_object_key(&v, 2), frost_get_object_key_length(&v, 2));
    EXPECT_EQ_INT(FROST_TRUE,   frost_get_type(frost_get_object_value(&v, 2)));
    EXPECT_EQ_STRING("i", frost_get_object_key(&v, 3), frost_get_object_key_length(&v, 3));
    EXPECT_EQ_INT(FROST_NUMBER, frost_get_type(frost_get_object_value(&v, 3)));
    EXPECT_EQ_DOUBLE(123.0, frost_get_number(frost_get_object_value(&v, 3)));
    EXPECT_EQ_STRING("s", frost_get_object_key(&v, 4), frost_get_object_key_length(&v, 4));
    EXPECT_EQ_INT(FROST_STRING, frost_get_type(frost_get_object_value(&v, 4)));
    EXPECT_EQ_STRING("abc", frost_get_string(frost_get_object_value(&v, 4)), frost_get_string_length(frost_get_object_value(&v, 4)));
    EXPECT_EQ_STRING("a", frost_get_object_key(&v, 5), frost_get_object_key_length(&v, 5));
    EXPECT_EQ_INT(FROST_ARRAY, frost_get_type(frost_get_object_value(&v, 5)));
    EXPECT_EQ_SIZE_T(3, frost_get_array_size(frost_get_object_value(&v, 5)));
    for (i = 0; i < 3; i++) {
        frost_value* e = frost_get_array_element(frost_get_object_value(&v, 5), i);
        EXPECT_EQ_INT(FROST_NUMBER, frost_get_type(e));
        EXPECT_EQ_DOUBLE(i + 1.0, frost_get_number(e));
    }
    EXPECT_EQ_STRING("o", frost_get_object_key(&v, 6), frost_get_object_key_length(&v, 6));
    {
        frost_value* o = frost_get_object_value(&v, 6);
        EXPECT_EQ_INT(FROST_OBJECT, frost_get_type(o));
        for (i = 0; i < 3; i++) {
            frost_value* ov = frost_get_object_value(o, i);
            EXPECT_TRUE('1' + i == frost_get_object_key(o, i)[0]);
            EXPECT_EQ_SIZE_T(1, frost_get_object_key_length(o, i));
            EXPECT_EQ_INT(FROST_NUMBER, frost_get_type(ov));
            EXPECT_EQ_DOUBLE(i + 1.0, frost_get_number(ov));
        }
    }
    frost_free(&v);
}

#define TEST_ERROR(error, json)\
    do {\
        frost_value v;\
        frost_init(&v);\
        v.type = FROST_FALSE;\
        EXPECT_EQ_INT(error, frost_parse(&v, json));\
        EXPECT_EQ_INT(FROST_NULL, frost_get_type(&v));\
        frost_free(&v);\
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

    /* 无效数组 */
    TEST_ERROR(FROST_PARSE_INVALID_VALUE, "[1,]");
    TEST_ERROR(FROST_PARSE_INVALID_VALUE, "[\"a\", nul]");
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

static void test_parse_missing_quotation_mark() {
    TEST_ERROR(FROST_PARSE_MISS_QUOTATION_MARK, "\"");
    TEST_ERROR(FROST_PARSE_MISS_QUOTATION_MARK, "\"abc");
}

static void test_parse_invalid_string_escape() {
    TEST_ERROR(FROST_PARSE_INVALID_STRING_ESCAPE, "\"\\v\"");
    TEST_ERROR(FROST_PARSE_INVALID_STRING_ESCAPE, "\"\\'\"");
    TEST_ERROR(FROST_PARSE_INVALID_STRING_ESCAPE, "\"\\0\"");
    TEST_ERROR(FROST_PARSE_INVALID_STRING_ESCAPE, "\"\\x12\"");
}

static void test_parse_invalid_string_char() {
    TEST_ERROR(FROST_PARSE_INVALID_STRING_CHAR, "\"\x01\"");
    TEST_ERROR(FROST_PARSE_INVALID_STRING_CHAR, "\"\x1F\"");
}

static void test_parse_invalid_unicode_hex() {
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_HEX, "\"\\u\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_HEX, "\"\\u0\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_HEX, "\"\\u01\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_HEX, "\"\\u012\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_HEX, "\"\\u/000\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_HEX, "\"\\uG000\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_HEX, "\"\\u0/00\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_HEX, "\"\\u0G00\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_HEX, "\"\\u00/0\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_HEX, "\"\\u00G0\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_HEX, "\"\\u000/\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_HEX, "\"\\u000G\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_HEX, "\"\\u 123\"");
}

static void test_parse_invalid_unicode_surrogate() {
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uDBFF\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\\\\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\uDBFF\"");
    TEST_ERROR(FROST_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\uE000\"");
}

static void test_parse_miss_comma_or_square_bracket() {
    TEST_ERROR(FROST_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[1");
    TEST_ERROR(FROST_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[1}");
    TEST_ERROR(FROST_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[1 2");
    TEST_ERROR(FROST_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[[]");
}

static void test_parse_miss_key() {
    TEST_ERROR(FROST_PARSE_MISS_KEY, "{:1,");
    TEST_ERROR(FROST_PARSE_MISS_KEY, "{1:1,");
    TEST_ERROR(FROST_PARSE_MISS_KEY, "{true:1,");
    TEST_ERROR(FROST_PARSE_MISS_KEY, "{false:1,");
    TEST_ERROR(FROST_PARSE_MISS_KEY, "{null:1,");
    TEST_ERROR(FROST_PARSE_MISS_KEY, "{[]:1,");
    TEST_ERROR(FROST_PARSE_MISS_KEY, "{{}:1,");
    TEST_ERROR(FROST_PARSE_MISS_KEY, "{\"a\":1,");
}

static void test_parse_miss_colon() {
    TEST_ERROR(FROST_PARSE_MISS_COLON, "{\"a\"}");
    TEST_ERROR(FROST_PARSE_MISS_COLON, "{\"a\",\"b\"}");
}

static void test_parse_miss_comma_or_curly_bracket() {
    TEST_ERROR(FROST_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1");
    TEST_ERROR(FROST_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1]");
    TEST_ERROR(FROST_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1 \"b\"");
    TEST_ERROR(FROST_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":{}");
}

static void test_parse() {
    test_parse_null();
    test_parse_true();
    test_parse_false();
    test_parse_number();
    test_parse_string();
    test_parse_array(); 
    test_parse_object();

    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();
    test_parse_number_too_big();
    test_parse_missing_quotation_mark();
    test_parse_invalid_string_escape();
    test_parse_invalid_string_char();
    test_parse_invalid_unicode_hex();
    test_parse_invalid_unicode_surrogate();
    test_parse_miss_comma_or_square_bracket();
    test_parse_miss_key();
    test_parse_miss_colon();
    test_parse_miss_comma_or_curly_bracket();

}

static void test_access_null() {
    frost_value val;
    frost_init(&val);
    frost_set_string(&val, "a", 1);
    frost_set_null(&val);
    EXPECT_EQ_INT(FROST_NULL, frost_get_type(&val));
    frost_free(&val);
}

static void test_access_boolean() {
    frost_value val;
    frost_init(&val);
    frost_set_string(&val, "a", 1);
    frost_set_boolean(&val, 1);
    EXPECT_TRUE(frost_get_boolean(&val));
    frost_set_boolean(&val, 0);
    EXPECT_FALSE(frost_get_boolean(&val));
    frost_free(&val);
}

static void test_access_number() {
    frost_value val;
    frost_init(&val);
    frost_set_string(&val, "a", 1);
    frost_set_number(&val, 1234.5);
    EXPECT_EQ_DOUBLE(1234.5, frost_get_number(&val));
    frost_free(&val);
}

static void test_access_string() {
    frost_value val;
    frost_init(&val);
    frost_set_string(&val, "", 0);
    EXPECT_EQ_STRING("", frost_get_string(&val), frost_get_string_length(&val));
    frost_set_string(&val, "Hello", 5);
    EXPECT_EQ_STRING("Hello", frost_get_string(&val), frost_get_string_length(&val));
    frost_free(&val);
}

static void test_access() {
    test_access_null();
    test_access_boolean();
    test_access_number();
    test_access_string();
}


auto main() -> int {
    test_parse();
    test_access();  
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    return main_ret;
}