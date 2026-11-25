# 第一次课后作业：登录表单独立练习
# 用例1 正常登录：自定用户名 + 非真实密码 + 上海 + 女性 + 阅读/音乐 + 不勾选记住我
# 用例2 用户名为空
# 用例3 密码为空
# 页面提示使用显式等待获取，实际结果与预期结果用 assert 比较
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import Select, WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from pathlib import Path
import time

PAGE = Path(__file__).resolve().parent / "示例.html"
SHOTS = Path(__file__).resolve().parent / "截图"

USERNAME = "stu_2026"      # 自定用户名
PASSWORD = "Fake@2026"     # 非真实密码

EXPECTED = {
    1: f"登录成功！用户：{USERNAME}，城市：sh，性别：female，爱好：read/music，记住我：false",
    2: "用户名和密码都不能为空！",
    3: "用户名和密码都不能为空！",
}


def fill_and_submit(driver, username, password):
    driver.get(PAGE.as_uri())
    wait = WebDriverWait(driver, 10)
    if username:
        driver.find_element(By.ID, "username").send_keys(username)
    if password:
        driver.find_element(By.NAME, "password").send_keys(password)
    Select(driver.find_element(By.ID, "city")).select_by_visible_text("上海")
    female = driver.find_element(By.ID, "gender-female")
    if not female.is_selected():
        female.click()
    for hobby_id in ("hobby-read", "hobby-music"):
        box = driver.find_element(By.ID, hobby_id)
        if not box.is_selected():
            box.click()
    # “记住我”保持不勾选
    driver.find_element(By.ID, "loginBtn").click()
    msg = wait.until(EC.visibility_of_element_located((By.ID, "message")))
    return msg.text


options = webdriver.ChromeOptions()
driver = webdriver.Chrome(options=options)
try:
    cases = [
        (1, USERNAME, PASSWORD, "正常登录"),
        (2, "", PASSWORD, "用户名为空"),
        (3, USERNAME, "", "密码为空"),
    ]
    passed_count = 0
    for no, user, pwd, name in cases:
        actual = fill_and_submit(driver, user, pwd)
        passed = actual == EXPECTED[no]
        driver.save_screenshot(str(SHOTS / f"独立练习_用例{no}_{name}.png"))
        print(f"用例{no}（{name}）")
        print(f"  输入：用户名={user if user else '(空)'}，密码={'(空)' if not pwd else '******'}，"
              f"城市=上海，性别=女，爱好=阅读+音乐，记住我=不勾选")
        print(f"  预期：{EXPECTED[no]}")
        print(f"  实际：{actual}")
        print(f"  结论：{'通过' if passed else '失败'}")
        assert actual == EXPECTED[no], f"用例{no}断言失败：实际提示为 {actual}"
        passed_count += 1
        time.sleep(0.5)
    print(f"共 {len(cases)} 条用例，通过 {passed_count} 条，失败 {len(cases) - passed_count} 条。")
finally:
    driver.quit()
