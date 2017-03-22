
#include "Common.h"
#include "FileService.h"
#include "BlockService.h"
#include "BPlusTree.h"
#include "Configuration.h"
#include "RecordService.h"
#include "QueryFilter.h"

using namespace std;

class SQLException{
public:
    int code;
    std::string message;
    SQLException(int code = 0, std::string && message = "")
            :code(code), message(message){};
};

class SQLExecuteException: public SQLException{
public:
    SQLExecuteException(int code = 0, std::string && message = "")
            :SQLException(code, std::move(message)){}

};

class SQLSyntaxException: public SQLException{
public:
    SQLSyntaxException(int code = 0, std::string && message = "")
            :SQLException(code, std::move(message)){}
};

class SQLTypeException: public SQLException{
public:
    SQLTypeException(int code = 0, std::string && message = "")
            :SQLException(code, std::move(message)){}
};


// int char float

class ColumnModel{
public:
    enum class Type{
        Char, Int, Float
    };
    std::string name;
    int size;
    Type type;
    ColumnModel( JSON * config ){
        JSONObject * data = config->toObject();
        name = data->get("name")->asCString();
        size = (int)(data->get("typeSize")->toInteger()->value);
        const char * typestr = data->get("type")->asCString();
        if( strcmp(typestr, "int") == 0){
            type = Type::Int;
        }
        else if( strcmp(typestr, "char") == 0){
            type = Type::Char;
        }
        else if( strcmp(typestr, "float") == 0){
            type = Type::Float;
        }
        else throw "fuck type";
    }
};

class IndexModel{
    int fid;
    FileService * fileService;
    public:
    IndexModel(FileService * fileService, std::string && name, JSON * config){
        this->fileService = fileService;
        fid = fileService->openFile((name + ".cdi").c_str());
    }
};



class TableModel{
    int fid;
    FileService * fileService;
    std::string name;
    std::vector<ColumnModel> columns;
    std::vector<IndexModel> indices;
    std::map<std::string, int> keyindex;
    public:
    TableModel(FileService * fileService, std::string && name, JSON * config)
    :name(name)
    {
        // open file
        this->fileService = fileService;
        fid = fileService->openFile((name + ".cdt").c_str());
        JSONArray * data = config->get("columns")->toArray();
        for(auto & column: data->elements){
            columns.emplace_back(column);
        }
        // build fast search keyindex
        for(int i = 0; i < columns.size(); i++){
            keyindex.emplace(columns[i].name, i);
        }
        JSONObject * jarr = config->get("indices")->toObject();
        for(auto & index : jarr->hashMap){
            indices.emplace_back(fileService, name + "_" + index.first, index.second);
        }
    }
    ColumnModel * getColumn(int cid){
        return &columns[cid];
    }
    int getColumnIndex(const std::string & str){
        auto iter = keyindex.find(str);
        if( iter == keyindex.end()) throw SQLExecuteException(1, "unknown column");
        return iter->second;
    }
};


class DataBaseModel{
    std::map<std::string, TableModel * > tables;
    public:
    DataBaseModel(FileService * fileService, const std::string & name, JSON * config){
        JSONObject * data = config->get("tables")->toObject();
        for(auto & table: data->hashMap){
            tables.emplace(table.first,
             new TableModel(fileService, name + "_" + table.first, table.second));
        }
    }
    ~DataBaseModel(){
        for( auto & x : tables){
            delete x.second;
        }
    }
    TableModel * getTableByName(const std::string & str){
        return tables[str];
    }
};

class MetaDataService{
    FileService * fileService;
    JSON * data;
    std::map<std::string, DataBaseModel *> dataBases;
public:
    ~MetaDataService(){
        for( auto & x: dataBases){
            delete x.second;
        }
    }
    DataBaseModel * getDataBase(const string & name){
        return dataBases[name];
    }
    MetaDataService(FileService * fileService)
        :fileService(fileService){
        data = JSON::fromFile(Configuration::attrCString("meta_file_path"));
        // Load All DataBases
        JSONObject * dbs = data->get("databases")->toObject();
        for(auto & db: dbs->hashMap){
            dataBases.emplace(db.first, new DataBaseModel(fileService, db.first, db.second));
        }
    }
};
// [nullptr][ ][ ][ ][ ]
// 0->full

