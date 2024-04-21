#ifndef FROSTJSON_H__
#define FROSTJSON_H__

using frost_type = enum{ FROST_NULL, FROST_TRUE, FROST_FALSE, FROST_NUMBER, FROST_STRING, FROST_ARRAY, FROST_OBJECT };

using frost_value = struct{
    frost_type type;
};

enum{
    FROST_PARSE_OK = 0,
    FROST_PARSE_EXPECT_VALUE,
    FROST_PARSE_INVALID_VALUE,
    FORST_PARSE_ROOT_NOT_SINGULAR
};

auto frost_parse(frost_value* val, const char* json) -> int; //解析json

auto frost_get_type(const frost_value* val) -> frost_type; //访问数据

#endif /* FROSTJSON_H__ */