//
// Created by 张程易 on 2017/4/3.
//

#ifndef DB_METADATASERVICE_H
#define DB_METADATASERVICE_H

#include "../Common.h"
#include "FileService.h"
#include "../Models/DataBaseModel.h"
#include "../Configuration.h"

class MetaDataService{
    FileService * fileService;
    JSON * data;
    std::map<std::string, DataBaseModel *> dataBases;
public:
    ~MetaDataService();
    void createDataBase(const std::string & name);
    DataBaseModel * getDataBase(const std::string & name);
    MetaDataService(FileService * fileService);
};


#endif //DB_METADATASERVICE_H