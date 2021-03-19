/*
 * =====================================================================================
 *
 *       Filename:  protocal.h
 *
 *    Description:  protocal definition.
 *
 *        Version:  1.0
 *        Created:  20/03/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __PROTOCAL_H__
#define __PROTOCAL_H__

#define PROTOCOL_MAGIC		0xFDFC
#define PACKET_MAX_LENGTH	0x00200000

typedef enum ACTICETYPE {
	KEEPALIVE = 1,
	DEVICEACTIVE = 2,
	ADCLICK = 3,
	DISTINCT = 4,
	CHANNEL_CALLBACK = 5,
	SECURITY_CALLBACK = 6,
	APP_MONITER = 7,
	BJSECURITY_CALLBACK = 8,
}E_CMDTYPE;

typedef enum ADCOLLECTRSP
{
	ADRSP_WECHAT_SUCCESS = -10,
	ADRSP_WECHAT_ERR = -9,
	ADRSP_JRTT_SUCCESS = -8,
	ADRSP_JRTT_ERR = -7,
	ADRSP_SPECIAL_SUCCESS = -6,
	ADRSP_SPECIAL_ERR = -5,
	ADRSP_NETWORK_ERR = -2,
	ADRSP_SYSTEM_ERR = -1,
	ADRSP_SUCCESS = 0,
	ADRSP_PARAMETER_ERR = 1,
	ADRSP_UNKNOW_FUNCTION_ID = 2,
	ADRSP_NOT_LOGIN = 3,
	ADRSP_SIGN_ERROR = 4,
	ADRSP_INSERT_ERR = 5,
	ADRSP_NOT_FIND = 6,
}E_ADCOLLECTRSP;

typedef enum ADCOLLECTTYPE
{
	ADTP_KEEPALIVE = 1,
	ADTP_DEVICE_ACTIVE = 2,
	ADTP_ADCOLLECT_CLICK = 3,
	ADTP_DUPLICATE_REMOVAL = 4,
	ADTP_QUERY_UNION_BY_IDFA = 5,
}E_ADCOLLECTTYPE;

typedef enum REPEATIDFATYPE
{
	RPTP_NO = 0,
	RPTP_YES = 1,
}E_REPEATIDFATYPE;

typedef enum QUERYDTCRESULT
{
	QDR_NODTC = -1,
	QDR_SUCCESS = 0,
	QDR_NOTFIND = 1,
	QDR_EXECERROR = 2,
}E_QUERYDTCRESULT;

#endif

