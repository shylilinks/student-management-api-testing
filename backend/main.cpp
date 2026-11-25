#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mysql.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#pragma comment(lib,"Ws2_32.lib")

namespace fs = std::filesystem;

struct Config {
    std::string host = "127.0.0.1";
    std::string user = "root";
    std::string password;
    std::string database = "student_management";
    unsigned port = 3306;
};

struct Request {
    std::string method;
    std::string target;
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;
};

struct Response {
    int status = 200;
    std::string type = "application/json; charset=utf-8";
    std::string body;
};

struct Student {
    std::string no;
    std::string name;
    std::string gender;
    std::string major;
    std::string phone;
    int age = 0;
};

struct User {
    unsigned long id = 0;
    std::string name;
    std::string role;
};

static std::string trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

static std::string escJson(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c < 32) {
            const char h[] = "0123456789abcdef";
            r += "\\u00";
            r += h[c >> 4];
            r += h[c & 15];
        } else r += char(c);
    }
    return r;
}

static std::string unescape(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\' || i + 1 == s.size()) {
            r += s[i];
            continue;
        }
        switch (s[++i]) {
            case 'n': r += '\n'; break;
            case 'r': r += '\r'; break;
            case 't': r += '\t'; break;
            case '"': r += '"'; break;
            case '\\': r += '\\'; break;
            default: r += s[i];
        }
    }
    return r;
}

static std::string urlDecode(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') r += ' ';
        else if (s[i] == '%' && i + 2 < s.size()) {
            try {
                r += char(std::stoi(s.substr(i + 1, 2), nullptr, 16));
                i += 2;
            } catch (...) {
                r += s[i];
            }
        } else r += s[i];
    }
    return r;
}

static bool jstr(const std::string& b, const std::string& k, std::string& out) {
    auto p = b.find("\"" + k + "\"");
    if (p == std::string::npos || (p = b.find(':', p)) == std::string::npos) return false;
    for (++p; p < b.size() && std::isspace((unsigned char)b[p]); ++p);
    if (p >= b.size() || b[p] != '"') return false;
    std::string raw;
    bool slash = false;
    for (++p; p < b.size(); ++p) {
        if (!slash && b[p] == '"') {
            out = unescape(raw);
            return true;
        }
        raw += b[p];
        if (slash) slash = false;
        else if (b[p] == '\\') slash = true;
    }
    return false;
}

static bool jint(const std::string& b, const std::string& k, int& out) {
    auto p = b.find("\"" + k + "\"");
    if (p == std::string::npos || (p = b.find(':', p)) == std::string::npos) return false;
    for (++p; p < b.size() && std::isspace((unsigned char)b[p]); ++p);
    auto a = p;
    if (p < b.size() && b[p] == '-') ++p;
    while (p < b.size() && std::isdigit((unsigned char)b[p])) ++p;
    if (a == p) return false;
    try {
        out = std::stoi(b.substr(a, p - a));
        return true;
    } catch (...) {
        return false;
    }
}

static bool jarray(const std::string& b, const std::string& k, std::vector<std::string>& a) {
    auto p = b.find("\"" + k + "\"");
    if (p == std::string::npos || (p = b.find(':', p)) == std::string::npos) return false;
    for (++p; p < b.size() && std::isspace((unsigned char)b[p]); ++p);
    if (p == b.size() || b[p++] != '[') return false;
    for (;;) {
        for (; p < b.size() && std::isspace((unsigned char)b[p]); ++p);
        if (p < b.size() && b[p] == ']') return true;
        if (p == b.size() || b[p] != '{') return false;
        auto start = p;
        int deep = 0;
        bool quote = false, slash = false;
        for (; p < b.size(); ++p) {
            char c = b[p];
            if (quote) {
                if (slash) slash = false;
                else if (c == '\\') slash = true;
                else if (c == '"') quote = false;
                continue;
            }
            if (c == '"') quote = true;
            else if (c == '{') ++deep;
            else if (c == '}' && !--deep) {
                a.push_back(b.substr(start, p++ - start + 1));
                break;
            }
        }
        for (; p < b.size() && std::isspace((unsigned char)b[p]); ++p);
        if (p < b.size() && b[p++] == ',') continue;
        return p && b[p - 1] == ']';
    }
}

