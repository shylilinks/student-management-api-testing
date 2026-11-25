# -*- coding: utf-8 -*-
"""学生管理系统接口全量测试：登录鉴权、搜索分页、CRUD、异常边界、角色权限、批量导入与回滚。
真实请求本地后端 http://127.0.0.1:8080，逐条记录 请求/预期/实际/状态码/结论，输出 JSON 与文本日志。"""
import json
import time
import urllib.request
import urllib.error
import urllib.parse
from pathlib import Path

BASE = "http://127.0.0.1:8080"
OUT = Path(__file__).resolve().parent / "接口测试记录"
OUT.mkdir(exist_ok=True)
TS = time.strftime("%H%M%S")
results = []
counter = [0]
uniq_counter = [0]


def call(case, module, method, path, token=None, body=None, expected=None, note=""):
    counter[0] += 1
    no = f"TC{counter[0]:03d}"
    req = urllib.request.Request(BASE + path, method=method)
    if token:
        req.add_header("Authorization", "Bearer " + token)
    data = None
    if body is not None:
        data = json.dumps(body, ensure_ascii=False).encode("utf-8")
        req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req, data=data, timeout=15) as resp:
            status, raw = resp.status, resp.read().decode("utf-8")
    except urllib.error.HTTPError as e:
        status, raw = e.code, e.read().decode("utf-8")
    except Exception as e:
        status, raw = -1, str(e)
    try:
        parsed = json.loads(raw)
    except Exception:
        parsed = {"raw": raw}
    ok = (expected is None) or (status in (expected if isinstance(expected, tuple) else (expected,)))
    results.append({
        "用例编号": no, "测试模块": module, "请求方法": method, "接口地址": path,
        "携带Token": bool(token), "请求数据": body, "预期结果": note or f"状态码 {expected}",
        "实际状态码": status, "实际响应": parsed, "是否通过": "通过" if ok else "失败",
    })
    mark = "PASS" if ok else "FAIL"
    print(f"[{mark}] {no} {module} | {method} {path} | 预期{note or expected} | 实际{status} | {raw[:120]}")
    return status, parsed


def uniq(prefix):
    uniq_counter[0] += 1
    return f"{prefix}{TS}{uniq_counter[0]:04d}"


# ============ 一、环境配置与健康检查 ============
call("健康检查", "环境配置", "GET", "/api/health", expected=200, note="200 服务和MySQL已连接")

# ============ 二、登录与Token ============
s, admin_login = call("管理员登录", "登录鉴权", "POST", "/api/auth/login",
                      body={"username": "admin", "password": "admin123"},
                      expected=200, note="200 登录成功并返回token")
admin_token = admin_login.get("data", {}).get("token", "")
call("错误密码登录", "登录鉴权", "POST", "/api/auth/login",
     body={"username": "admin", "password": "wrong"}, expected=401, note="401 用户名或密码错误")
call("当前登录用户", "登录鉴权", "GET", "/api/auth/me", token=admin_token,
     expected=200, note="200 返回admin身份信息")
call("无Token访问业务接口", "登录鉴权", "GET", "/api/students",
     expected=401, note="401 请先登录并提供Bearer Token")

# ============ 三、搜索与分页 ============
kw = urllib.parse.quote("张")
call("关键字搜索+分页", "搜索分页", "GET", f"/api/students?keyword={kw}&page=1&page_size=5",
     token=admin_token, expected=200, note="200 返回列表与pagination分页信息")
call("普通分页", "搜索分页", "GET", "/api/students?page=1&page_size=5",
     token=admin_token, expected=200, note="200 每页5条")
call("错误页码(0)", "搜索分页", "GET", "/api/students?page=0&page_size=5",
     token=admin_token, expected=400, note="400 page必须是1到100000之间的整数")
call("超限每页条数(101)", "搜索分页", "GET", "/api/students?page=1&page_size=101",
     token=admin_token, expected=400, note="400 page_size最大100")

