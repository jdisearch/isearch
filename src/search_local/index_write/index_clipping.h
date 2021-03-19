/*
 * =====================================================================================
 *
 *       Filename:  index_clipping.h
 *
 *    Description:  IndexClipping class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  shrewdlin, linjinming@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef SRC_INDEX_GEN_INDEX_CLIPPING_H_
#define SRC_INDEX_GEN_INDEX_CLIPPING_H_
#include <map>
#include <set>
#include <vector>
#include <sstream>
#include "index_conf.h"
#include "dtcapi.h"
using namespace std;

struct index_item {
	uint32_t created_time;
	uint8_t field;
	uint32_t freq;
	string doc_id;
	int doc_version;
	time_t end_time;
    bool friend operator<(const struct index_item &left, const struct index_item &right)  //对于<的重载
    {
        if (left.field < right.field)
        {
            return true;
        }
        else if(left.field == right.field)
        {
        	if(left.freq < right.freq){
        		return true;
        	}
        	else if(left.freq == right.freq){
        		if(left.created_time < right.created_time){
        			return true;
        		}else if(left.created_time == right.created_time){
        			if(left.doc_id < right.doc_id)
        				return true;
        			else
        				return false;
        		}
        		else{
        			return false;
        		}
        	}else{
        		return false;
        	}
        }
        else
        {
        	return false;
        }
    }
};

class IndexClipping {
public:
	IndexClipping(DTC::Server* server);
	~IndexClipping();

	bool get_rows_and_index_clipping(DTC::Server* dtc_server,string key,uint32_t rows_limit);
	bool get_rows_and_top_index_clipping(DTC::Server* dtc_server,string key,uint32_t rows_limit);
private:
	bool do_delete_index_dtc(DTC::Server* dtc_server,string key,const struct index_item &item);
	bool do_index_clipping(DTC::Server* dtc_server,string key,uint32_t rows_limit);

	bool do_delete_top_index_dtc(DTC::Server* dtc_server,string key,const struct index_item &item);
	bool do_top_index_clipping(DTC::Server* dtc_server,string key,uint32_t rows_limit);

	bool is_active_doc(string &doc_id,uint32_t appid,int doc_version,int top);


private:
	set<struct index_item> indexSet;
	DTC::Server* snapshot_server;

};



#endif /* SRC_INDEX_GEN_INDEX_CLIPPING_H_ */
