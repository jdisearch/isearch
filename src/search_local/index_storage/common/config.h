/*
 * =====================================================================================
 *
 *       Filename:  config.h
 *
 *    Description:  configuration acquiry and store.
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  qiulu, choulu@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdio.h>
#include <string.h>
#include <string>
#include <map>

class AutoConfig
{
public:
	AutoConfig(){};
	virtual ~AutoConfig(){};
	virtual int get_int_val(const char *key, const char *inst, int def = 0) = 0;
	virtual unsigned long long get_size_val(const char *key, const char *inst, unsigned long long def = 0, char unit = 0) = 0;
	virtual int get_idx_val(const char *, const char *, const char *const *, int = 0) = 0;
	virtual const char *get_str_val(const char *key, const char *inst) = 0;
};

class DTCConfig
{
public:
	DTCConfig(){};
	~DTCConfig(){};

	AutoConfig *get_auto_config_instance(const char *);
	int get_int_val(const char *sec, const char *key, int def = 0);
	unsigned long long get_size_val(const char *sec, const char *key, unsigned long long def = 0, char unit = 0);
	int get_idx_val(const char *, const char *, const char *const *, int = 0);
	const char *get_str_val(const char *sec, const char *key);
	bool has_section(const char *sec);
	bool has_key(const char *sec, const char *key);
	void Dump(FILE *fp, bool dec = false);
	int Dump(const char *fn, bool dec = false);
	int parse_config(const char *f = 0, const char *s = 0, bool bakconfig = false);
	int parse_buffered_config(char *buf, const char *fn = 0, const char *s = 0, bool bakconfig = false);

private:
	struct nocase
	{
		bool operator()(const std::string &a, const std::string &b) const
		{
			return strcasecmp(a.c_str(), b.c_str()) < 0;
		}
	};

	typedef std::map<std::string, std::string, nocase> keymap_t;
	typedef std::map<std::string, keymap_t, nocase> secmap_t;

private:
	std::string filename;
	secmap_t smap;
};

#endif
