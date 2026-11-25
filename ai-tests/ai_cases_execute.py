# -*- coding: utf-8 -*-
"""执行人工评审修订后的 AI 测试用例（21 条 + 数据清理），逐条真实请求并记录结果。"""
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
n = [0]
uc = [0]


def uniq(prefix):
    uc[0] += 1
    return f"{prefix}{TS}{uc[0]:04d}"


def call(case, source, method, path, token=None, body=None, expected=None, note="", expect_empty_data=False):
    n[0] += 1
    no = f"AI{n[0]:03d}"
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
    try:
        parsed = json.loads(raw)
    except Exception:
        parsed = {"raw": raw}
    if expect_empty_data:
        ok = (status == expected) and (parsed.get("data") == [])
    else:
        ok = (expected is None) or (status in (expected if isinstance(expected, tuple) else (expected,)))
    results.append({
        "用例编号": no, "用例来源": source, "原AI编号": case.split(" ")[0] if case else "",
        "测试场景": case, "请求方法": method, "接口地址": path, "请求数据": body,
        "预期结果": note or f"状态码 {expected}", "实际状态码": status, "实际响应": parsed,
        "是否通过": "通过" if ok else "失败",
    })
    mark = "PASS" if ok else "FAIL"
    print(f"[{mark}] {no} [{source}] {case} | 实际{status} | {raw[:100]}")
    return status, parsed


# ---- 登录获取各类 Token ----
call("A-02 管理员登录（正常）", "AI生成", "POST", "/api/auth/login",
     body={"username": "admin", "password": "admin123"}, expected=200, note="200 登录成功返回token")
s, r = call("补-4 错误密码登录", "人工补充", "POST", "/api/auth/login",
            body={"username": "admin", "password": "wrong123"}, expected=401, note="401 用户名或密码错误")
s, ra = call("A-02b 管理员重新登录取Token", "AI生成", "POST", "/api/auth/login",
             body={"username": "admin", "password": "admin123"}, expected=200, note="200 获取admin token")
admin_token = ra.get("data", {}).get("token", "")
s, rt = call("teacher登录（供A-09使用）", "AI生成", "POST", "/api/auth/login",
             body={"username": "teacher", "password": "teacher123"}, expected=200, note="200 获取teacher token")
teacher_token = rt.get("data", {}).get("token", "")
s, rv = call("viewer登录（供A-14使用）", "AI生成", "POST", "/api/auth/login",
             body={"username": "viewer", "password": "viewer123"}, expected=200, note="200 获取viewer token")
viewer_token = rv.get("data", {}).get("token", "")

# ---- 健康检查 ----
call("A-01 健康检查", "AI生成", "GET", "/api/health", expected=200, note="200 服务和MySQL已连接")

# ---- 新增学生 ----
a3_no = uniq("AI")
s, a3 = call("A-03 新增学生（正常数据）", "AI生成", "POST", "/api/students", token=admin_token,
             body={"student_no": a3_no, "name": "测试学生", "gender": "男", "age": 20,
                   "major": "软件工程", "phone": "13800001111"},
             expected=201, note="201 新增成功返回id")
a3_id = a3.get("data", {}).get("id")
call("A-04 新增学生（姓名为空）", "AI生成", "POST", "/api/students", token=admin_token,
     body={"student_no": uniq("AI"), "name": "", "gender": "男", "age": 20,
           "major": "软件工程", "phone": "13800001112"},
     expected=400, note="400 学号和姓名不能为空")
call("A-05 新增学生（年龄0）", "AI生成", "POST", "/api/students", token=admin_token,
     body={"student_no": uniq("AI"), "name": "年龄零", "gender": "男", "age": 0,
           "major": "软件工程", "phone": "13800001113"},
     expected=400, note="400 年龄必须是1到150之间的整数")
call("A-06 新增学生（年龄151）", "AI生成", "POST", "/api/students", token=admin_token,
     body={"student_no": uniq("AI"), "name": "年龄超", "gender": "男", "age": 151,
           "major": "软件工程", "phone": "13800001114"},
     expected=400, note="400 年龄必须是1到150之间的整数")
call("A-07 新增学生（学号为空）", "AI生成", "POST", "/api/students", token=admin_token,
     body={"student_no": "", "name": "无学号", "gender": "男", "age": 20,
           "major": "软件工程", "phone": "13800001115"},
     expected=400, note="400 学号和姓名不能为空")
call("A-08 新增学生（重复学号）", "AI生成", "POST", "/api/students", token=admin_token,
     body={"student_no": a3_no, "name": "重复学号", "gender": "男", "age": 20,
           "major": "软件工程", "phone": "13800001116"},
     expected=409, note="409 学号已存在")
call("A-13 新增学生（未登录无Token）", "AI生成", "POST", "/api/students",
     body={"student_no": uniq("AI"), "name": "无Token", "gender": "男", "age": 20,
           "major": "软件工程", "phone": "13800001117"},
     expected=401, note="401 请先登录")
