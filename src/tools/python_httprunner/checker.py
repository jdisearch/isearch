def check_options(case_options):
    """
    检查post请求参数的合法性
    :param case_options: 用例内容
    :return:
    """
    if 'url' not in case_options.keys():
        print("url 参数未设置,跳过此调用例执行，参数为：%s" % case_options)
        return False
    if 'body' not in case_options.keys():
        print("body 参数未设置,跳过此调用例执行，参数为：%s" % case_options)
        return False
    if 'headers' not in case_options.keys():
        print("headers 参数未设置,跳过此调用例执行，参数为：%s" % case_options)
        return False
    if 'assert_type' not in case_options.keys():
        print("assert_type 参数未设置,跳过此调用例执行，参数为：%s" % case_options)
        return False
    if 'assert_value' not in case_options.keys():
        print("assert_value 参数未设置,跳过此调用例执行，参数为：%s" % case_options)
        return False
    return True