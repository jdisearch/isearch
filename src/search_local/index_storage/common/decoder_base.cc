/*
 * =====================================================================================
 *
 *       Filename:  decoder_base.cc
 *
 *    Description:  decode unit.
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
#include "decoder_base.h"
#include "poll_thread.h"

DecoderUnit::DecoderUnit(PollThread *owner, int idletimeout)
{
	this->owner = owner;
	this->idleTime = idletimeout;
	this->idleList = owner->get_timer_list(idletimeout);
}

DecoderUnit::~DecoderUnit()
{
}