// 0 - 7 is header!!


class SQLSession{
    MetaDataService * metaDataService;
    DataBaseModel * dataBaseModel;
public:
    SQLSession(MetaDataService *metaDataService, DataBaseModel *dataBaseModel) : metaDataService(metaDataService),
                                                                                 dataBaseModel(dataBaseModel){}

    void loadTable(const string & name){
        dataBaseModel = metaDataService->getDataBase(name);
    }
    TableModel * getTable(const string & name){
        if( dataBaseModel == nullptr) return nullptr;
        return dataBaseModel->getTableByName(name);
    }

};

class SQLStatement{

};

class SQLCondition{
public:
    enum class Type{
        gt, lt, gte, lte, eq, neq
    };
    Type type;
    int col;
    void * value;

    SQLCondition(Type type, int col, void *value) : type(type), col(col), value(value) {}
};

class SQLWhereClause{
    std::vector<SQLCondition *> conds;
public:
    void addCondition(SQLCondition * cond){
        conds.emplace_back(cond);
    }
};

class SQLSelectStatement : public SQLStatement{
    TableModel * table;
    std::vector<size_t> columns;
    SQLWhereClause * where;
public:
    SQLSelectStatement(TableModel *table, const vector<std::string> &columns, SQLWhereClause *where) : table(table), where(where) {
        for(auto & key : columns){
            this->columns.emplace_back(table->getColumnIndex(key));
        }
    }
};

class SQLParser{
    SQLSession * sqlSession;
    const char * str = "select * from test where id >= 20 and value = 'F';";
public:
    SQLParser(SQLSession *sqlSession) : sqlSession(sqlSession) {}

    enum class TokenType {
        integer, real, name, string, ope, null, __keyword
    };
    struct Token {
        int begin, end;
        TokenType type;
        Token(int begin_,int end_, TokenType type_)
                :begin(begin_),end(end_),type(type_){}
    };
    std::vector<Token> tokens;
    size_t pos = 0;

    inline bool isNum(char x) {
        return x >= '0' && x <= '9';
    }
    inline bool isChar(char x) {
        return (x >= 'a' && x <= 'z') || (x >= 'A' && x <='Z');
    }
    void tokenize() {
        TokenType status = TokenType::null;
        int saved_pos = 0;
        for (int i = 0; str[i] != 0; i++) {
            switch (status) {
                case TokenType::null:
                    if (str[i] == '\'' || str[i] == '\"') {
                        status = TokenType::string;
                    }
                    else if (str[i] == '`') {
                        status = TokenType::name;
                    }
                    else if (isNum(str[i]) || str[i] == '-') {
                        status = TokenType::integer;
                    }
                    else if (isChar(str[i])) {
                        status = TokenType::__keyword;
                    }
                    else if (str[i] == ' ' || str[i] == '\t' || str[i] == '\n' || str[i] == '\r') {
                        break;
                    }
                    else {
                        int st = i;
                        if (str[i] == '!' && str[i + 1] == '=') {
                            i++;
                        }
                        else if (str[i] == '<' && str[i + 1] == '>') {
                            i++;
                        }
                        else if (str[i] == '<' && str[i + 1] == '=') {
                            i++;
                        }
                        else if (str[i] == '>' && str[i + 1] == '=') {
                            i++;
                        }
                        tokens.emplace_back(st, i, TokenType::ope);
                        break;
                    }
                    saved_pos = i;
                    break;
                case TokenType::string:
                    if ((str[i] == '\"' || str[i] == '\'') && str[i - 1] != '\\') {
                        tokens.emplace_back(saved_pos + 1, i - 1, TokenType::string);
                        status = TokenType::null;
                    }break;
                case TokenType::name:
                    if (str[i] == '`'  && str[i - 1] != '\\') {
                        tokens.emplace_back(saved_pos, i - 1, TokenType::string);
                        status = TokenType::null;
                    }
                    break;
                case TokenType::integer:
                    if (str[i] == '.' || str[i] == 'e') {
                        status = TokenType::real;
                        if (str[i] == 'e' && str[i] == '-')i++;
                        break;
                    }
                    if (!isNum(str[i])) {
                        tokens.emplace_back(saved_pos, i - 1, TokenType::integer);
                        status = TokenType::null;
                        i--;
                    }
                    break;
                case TokenType::real:
                    if (!isNum(str[i])) {
                        tokens.emplace_back(saved_pos, i - 1, TokenType::real);
                        status = TokenType::null;
                        i--;
                    }
                    break;
                case TokenType::__keyword:
                    if (!isChar(str[i])) {
                        tokens.emplace_back(saved_pos, i - 1, TokenType::name);
                        status = TokenType::null;
                        i--;
                    }
                    break;
                case TokenType::ope:break;
            }
        }
    }


