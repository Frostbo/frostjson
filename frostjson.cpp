/*
在这个 JSON 语法子集下，我们定义 3 种错误码：
若一个 JSON 只含有空白，传回 LEPT_PARSE_EXPECT_VALUE。
若一个值之后，在空白之后还有其他字符，传回 LEPT_PARSE_ROOT_NOT_SINGULAR。
若值不是那三种字面值，传回 LEPT_PARSE_INVALID_VALUE。
*/
#include "frostjson.h"
#include <cassert>
#include <cstdlib>

#define EXPECT(c, ch)  do { assert(*c->json == (ch)); c->json++; } while(0)

using frost_context = struct{
    const char* json;
};

/* ws = *(%x20 / %x09 / %x0A / %x0D) */
static void frost_parse_whitespace(frost_context* cot) {
    const char *par = cot->json;
    while (*par == ' ' || *par == '\t' || *par == '\n' || *par == '\r')
        par++;
    cot->json = par;
}

/* null  = "null" */
static auto frost_parse_null(frost_context* cot, frost_value* val) -> int {
    EXPECT(cot, 'n');
    if (cot->json[0] != 'u' || cot->json[1] != 'l' || cot->json[2] != 'l')
        return FROST_PARSE_INVALID_VALUE;
    cot->json += 3;
    val->type = FROST_NULL;
    return FROST_PARSE_OK;
}

/* true  = "true" */
static auto frost_parse_true(frost_context* cot, frost_value* val) -> int {
    EXPECT(cot, 't');
    if (cot->json[0] != 'r' || cot->json[1] != 'u' || cot->json[2] != 'e')
        return FROST_PARSE_INVALID_VALUE;
    cot->json += 3;
    val->type = FROST_TRUE;
    return FROST_PARSE_OK;
}

/* true  = "false" */
static auto frost_parse_false(frost_context* cot, frost_value* val) -> int {
    EXPECT(cot, 'f');
    if (cot->json[0] != 'a' || cot->json[1] != 'l' || cot->json[2] != 's' || cot->json[3] != 'e')
        return FROST_PARSE_INVALID_VALUE;
    cot->json += 4;
    val->type = FROST_FALSE;
    return FROST_PARSE_OK;
}

/* value = null / false / true */
static auto frost_parse_value(frost_context* cot, frost_value* val) -> int {
    switch (*cot->json) {
        case 'n':  return frost_parse_null(cot, val);
        case 't':  return frost_parse_true(cot, val);
        case 'f':  return frost_parse_false(cot, val);
        case '\0': return FROST_PARSE_EXPECT_VALUE;
        default:   return FROST_PARSE_INVALID_VALUE;
    }
}

/* JSON-text = ws value ws */
auto frost_parse(frost_value* val, const char* json) -> int {
    frost_context cot;
    int ret = 0;
    assert(val != nullptr);
    cot.json = json;
    val->type = FROST_NULL;
    frost_parse_whitespace(&cot);
    ret = frost_parse_value(&cot, val);
    if(ret == FROST_PARSE_OK){
        frost_parse_whitespace(&cot);
        if(*cot.json != '\0'){
             ret = FORST_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

auto frost_get_type(const frost_value* val) -> frost_type{
    assert(val != nullptr);
    return val->type;
}