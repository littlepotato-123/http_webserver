#include "request.h"
using namespace std;

const unordered_set<string> Request::DEFAULT_HTML {
    "/index", "/register", "/login", "/welcome", "/video", "/picture"
};

const unordered_map<string, int> Request::DEFAULT_HTML_TAG {
    {"/register.html", 0}, {"/login.html", 1}
};

void Request::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool Request::IsKeepAlive() const {
    if(header_.count("Connection")) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

bool Request::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";
    //printf("Parse发生段错误1\n");
    if(buff.ReadableBytes() <= 0) {
        return false;
    }
    while(buff.ReadableBytes() && state_ != FINISH) {
        const char* lineEnd = search((const char*)buff.Peek(), (const char*)buff.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line((const char*)buff.Peek(), lineEnd);
        switch(state_)
        {
            case REQUEST_LINE:
                if(!ParseRequestLine_(line)) {
                    return false;
                }
                ParsePath_();
                //printf("Parse发生段错误2\n");
                break;
            
            case HEADERS:
                ParseHeader_(line);
                if(buff.ReadableBytes() <= 2) {
                    state_ = FINISH;
                }
                //printf("Parse发生段错误3\n");
                break;
            
            case BODY:
                ParseBody_(line);
                //printf("Parse发生段错误4\n");
                break;

            default:
                break;
        }
        //printf("Parse发生段错误5\n");
        if(lineEnd == (char* )buff.BeginWrite()) break;
        buff.RetrieveUntil((const std::uint8_t*)lineEnd + 2);
        //printf("Parse发生段错误6\n");
    }
    return 1;
}

void Request::ParsePath_(){
    if(path_ == "/") {
        path_ = "/index.html";
    }
    else {
        for(auto& item : DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

bool Request::ParseRequestLine_(const string& line) {
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;
        return 1;
    }
    return 0;
}

void Request::ParseHeader_(const string& line) {
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];
    }
    else state_ = BODY;
}

void Request::ParseBody_(const string& line) {
    body_ = line;
    ParsePost_();
    state_ = FINISH;
}

int Request::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch-'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

void Request::ParsePost_() {
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();
        if(DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    //printf("verify通过了\n");
                    path_ = "/welcome.html";
                }
                else {
                    path_ = "/error.html";
                }
            }
        }
    }
}

void Request::ParseFromUrlencoded_() {
    if(body_.size() == 0) return;
    //printf("ParseFromUrlencoded_发生段错误1\n");
    string k, v;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for(;i < n; ++i) {
        char ch = body_[i];
        switch (ch)
        {
        case '=':
            k = body_.substr(j, i-j);
            j = i + 1;       
            break;
            //printf("ParseFromUrlencoded_发生段错误2\n");
        case '+':
            body_[i] = ' ';
            break;
        case '%':
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            //printf("ParseFromUrlencoded_发生段错误3\n");
            break;
        case '&':
            v = body_.substr(j, i - j);
            j = i + 1;
            post_[k] = v;
            //printf("ParseFromUrlencoded_发生段错误4\n");
            break;
        default:
            break;
        }
    }
    //printf("ParseFromUrlencoded_发生段错误5\n");
    assert(j <= i);
    if(post_.count(k) == 0 && j < i) {
        v = body_.substr(j, i - j);
        post_[k] = v;
    }
    //printf("ParseFromUrlencoded_发生段错误6\n");
}

bool Request::UserVerify(const string& name, const string& pwd, bool isLogin) {
    if(name == "" || pwd == " ") return 0;
    MYSQL* sql;
    //printf("1verify\n");
    sqlconnRAII sql_guard(&sql);
    //printf("2verify\n,sql==nullptr-->%d\n", (bool)(sql == nullptr));
    bool flag = 0;

    unsigned int j = 0;
    std::string query;
   // MYSQL_FIELD* fields = nullptr;
    MYSQL_RES* res = nullptr;

    if(!isLogin) flag = 1;

    {
        stringstream ss;
        ss<<"SELECT username, password FROM user WHERE username='"<<name<<"' LIMIT 1";
        getline(ss, query);
    }
    //printf("段错误1\n");
    if(mysql_query(sql, query.data())) {
        mysql_free_result(res);
        return false;
    }
    //printf("段错误2\n");
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
   //fields = mysql_fetch_field(res);
   //printf("3verify\n");
   while(MYSQL_ROW row = mysql_fetch_row(res)) {
       string username(row[0]);
       string password(row[1]);
       if(isLogin) {
           if(pwd == password && username == name) flag = 1;
           else {
               //密码错误
               flag = 0;
           }
       }
       else {
           //重复使用名称
           if(username == name) flag = 0;
       }
   }
   mysql_free_result(res);
    //printf("4verify\n");
   if(!isLogin && flag) {
       query.clear();
    {
        stringstream ss;
        ss<<"INSERT INTO user(username, password) VALUES('" << name << "','" << pwd << "')";
        getline(ss, query);
    }
    if(mysql_query(sql, query.data())) {
        flag = 0;
    }
    flag = 1;
   }
   //printf("5verify\n");
   return flag;
}

std::string Request::path() const {
    return path_;
}

std::string& Request::path() {
    return path_;
}

std::string Request::method() const {
    return method_;
}

std::string Request::version() const {
    return version_;
}

std::string Request::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string Request::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}