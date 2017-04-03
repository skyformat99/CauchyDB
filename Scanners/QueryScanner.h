//
// Created by 张程易 on 2017/3/29.
//

#ifndef DB_QUERYSCANNER_H
#define DB_QUERYSCANNER_H

#include "../Common.h"
#include "../Services/RecordService.h"
#include "../Models/TableModel.h"

class QueryScanner{
protected:
    size_t len;
    size_t perBlock;
    RecordService * recordService;
public:
    QueryScanner(size_t len, RecordService *recordService);

    virtual void scan(std::function<void(size_t, void *)> consumer) = 0;
    virtual JSON * toJSON() = 0;
};

#endif //DB_QUERYSCANNER_H