    bool tokencmp(Token & token, const char * target){
        int len1 = token.end - token.begin + 1;
        int len2 = (int)strlen(target);
        if( len1 != len2 ) return false;
        return strncasecmp(str + token.begin, target, (size_t)len1) == 0;
    }

    SQLWhereClause * parseWhereClause(TableModel * tableModel){
        Token token = tokens[pos];
        SQLWhereClause * sqlWhereClause = new SQLWhereClause();
        try {
            while (token.type == TokenType::name) {
                int cid = tableModel->getColumnIndex(std::string(str, token.begin, token.end - token.begin + 1));
                SQLCondition::Type type;
                pos ++; token = tokens[pos]; // col
                if(tokencmp(token, "<")){
                    type = SQLCondition::Type::lt;
                }
                else if(tokencmp(token, ">")){
                    type = SQLCondition::Type::gt;
                }
                else if(tokencmp(token, ">=")){
                    type = SQLCondition::Type::gte;
                }
                else if(tokencmp(token, "<=")){
                    type = SQLCondition::Type::lte;
                }
                else if(tokencmp(token, "<>")){
                    type = SQLCondition::Type::neq;
                }
                else if(tokencmp(token, "=")){
                    type = SQLCondition::Type::eq;
                }
                else throw new SQLSyntaxException(0, "illegal opes");
                pos ++; token = tokens[pos]; // op
                void * data;
                ColumnModel * columnModel = tableModel->getColumn(cid);
                if(token.type == TokenType::integer){
                    if( columnModel->type != ColumnModel::Type::Int ) throw SQLTypeException(3, "type error");
                    data = new int[1];
                    sscanf(str + token.begin, "%d", data);
                }
                else if(token.type == TokenType::string){
                    if( columnModel->type != ColumnModel::Type::Char ) throw SQLTypeException(3, "type error");
                    int len = columnModel->size;
                    data = new char[len + 1];
                    memcpy(data, str + token.begin, len);
                    ((char *)data)[len] = 0;
                }
                else if(token.type == TokenType::real){
                    if( columnModel->type != ColumnModel::Type::Float ) throw SQLTypeException(3, "type error");
                    data = new double[1];
                    sscanf(str + token.begin, "%lf", data);
                }
                else throw new SQLSyntaxException(0, "illegal operand");
                sqlWhereClause->addCondition(new SQLCondition(type, cid, data));
                pos ++; token = tokens[pos]; // imm
                if(!tokencmp(token, "and")) break;
                pos ++; token = tokens[pos]; // and
            }
            return sqlWhereClause;
        }catch(SQLException e){
            delete sqlWhereClause;
            throw e;
        }
    }

