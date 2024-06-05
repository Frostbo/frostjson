#include "frostjson.h"
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdio.h>

#ifndef FROST_PARSE_STACK_INIT_SIZE
#define FROST_PARSE_STACK_INIT_SIZE 256
#endif
#ifndef FROST_PARSE_STRINGIFY_INIT_SIZE
#define FROST_PARSE_STRINGIFY_INIT_SIZE 256
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

#define PUTS(c, s, len) memcpy(frost_context_push(c, len), s, len)

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

/* 返回指定位置数据 */
static auto frost_context_pop(frost_context* cot, size_t size) -> void*
{
    assert(cot->top >= size);
    return cot->stack + (cot->top -= size);
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
        cot->top = head;  \
        return ret;       \
    } while (0)

static auto frost_parse_string_raw(frost_context* cot, char** str, size_t* len) -> int
{
    size_t head = cot->top;
    unsigned uns = 0;
    unsigned usi = 0;
    const char* end = nullptr;
    EXPECT(cot, '\"');
    end = cot->json;
    for (;;) {
        char ch = *end++;
        switch (ch) {
        case '\"':
            *len = cot->top - head;
            *str = static_cast<char*>(frost_context_pop(cot, *len));
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

static auto frost_parse_string(frost_context* cot, frost_value* val) -> int
{
    int ret = 0;
    char* str = nullptr;
    size_t len = 0;
    ret = frost_parse_string_raw(cot, &str, &len);
    if (ret == FROST_PARSE_OK)
        frost_set_string(val, str, len);
    return ret;
}

static auto frost_parse_value(frost_context* cot, frost_value* val) -> int;
static auto frost_parse_array(frost_context* cot, frost_value* val) -> int
{
    size_t i = 0;
    size_t size = 0;
    int ret;
    EXPECT(cot, '[');
    frost_parse_whitespace(cot);
    if (*cot->json == ']') {
        cot->json++;
        val->type = FROST_ARRAY;
        val->u.a.size = 0;
        val->u.a.e = nullptr;
        return FROST_PARSE_OK;
    }
    for (;;) {
        frost_value cac;
        frost_init(&cac);
        ret = frost_parse_value(cot, &cac);
        if (ret != FROST_PARSE_OK)
            break;
        memcpy(frost_context_push(cot, sizeof(frost_value)), &cac, sizeof(frost_value));
        size++;
        frost_parse_whitespace(cot);
        if (*cot->json == ',') {
            cot->json++;
            frost_parse_whitespace(cot);
        } else if (*cot->json == ']') {
            cot->json++;
            val->type = FROST_ARRAY;
            val->u.a.size = size;
            size *= sizeof(frost_value);
            val->u.a.e = (frost_value*)malloc(size);
            memcpy(val->u.a.e, frost_context_pop(cot, size), size);
            return FROST_PARSE_OK;
        } else {
            ret = FROST_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }
    for (i = 0; i < size; i++) {
        auto* value = (frost_value*)frost_context_pop(cot, sizeof(frost_value));
        frost_free(value);
    }
    return ret;
}

static auto frost_parse_object(frost_context* cot, frost_value* val) -> int
{
    size_t size = 0;
    size_t i = 0;
    frost_member mem;
    int ret = 0;
    EXPECT(cot, '{');
    frost_parse_whitespace(cot);
    if (*cot->json == '}') {
        cot->json++;
        val->type = FROST_OBJECT;
        val->u.o.m = 0;
        val->u.o.size = 0;
        return FROST_PARSE_OK;
    }
    mem.k = nullptr;
    size = 0;
    for (;;) {
        char* str = nullptr;
        frost_init(&mem.v);
        if (*cot->json != '"') {
            ret = FROST_PARSE_MISS_KEY;
            break;
        }
        ret = frost_parse_string_raw(cot, &str, &mem.klen);
        if (ret != FROST_PARSE_OK)
            break;
        mem.k = (char*)malloc(mem.klen + 1);
        memcpy(mem.k, str, mem.klen);
        mem.k[mem.klen] = '\0';
        frost_parse_whitespace(cot);
        if (*cot->json != ':') {
            ret = FROST_PARSE_MISS_COLON;
            break;
        }
        cot->json++;
        frost_parse_whitespace(cot);
        ret = frost_parse_value(cot, &mem.v);
        if (ret != FROST_PARSE_OK)
            break;
        memcpy(frost_context_push(cot, sizeof(frost_member)), &mem, sizeof(frost_member));
        size++;
        mem.k = nullptr;
        frost_parse_whitespace(cot);
        if (*cot->json == ',') {
            cot->json++;
            frost_parse_whitespace(cot);
        } else if (*cot->json == '}') {
            size_t sit = sizeof(frost_member) * size;
            cot->json++;
            val->type = FROST_OBJECT;
            val->u.o.size = size;
            val->u.o.m = (frost_member*)malloc(sit);
            memcpy(val->u.o.m, frost_context_pop(cot, sit), sit);
            return FROST_PARSE_OK;
        } else {
            ret = FROST_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }
    free(mem.k);
    for (i = 0; i < size; i++) {
        auto* mbe = (frost_member*)frost_context_pop(cot, sizeof(frost_member));
        free(mbe->k);
        frost_free(&mbe->v);
    }
    val->type = FROST_NULL;
    return ret;
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
    case '[':
        return frost_parse_array(cot, val);
    case '{':
        return frost_parse_object(cot, val);
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

static void frost_stringify_string(frost_context* cot, const char* str, size_t len)
{
    static const char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    size_t i = 0;
    size_t size = 0;
    char *head = nullptr;
    char *next = nullptr;
    assert(str != nullptr);
    size = len * 6 + 2;
    next = head = (char*)frost_context_push(cot, size); 
    *next++ = '"';
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)str[i];
        switch (ch) {
        case '\"':
            *next++ = '\\';
            *next++ = '\"';
            break;
        case '\\':
            *next++ = '\\';
            *next++ = '\\';
            break;
        case '\b':
            *next++ = '\\';
            *next++ = 'b';
            break;
        case '\f':
            *next++ = '\\';
            *next++ = 'f';
            break;
        case '\n':
            *next++ = '\\';
            *next++ = 'n';
            break;
        case '\r':
            *next++ = '\\';
            *next++ = 'r';
            break;
        case '\t':
            *next++ = '\\';
            *next++ = 't';
            break;
        default:
            if (ch < 0x20) {
                *next++ = '\\';
                *next++ = 'u';
                *next++ = '0';
                *next++ = '0';
                *next++ = hex_digits[ch >> 4];
                *next++ = hex_digits[ch & 15];
            } else
                *next++ = str[i];
        }
    }
    *next++ = '"';
    cot->top -= size - (next - head);
}

static void frost_stringify_value(frost_context* cot, const frost_value* val)
{
    size_t i = 0;
    switch (val->type) {
    case FROST_NULL:
        PUTS(cot, "null", 4);
        break;
    case FROST_FALSE:
        PUTS(cot, "false", 5);
        break;
    case FROST_TRUE:
        PUTS(cot, "true", 4);
        break;
    case FROST_NUMBER: 
    {
        char* ch = (char*)frost_context_push(cot, 32);
        cot->top -= 32 - sprintf(ch, "%.17g", val->u.n);
    }
        break;
    case FROST_STRING:
        frost_stringify_string(cot, val->u.s.s, val->u.s.len);
        break;
    case FROST_ARRAY:
        PUTC(cot, '[');
        for (i = 0; i < val->u.a.size; i++) {
            if (i > 0)
                PUTC(cot, ',');
            frost_stringify_value(cot, &val->u.a.e[i]);
        }
        PUTC(cot, ']');
        break;
    case FROST_OBJECT:
        PUTC(cot, '{');
        for (i = 0; i < val->u.o.size; i++) {
            if (i > 0)
                PUTC(cot, ',');
            frost_stringify_string(cot, val->u.o.m[i].k, val->u.o.m[i].klen);
            PUTC(cot, ':');
            frost_stringify_value(cot, &val->u.o.m[i].v);
        }
        PUTC(cot, '}');
        break;
    default:
        assert(0 && "invalid type");
    }
}

auto frost_stringify(const frost_value* val, size_t* length) -> char*
{
    frost_context cot;
    assert(val != nullptr);
    cot.stack = (char*)malloc(cot.size = FROST_PARSE_STRINGIFY_INIT_SIZE);
    cot.top = 0;
    frost_stringify_value(&cot, val);
    if (length != nullptr)
        *length = cot.top;
    PUTC(&cot, '\0');
    return cot.stack;
}

void frost_copy(frost_value* dst, const frost_value* src) {
    assert(src != nullptr && dst != nullptr && src != dst);
    size_t i;
    switch (src->type) {
        case FROST_STRING:
            frost_set_string(dst, src->u.s.s, src->u.s.len);
            break;
        case FROST_ARRAY:
            frost_set_array(dst, src->u.a.size);
            for(i = 0; i < src->u.a.size; i++){
                frost_copy(&dst->u.a.e[i], &src->u.a.e[i]);
            }
            dst->u.a.size = src->u.a.size;
            break;
        case FROST_OBJECT:
            frost_set_object(dst, src->u.o.size);
            for(i = 0; i < src->u.o.size; i++){
                frost_value * val = frost_set_object_value(dst, src->u.o.m[i].k, src->u.o.m[i].klen);
                frost_copy(val, &src->u.o.m[i].v);
            }
            dst->u.o.size = src->u.o.size;
            break;
        default:
            frost_free(dst);
            memcpy(dst, src, sizeof(frost_value));
            break;
    }
}

void frost_move(frost_value* dst, frost_value* src) {
    assert(dst != nullptr && src != nullptr && src != dst);
    frost_free(dst);
    memcpy(dst, src, sizeof(frost_value));
    frost_init(src);
}

void frost_swap(frost_value* lhs, frost_value* rhs) {
    assert(lhs != nullptr && rhs != nullptr);
    if (lhs != rhs) {
        frost_value temp;
        memcpy(&temp, lhs, sizeof(frost_value));
        memcpy(lhs,   rhs, sizeof(frost_value));
        memcpy(rhs, &temp, sizeof(frost_value));
    }
}

void frost_free(frost_value* val)
{
    size_t i;
    assert(val != nullptr);
    switch (val->type) {
    case FROST_STRING:
        free(val->u.s.s);
        break;
    case FROST_ARRAY:
        for (i = 0; i < val->u.a.size; i++)
            frost_free(&val->u.a.e[i]);
        free(val->u.a.e);
        break;
    case FROST_OBJECT:
        for (i = 0; i < val->u.o.size; i++) {
            free(val->u.o.m[i].k);
            frost_free(&val->u.o.m[i].v);
        }
        free(val->u.o.m);
        break;
    default:
        break;
    }
    val->type = FROST_NULL;
}

auto frost_get_type(const frost_value* val) -> frost_type
{
    assert(val != nullptr);
    return val->type;
}

auto frost_is_equal(const frost_value* lhs, const frost_value* rhs) -> int {
    size_t i = 0;
    size_t index = 0;
    assert(lhs != nullptr && rhs != nullptr);
    if (lhs->type != rhs->type)
        return 0;
    switch (lhs->type) {
        case FROST_STRING:
            return lhs->u.s.len == rhs->u.s.len && 
                memcmp(lhs->u.s.s, rhs->u.s.s, lhs->u.s.len) == 0;
        case FROST_NUMBER:
            return lhs->u.n == rhs->u.n;
        case FROST_ARRAY:
            if (lhs->u.a.size != rhs->u.a.size)
                return 0;
            for (i = 0; i < lhs->u.a.size; i++)
                if (frost_is_equal(&lhs->u.a.e[i], &rhs->u.a.e[i]) == 0)
                    return 0;
            return 1;
        case FROST_OBJECT:
            if(lhs->u.o.size != rhs->u.o.size)
                return 0;
            for(i = 0; i < lhs->u.o.size; i++){
                index = frost_find_object_index(rhs, lhs->u.o.m[i].k, lhs->u.o.m[i].klen);
                if(index == FROST_KEY_NOT_EXIST)
                    return 0;
                if (frost_is_equal(&lhs->u.o.m[i].v, &rhs->u.o.m[index].v) == 0)
                    return 0;
            }
            return 1;
        default:
            return 1;
    }
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

void frost_set_array(frost_value* val, size_t capacity) {
    assert(val != nullptr);
    frost_free(val);
    val->type = FROST_ARRAY;
    val->u.a.size = 0;
    val->u.a.capacity = capacity;
    val->u.a.e = capacity > 0 ? (frost_value*)malloc(capacity * sizeof(frost_value)) : nullptr;
}


auto frost_get_array_size(const frost_value* val) -> size_t
{
    assert(val != nullptr && val->type == FROST_ARRAY);
    return val->u.a.size;
}

auto frost_get_array_capacity(const frost_value* val) -> size_t {
    assert(val != nullptr && val->type == FROST_ARRAY);
    return val->u.a.capacity;
}

void frost_reserve_array(frost_value* val, size_t capacity) {
    assert(val != nullptr && val->type == FROST_ARRAY);
    if (val->u.a.capacity < capacity) {
        val->u.a.capacity = capacity;
        val->u.a.e = (frost_value*)realloc(val->u.a.e, capacity * sizeof(frost_value));
    }
}

void frost_shrink_array(frost_value* val) {
    assert(val != nullptr && val->type == FROST_ARRAY);
    if (val->u.a.capacity > val->u.a.size) {
        val->u.a.capacity = val->u.a.size;
        val->u.a.e = (frost_value*)realloc(val->u.a.e, val->u.a.capacity * sizeof(frost_value));
    }
}

void frost_clear_array(frost_value* val) {
    assert(val != nullptr && val->type == FROST_ARRAY);
    frost_erase_array_element(val, 0, val->u.a.size);
}

auto frost_get_array_element(frost_value* val, size_t index) -> frost_value* {
    assert(val != nullptr && val->type == FROST_ARRAY);
    assert(index < val->u.a.size);
    return &val->u.a.e[index];
}

/*添加*/
auto frost_pushback_array_element(frost_value* val) -> frost_value* {
    assert(val != nullptr && val->type == FROST_ARRAY);
    if (val->u.a.size == val->u.a.capacity)
        frost_reserve_array(val, val->u.a.capacity == 0 ? 1 : val->u.a.capacity * 2);
    frost_init(&val->u.a.e[val->u.a.size]);
    return &val->u.a.e[val->u.a.size++];
}

/*弹出*/
void frost_popback_array_element(frost_value* val) {
    assert(val != nullptr && val->type == FROST_ARRAY && val->u.a.size > 0);
    frost_free(&val->u.a.e[--val->u.a.size]);
}

/*插入*/
auto frost_insert_array_element(frost_value* val, size_t index) -> frost_value* {
    assert(val != nullptr && val->type == FROST_ARRAY && index <= val->u.a.size);
    if(val->u.a.size == val->u.a.capacity) frost_reserve_array(val, val->u.a.capacity == 0 ? 1 : (val->u.a.size << 1)); //扩容为原来一倍
    memcpy(&val->u.a.e[index + 1], &val->u.a.e[index], (val->u.a.size - index) * sizeof(frost_value));
    frost_init(&val->u.a.e[index]);
    val->u.a.size++;
    return &val->u.a.e[index];
}

/*删除*/
void frost_erase_array_element(frost_value* val, size_t index, size_t count) {
    assert(val != nullptr && val->type == FROST_ARRAY && index + count <= val->u.a.size);
    size_t i;
    for(i = index; i < index + count; i++){
        frost_free(&val->u.a.e[i]);
    }
    memcpy(val->u.a.e + index, val->u.a.e + index + count, (val->u.a.size - index - count) * sizeof(frost_value));
    for(i = val->u.a.size - count; i < val->u.a.size; i++)
        frost_init(&val->u.a.e[i]);
    val->u.a.size -= count;
}


void frost_set_object(frost_value* val, size_t capacity) {
    assert(val != nullptr);
    frost_free(val);
    val->type = FROST_OBJECT;
    val->u.o.size = 0;
    val->u.o.capacity = capacity;
    val->u.o.m = capacity > 0 ? (frost_member*)malloc(capacity * sizeof(frost_member)) : nullptr;
}

auto frost_get_object_size(const frost_value* val) -> size_t
{
    assert(val != nullptr && val->type == FROST_OBJECT);
    return val->u.o.size;
}

auto frost_get_object_capacity(const frost_value* val) -> size_t {
    assert(val != nullptr && val->type == FROST_OBJECT);
    return val->u.o.capacity;
}

void frost_reserve_object(frost_value* val, size_t capacity) {
    assert(val != nullptr && val->type == FROST_OBJECT);
    if (val->u.o.capacity < capacity) {
        val->u.o.capacity = capacity;
        val->u.o.m = (frost_member*)realloc(val->u.o.m, capacity * sizeof(frost_member));
    }
}

void frost_shrink_object(frost_value* val) {
    assert(val != nullptr && val->type == FROST_OBJECT);
    if (val->u.o.capacity > val->u.o.size) {
        val->u.o.capacity = val->u.o.size;
        val->u.o.m = (frost_member*)realloc(val->u.o.m, val->u.o.capacity * sizeof(frost_member));
    }
}

void frost_clear_object(frost_value* val) {
    assert(val != nullptr && val->type == FROST_OBJECT);
    size_t i = 0;
    for(i = 0; i < val->u.o.size; i++){
        free(val->u.o.m[i].k);
        val->u.o.m[i].k = nullptr;
        val->u.o.m[i].klen = 0;
        frost_free(&val->u.o.m[i].v);
    }
    val->u.o.size = 0;
}

auto frost_get_object_key(const frost_value* val, size_t index) -> const char*
{
    assert(val != nullptr && val->type == FROST_OBJECT);
    assert(index < val->u.o.size);
    return val->u.o.m[index].k;
}

auto frost_get_object_key_length(const frost_value* val, size_t index) -> size_t
{
    assert(val != nullptr && val->type == FROST_OBJECT);
    assert(index < val->u.o.size);
    return val->u.o.m[index].klen;
}

auto frost_get_object_value(const frost_value* val, size_t index) -> frost_value*
{
    assert(val != nullptr && val->type == FROST_OBJECT);
    assert(index < val->u.o.size);
    return &val->u.o.m[index].v;
}

auto frost_find_object_index(const frost_value* val, const char* key, size_t klen) -> size_t {
    size_t i = 0;
    assert(val != nullptr && val->type == FROST_OBJECT && key != nullptr);
    for (i = 0; i < val->u.o.size; i++)
        if (val->u.o.m[i].klen == klen && memcmp(val->u.o.m[i].k, key, klen) == 0)
            return i;
    return FROST_KEY_NOT_EXIST;
}

auto frost_find_object_value(frost_value* val, const char* key, size_t klen) -> frost_value* {
    size_t index = frost_find_object_index(val, key, klen);
    return index != FROST_KEY_NOT_EXIST ? &val->u.o.m[index].v : nullptr;
}

auto frost_set_object_value(frost_value* val, const char* key, size_t klen) -> frost_value* {
    assert(val != nullptr && val->type == FROST_OBJECT && key != nullptr);
    size_t i, index;
    index = frost_find_object_index(val, key, klen);
    if(index != FROST_KEY_NOT_EXIST)
        return &val->u.o.m[index].v;
    if(val->u.o.size == val->u.o.capacity){
        frost_reserve_object(val, val->u.o.capacity == 0 ? 1 : (val->u.o.capacity << 1));
    }
    i = val->u.o.size;
    val->u.o.m[i].k = (char *)malloc((klen + 1));
    memcpy(val->u.o.m[i].k, key, klen);
    val->u.o.m[i].k[klen] = '\0';
    val->u.o.m[i].klen = klen;
    frost_init(&val->u.o.m[i].v);
    val->u.o.size++;
    return &val->u.o.m[i].v;
}

void frost_remove_object_value(frost_value* val, size_t index) {
    assert(val != nullptr && val->type == FROST_OBJECT && index < val->u.o.size);
    free(val->u.o.m[index].k);
    frost_free(&val->u.o.m[index].v);
    memcpy(val->u.o.m + index, val->u.o.m + index + 1, (val->u.o.size - index - 1) * sizeof(frost_member));
    val->u.o.m[--val->u.o.size].k = nullptr;
    val->u.o.m[val->u.o.size].klen = 0;
    frost_init(&val->u.o.m[val->u.o.size].v);
}