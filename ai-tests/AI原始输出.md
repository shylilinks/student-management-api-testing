# AI 原始输出（未修改，由 AI 助手直接生成）

以下为 AI 根据提示词直接生成的测试用例，共 16 条，未经人工修改，保留原始内容（含错误），作为人工评审的输入。

---

## AI 生成：学生管理系统接口测试用例（第一版）

**A-01 健康检查（正常）**
- 接口：GET /api/health
- 场景：确认服务和数据库连接正常
- 预期：200，返回"服务和 MySQL 已连接"

**A-02 管理员登录（正常）**
- 接口：POST /api/auth/login
- 数据：{"username":"admin","password":"admin123"}
- 预期：200，返回 token

**A-03 新增学生（正常数据）**
- 接口：POST /api/students，携带 admin 的 Bearer Token
- 数据：{"student_no":"20261001","name":"测试学生","gender":"男","age":20,"major":"软件工程","phone":"13800001111"}
- 预期：201，返回新增学生的 id

**A-04 新增学生（姓名为空）**
- 接口：POST /api/students
- 数据：{"student_no":"20261002","name":"","gender":"男","age":20,"major":"软件工程","phone":"13800001112"}
- 预期：400，提示姓名不能为空

**A-05 新增学生（年龄为 0，非法参数）**
- 接口：POST /api/students
- 数据：{"student_no":"20261003","name":"年龄零","gender":"男","age":0,"major":"软件工程","phone":"13800001113"}
- 预期：400

**A-06 新增学生（年龄为 151，非法参数）**
- 接口：POST /api/students
- 数据：{"student_no":"20261004","name":"年龄超","gender":"男","age":151,"major":"软件工程","phone":"13800001114"}
- 预期：400

**A-07 新增学生（学号为空）**
- 接口：POST /api/students
- 数据：{"student_no":"","name":"无学号","gender":"男","age":20,"major":"软件工程","phone":"13800001115"}
- 预期：400

**A-08 新增学生（重复学号）**
- 接口：POST /api/students
- 数据：与 A-03 相同学号 20261001 再次新增
- 预期：409，提示学号已存在

**A-09 删除学生（teacher 角色）**
- 接口：DELETE /api/students/{id}，携带 teacher 的 Bearer Token
- 场景：验证 teacher 可以删除自己新增的学生
- 预期：200，删除成功

**A-10 学生列表（page=0）**
- 接口：GET /api/students?page=0&page_size=10
- 场景：页码从 0 开始查询
- 预期：200，自动按第一页返回

**A-11 学生列表（关键字搜索）**
- 接口：GET /api/students?keyword=测试&page=1&page_size=5
- 预期：200，返回匹配的学生列表和分页信息

**A-12 批量导入（101 条边界）**
- 接口：POST /api/students/batch
- 数据：{"students":[101 条合法学生记录]}
- 场景：验证大批量导入能力
- 预期：201，全部导入成功

**A-13 新增学生（未登录，无 Token）**
- 接口：POST /api/students，不携带 Authorization 头
- 预期：401

**A-14 新增学生（viewer 越权）**
- 接口：POST /api/students，携带 viewer 的 Bearer Token
- 数据：一条合法学生记录
- 预期：403

**A-15 批量导入（正常 2 条）**
- 接口：POST /api/students/batch
- 数据：{"students":[2 条不同学号的合法记录]}
- 预期：201，返回 inserted=2

**A-16 批量导入（含重复学号）**
- 接口：POST /api/students/batch
- 数据：{"students":[2 条学号相同的记录]}
- 预期：409，事务回滚，两条都不写入数据库
