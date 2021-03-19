#ifndef __TTC_VERSION_H
#define __TTC_VERSION_H

#define TTC_MAJOR_VERSION       1
#define TTC_MINOR_VERSION       0
#define TTC_BETA_VERSION        0

/*major.minor.beta*/
#define TTC_VERSION "1.0.0"
/* the following show line should be line 11 as line number is used in Makefile */
#define TTC_GIT_VERSION ""
#define TTC_VERSION_DETAIL TTC_VERSION"."TTC_GIT_VERSION

extern const char compdatestr[];
extern const char comptimestr[];
extern const char version[];
extern const char version_detail[];
extern long       compile_time;

#endif