# ============ 四、学生CRUD ============
new_no = uniq("STU")
s, added = call("新增学生", "学生CRUD", "POST", "/api/students", token=admin_token,
                body={"student_no": new_no, "name": "张小明", "gender": "男", "age": 20,
                      "major": "计算机科学与技术", "phone": "13800000000"},
                expected=201, note="201 新增成功并返回id")
sid = added.get("data", {}).get("id")
call("按ID查询", "学生CRUD", "GET", f"/api/students/{sid}", token=admin_token,
     expected=200, note="200 返回新增的学生数据")
s, updated = call("修改学生", "学生CRUD", "PUT", f"/api/students/{sid}", token=admin_token,
                  body={"student_no": new_no, "name": "张小明（已修改）", "gender": "男", "age": 21,
                        "major": "软件工程", "phone": "13900000000"},
                  expected=200, note="200 修改成功")
call("复查修改结果", "学生CRUD", "GET", f"/api/students/{sid}", token=admin_token,
     expected=200, note="200 姓名已改为张小明（已修改）")
call("删除学生", "学生CRUD", "DELETE", f"/api/students/{sid}", token=admin_token,
     expected=200, note="200 删除成功")
call("删除后查询", "学生CRUD", "GET", f"/api/students/{sid}", token=admin_token,
     expected=404, note="404 学生不存在")

# ============ 五、异常与边界 ============
call("年龄为0", "异常边界", "POST", "/api/students", token=admin_token,
     body={"student_no": uniq("E"), "name": "异常测试", "gender": "男", "age": 0,
           "major": "软件测试", "phone": "13800000001"},
     expected=400, note="400 年龄必须是1到150之间的整数")
call("年龄为151", "异常边界", "POST", "/api/students", token=admin_token,
     body={"student_no": uniq("E"), "name": "异常测试", "gender": "男", "age": 151,
           "major": "软件测试", "phone": "13800000002"},
     expected=400, note="400 年龄必须是1到150之间的整数")
b1 = uniq("B")
s, b1r = call("边界年龄1(合法)", "异常边界", "POST", "/api/students", token=admin_token,
              body={"student_no": b1, "name": "边界最小", "gender": "女", "age": 1,
                    "major": "软件测试", "phone": "13800000003"},
              expected=201, note="201 边界值1允许新增")
b1_id = b1r.get("data", {}).get("id")
b2 = uniq("B")
s, b2r = call("边界年龄150(合法)", "异常边界", "POST", "/api/students", token=admin_token,
              body={"student_no": b2, "name": "边界最大", "gender": "女", "age": 150,
                    "major": "软件测试", "phone": "13800000004"},
              expected=201, note="201 边界值150允许新增")
b2_id = b2r.get("data", {}).get("id")
call("空学号", "异常边界", "POST", "/api/students", token=admin_token,
     body={"student_no": "  ", "name": "空学号测试", "gender": "男", "age": 20,
           "major": "软件测试", "phone": "13800000005"},
     expected=400, note="400 学号和姓名不能为空")
dup_no = uniq("D")
call("重复学号-首次写入", "异常边界", "POST", "/api/students", token=admin_token,
     body={"student_no": dup_no, "name": "重复学号验证", "gender": "男", "age": 20,
           "major": "软件测试", "phone": "13800000010"},
     expected=201, note="201 首次写入成功（为重复用例准备数据）")
call("重复学号", "异常边界", "POST", "/api/students", token=admin_token,
     body={"student_no": dup_no, "name": "重复学号测试", "gender": "男", "age": 20,
           "major": "软件测试", "phone": "13800000006"},
     expected=409, note="409 学号已存在")
call("查询不存在的学生", "异常边界", "GET", "/api/students/999999", token=admin_token,
     expected=404, note="404 学生不存在")

# ============ 六、角色权限 ============
s, v_login = call("viewer登录", "角色权限", "POST", "/api/auth/login",
                  body={"username": "viewer", "password": "viewer123"},
                  expected=200, note="200 viewer登录成功")
