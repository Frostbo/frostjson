#include "frostjson.h"
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#ifndef FROST_PARSE_STACK_INIT_SIZE
#define FROST_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch)             \
    do {                          \
        assert(*c->json == (ch)); \
        c->json++;                \
    } while (0)

#define PUTC(c, ch)                                         \
    do {                                                    \
        *(char*)frost_context_push(c, sizeof(char)) = (ch); \
    } while (0)

using frost_context = struct {
    const char* json;
    char* stack;
    size_t size, top;
};

static auto frost_context_push(frost_context* cot, size_t size) -> void*
{
    void* ret = nullptr;
    assert(size > 0);
    if (cot->top + size >= cot->size) {
        if (cot->size == 0)
            cot->size = FROST_PARSE_STACK_INIT_SIZE;
        while (cot->top + size >= cot->size)
            cot->size += cot->size >> 1; /* c->size * 1.5 */
        cot->stack = (char*)realloc(cot->stack, cot->size);
    }
    ret = cot->stack + cot->top;
    cot->top += size;
    return ret;
}

static auto frost_context_pop(frost_context* c, size_t size) -> void*
{
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

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
    if (*end == '-')
        end++;
    if (*end == '0')
        end++;
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
        if (*end == '+' || *end == '-')
            end++;
        if (isdigit(*end) == 0)
            return FROST_PARSE_INVALID_VALUE;
        while (isdigit(*end) != 0)
            end++;
    }
    errno = 0;
    val->u.n = strtod(cot->json, nullptr);
    if (errno == ERANGE && (val->u.n == HUGE_VAL || val->u.n == -HUGE_VAL)) {
        return FROST_PARSE_NUMBER_TOO_BIG;
    }
    cot->json = end;
    val->type = FROST_NUMBER;
    return FROST_PARSE_OK;
}

/*读取4位16进制数字*/
static auto frost_parse_hex4(const char* end, unsigned* uns) -> const char*
{
    int i = 0;
    *uns = 0;
    for (i = 0; i < 4; i++) {
        char ch = *end++;
        *uns <<= 4;
        if (ch >= '0' && ch <= '9')
            *uns |= ch - '0';
        else if (ch >= 'A' && ch <= 'F')
            *uns |= ch - ('A' - 10);
        else if (ch >= 'a' && ch <= 'f')
            *uns |= ch - ('a' - 10);
        else
            return nullptr;
    }
    return end;
}

static void frost_encode_utf8(frost_context* cot, unsigned uns)
{
    if (uns <= 0x7F)
        PUTC(cot, uns & 0xFF);
    else if (uns <= 0x7FF) {
        PUTC(cot, 0xC0 | ((uns >> 6) & 0xFF));
        PUTC(cot, 0x80 | (uns & 0x3F));
    } else if (uns <= 0xFFFF) {
        PUTC(cot, 0xE0 | ((uns >> 12) & 0xFF));
        PUTC(cot, 0x80 | ((uns >> 6) & 0x3F));
        PUTC(cot, 0x80 | (uns & 0x3F));
    } else {
        assert(uns <= 0x10FFFF);
        PUTC(cot, 0xF0 | ((uns >> 18) & 0xFF));
        PUTC(cot, 0x80 | ((uns >> 12) & 0x3F));
        PUTC(cot, 0x80 | ((uns >> 6) & 0x3F));
        PUTC(cot, 0x80 | (uns & 0x3F));
    }
}

#define STRING_ERROR(ret) \
    do {                  \
        cot->top = head;    \
        return ret;       \
    } while (0)

