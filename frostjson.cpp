#include "frostjson.h"
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdlib>

#define EXPECT(c, ch)             \
    do {                          \
        assert(*c->json == (ch)); \
        c->json++;                \
    } while (0)

using frost_context = struct {
    const char* json;
};

static void frost_parse_whitespace(frost_context* cot)
{
    const char* par = cot->json;
    while (*par == ' ' || *par == '\t' || *par == '\n' || *par == '\r')
        par++;
    cot->json = par;
}

static auto frost_parse_literal(frost_context* cot, frost_value* val, const char* literal, frost_type type) -> int
{
    size_t i = 0;
    EXPECT(cot, literal[0]);
    for (i = 0; literal[i + 1] != 0; i++) {
        if (cot->json[i] != literal[i + 1]) {
            return FROST_PARSE_INVALID_VALUE;
        }
    }
    cot->json += i;
    val->type = type;
    return FROST_PARSE_OK;
}

static auto frost_parse_number(frost_context* cot, frost_value* val) -> int
{
    const char* end = cot->json;
    if (*end == '-') end++;
    if (*end == '0') end++;
    else {
        if (isdigit(*end) == 0) 
            return FROST_PARSE_INVALID_VALUE;
        while (isdigit(*end) != 0)
            end++;
    }
    if (*end == '.') {
        end++;
        if (isdigit(*end) == 0) 
            return FROST_PARSE_INVALID_VALUE;
        while (isdigit(*end) != 0)
            end++;
    }
    if (*end == 'e' || *end == 'E') {
        end++;
        if (*end == '+' || *end == '-') end++;
        if (isdigit(*end) == 0) 
            return FROST_PARSE_INVALID_VALUE;
        while (isdigit(*end) != 0) 
            end++;
    }
    errno = 0;
    val->num = strtod(cot->json, nullptr);
    if (errno == ERANGE && (val->num == HUGE_VAL || val->num == -HUGE_VAL)) {
        return FROST_PARSE_NUMBER_TOO_BIG;
    }
    cot->json = end;
    val->type = FROST_NUMBER;
    return FROST_PARSE_OK;
}

static auto frost_parse_value(frost_context* cot, frost_value* val) -> int
{
    switch (*cot->json) {
    case 'n':
        return frost_parse_literal(cot, val, "null", FROST_NULL);
    case 't':
        return frost_parse_literal(cot, val, "true", FROST_TRUE);
    case 'f':
        return frost_parse_literal(cot, val, "false", FROST_FALSE);
    default:
        return frost_parse_number(cot, val);
    case '\0':
        return FROST_PARSE_EXPECT_VALUE;
    }
}

auto frost_parse(frost_value* val, const char* json) -> int
{
    frost_context cot;
    int ret = 0;
    assert(val != nullptr);
    cot.json = json;
    val->type = FROST_NULL;
    frost_parse_whitespace(&cot);
    ret = frost_parse_value(&cot, val);
    if (ret == FROST_PARSE_OK) {
        frost_parse_whitespace(&cot);
        if (*cot.json != '\0') {
            val->type = FROST_NULL;
            ret = FORST_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

auto frost_get_type(const frost_value* val) -> frost_type
{
    assert(val != nullptr);
    return val->type;
}

auto frost_get_number(const frost_value* val) -> double
{
    assert(val != nullptr && val->type == FROST_NUMBER);
    return val->num;
}

/* JSON-text = ws value ws
在这个 JSON 语法子集下，我们定义 3 种错误码：
若一个 JSON 只含有空白，传回 LEPT_PARSE_EXPECT_VALUE。
若一个值之后，在空白之后还有其他字符，传回 LEPT_PARSE_ROOT_NOT_SINGULAR。
若值不是那三种字面值，传回 LEPT_PARSE_INVALID_VALUE。
/value = null / false / true / number
*/