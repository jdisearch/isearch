import xlwt
import config
import time
import os
import json
import colorama
from colorama import init, Fore, Back, Style

init(autoreset=True)


def write(result_list):
    """
    将执行测试结果的总汇内容写入xls文件中
    :param result_list:
    :return:
    """
    # 读取配置文件，若开启xls报告，则进行xsl报告生成
    if not config.xls_report:
        return
    if not os.path.exists(config.report_path):  # 从配置文件中获取父目录路径，若目录不存在则进行创建
        os.mkdir(config.report_path)
    # 拼接生成的报告文件路径
    report_path = os.path.join(config.report_path, time.strftime("%Y%m%d%H%M%S", time.localtime()) + '_report.xls')
    print('\033[1;33m====== 生成xls格式的测试报告 ======\033[0m')
    # 创建一个workbook 设置编码
    workbook = xlwt.Workbook(encoding='utf-8')
    # 创建一个worksheet
    worksheet = workbook.add_sheet('测试结果')
    # 设置列表头部样式
    alignment = xlwt.Alignment()
    alignment.horz = xlwt.Alignment.HORZ_CENTER
    alignment.vert = xlwt.Alignment.VERT_CENTER
    header_style = xlwt.XFStyle()
    header_style.alignment = alignment
    col1 = worksheet.col(0)
    col1.width = 256 * 8
    col2 = worksheet.col(1)
    col2.width = 256 * 35
    col3 = worksheet.col(2)
    col3.width = 256 * 80
    col4 = worksheet.col(3)
    col4.width = 256 * 10
    col5 = worksheet.col(4)
    col5.width = 256 * 40
    col6 = worksheet.col(5)
    col6.width = 256 * 12
    col7 = worksheet.col(6)
    col7.width = 256 * 10
    # 设置表头字体大小和颜色
    font = xlwt.Font()
    font.height = 20 * 12
    font.colour_index = 17
    header_style.font = font
    # 设置头部第一行的行高
    tall_style = xlwt.easyxf('font:height 480')
    row1 = worksheet.row(0)
    row1.set_style(tall_style)

    # 定义表格头部栏文本
    worksheet.write(0, 0, '序号', header_style)
    worksheet.write(0, 1, '接口名称', header_style)
    worksheet.write(0, 2, '被测接口', header_style)
    worksheet.write(0, 3, '请求方式', header_style)
    worksheet.write(0, 4, '请求参数', header_style)
    worksheet.write(0, 5, '耗时(秒)', header_style)
    worksheet.write(0, 6, '测试结果', header_style)
    worksheet.write(0, 7, '失败备注', header_style)

    # 结果单元格样式
    col_style = xlwt.XFStyle()
    col_style.alignment = alignment
    style_success = xlwt.easyxf('pattern: pattern solid, fore_colour green')
    style_success.alignment = alignment
    style_fail = xlwt.easyxf('pattern: pattern solid, fore_colour red')
    style_fail.alignment = alignment

    index = 1  # 设置序号
    for result in result_list:  # 循环遍历出测试结果并按顺序写入xls文件中
        worksheet.write(index, 0, index, col_style)
        worksheet.write(index, 1, result['case_options']['interface_name'])
        worksheet.write(index, 2, result['case_options']['url'])
        worksheet.write(index, 3, str(result['case_options']['request_type']).upper(), col_style)
        if result['case_options'].get("body", ) is not None:  # 存在body参数时则输出到文件
            worksheet.write(index, 4, json.dumps(result['case_options'].get('body', )))
        worksheet.write(index, 5, result['cost_time'], col_style)
        res = "通过" if result['check_result'] else '失败'  # 通过检测结果True和False来重新设置结果文本问 通过和失败

        if result['check_result']:
            worksheet.write(index, 6, res, style_success)  # 成功文本背景染色为绿色
        else:
            worksheet.write(index, 6, res, style_fail)  # 成功文本背景染色为红色
            worksheet.write(index, 7, '断言内容：' + str(result['case_options']['assert_value']) + "\n\r实际返回：" + str(
                result['response_result'])[0:30000])  # 失败用例，将断言内容和实际结果进行输出,实际结果输出长度在30000以内
        index += 1
    workbook.save(report_path)
    print("报告成功创建：" + str(report_path))
    return