    SQLSelectStatement *  parseSelectStatement(){
        Token token = tokens[pos];
        std::vector<std::string> columns;
        bool ok = false;
        if(str[token.begin] == '*'){
            pos ++; token = tokens[pos];
            if(tokencmp(token, "from")){ok = true;}
            else throw SQLSyntaxException(2, "syntax error");
        }
        else{
            while(token.type == TokenType::name){
                columns.emplace_back(str, token.begin, token.end - token.begin + 1);
                pos ++; token = tokens[pos];
                if(tokencmp(token, "FROM")){ok = true; break;}
                if(str[token.begin] != ',') throw SQLSyntaxException(2, "syntax error");
                pos ++; token = tokens[pos];
            }
            if(!ok)throw SQLSyntaxException(2, "syntax error");
        }
        pos ++; token = tokens[pos];//FROM
        TableModel * table = sqlSession->getTable(std::string(str, token.begin, token.end - token.begin + 1));
        SQLWhereClause * where = nullptr;
        pos ++; token = tokens[pos];//tableName
        while(str[token.begin] != ';') {
            if (tokencmp(token, "WHERE")) {
                pos ++; where = parseWhereClause(table);
            } else if (tokencmp(token, "ORDER")) {

            } else {
                if (where != nullptr) delete where;
                throw SQLSyntaxException(2, "syntax error");
            }
            token = tokens[pos];
        }
        return new SQLSelectStatement(table, columns, where);
    }

    SQLStatement * parseSQLStatement(){
        pos = 0;
        int len = tokens[pos].end - tokens[pos].begin + 1;
        Token token = tokens[pos];pos++;
        if( len == 6){
            if(tokencmp(token, "select")){
                return parseSelectStatement();
            }
            else if( tokencmp(token, "delete")){

            }
            else if( tokencmp(token, "insert")){

            }
            else if( tokencmp(token, "create")){

            }
            else{
                throw SQLSyntaxException(0, "keyword error");
            }
        }
        else{
            throw SQLSyntaxException(0, "keyword error");
        }
        return nullptr;
    }

    void test(){
        for(auto & x: tokens){
            for(int i = x.begin; i<= x.end; i++){
                putchar(str[i]);
            }
            printf("%d\n", x.end - x.begin + 1);

        }
    }

};


class TableFacade{
    BlockService * blockService;
public:
    TableFacade(BlockService * blockService)
    :blockService(blockService){}

    size_t findOne(void * key){
        //fuck everyone
    }
};

/*
 * a>=35 && a<=35
 * SQL => QueryPlan
 * QueryPlan
 * type => select
 * front => linear / index
 *      index: { on : "primiary", "
 *
 *
 */

class IndexFacade{
    BlockService * blockService;
public:
    IndexFacade(BlockService * blockService)
        :blockService(blockService){}
    size_t selectEqual(int fid, void * key, size_t len);
    //IndexIterator * selectRange(int fid, bool leftEq, bool rightEq, void * left, void * right, size_t len);

    void insert(int fid, void * key, size_t len, size_t value);
    void remove(int fid, void * key, size_t len);
};

class ApplicationContainer{
public:
    BlockService * blockService;
    FileService * fileService;
    MetaDataService * metaDataService;
    RecordService * recordService;
    ApplicationContainer(){}
    ~ApplicationContainer(){
        delete blockService;
        delete fileService;
        delete metaDataService;
        delete recordService;
    }
    void start(){
        // The bootstrap persedure of IoC Container:
        // Initial Configuration Singleton
        // Load FileService
        // Load MetaDataService { Register File => FileService }
        // Load BlockService { Inject FileService }
        // Load QueryRunnerService{ Inject BlockService }
        // Load QueryBuilderSerice { Inject QueryRunnerService }
        // Load SQLInterpreterService { Inject QueryBuilderSerice }
        Configuration::initialize();
        fileService = new FileService();
        metaDataService = new MetaDataService(fileService);
        blockService = new BlockService(fileService);
        recordService = new RecordService(blockService);
    }

