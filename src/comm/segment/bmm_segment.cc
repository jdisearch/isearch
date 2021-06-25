#include "bmm_segment.h"

BmmSegment::BmmSegment()
{
}

BmmSegment::~BmmSegment()
{
}

void BmmSegment::ConcreteSplit(const string& str, uint32_t appid, vector<string>& bmm_list){
	iutf8string phrase(str);
    int maxlen = MAX_WORD_LEN;
    int len_phrase = phrase.length();
    int i = len_phrase, j = 0;

    while (i > 0) {
        int start = i - maxlen;
        if (start < 0)
            start = 0;
        iutf8string phrase_sub = phrase.utf8substr(start, i-start);
        for (j = 0; j < phrase_sub.length(); j++) {
            if (j == phrase_sub.length() - 1)
                break;
            iutf8string key = phrase_sub.utf8substr(j, phrase_sub.length()-j);
            if (wordValid(key.stlstring(), appid) == true) {
                bmm_list.insert(bmm_list.begin(), key.stlstring());
                i -= key.length();
                break;
            }
        }
        if (j == phrase_sub.length() - 1) {
            bmm_list.insert(bmm_list.begin(), "" + phrase_sub[j]);
            i--;
        }
    }
    return;
}