static std::map<std::string, std::string> query(const std::string& target) {
    std::map<std::string, std::string> r;
    auto q = target.find('?');
    if (q == std::string::npos) return r;
    std::istringstream in(target.substr(q + 1));
    std::string x;
    while (std::getline(in, x, '&')) {
        auto e = x.find('=');
        r[urlDecode(x.substr(0, e))] = e == std::string::npos ? "" : urlDecode(x.substr(e + 1));
    }
    return r;
}

static Config config() {
    Config c;
    std::ifstream f("config.ini");
    std::string l;
    while (std::getline(f, l)) {
        auto e = l.find('=');
        if (e == std::string::npos) continue;
        auto k = lower(trim(l.substr(0, e))), v = trim(l.substr(e + 1));
        if (k == "host") c.host = v;
        else if (k == "port") {
            try {
                c.port = std::stoul(v);
            } catch (...) {}
        } else if (k == "user") c.user = v;
        else if (k == "password") c.password = v;
        else if (k == "database") c.database = v;
    }
    if (auto v = std::getenv("STUDENT_DB_HOST")) c.host = v;
    if (auto v = std::getenv("STUDENT_DB_USER")) c.user = v;
    if (auto v = std::getenv("STUDENT_DB_PASSWORD")) c.password = v;
    return c;
}

class Database {
    Config cfg;
    MYSQL* db;

    bool sql(const std::string& s, std::string& e) {
        if (mysql_query(db, s.c_str()) == 0) return true;
        e = mysql_error(db);
        return false;
    }

    std::string esc(const std::string& s) {
        std::string r(s.size() * 2 + 1, '\0');
        r.resize(mysql_real_escape_string(db, r.data(), s.c_str(), (unsigned long)s.size()));
        return r;
    }

    bool insert(const Student& s, unsigned long& id, std::string& e, unsigned& code) {
        std::string q = "INSERT INTO students (student_no,name,gender,age,major,phone) VALUES ('"
                        + esc(s.no) + "','" + esc(s.name) + "','" + esc(s.gender) + "',"
                        + std::to_string(s.age) + ",'" + esc(s.major) + "','" + esc(s.phone) + "')";
        if (!sql(q, e)) {
            code = mysql_errno(db);
            return false;
        }
        id = (unsigned long)mysql_insert_id(db);
        return true;
    }

    bool students(const std::string& q, std::string& out, std::string& e) {
        if (!sql(q, e)) return false;
        MYSQL_RES* r = mysql_store_result(db);
        if (!r) {
            e = mysql_error(db);
            return false;
        }
        std::ostringstream o;
        o << "[";
        bool first = true;
        MYSQL_ROW x;
        while ((x = mysql_fetch_row(r))) {
            if (!first) o << ",";
            first = false;
            o << "{\"id\":" << (x[0] ? x[0] : "0")
              << ",\"student_no\":\"" << escJson(x[1] ? x[1] : "")
              << "\",\"name\":\"" << escJson(x[2] ? x[2] : "")
              << "\",\"gender\":\"" << escJson(x[3] ? x[3] : "")
              << "\",\"age\":" << (x[4] ? x[4] : "0")
              << ",\"major\":\"" << escJson(x[5] ? x[5] : "")
              << "\",\"phone\":\"" << escJson(x[6] ? x[6] : "")
              << "\",\"created_at\":\"" << escJson(x[7] ? x[7] : "")
              << "\",\"updated_at\":\"" << escJson(x[8] ? x[8] : "") << "\"}";
        }
        o << "]";
        mysql_free_result(r);
        out = o.str();
        return true;
    }

    std::string where(const std::string& k) {
        if (k.empty()) return "";
        auto x = "%" + esc(k) + "%";
        return " WHERE student_no LIKE '" + x + "' OR name LIKE '" + x + "' OR major LIKE '" + x + "'";
    }

public:
    explicit Database(const Config& c) : cfg(c), db(mysql_init(nullptr)) {}
    ~Database() {
        if (db) mysql_close(db);
    }

