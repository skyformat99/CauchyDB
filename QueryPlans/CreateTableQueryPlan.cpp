//
// Created by 张程易 on 2017/4/8.
//

#include "CreateTableQueryPlan.h"

CreateTableQueryPlan::CreateTableQueryPlan(const std::string &name, SQLSession *sqlSession, JSONArray *def, JSONObject *indices) : name(name),
                                                                                                        sqlSession(
                                                                                                                sqlSession)
{
    config = new JSONObject();
    config->set("columns", def);
    config->set("indices", indices);
}

JSON *CreateTableQueryPlan::toJSON()  {
    auto json = new JSONObject();
    json->set("type", "create");
    json->set("subType", "table");
    json->set("name", name);
    json->set("config", config);
    return json;
}

JSON *CreateTableQueryPlan::runQuery(RecordService *recordService)  {
    auto db = sqlSession->getDataBaseModel();
    db->createTable(name, config);
    return new JSONInteger(0);
}

