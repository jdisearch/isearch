#include "fmm_segment.h"

FmmSegment::FmmSegment()
{
}

FmmSegment::~FmmSegment()
{
}

void FmmSegment::ConcreteSplit(iutf8string& phrase, uint32_t appid, vector<string>& fmm_list){
    int maxlen = MAX_WORD_LEN;
    int len_phrase = phrase.length();
    int i = 0, j = 0;

    while (i < len_phrase) {
        int end = i + maxlen;
        if (end >= len_phrase)
            end = len_phrase;
        iutf8string phrase_sub = phrase.utf8substr(i, end - i);
        for (j = phrase_sub.length(); j >= 0; j--) {
            if (j == 1)
                break;
            iutf8string key = phrase_sub.utf8substr(0, j);
            if (wordValid(key.stlstring(), appid) == true) {
                fmm_list.push_back(key.stlstring());
                i += key.length();
                break;
            }
        }
        if (j == 1) {
            fmm_list.push_back(phrase_sub[0]);
            i++;
        }
    }
    return;
}