    bool setup(std::string& e) {
        if (!db) {
            e = "无法初始化 MySQL 客户端。";
            return false;
        }
        if (!mysql_real_connect(db, cfg.host.c_str(), cfg.user.c_str(), cfg.password.c_str(),
                                nullptr, cfg.port, nullptr, 0)) {
            e = mysql_error(db);
            return false;
        }
        mysql_set_character_set(db, "utf8mb4");
        if (!sql("CREATE DATABASE IF NOT EXISTS `" + cfg.database +
                     "` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci", e) ||
            mysql_select_db(db, cfg.database.c_str()) != 0) {
            if (e.empty()) e = mysql_error(db);
            return false;
        }
        if (!sql("CREATE TABLE IF NOT EXISTS students ("
                 "id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,"
                 "student_no VARCHAR(30) NOT NULL UNIQUE,"
                 "name VARCHAR(50) NOT NULL,"
                 "gender VARCHAR(10) NOT NULL DEFAULT '未知',"
                 "age TINYINT UNSIGNED NOT NULL,"
                 "major VARCHAR(100) NOT NULL DEFAULT '',"
                 "phone VARCHAR(30) NOT NULL DEFAULT '',"
                 "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                 "updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
                 ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4", e))
            return false;
        if (!sql("CREATE TABLE IF NOT EXISTS users ("
                 "id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,"
                 "username VARCHAR(30) NOT NULL UNIQUE,"
                 "password_hash CHAR(64) NOT NULL,"
                 "role ENUM('admin','teacher','viewer') NOT NULL,"
                 "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP"
                 ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4", e))
            return false;
        return sql("INSERT INTO users (username,password_hash,role) VALUES "
                   "('admin',SHA2('admin123',256),'admin'),"
                   "('teacher',SHA2('teacher123',256),'teacher'),"
                   "('viewer',SHA2('viewer123',256),'viewer') "
                   "ON DUPLICATE KEY UPDATE username=VALUES(username)", e);
    }

    bool login(const std::string& n, const std::string& p, User& u, std::string& e) {
        std::string q = "SELECT id,username,role FROM users WHERE username='" + esc(n)
                        + "' AND password_hash=SHA2('" + esc(p) + "',256) LIMIT 1";
        if (!sql(q, e)) return false;
        MYSQL_RES* r = mysql_store_result(db);
        if (!r) {
            e = mysql_error(db);
            return false;
        }
        MYSQL_ROW x = mysql_fetch_row(r);
        if (!x) {
            mysql_free_result(r);
            return false;
        }
        u.id = std::stoul(x[0]);
        u.name = x[1];
        u.role = x[2];
        mysql_free_result(r);
        return true;
    }

    bool list(const std::string& k, int page, int size, std::string& out,
              unsigned long long& count, std::string& e) {
        auto w = where(k);
        if (!sql("SELECT COUNT(*) FROM students" + w, e)) return false;
        MYSQL_RES* r = mysql_store_result(db);
        if (!r) {
            e = mysql_error(db);
            return false;
        }
        MYSQL_ROW x = mysql_fetch_row(r);
        count = x && x[0] ? std::stoull(x[0]) : 0;
        mysql_free_result(r);
        std::string q = "SELECT id,student_no,name,gender,age,major,phone,created_at,updated_at "
                        "FROM students" + w + " ORDER BY id DESC LIMIT "
                        + std::to_string((unsigned long long)(page - 1) * size) + ","
                        + std::to_string(size);
        return students(q, out, e);
    }

    bool one(unsigned long id, std::string& out, bool& found, std::string& e) {
        std::string a;
        std::string q = "SELECT id,student_no,name,gender,age,major,phone,created_at,updated_at "
                        "FROM students WHERE id=" + std::to_string(id);
        if (!students(q, a, e)) return false;
        found = a != "[]";
        out = found ? a.substr(1, a.size() - 2) : "";
        return true;
    }

    bool add(const Student& s, unsigned long& id, std::string& e, unsigned& c) {
        return insert(s, id, e, c);
    }

    bool batch(const std::vector<Student>& a, std::vector<unsigned long>& ids, size_t& failed,
               std::string& e, unsigned& c) {
        if (mysql_autocommit(db, 0) != 0) {
            e = mysql_error(db);
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i) {
            unsigned long id;
            if (!insert(a[i], id, e, c)) {
                failed = i;
                mysql_rollback(db);
                mysql_autocommit(db, 1);
                return false;
            }
            ids.push_back(id);
        }
        if (mysql_commit(db) != 0) {
            e = mysql_error(db);
            mysql_rollback(db);
            mysql_autocommit(db, 1);
            return false;
        }
        mysql_autocommit(db, 1);
        return true;
    }

    bool update(unsigned long id, const Student& s, bool& changed, std::string& e, unsigned& c) {
        std::string q = "UPDATE students SET student_no='" + esc(s.no) + "',name='" + esc(s.name)
                        + "',gender='" + esc(s.gender) + "',age=" + std::to_string(s.age)
                        + ",major='" + esc(s.major) + "',phone='" + esc(s.phone)
                        + "' WHERE id=" + std::to_string(id);
        if (!sql(q, e)) {
            c = mysql_errno(db);
            return false;
        }
        changed = mysql_affected_rows(db) > 0;
        return true;
    }

    bool remove(unsigned long id, bool& changed, std::string& e) {
        if (!sql("DELETE FROM students WHERE id=" + std::to_string(id), e)) return false;
        changed = mysql_affected_rows(db) > 0;
        return true;
    }
};

class Sessions {
    struct S {
        User user;
        std::chrono::system_clock::time_point until;
    };
    std::map<std::string, S> data;

public:
    std::string add(const User& u) {
        static std::mt19937_64 r(std::random_device{}());
        static const char h[] = "0123456789abcdef";
        std::string t;
        for (int i = 0; i < 48; ++i) t += h[r() & 15];
        data[t] = {u, std::chrono::system_clock::now() + std::chrono::hours(8)};
        return t;
    }

    bool get(const std::string& t, User& u) {
        auto i = data.find(t);
        if (i == data.end() || i->second.until < std::chrono::system_clock::now()) {
            if (i != data.end()) data.erase(i);
            return false;
        }
        u = i->second.user;
        return true;
    }

    void del(const std::string& t) { data.erase(t); }
};

static Response ok(int s, const std::string& b) {
    return {s, "application/json; charset=utf-8", b};
}

static Response fail(int s, const std::string& m) {
    return ok(s, "{\"success\":false,\"message\":\"" + escJson(m) + "\"}");
}

static bool student(const std::string& b, Student& s, std::string& e) {
    if (!jstr(b, "student_no", s.no) || !jstr(b, "name", s.name) ||
        !jstr(b, "gender", s.gender) || !jstr(b, "major", s.major) ||
        !jstr(b, "phone", s.phone) || !jint(b, "age", s.age)) {
        e = "请求体必须包含 student_no、name、gender、age、major、phone 字段。";
        return false;
    }
    s.no = trim(s.no);
    s.name = trim(s.name);
    s.gender = trim(s.gender);
    s.major = trim(s.major);
    s.phone = trim(s.phone);
    if (s.no.empty() || s.name.empty()) {
        e = "学号和姓名不能为空。";
        return false;
    }
    if (s.no.size() > 30 || s.name.size() > 50 || s.gender.size() > 10 ||
        s.major.size() > 100 || s.phone.size() > 30) {
        e = "字段长度超出限制。";
        return false;
    }
    if (s.age < 1 || s.age > 150) {
        e = "年龄必须是 1 到 150 之间的整数。";
        return false;
    }
    return true;
}

static bool pageInt(const std::map<std::string, std::string>& q, const std::string& n,
                    int def, int lo, int hi, int& v, std::string& e) {
    auto i = q.find(n);
    if (i == q.end() || i->second.empty()) {
        v = def;
        return true;
    }
    try {
        auto x = std::stoll(i->second);
        if (x < lo || x > hi) throw 1;
        v = (int)x;
        return true;
    } catch (...) {
        e = n + " 必须是 " + std::to_string(lo) + " 到 " + std::to_string(hi) + " 之间的整数。";
        return false;
    }
}

static bool idOf(const std::smatch& m, unsigned long& id) {
    try {
        auto x = std::stoull(m[1]);
        if (!x || x > std::numeric_limits<unsigned long>::max()) return false;
        id = (unsigned long)x;
        return true;
    } catch (...) {
        return false;
    }
}

static bool auth(const Request& r, Sessions& s, User& u, std::string& t) {
    auto i = r.headers.find("authorization");
    if (i == r.headers.end()) return false;
    auto v = i->second;
    if (lower(v.substr(0, 7)) != "bearer ") return false;
    t = trim(v.substr(7));
    return !t.empty() && s.get(t, u);
}

static bool write(const User& u) { return u.role == "admin" || u.role == "teacher"; }

static bool admin(const User& u) { return u.role == "admin"; }

static Response api(Database& db, Sessions& ss, const Request& r) {
    if (r.method == "OPTIONS") return {204, "text/plain; charset=utf-8", ""};
    if (r.method == "GET" && r.path == "/api/health")
        return ok(200, "{\"success\":true,\"message\":\"服务和 MySQL 已连接。\"}");

    if (r.path == "/api/auth/login") {
        if (r.method != "POST") return fail(405, "登录接口仅支持 POST。 ");
        std::string n, p, e;
        if (!jstr(r.body, "username", n) || !jstr(r.body, "password", p) ||
            trim(n).empty() || p.empty())
            return fail(400, "请提供 username 和 password。 ");
        User u;
        if (!db.login(trim(n), p, u, e))
            return e.empty() ? fail(401, "用户名或密码错误。 ") : fail(500, e);
        auto t = ss.add(u);
        return ok(200, "{\"success\":true,\"message\":\"登录成功。\",\"data\":{\"token\":\"" + t
                           + "\",\"user\":{\"id\":" + std::to_string(u.id)
                           + ",\"username\":\"" + escJson(u.name)
                           + "\",\"role\":\"" + u.role + "\"}}}");
    }

    User u;
    std::string token;
    if (!auth(r, ss, u, token))
        return fail(401, "请先登录，并在 Authorization 中提供 Bearer Token。 ");

    if (r.path == "/api/auth/me")
        return r.method == "GET"
                   ? ok(200, "{\"success\":true,\"data\":{\"id\":" + std::to_string(u.id)
                                 + ",\"username\":\"" + escJson(u.name)
                                 + "\",\"role\":\"" + u.role + "\"}}}")
                   : fail(405, "当前用户接口仅支持 GET。 ");

    if (r.path == "/api/auth/logout") {
        if (r.method != "POST") return fail(405, "退出登录接口仅支持 POST。 ");
        ss.del(token);
        return ok(200, "{\"success\":true,\"message\":\"已退出登录。\"}");
    }

    if (r.path == "/api/students/batch") {
        if (r.method != "POST") return fail(405, "批量导入接口仅支持 POST。 ");
        if (!admin(u)) return fail(403, "仅 admin 可以批量导入。 ");
        std::vector<std::string> x;
        if (!jarray(r.body, "students", x) || x.empty() || x.size() > 100)
            return fail(400, "students 必须是包含 1 到 100 条学生数据的数组。 ");
        std::vector<Student> a;
        for (size_t i = 0; i < x.size(); ++i) {
            Student z;
            std::string e;
            if (!student(x[i], z, e))
                return fail(400, "第 " + std::to_string(i + 1) + " 条数据无效：" + e);
            a.push_back(z);
        }
        std::vector<unsigned long> ids;
        size_t f = 0;
        std::string e;
        unsigned c = 0;
        if (!db.batch(a, ids, f, e, c))
            return ok(c == 1062 ? 409 : 500,
                      "{\"success\":false,\"message\":\"批量导入已整体回滚：第 "
                          + std::to_string(f + 1) + " 条数据失败，"
                          + escJson(c == 1062 ? "学号重复。" : e)
                          + "\",\"failed_index\":" + std::to_string(f) + "}");
        std::ostringstream o;
        o << "[";
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i) o << ",";
            o << ids[i];
        }
        o << "]";
        return ok(201, "{\"success\":true,\"message\":\"批量导入成功。\",\"data\":{\"inserted\":"
                           + std::to_string(ids.size()) + ",\"ids\":" + o.str() + "}}");
    }

    if (r.path == "/api/students") {
        if (r.method == "GET") {
            auto q = query(r.target);
            int p = 1, z = 10;
            std::string e;
            if (!pageInt(q, "page", 1, 1, 100000, p, e) ||
                !pageInt(q, "page_size", 10, 1, 100, z, e))
                return fail(400, e);
            std::string d;
            unsigned long long total = 0;
            if (!db.list(q.count("keyword") ? q.at("keyword") : "", p, z, d, total, e))
                return fail(500, e);
            auto pages = total ? (total + z - 1) / z : 0;
            return ok(200, "{\"success\":true,\"data\":" + d
                               + ",\"pagination\":{\"page\":" + std::to_string(p)
                               + ",\"page_size\":" + std::to_string(z)
                               + ",\"total\":" + std::to_string(total)
                               + ",\"total_pages\":" + std::to_string(pages) + "}}");
        }
        if (r.method == "POST") {
            if (!write(u)) return fail(403, "viewer 角色只能查询学生信息。 ");
            Student s;
            std::string e;
            if (!student(r.body, s, e)) return fail(400, e);
            unsigned long id;
            unsigned c = 0;
            if (!db.add(s, id, e, c))
                return fail(c == 1062 ? 409 : 500, c == 1062 ? "学号已存在。" : e);
            return ok(201, "{\"success\":true,\"message\":\"新增成功。\",\"data\":{\"id\":"
                               + std::to_string(id) + "}}");
        }
        return fail(405, "学生集合仅支持 GET 和 POST。 ");
    }

    std::smatch m;
    if (!std::regex_match(r.path, m, std::regex("^/api/students/([0-9]+)$")))
        return fail(404, "接口不存在。 ");
    unsigned long id;
    if (!idOf(m, id)) return fail(400, "学生 ID 无效。 ");

    if (r.method == "GET") {
        std::string d, e;
        bool found;
        if (!db.one(id, d, found, e)) return fail(500, e);
        return found ? ok(200, "{\"success\":true,\"data\":" + d + "}")
                     : fail(404, "学生不存在。 ");
    }
    if (r.method == "PUT") {
        if (!write(u)) return fail(403, "viewer 角色只能查询学生信息。 ");
        Student s;
        std::string e;
        if (!student(r.body, s, e)) return fail(400, e);
        bool changed;
        unsigned c = 0;
        if (!db.update(id, s, changed, e, c))
            return fail(c == 1062 ? 409 : 500, c == 1062 ? "学号已存在。" : e);
        if (!changed) {
            std::string d;
            bool found;
            if (!db.one(id, d, found, e)) return fail(500, e);
            if (!found) return fail(404, "学生不存在。 ");
        }
        return ok(200, "{\"success\":true,\"message\":\"修改成功。\"}");
    }
    if (r.method == "DELETE") {
        if (!admin(u)) return fail(403, "仅 admin 可以删除学生。 ");
        bool changed;
        std::string e;
        if (!db.remove(id, changed, e)) return fail(500, e);
        return changed ? ok(200, "{\"success\":true,\"message\":\"删除成功。\"}")
                       : fail(404, "学生不存在。 ");
    }
    return fail(405, "不支持的请求方法。 ");
}

static bool receive(SOCKET c, Request& r) {
    std::string raw;
    char b[4096];
    size_t total = 0;
    while (raw.size() < 1024 * 1024) {
        int n = recv(c, b, sizeof(b), 0);
        if (n <= 0) return false;
        raw.append(b, n);
        auto end = raw.find("\r\n\r\n");
        if (end == std::string::npos) continue;
        if (!total) {
            std::istringstream h(raw.substr(0, end));
            std::string line;
            std::getline(h, line);
            std::istringstream first(trim(line));
            first >> r.method >> r.target;
            while (std::getline(h, line)) {
                auto k = line.find(':');
                if (k != std::string::npos)
                    r.headers[lower(trim(line.substr(0, k)))] = trim(line.substr(k + 1));
            }
            size_t len = 0;
            if (auto i = r.headers.find("content-length"); i != r.headers.end())
                try {
                    len = std::stoul(i->second);
                } catch (...) {
                    return false;
                }
            total = end + 4 + len;
        }
        if (raw.size() >= total) {
            r.path = r.target.substr(0, r.target.find('?'));
            r.body = raw.substr(end + 4, total - end - 4);
            return !r.method.empty();
        }
    }
    return false;
}

static void sendAll(SOCKET c, const Response& r) {
    const char* reason = r.status == 200 ? "OK"
                         : r.status == 201 ? "Created"
                         : r.status == 204 ? "No Content"
                         : r.status == 400 ? "Bad Request"
                         : r.status == 401 ? "Unauthorized"
                         : r.status == 403 ? "Forbidden"
                         : r.status == 404 ? "Not Found"
                         : r.status == 405 ? "Method Not Allowed"
                         : r.status == 409 ? "Conflict"
                                           : "Internal Server Error";
    std::string text = "HTTP/1.1 " + std::to_string(r.status) + " " + reason
                       + "\r\nContent-Type: " + r.type
                       + "\r\nContent-Length: " + std::to_string(r.body.size())
                       + "\r\nAccess-Control-Allow-Origin: *"
                         "\r\nAccess-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS"
                         "\r\nAccess-Control-Allow-Headers: Content-Type, Authorization"
                         "\r\nConnection: close\r\n\r\n"
                       + r.body;
    for (size_t i = 0; i < text.size();) {
        int n = send(c, text.data() + i, (int)(text.size() - i), 0);
        if (n <= 0) break;
        i += n;
    }
}

static std::string indexHtml() {
    char p[MAX_PATH]{};
    GetModuleFileNameA(nullptr, p, MAX_PATH);
    std::vector<fs::path> a = {fs::current_path() / "web/index.html",
                               fs::path(p).parent_path() / "web/index.html"};
    for (auto& x : a) {
        std::ifstream f(x, std::ios::binary);
        if (f) return {std::istreambuf_iterator<char>(f), {}};
    }
    return "<h1>未找到 web/index.html</h1>";
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    Database db(config());
    std::string e;
    if (!db.setup(e)) {
        std::cerr << "MySQL 连接或初始化失败：" << e << "\n";
        return 1;
    }
    std::cout << "MySQL 连接成功。\n";
    WSADATA w{};
    if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return 1;
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    BOOL yes = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(8080);
    if (bind(s, (sockaddr*)&a, sizeof(a)) == SOCKET_ERROR ||
        listen(s, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "无法监听 http://127.0.0.1:8080（端口可能已被占用）。\n";
        return 1;
    }
    std::cout << "学生管理系统已启动：http://127.0.0.1:8080\n"
                 "演示账号：admin/admin123、teacher/teacher123、viewer/viewer123\n";
    Sessions sessions;
    for (;;) {
        SOCKET c = accept(s, nullptr, nullptr);
        Request r;
        if (c != INVALID_SOCKET && receive(c, r)) {
            Response out = r.path.rfind("/api/", 0) == 0
                               ? api(db, sessions, r)
                               : (r.method == "GET" && (r.path == "/" || r.path == "/index.html"))
                                     ? Response{200, "text/html; charset=utf-8", indexHtml()}
                                     : fail(404, "页面不存在。");
            sendAll(c, out);
        }
        if (c != INVALID_SOCKET) closesocket(c);
    }
}
