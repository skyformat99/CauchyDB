//
// Created by 张程易 on 2017/4/2.
//

#include "RangeIndexQueryScanner.h"

RangeIndexQueryScanner::RangeIndexQueryScanner(AbstractIndexRunner * indexRunner, RecordService * recordService, size_t len, int tfid,
void * left, void * right, bool leq, bool req, bool withLeft, bool withRight, std::string on) : QueryScanner(
        len,recordService),indexRunner(indexRunner),  tfid(tfid), left(left), right(right), leq(leq), req(req),
withLeft(withLeft), withRight(withRight), on(on) {}

void RangeIndexQueryScanner::scan(std::function<void(size_t, void *)> consumer)  {
    indexRunner->findByRange(withLeft, left, leq, withRight, right, req,
                             [consumer, this](size_t id, size_t blk){
                                 recordService->read(tfid, blk / BLOCK_SIZE, blk % BLOCK_SIZE, len);
                             });
}

JSON *RangeIndexQueryScanner::toJSON()  {
    auto json = new JSONObject();
    json->set("type", "RangeIndexQueryScanner");
    json->set("on", on);
    json->set("left",ColumnTypeUtil::toJSON(indexRunner->getType(), left));
    json->set("right", ColumnTypeUtil::toJSON(indexRunner->getType(), right));
    json->set("withLeft", withLeft);
    json->set("withRight", withRight);
    json->set("leq", leq);
    json->set("req", req);
    return json;
}