#ifndef FROSTJSON_H__
#define FROSTJSON_H__

#include <cstddef>

using frost_type = enum{ FROST_NULL, FROST_TRUE, FROST_FALSE, FROST_NUMBER, FROST_STRING, FROST_ARRAY, FROST_OBJECT };

using frost_value = struct{
    union{
        struct { char* s; size_t len; }s;
        double n;
    }u;
    frost_type type;
};

enum{
    FROST_PARSE_OK = 0,
    FROST_PARSE_EXPECT_VALUE,
    FROST_PARSE_INVALID_VALUE,
    FORST_PARSE_ROOT_NOT_SINGULAR,
    FROST_PARSE_NUMBER_TOO_BIG,
    FROST_PARSE_MISS_QUOTATION_MARK,
    FROST_PARSE_INVALID_STRING_ESCAPE,
    FROST_PARSE_INVALID_STRING_CHAR
};

#define frost_init(v) do { (v)->type = FROST_NULL; } while(0)

auto frost_parse(frost_value* val, const char* json) -> int; //解析json
auto frost_get_type(const frost_value* val) -> frost_type; 
void frost_free(frost_value* val);  // 释放

#define frost_set_null(v) frost_free(v)

auto frost_get_boolean(const frost_value* val) -> int; 
void frost_set_boolean(frost_value* val, int bol);

auto frost_get_number(const frost_value* val) -> double;  
void frost_set_number(frost_value* val, double n);

auto frost_get_string(const frost_value* val) -> const char*;
auto frost_get_string_length(const frost_value* val) -> size_t;
void frost_set_string(frost_value* val, const char* str, size_t len);



#endif /* FROSTJSON_H__ */