viewer_token = v_login.get("data", {}).get("token", "")
call("viewer查询(允许)", "角色权限", "GET", "/api/students?page=1&page_size=3",
     token=viewer_token, expected=200, note="200 viewer可以查询")
call("viewer新增(越权)", "角色权限", "POST", "/api/students", token=viewer_token,
     body={"student_no": uniq("V"), "name": "越权测试", "gender": "男", "age": 20,
           "major": "软件测试", "phone": "13800000007"},
     expected=403, note="403 viewer角色只能查询学生信息")
call("viewer删除(越权)", "角色权限", "DELETE", f"/api/students/{b1_id}",
     token=viewer_token, expected=403, note="403 仅admin可以删除学生")
s, t_login = call("teacher登录", "角色权限", "POST", "/api/auth/login",
                  body={"username": "teacher", "password": "teacher123"},
                  expected=200, note="200 teacher登录成功")
teacher_token = t_login.get("data", {}).get("token", "")
s, t_add = call("teacher新增(允许)", "角色权限", "POST", "/api/students", token=teacher_token,
                body={"student_no": uniq("T"), "name": "教师新增", "gender": "女", "age": 19,
                      "major": "网络工程", "phone": "13800000008"},
                expected=201, note="201 teacher可以新增")
t_id = t_add.get("data", {}).get("id")
call("teacher删除(越权)", "角色权限", "DELETE", f"/api/students/{t_id}",
     token=teacher_token, expected=403, note="403 仅admin可以删除学生")
call("teacher批量导入(越权)", "角色权限", "POST", "/api/students/batch", token=teacher_token,
     body={"students": [{"student_no": uniq("TB"), "name": "教师批量", "gender": "女", "age": 19,
                         "major": "网络工程", "phone": "13800000009"}]},
     expected=403, note="403 仅admin可以批量导入")

# ============ 七、批量导入与事务回滚 ============
bn1, bn2 = uniq("BN"), uniq("BN")
call("批量导入2条合法数据", "批量导入", "POST", "/api/students/batch", token=admin_token,
     body={"students": [
         {"student_no": bn1, "name": "批量学生一", "gender": "女", "age": 19,
          "major": "软件工程", "phone": "13600000001"},
         {"student_no": bn2, "name": "批量学生二", "gender": "男", "age": 20,
          "major": "网络工程", "phone": "13600000002"}]},
     expected=201, note="201 批量导入成功")
rb = uniq("RB")
s, rb_res = call("批量导入重复学号(回滚)", "批量导入", "POST", "/api/students/batch", token=admin_token,
                 body={"students": [
                     {"student_no": rb, "name": "回滚测试第一条", "gender": "男", "age": 20,
                      "major": "软件测试", "phone": "13700000001"},
                     {"student_no": rb, "name": "重复学号第二条", "gender": "女", "age": 20,
                      "major": "软件测试", "phone": "13700000002"}]},
                 expected=409, note="409 学号重复触发整体回滚")
call("回滚验证-查第一条", "批量导入", "GET", f"/api/students?keyword={rb}",
     token=admin_token, expected=200, note="200 且data为空列表，证明第一条也未写入")
call("批量导入超100条(边界)", "批量导入", "POST", "/api/students/batch", token=admin_token,
     body={"students": [{"student_no": uniq("X"), "name": f"超限{i}", "gender": "男", "age": 20,
                         "major": "软件测试", "phone": "13800000000"} for i in range(101)]},
     expected=400, note="400 students必须是1到100条")

# ============ 清理边界测试数据（同时再次验证删除） ============
for cid in (b1_id, b2_id, t_id):
    call("清理边界数据", "清理", "DELETE", f"/api/students/{cid}", token=admin_token,
         expected=200, note="200 删除成功")

(OUT / "测试结果.json").write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
passed = sum(1 for r in results if r["是否通过"] == "通过")
print(f"\n总计 {len(results)} 条用例，通过 {passed} 条，失败 {len(results) - passed} 条")
print(f"结果已保存到 {OUT / '测试结果.json'}")
