import requests
import traceback


def post(url, headers=None, body=None):
    """
    发送post请求
    :param url:请求地址
    :param headers:头部信息
    :param body:请求入参
    :return:
    """

    response_post = requests.post(url, json=body, headers=headers)
    res = response_post.text
    if res.__contains__('<title>登录页</title>'):
        return {"错误": "cookie失效"}
    return response_post.json()


def get(url, headers, body):
    """
    发送get请求
    :param headers: 头部信息
    :param url:请求地址
    :param body:请求入参
    :return:
    """
    response_get = requests.get(url, params=body, headers=headers)
    res = response_get.text
    if res.__contains__('<title>登录页</title>'):
        return {"错误": "cookie失效"}
    return response_get.json()
