//
// Created by 张程易 on 2017/3/29.
//

#ifndef DB_COLUMNMODEL_H
#define DB_COLUMNMODEL_H

#include "../Common.h"

class ColumnModel{

public:
    inline ColumnModel( JSON * config ){
        JSONObject * data = config->toObject();
        name = data->get("name")->asCString();
        size = (size_t)data->get("typeSize")->toInteger()->value;
        const char * typestr = data->get("type")->asCString();
        if( strcmp(typestr, "int") == 0){
            type = ColumnType::Int;
        }
        else if( strcmp(typestr, "char") == 0){
            type = ColumnType::Char;
        }
        else if( strcmp(typestr, "float") == 0){
            type = ColumnType::Float;
        }
        else throw "fuck type";
    }

    inline size_t getSize() const {
        return size;
    }

    inline ColumnType getType() const {
        return type;
    }

    inline const std::string &getName() const {
        return name;
    }

    inline size_t getOn() const {
        return on;
    }

private:
    std::string name;
    size_t size;
    ColumnType type;
    size_t on;
};

#endif //DB_COLUMNMODEL_H