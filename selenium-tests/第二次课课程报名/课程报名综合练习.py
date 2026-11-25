# 第二次课后作业：课程报名综合验证（pages/comprehensive.html）
# 四条用例均使用 assert 比较完整提示文本，动态反馈使用 WebDriverWait 等待；
# 姓名/课程为空时页面直接显示必填错误，不应出现确认弹窗。
from pathlib import Path
import time

from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import Select, WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from selenium.common.exceptions import NoAlertPresentException

ROOT = Path(__file__).resolve().parent
PAGE = ROOT / "pages" / "comprehensive.html"
SHOTS = ROOT / "截图"


def alert_exists(driver):
    try:
        driver.switch_to.alert
        return True
    except NoAlertPresentException:
        return False


def run_case(driver, case_no, name, course, action, expected):
    driver.get(PAGE.resolve().as_uri())
    wait = WebDriverWait(driver, 10)

    if name:
        driver.find_element(By.ID, "student-name").send_keys(name)
    if course:
        Select(driver.find_element(By.ID, "course")).select_by_value(course)

    driver.find_element(By.ID, "submit").click()

    if name and course:
        alert = wait.until(EC.alert_is_present())
        assert alert.text == "确认提交报名？", f"确认弹窗文案异常：{alert.text}"
        if action == "accept":
            alert.accept()
        elif action == "cancel":
            alert.dismiss()
        else:
            raise ValueError(f"完整表单用例的 action 不支持：{action}")
    else:
        time.sleep(1)
        assert not alert_exists(driver), "必填项为空时不应出现确认弹窗"

    actual = wait.until(
        EC.visibility_of_element_located((By.ID, "result"))
    ).text

    assert actual == expected, f"用例{case_no}断言失败：预期 {expected!r}，实际 {actual!r}"
    driver.save_screenshot(str(SHOTS / f"综合作业_用例{case_no}.png"))

    print(f"用例{case_no}: 姓名={name if name else '(空)'}，课程={course if course else '(未选)'}，操作={action}")
    print(f"  预期结果：{expected}")
    print(f"  实际结果：{actual}")
    print(f"  结论：通过")


options = webdriver.ChromeOptions()
driver = webdriver.Chrome(options=options)
try:
    cases = [
        (1, "周同学", "selenium", "accept", "报名成功：周同学｜selenium"),
        (2, "周同学", "python", "cancel", "已取消提交"),
        (3, "", "selenium", "empty_name", "提交失败：姓名和课程都必填"),
        (4, "周同学", "", "empty_course", "提交失败：姓名和课程都必填"),
    ]
    for no, name, course, action, expected in cases:
        run_case(driver, no, name, course, action, expected)
    print("四条用例全部断言通过。")
finally:
    driver.quit()
