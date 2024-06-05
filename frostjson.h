#ifndef FROSTJSON_H__
#define FROSTJSON_H__

#include <cstddef>

using frost_type = enum{ FROST_NULL, FROST_TRUE, FROST_FALSE, FROST_NUMBER, FROST_STRING, FROST_ARRAY, FROST_OBJECT };

#define FROST_KEY_NOT_EXIST ((size_t)-1)

using frost_value = struct frost_value;
using frost_member = struct frost_member;

struct frost_value{
    union{
        struct { frost_member* m; size_t size, capacity; }o;  /* object: members, member count */
        struct { frost_value* e; size_t size, capacity; }a;   /* array:  elements, element count */
        struct { char* s; size_t len; }s;           /* string: null-terminated string, string length */
        double n;                                   /* number */
    }u; 
    frost_type type;
};

struct frost_member{
    char* k;
    size_t klen;
    frost_value v;
};

enum{
    FROST_PARSE_OK = 0,
    FROST_PARSE_EXPECT_VALUE,
    FROST_PARSE_INVALID_VALUE,
    FORST_PARSE_ROOT_NOT_SINGULAR,
    FROST_PARSE_NUMBER_TOO_BIG,
    FROST_PARSE_MISS_QUOTATION_MARK,
    FROST_PARSE_INVALID_STRING_ESCAPE,
    FROST_PARSE_INVALID_STRING_CHAR,
    FROST_PARSE_INVALID_UNICODE_HEX,
    FROST_PARSE_INVALID_UNICODE_SURROGATE,
    FROST_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,
    FROST_PARSE_MISS_KEY,
    FROST_PARSE_MISS_COLON,
    FROST_PARSE_MISS_COMMA_OR_CURLY_BRACKET,    
};


#define frost_init(v) do { (v)->type = FROST_NULL; } while(0)

auto frost_parse(frost_value* val, const char* json) -> int; //解析json
auto frost_stringify(const frost_value* val, size_t* length) -> char*;

void frost_copy(frost_value* dst, const frost_value* src);
void frost_move(frost_value* dst, frost_value* src);
void frost_swap(frost_value* lhs, frost_value* rhs);

void frost_free(frost_value* val);  // 释放

#define frost_set_null(v) frost_free(v)

auto frost_get_type(const frost_value* val) -> frost_type;
auto frost_is_equal(const frost_value* lhs, const frost_value* rhs) -> int;

auto frost_get_boolean(const frost_value* val) -> int; 
void frost_set_boolean(frost_value* val, int bol);

auto frost_get_number(const frost_value* val) -> double;  
void frost_set_number(frost_value* val, double n);

auto frost_get_string(const frost_value* val) -> const char*;
auto frost_get_string_length(const frost_value* val) -> size_t;
void frost_set_string(frost_value* val, const char* str, size_t len);

void frost_set_array(frost_value* val, size_t capacity);
auto frost_get_array_size(const frost_value* val) -> size_t; 
auto frost_get_array_capacity(const frost_value* val) -> size_t;
void frost_reserve_array(frost_value* val, size_t capacity);
void frost_shrink_array(frost_value* val);
void frost_clear_array(frost_value* val);
auto frost_get_array_element(frost_value* val, size_t index) -> frost_value*;
auto frost_pushback_array_element(frost_value* val) -> frost_value*;
void frost_popback_array_element(frost_value* val);
auto frost_insert_array_element(frost_value* val, size_t index) -> frost_value*;
void frost_erase_array_element(frost_value* val, size_t index, size_t count);

void frost_set_object(frost_value* val, size_t capacity);
auto frost_get_object_size(const frost_value* val) -> size_t;
auto frost_get_object_capacity(const frost_value* val) -> size_t;
void frost_reserve_object(frost_value* val, size_t capacity);
void frost_shrink_object(frost_value* val);
void frost_clear_object(frost_value* val);
auto frost_get_object_key(const frost_value* val, size_t index) -> const char*;
auto frost_get_object_key_length(const frost_value* val, size_t index) -> size_t;
auto frost_get_object_value(const frost_value* val, size_t index) -> frost_value*;
auto frost_find_object_index(const frost_value* val, const char* key, size_t klen) -> size_t;
auto frost_find_object_value(frost_value* val, const char* key, size_t klen) -> frost_value*;
auto frost_set_object_value(frost_value* val, const char* key, size_t klen) -> frost_value*;
void frost_remove_object_value(frost_value* val, size_t index);

#endif /* FROSTJSON_H__ */