# 学生管理系统接口测试项目

《软件测试与质量保证》课程实验项目：对本地运行的学生管理系统（C++ + MySQL）进行接口测试、Selenium 自动化测试与 AI 辅助测试用例设计的完整实践。

## 项目内容

| 目录 | 说明 |
| --- | --- |
| `backend/` | 被测系统：本地学生管理系统源码、数据库脚本与 Postman 集合（课程提供，仅用于测试） |
| `api-tests/` | 手工设计的接口黑盒测试脚本（38 条用例：登录鉴权、搜索分页、CRUD、异常边界、角色权限、批量导入回滚）及执行结果 |
| `ai-tests/` | AI 辅助测试流程：AI 提示词、AI 原始输出、人工评审说明、评审修订后用例的执行脚本与结果 |
| `selenium-tests/` | Selenium 两个实验的独立练习脚本：登录表单练习（3 用例）、课程报名综合验证（4 用例） |
| `screenshots/` | 关键测试证据截图 |
| `docs/` | 测试结论与实验记录文档 |

## 测试结果摘要

- 手工黑盒接口用例：38 条，全部通过
- AI 生成并经人工修订用例：21 条（修正 AI 原始输出中 3 处预期错误，补充 5 条遗漏场景），全部通过
- Selenium 独立练习：登录表单 3/3 通过，课程报名 4/4 通过（assert 断言 + 显式等待）

## 运行方式

1. 准备 MySQL 8.0，复制 `backend/config.ini.example` 为 `config.ini` 并填写本机密码。
2. 运行 `backend` 中编译好的服务程序（或用 Visual Studio 2022 打开 `backend/postman测试项目.sln` 生成运行），服务地址 `http://127.0.0.1:8080`。
3. 接口测试：`python api-tests/api_test.py`
4. AI 用例执行：`python ai-tests/ai_cases_execute.py`
5. Selenium 练习：在 `selenium-tests` 对应目录执行（需要 Python 3.9+、selenium、Chrome/Edge）。

## 说明

- 被测的后端程序与 Postman 基础集合由课程提供，本仓库的测试用例设计、测试脚本、AI 提示词整理与人工评审、实验记录为本人原创工作。
- 演示账号（admin/teacher/viewer）仅用于本地教学测试。
- 仓库中不包含任何数据库密码、真实 Token 等敏感信息。