    void testSQL(){
        auto session = new SQLSession(metaDataService, nullptr);
        session->loadTable("test");
        auto parser = new SQLParser(session);
        parser->tokenize();
        parser->test();
        auto sql = parser->parseSQLStatement();
    }

#define __fo(x) ((x) / BLOCK_SIZE )
#define __bo(x) ((x) % BLOCK_SIZE )

    void testRecord(int fid){
        char buffer[BLOCK_SIZE];
        buffer[0] = 'a';
        buffer[1] = 'v';
        buffer[2] = 0;
        auto a = recordService->insert(fid, buffer, 8);
        buffer[1] = '1';
        auto b = recordService->insert(fid, buffer, 8);
        buffer[1] = '2';
        auto c = recordService->insert(fid, buffer, 8);
        buffer[1] = '3';
        auto d = recordService->insert(fid, buffer, 8);
        printf("read from record 0 %s\n", recordService->read(fid, __fo(a),__bo(a), 8));
        printf("read from record 1 %s\n", recordService->read(fid, __fo(b),__bo(b), 8));
        printf("read from record 2 %s\n", recordService->read(fid, __fo(c),__bo(c), 8));
        printf("read from record 3 %s\n", recordService->read(fid, __fo(d),__bo(d), 8));
        recordService->write(fid, 0, 8, buffer, 8);
        //recordFacade->remove(fid, 16, 8);
        for(int i = 0; i<= 50; i++){
            printf("read from record %d %s\n",
                   i,
                   recordService->read(fid,__fo(i * 8), __bo(i * 8), 8));
        }
    }
    void testFile(){
        int fid = fileService->openFile("aes.jb");
        printf("open file: fid = %d\n", fid);
        size_t offset = fileService->allocBlock(fid);
        printf("alloc block at offset = %d\n", (int) offset);
        char * data = new char[BLOCK_SIZE];
        data[0] = 'a';
        data[1] = 'b';
        data[2] = 0;
        fileService->writeBlock(fid, offset, (void *)data);
        printf("write block with str = %s\n", data);
        char * out = (char *)fileService->readBlock(fid, offset);
        printf("read block with str = %s\n", out);
        delete[] out;
        delete[] data;
    }
    void testBlockSr(int fid){
        size_t offset = blockService->allocBlock(fid);
        BlockItem * blk = blockService->getBlock(fid, offset);
        char * data = (char * )blk->value;
        data[0] = 'a';
        data[1] = 'b';
        blk->modified = 1;
        blk = blockService->getBlock(fid, offset);
        blk = blockService->getBlock(fid, offset);
        blk = blockService->getBlock(fid, offset);
        blk = blockService->getBlock(fid, offset);
        blk = blockService->getBlock(fid, offset);

    }
    void testBlock(){
        int fid = fileService->openFile("aes.jb");
        for(int i=1;i<=20; i++)testBlockSr(fid);
    }
};

//const int NODE_SIZE = 5;
//const int NODE_SIZE_HALF = (NODE_SIZE + 1) / 2; // 2
// 4 bytes (int) 4 bytes(float) 4 bytes char???fucking fuck
// NODE_SIZE_EQU ::=
// NODE_SIZE = (BLOCK_SIZE - 8 ) / ( key_type_size );
// 2 bytes size + 2 bytes isLeaf
// 8 bytes int, 8 bytes long, 16 bytes char, 64 bytes, 256 bytes
#include <sstream>
const char * itos(int i){
    char * buffer = new char[36];
    ostringstream os;
    os << i;
    strcpy(buffer, os.str().c_str());
    return buffer;
}





//BPlusTree<char[16]> bPlusTree;
//int main(){
//    bPlusTree.T_BPLUS_TEST();
//    return 0;
//}
//
//
//int main2(){
//    tokenize();
//    for(auto & x : tokens){
//        for(int i = x.begin; i <= x.end; i++)putchar(str[i]);
//        printf("\n");
//    }
//    return 0;
//}