call("A-14 新增学生（viewer越权）", "AI生成", "POST", "/api/students", token=viewer_token,
     body={"student_no": uniq("AI"), "name": "越权新增", "gender": "男", "age": 20,
           "major": "软件工程", "phone": "13800001118"},
     expected=403, note="403 viewer角色只能查询学生信息")
b1 = uniq("AIB")
s, b1r = call("补-1 边界年龄1（合法）", "人工补充", "POST", "/api/students", token=admin_token,
              body={"student_no": b1, "name": "边界最小", "gender": "女", "age": 1,
                    "major": "软件测试", "phone": "13800001119"},
              expected=201, note="201 边界值1允许新增")
b1_id = b1r.get("data", {}).get("id")
b2 = uniq("AIB")
s, b2r = call("补-2 边界年龄150（合法）", "人工补充", "POST", "/api/students", token=admin_token,
              body={"student_no": b2, "name": "边界最大", "gender": "女", "age": 150,
                    "major": "软件测试", "phone": "13800001120"},
              expected=201, note="201 边界值150允许新增")
b2_id = b2r.get("data", {}).get("id")

# ---- teacher 删除（评审修正：应为403） ----
t_no = uniq("AIT")
s, t_add = call("A-09a teacher新增一条学生", "AI生成", "POST", "/api/students", token=teacher_token,
                body={"student_no": t_no, "name": "教师新增", "gender": "女", "age": 22,
                      "major": "网络工程", "phone": "13800001121"},
                expected=201, note="201 teacher可以新增")
t_id = t_add.get("data", {}).get("id")
call("A-09r teacher删除学生（修正后）", "AI生成并经人工修订", "DELETE", f"/api/students/{t_id}",
     token=teacher_token, expected=403, note="403 仅admin可以删除学生（AI原预期200，已修正）")

# ---- 列表搜索分页 ----
kw = urllib.parse.quote("测试")
call("A-11 列表关键字搜索", "AI生成", "GET", f"/api/students?keyword={kw}&page=1&page_size=5",
     token=admin_token, expected=200, note="200 返回匹配列表与分页信息")
call("A-10r page=0（修正后）", "AI生成并经人工修订", "GET", "/api/students?page=0&page_size=10",
     token=admin_token, expected=400, note="400 page必须是1到100000之间的整数（AI原预期200，已修正）")
call("补-3 page_size=101 超上限", "人工补充", "GET", "/api/students?page=1&page_size=101",
     token=admin_token, expected=400, note="400 page_size必须是1到100之间的整数")

# ---- 批量导入 ----
call("A-12r 批量导入101条（修正后）", "AI生成并经人工修订", "POST", "/api/students/batch", token=admin_token,
     body={"students": [{"student_no": uniq("AIX"), "name": f"超限{i}", "gender": "男", "age": 20,
                         "major": "软件测试", "phone": "13800000000"} for i in range(101)]},
     expected=400, note="400 students必须是1到100条（AI原预期201，已修正）")
bn1, bn2 = uniq("AIBN"), uniq("AIBN")
call("A-15 批量导入2条合法", "AI生成", "POST", "/api/students/batch", token=admin_token,
     body={"students": [
         {"student_no": bn1, "name": "批量学生一", "gender": "女", "age": 19,
          "major": "软件工程", "phone": "13600000001"},
         {"student_no": bn2, "name": "批量学生二", "gender": "男", "age": 20,
          "major": "网络工程", "phone": "13600000002"}]},
     expected=201, note="201 inserted=2")
rb = uniq("AIRB")
call("A-16 批量导入含重复学号", "AI生成", "POST", "/api/students/batch", token=admin_token,
     body={"students": [
         {"student_no": rb, "name": "回滚第一条", "gender": "男", "age": 20,
          "major": "软件测试", "phone": "13700000001"},
         {"student_no": rb, "name": "回滚重复条", "gender": "女", "age": 20,
          "major": "软件测试", "phone": "13700000002"}]},
     expected=409, note="409 整体回滚")
kw2 = urllib.parse.quote(rb)
call("补-5 A-16回滚数据库核对", "人工补充", "GET", f"/api/students?keyword={kw2}",
     token=admin_token, expected=200, note="200 且data为空列表，证明第一条也未写入",
     expect_empty_data=True)

# ---- 数据清理（不计入测试结论） ----
for cid in (b1_id, b2_id, t_id):
    call(f"清理临时数据 id={cid}", "数据清理", "DELETE", f"/api/students/{cid}",
         token=admin_token, expected=200, note="200 删除成功")

(OUT / "测试结果_AI用例.json").write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
passed = sum(1 for r in results if r["用例来源"] != "数据清理" and r["是否通过"] == "通过")
total = sum(1 for r in results if r["用例来源"] != "数据清理")
print(f"\nAI评审后用例共 {total} 条，通过 {passed} 条，失败 {total - passed} 条")
