import colorama
from colorama import init, Fore, Back, Style

init(autoreset=True)


def output_report(result_list):
    print('\033[1;33m========== 统计测试结果 ==========\033[0m')
    print("执行总数： %s" % len(result_list))
    success_count = 0
    fail_count = 0
    cost_time = 0
    for result in result_list:
        cost_time += result['cost_time']
        if result['check_result']:
            success_count += 1
        else:
            fail_count += 1
    print('成功/失败：\033[1;32;40m%s\033[0m / \033[1;31;40m%s\033[0m' % (success_count, fail_count))
    print("执行总时长：%s 秒\n" % round(cost_time, 3))
    return
