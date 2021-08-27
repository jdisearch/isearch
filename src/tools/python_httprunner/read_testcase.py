import xlrd
import os


def read(testcase_file_path):
    """
    读取接口测试用例源文件，将读取的有效用例写入字典
    :param testcase_file_path: 读取用例文件的路径地址
    :return:
    """
    if not os.path.exists(testcase_file_path):
        print("%s 文件不存在，无法读取到文件内容" % testcase_file_path)
        return
    # 在命令行打印提示信息
    print('\n\033[1;33m====== 读取测试用例列表文件 ======\033[0m')
    print("文件地址：%s" % testcase_file_path)
    # 读取xls文件内容
    xls = xlrd.open_workbook(testcase_file_path)
    sh = xls.sheet_by_index(0)
    # 获得公共的头部文本信息
    common_header_text = sh.row_values(1)[2]
    # 头部无效行数定义，文件头3行是描述文本和列表头部栏位，不作为测试用例内容
    head_row_num = 3
    # 头部无效行索引定义，索引从0开始，文件0~2索引是描述文本和列表头部栏位，不作为测试用例内容
    head_row_index = head_row_num - 1
    # 预判断有效测试用例行数（即处理头部定义外可以视为测试用例的行，但不一定准确，比如出现行乱填的情况，需要在程序中进行排除）
    total_row_num = sh.nrows  # 文件总行数
    need_read_num = total_row_num - head_row_num  # 需要读取的总行数

    i = 1
    testcase_list = []
    print("读取到被测接口信息如下：")
    while i <= need_read_num:
        row_index = head_row_index + i  # 获得需要读取的行索引
        row_testcase_info = sh.row_values(row_index)  # 获取到一行测试用例文本信息

        is_execute = row_testcase_info[0]
        if is_execute == '是':  # 只有"是否执行"单元格为 是 时才将用例加入被执行序列
            print(str(row_testcase_info)[0:120] + '......')
            # 将每条测试用例组合为一个字典进行存储
            testcase = {'interface_name': row_testcase_info[1], 'url': row_testcase_info[2],
                        'assert_type': row_testcase_info[6],
                        'assert_value': row_testcase_info[7]}

            request_type = row_testcase_info[3]
            testcase['request_type'] = request_type

            body = row_testcase_info[5]
            body.strip()
            if len(body) > 0:  # 如果body没有填值，则默认随便给一个任意值
                testcase['body'] = eval(row_testcase_info[5])
            else:
                testcase['body'] = {"test": "test"}
            headers = row_testcase_info[4]  # 获得行中的headers文本
            if headers == "公共头部":  # 如果文本填写内文为 '公共头部'，则取文件第二行的"公共头部信息"文本值
                headers = common_header_text
            testcase['headers'] = eval(headers)
            testcase_list.append(testcase)
        i += 1

    print('用例总数：\033[1;32;40m%s条\033[0m' % len(testcase_list))
    return testcase_list
