import net_request
import checker
import time
import json
import config
import colorama
from colorama import init, Fore, Back, Style

init(autoreset=True)


def run(testcase_list):
    print('\n\033[1;33m========= 执行接口测试 ==========\033[0m')
    result_list = []
    i = 1
    for testcase in testcase_list:
        print('[测试接口%s]' % i)
        result_list.append(run_case(testcase))
        i += 1
    return result_list


def run_case(case_options):
    """
    运行测试用例
    :param case_options: 测试用例执行所需要的相关参数字典
    :return:返回空值
    """
    # 如果从用例参数中不能获取request_type，则用例跳出，不做执行
    if 'request_type' not in case_options.keys():
        print("request_type 参数未设置,跳过此调用例执行，参数为：%s" % case_options)
        return

    request_type = case_options['request_type']  # 获得请求类型
    response_result = ''  # 请求返回后的存储变量
    begin_time = time.time()  # 记录请求前的时间

    # 执行post类型的请求
    if request_type.lower() == 'post':
        # 对用例参数的完整请进行检测
        if not checker.check_options(case_options):
            return
        # 发送post请求
        response_result = net_request.post(case_options['url'], case_options['headers'], case_options['body'])

    # 执行get类型的请求
    if request_type.lower() == 'get':
        if not checker.check_options(case_options):
            return
        # 发送get请求
        response_result = net_request.get(case_options['url'], case_options['headers'], case_options['body'])

    end_time = time.time()  # 记录请求完成后的时间
    # 对执行结果进行判断，检查是否用例的通过情况
    check_result = analyse_result(response_result, case_options['assert_value'], case_options['assert_type'])
    # 将执行信息输出到控制台
    cost_time = round(end_time - begin_time, 3)
    output_execute_info(case_options, response_result, check_result, cost_time)
    # 将执行结果进行组装并返回给调用方
    return {'case_options': case_options, 'response_result': response_result, 'check_result': check_result,
            'cost_time': cost_time}


def analyse_result(real_result, assert_value, assert_type):
    """
    分析请求返回的测试结果
    :param real_result: 返回请求的结果，json串
    :param assert_value: 期望结果字串，来自请求的case_options字典
    :param assert_type: 断言的判断方式，来自请求的case_options字典（提供：包含、相等）
    :return:返回判断的结果，通过未为True，失败为False
    """

    # 处理包含的逻辑，如果请求返回结果包含断言值，则判断结果为True
    if '包含' == assert_type:
        if json.dumps(real_result, ensure_ascii=False).__contains__(str(assert_value)):
            return True
    # 处理相等的逻辑，如果请求返回结果与断言值相同，则判断结果为True
    if '相等' == assert_type:
        if str(assert_value) == json.dumps(real_result, ensure_ascii=False):
            return True
    return False


def output_execute_info(case_options, response_result, check_result, time_consuming):
    """
    在命令行输出测试执行报告信息(支持简单模式和详细模式输出)
    :param case_options: 原测试用例信息
    :param response_result: 请求返回结果
    :param check_result: 测试结果，True、False
    :param time_consuming: 测试用例执行耗时
    :return:
    """
    if config.output_model.lower() == 'simple':
        simple_output(case_options, response_result, check_result, time_consuming)
    elif config.output_model.lower() == 'detail':
        detail_output(case_options, response_result, check_result, time_consuming)
    else:
        print("请到config.py文件中配置输出模式（output_model）！")
    return


def detail_output(case_options, response_result, check_result, time_consuming):
    """
    在命令行输出测试执行报告信息(详细模式)
    :param case_options: 原测试用例信息
    :param response_result: 请求返回结果
    :param check_result: 测试结果，True、False
    :param time_consuming: 测试用例执行耗时
    :return:
    """
    print("请求接口：", case_options['url'])
    print("接口名称：", case_options['interface_name'])
    print("请求类型：", case_options['request_type'].upper())
    print("请求参数：", case_options['body'])
    if check_result:  # 按测试结果对返回内容进行着色输出
        print('返回结果： \033[1;32;40m' + json.dumps(response_result, ensure_ascii=False) + '\033[0m')
        print('断言内容： \033[1;32;40m' + case_options['assert_value'] + '\033[0m')
    else:
        print('返回结果： \033[1;31;40m' + json.dumps(response_result, ensure_ascii=False) + '\033[0m')
        print('断言内容： \033[1;31;40m' + case_options['assert_value'] + '\033[0m')
    print('断言方式： %s' % case_options['assert_type'])
    print("执行耗时：", time_consuming, '秒')
    test_result = '\033[1;32;40m通过\033[0m' if check_result is True else '\033[1;31;40m失败\033[0m'  # 按测试结果染色命令行输出
    print('测试结果：', test_result)  # 无高亮
    print("\n")
    return


def simple_output(case_options, response_result, check_result, time_consuming):
    """
        在命令行输出测试执行报告信息(简单模式)
        :param case_options: 原测试用例信息
        :param response_result: 请求返回结果
        :param check_result: 测试结果，True、False
        :return:
        """
    print("请求接口：", case_options['url'])
    if check_result:  # 按测试结果对返回内容进行着色输出
        print('返回结果： \033[1;32;40m' + json.dumps(response_result, ensure_ascii=False)[0:120] + '......' + '\033[0m')
        print('断言内容： \033[1;32;40m' + case_options['assert_value'] + '\033[0m')
    else:
        print('返回结果： \033[1;31;40m' + json.dumps(response_result, ensure_ascii=False)[0:120] + '......' + '\033[0m')
        print('断言内容： \033[1;31;40m' + case_options['assert_value'] + '\033[0m')
    print("执行耗时：", time_consuming, '秒')
    test_result = '\033[1;32;40m通过\033[0m' if check_result is True else '\033[1;31;40m失败\033[0m'  # 按测试结果染色命令行输出
    print('测试结果：', test_result)  # 无高亮
    print("\n")
    return
