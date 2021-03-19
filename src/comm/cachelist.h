/*
 * =====================================================================================
 *
 *       Filename:  cachelist.h
 *
 *    Description:  cachelist class definition.
 *
 *        Version:  1.0
 *        Created:  28/05/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */


#ifndef __CACHELIST_H
#define __CACHELIST_H

#include "list.h"
#include "hlist.h"
#include <stdint.h>
#include "namespace.h"

#define MAX_HASH_DEPTH	65535
#define MAX_KEY_LENGTH  128

TTC_BEGIN_NAMESPACE

struct blslot
{
	struct hlist_node hash; /* link to hash_list */
	struct list_head  time; /* link to time_list */
	unsigned int	  ksize;
	unsigned int	  vsize;
	unsigned int	  expired;
	char 		      key[MAX_KEY_LENGTH];
	char 		      value[0];
}__attribute__((__aligned__(1)));

class CCacheList
{
	public:
		CCacheList();
		~CCacheList();

		int init_list(const unsigned max, const unsigned type, const unsigned expired);
		int add_list(const char *packedkey, const char *value, const unsigned ksize, const unsigned vsize);
		int in_list( const char *packedkey, const unsigned ksize, uint8_t *value, unsigned &vsize);

		/* dump all blslot in blacklist, debug only */	
		void dump_all_blslot(void);
	private:
		void stat_everything(const struct blslot *, const int add);

	protected:
		/* try expire all expired slot */
		int try_expired_list(void);

	protected:
		unsigned current_blslot_count;

	private:
		/* double linked hash list with single pointer list head */
		struct hlist_head hash_list[MAX_HASH_DEPTH];

		/* time list */
		struct list_head time_list;

		unsigned max_blslot_count;
		unsigned blslot_expired_time;
		unsigned key_type;
};

TTC_END_NAMESPACE
#endif
