import executor
import read_testcase
import write_xls_report
import config
import reporter

# 1、读取测试用例列表文件
testcase_list = read_testcase.read(config.testcase_file_path)

# 2、执行用例列表中的用例
result_list = executor.run(testcase_list)

# 3、统计测试结果并在终端进行显示
reporter.output_report(result_list)

# 4、将测试结果写入xls文件中
write_xls_report.write(result_list)
