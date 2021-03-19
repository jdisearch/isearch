#include <time.h>
#include "log.h"
#include "cachelist.h"
#include "keycodec.h"

TTC_USING_NAMESPACE

CCacheList::CCacheList():
	current_blslot_count(0),
	max_blslot_count(0),
	blslot_expired_time(0)
{
}

CCacheList::~CCacheList()
{
}

int CCacheList::init_list(const unsigned max, const unsigned keytype, const unsigned expired)
{
	for(int i=0; i < MAX_HASH_DEPTH; ++i)
	{
		INIT_HLIST_HEAD(hash_list + i);
	}

	INIT_LIST_HEAD(&time_list);

	key_type = keytype;
	current_blslot_count = 0;
	max_blslot_count     = max<1000000? max:1000000;  	/* max:1000000 slots */
	blslot_expired_time  = expired;

	return 0;
}

int CCacheList::add_list(const char *ptr, const char *val, const unsigned ksize, const unsigned vsize)
{
	if(!ptr || ksize >= MAX_KEY_LENGTH){
		return -1;
	}

	CKeyCodec key_codec(key_type);

	struct hlist_head *h 	= hash_list + key_codec.key_hash(ptr, ksize)%MAX_HASH_DEPTH;
	struct hlist_node *pos  = 0;
	struct blslot *tpos 	= 0;

	hlist_for_each_entry(tpos, pos, h, hash)
	{
		/* hit */
		if(0 == key_codec.key_compare(ptr, tpos->key, ksize))
		{
			tpos->ksize = ksize;
			tpos->expired = time(NULL) + blslot_expired_time;
			/* adjust time list */
			list_move_tail(&tpos->time, &time_list);
			return 0;
		}
	}

	if(current_blslot_count >= max_blslot_count)
		/* overflow */
		return -1;

	tpos = (struct blslot *) calloc(1, offsetof(struct blslot, value) + vsize + 1);
	if(NULL == tpos)
	{
		log_notice("allocate blacklist slot failed");
		return -1;
	}

	tpos->vsize = vsize;
	tpos->ksize = ksize;
	tpos->expired = time(NULL) + blslot_expired_time;
	memcpy(tpos->key, ptr, ksize);
	memcpy(tpos->value, val, vsize);
	tpos->value[vsize] = 0;
	log_debug("add info, key: %s, value: %s, hash: %u.", (char *)tpos->key, tpos->value, key_codec.key_hash(ptr, ksize)%MAX_HASH_DEPTH);

	list_add_tail(&tpos->time, &time_list);
	hlist_add_head(&tpos->hash, h);

	stat_everything(tpos, 1);
	return 0;
}

int CCacheList::in_list(const char *ptr, const unsigned ksize, uint8_t *value, unsigned &vsize )
{
	if(!ptr || ksize >= MAX_KEY_LENGTH)
		return 0;

	CKeyCodec key_codec(key_type);

	struct hlist_head *h 	= hash_list + key_codec.key_hash(ptr, ksize)%MAX_HASH_DEPTH;
	struct hlist_node *pos  = 0;
	struct blslot *tpos 	= 0;

	hlist_for_each_entry(tpos, pos, h, hash)
	{
		/* found */
		if(0 == key_codec.key_compare(ptr, tpos->key, ksize)){
			memcpy(value,tpos->value, tpos->vsize);
			vsize = tpos->vsize;
			return 1;
		}
	}

	return 0;
}

int CCacheList::try_expired_list(void)
{
	unsigned now = time(NULL);
	struct blslot *pos = 0;

	/* time->next is the oldest slot */
	while(!list_empty(&time_list))
	{
		pos = list_entry(time_list.next, struct blslot, time);
		if(pos->expired > now)
			break;

		list_del(&pos->time);
		hlist_del(&pos->hash);
		stat_everything(pos, 0);
		free(pos);
	}
	
	return 0;
}

/* TODO: 统计top10 */
void CCacheList::stat_everything(const struct blslot *slot, const int add)
{
	/* add */
	if(add)
		++current_blslot_count;
	/* delete */
	else 
		--current_blslot_count;
	return;
}

void CCacheList::dump_all_blslot(void)
{
	CKeyCodec key_codec(key_type);
	
	struct blslot *pos = 0;
	list_for_each_entry(pos, &time_list, time)
	{
		switch(key_type)
		{
			case 1:
				log_debug("key: %u size: %u expired: %u hash: %u",
						*(uint8_t *)key_codec.key_pointer(pos->key),
						pos->ksize,
						pos->expired,
						key_codec.key_hash(pos->key,pos->ksize)%MAX_HASH_DEPTH);
				break;
			case 2:
				log_debug("key: %u size: %u expired: %u hash: %u",
						*(uint16_t *)key_codec.key_pointer(pos->key),
						pos->ksize,
						pos->expired,
						key_codec.key_hash(pos->key,pos->ksize)%MAX_HASH_DEPTH);
				break;
			case 4:
				log_debug("key: %u size: %u expired: %u hash: %u",
						*(uint32_t *)key_codec.key_pointer(pos->key),
						pos->ksize,
						pos->expired,
						key_codec.key_hash(pos->key,pos->ksize)%MAX_HASH_DEPTH);
				break;
			default:
				log_debug("key: %10s size: %u expired: %u hash: %u",
						key_codec.key_pointer(pos->key),
						pos->ksize,
						pos->expired,
						key_codec.key_hash(pos->key,pos->ksize)%MAX_HASH_DEPTH);
				break;
		}
	}

	return ;
}
