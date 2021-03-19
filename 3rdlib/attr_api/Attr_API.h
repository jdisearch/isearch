#ifndef ATTR_API_H
#define ATTR_API_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


int Attr_API_Set(int attr,int iValue);


int Get_Attr_Value(int attr,int *iValue); 


int Attr_API(int attr,int iValue);


int get_adv_memlen();


int get_adv_memusedlen();


int adv_attr_set(int attr_id , size_t len , char* pvalue);


int get_adv_mem(size_t offset , size_t len , char* pOut);


int setNumAttrWithIP(const char* strIP, int iAttrID, int iValue);


int setStrAttrWithIP(const char* strIP, int iAttrID, size_t len , char* pval);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // ATTR_API_H

