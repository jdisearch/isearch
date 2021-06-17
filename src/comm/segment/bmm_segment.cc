#include "bmm_segment.h"

BmmSegment::BmmSegment()
{
}

BmmSegment::~BmmSegment()
{
}

void BmmSegment::ConcreteSplit(iutf8string& phrase, uint32_t appid, vector<string>& bmm_list){
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
                vector<string>::iterator iter = bmm_list.begin();
                bmm_list.insert(iter, key.stlstring());
                i -= key.length() - 1;
                break;
            }
        }
        if (j == phrase_sub.length() - 1) {
            vector<string>::iterator iter = bmm_list.begin();
            bmm_list.insert(iter, "" + phrase_sub[j]);
        }
        i -= 1;
    }
    return;
}