int main(){
    ApplicationContainer applicationIoCContainer;
    applicationIoCContainer.start();
    applicationIoCContainer.testSQL();
//    int fid = applicationIoCContainer.fileService->openFile("test.idx");
//    BPlusTree<char[16]> bPlusTree(applicationIoCContainer.blockService, fid);
//    bPlusTree.T_BPLUS_TEST();
    //applicationIoCContainer.testBlock();
    //applicationIoCContainer.testRecord(fid);
    return 0;
}

class QueryScanner{
protected:
    RecordService * recordService;
    BlockService * blockService;
public:
    QueryScanner(RecordService *recordService, BlockService *blockService) : recordService(recordService),
                                                                             blockService(blockService) {}

    virtual void scan( std::function<void(size_t, size_t)> consumer) = 0;
};

class LinearQueryScanner : public QueryScanner{
    int fid;
public:
    LinearQueryScanner(RecordService *recordService, BlockService *blockService, int fid) : QueryScanner(recordService,
                                                                                                             blockService),
                                                                                            fid(fid) {}
    
};

template <typename T>
class OneIndexQueryScanner : public QueryScanner{
    int ifid;
    int tfid;
    T value;
public:
    OneIndexQueryScanner(RecordService *recordService, BlockService *blockService, int ifid, int tfid, T value)
            : QueryScanner(recordService, blockService), ifid(ifid), tfid(tfid), value(value) {}

    void scan(std::function<void(size_t, size_t)> consumer) override {
        BPlusTree<T> bPlusTree(blockService, ifid);
        size_t ptr = bPlusTree.findOne(value);
        if(ptr != 0) consumer(1, ptr);
    }
};

template <typename T>
class RangeIndexQueryScanner : public QueryScanner{
    int ifid;
    int tfid;
    T left, right;
    bool leq, req;
public:
    RangeIndexQueryScanner(RecordService *recordService, BlockService *blockService, int ifid, int tfid, T left,
                           T right, bool leq, bool req) : QueryScanner(recordService, blockService), ifid(ifid),
                                                          tfid(tfid), left(left), right(right), leq(leq), req(req) {}

    void scan(std::function<void(size_t, size_t)> consumer) override {
        BPlusTree<T> bPlusTree(blockService, ifid);
        bPlusTree.findByRange(left, leq, right, req, consumer);
    }
};



class QueryProjection {
protected:
    int st;
    std::string name;
public:
    QueryProjection(int st, const string &name) : st(st), name(name) {}

    virtual std::string project(void * data) = 0;
};

class IntegerQueryProjection: public QueryProjection{
public:

    IntegerQueryProjection(int st, const string &name) : QueryProjection(st, name) {}

    std::string project(void * data) override {
        char str[64];
        sprintf(str,"%d", *(int *)(((char*)data) + st));
        return std::string(str);
    }
};

class CharQueryProjection:public QueryProjection{
    int len;
public:
    CharQueryProjection(int st, const string &name, int len) : QueryProjection(st, name), len(len) {}

    string project(void * data) override {
        return std::string((char *)data, st,  len);
    }
};

class FloatQueryProjection: public QueryProjection{
public:
    FloatQueryProjection(int st, const string &name) : QueryProjection(st, name) {}

    string project(void * data) override {
        char str[64];
        sprintf(str,"%.3lf", *(double *)(((char*)data) + st));
        return std::string(str);
    }
};



class QueryPlan{

};

class SelectQueryPlan: public SelectQueryPlan{
    QueryScanner * queryScanner;
    QueryFilter * queryFilter;
    std::vector<QueryProjection *> queryProjections;
public:
    SelectQueryPlan(QueryScanner *queryScanner, QueryFilter *queryFilter,
                    const vector<QueryProjection *> &queryProjections) : queryScanner(queryScanner),
                                                                         queryFilter(queryFilter),
                                                                         queryProjections(queryProjections) {}

};


/** 

    metablock datablock

**/ 