static auto frost_parse_string(frost_context* cot, frost_value* val) -> int
{
    size_t head = cot->top;
    size_t len = 0;
    unsigned uns = 0;
    unsigned usi = 0;
    const char* end = nullptr;
    EXPECT(cot, '\"');
    end = cot->json;
    for (;;) {
        char ch = *end++;
        switch (ch) {
        case '\"':
            len = cot->top - head;
            frost_set_string(val, (const char*)frost_context_pop(cot, len), len);
            cot->json = end;
            return FROST_PARSE_OK;
        case '\\':
            switch (*end++) {
            case '\"':
                PUTC(cot, '\"');
                break;
            case '\\':
                PUTC(cot, '\\');
                break;
            case '/':
                PUTC(cot, '/');
                break;
            case 'b':
                PUTC(cot, '\b');
                break;
            case 'f':
                PUTC(cot, '\f');
                break;
            case 'n':
                PUTC(cot, '\n');
                break;
            case 'r':
                PUTC(cot, '\r');
                break;
            case 't':
                PUTC(cot, '\t');
                break;
            case 'u':
                if ((end = frost_parse_hex4(end, &uns)) == nullptr)
                    STRING_ERROR(FROST_PARSE_INVALID_UNICODE_HEX);
                if (uns >= 0xD800 && uns <= 0xDBFF) { 
                    if (*end++ != '\\')
                        STRING_ERROR(FROST_PARSE_INVALID_UNICODE_SURROGATE);
                    if (*end++ != 'u')
                        STRING_ERROR(FROST_PARSE_INVALID_UNICODE_SURROGATE);
                    if ((end = frost_parse_hex4(end, &usi)) == nullptr)
                        STRING_ERROR(FROST_PARSE_INVALID_UNICODE_HEX);
                    if (usi < 0xDC00 || usi > 0xDFFF)
                        STRING_ERROR(FROST_PARSE_INVALID_UNICODE_SURROGATE);
                    uns = (((uns - 0xD800) << 10) | (usi - 0xDC00)) + 0x10000;
                }
                frost_encode_utf8(cot, uns);
                break;
            default:
                cot->top = head;
                return FROST_PARSE_INVALID_STRING_ESCAPE;
            }
            break;
        case '\0':
            cot->top = head;
            return FROST_PARSE_MISS_QUOTATION_MARK;
        default:
            if ((unsigned char)ch < 0x20) {
                cot->top = head;
                return FROST_PARSE_INVALID_STRING_CHAR;
            }
            PUTC(cot, ch);
        }
    }
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
    case '"':
        return frost_parse_string(cot, val);
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
    cot.stack = nullptr;
    cot.size = cot.top = 0;
    frost_init(val);
    frost_parse_whitespace(&cot);
    ret = frost_parse_value(&cot, val);
    if (ret == FROST_PARSE_OK) {
        frost_parse_whitespace(&cot);
        if (*cot.json != '\0') {
            val->type = FROST_NULL;
            ret = FORST_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(cot.top == 0);
    free(cot.stack);
    return ret;
}

void frost_free(frost_value* val)
{
    assert(val != nullptr);
    if (val->type == FROST_STRING)
        free(val->u.s.s);
    val->type = FROST_NULL;
}

auto frost_get_type(const frost_value* val) -> frost_type
{
    assert(val != nullptr);
    return val->type;
}

auto frost_get_boolean(const frost_value* val) -> int
{
    assert(val != nullptr && (val->type == FROST_TRUE || val->type == FROST_FALSE));
    return val->type == FROST_TRUE;
}

void frost_set_boolean(frost_value* val, int bol)
{
    frost_free(val);
    val->type = (bol != 0) ? FROST_TRUE : FROST_FALSE;
}

auto frost_get_number(const frost_value* val) -> double
{
    assert(val != nullptr && val->type == FROST_NUMBER);
    return val->u.n;
}

void frost_set_number(frost_value* val, double num)
{
    frost_free(val);
    val->u.n = num;
    val->type = FROST_NUMBER;
}

auto frost_get_string(const frost_value* val) -> const char*
{
    assert(val != nullptr && val->type == FROST_STRING);
    return val->u.s.s;
}

auto frost_get_string_length(const frost_value* val) -> size_t
{
    assert(val != nullptr && val->type == FROST_STRING);
    return val->u.s.len;
}

void frost_set_string(frost_value* val, const char* str, size_t len)
{
    assert(val != nullptr && (str != nullptr || len == 0));
    frost_free(val);
    val->u.s.s = (char*)malloc(len + 1);
    memcpy(val->u.s.s, str, len);
    val->u.s.s[len] = '\0';
    val->u.s.len = len;
    val->type = FROST_STRING;
}

/* JSON-text = ws value ws
在这个 JSON 语法子集下，我们定义 3 种错误码：
若一个 JSON 只含有空白，传回 LEPT_PARSE_EXPECT_VALUE。
若一个值之后，在空白之后还有其他字符，传回 LEPT_PARSE_ROOT_NOT_SINGULAR。
若值不是那三种字面值，传回 LEPT_PARSE_INVALID_VALUE。
/value = null / false / true / number
*/