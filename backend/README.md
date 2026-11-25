# 被测系统：学生管理系统（课程提供）

本目录为课程提供的本地学生管理系统（C++ HTTP 服务 + MySQL），作为被测对象，仅用于接口测试实验。

- `postman测试项目.sln` / `main.cpp`：Visual Studio 2022 工程，x64 Debug/Release 运行后监听 `http://127.0.0.1:8080`
- `config.ini.example`：复制为 `config.ini` 后填写本机 MySQL 账号密码（请勿提交真实密码）
- `database.sql`：数据库与演示账号初始化脚本（服务首次启动也会自动创建）
- `Student_Management_API.postman_collection.json`：课程提供的 Postman 集合
- `web/index.html`：浏览器前端页面

演示账号：admin/admin123（全部权限）、teacher/teacher123（查询、新增、修改）、viewer/viewer123（仅查询）。
