/*
 * =====================================================================================
 *
 *       Filename:  namespace.h
 *
 *    Description:  
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
#ifndef __DTC_VERSIONED_NAMESPACE_H
#define __DTC_VERSIONED_NAMESPACE_H

#include "version.h"

#if DTC_HAS_VERSIONED_NAMESPACE && DTC_HAS_VERSIONED_NAMESPACE == 1

#ifndef DTC_VERSIONED_NAMESPACE

#define MAKE_DTC_VERSIONED_NAMESPACE_IMPL(MAJOR, MINOR, BETA) DTC_##MAJOR##_##MINOR##_##BETA
#define MAKE_DTC_VERSIONED_NAMESPACE(MAJOR, MINOR, BETA) MAKE_DTC_VERSIONED_NAMESPACE_IMPL(MAJOR, MINOR, BETA)
#define DTC_VERSIONED_NAMESPACE MAKE_DTC_VERSIONED_NAMESPACE(DTC_MAJOR_VERSION, DTC_MINOR_VERSION, DTC_BETA_VERSION)

#endif //end DTC_VERSIONED_NAMESPACE

#define DTC_BEGIN_NAMESPACE           \
    namespace DTC_VERSIONED_NAMESPACE \
    {
#define DTC_END_NAMESPACE }
#define DTC_USING_NAMESPACE using namespace DTC_VERSIONED_NAMESPACE;

#else

#define DTC_BEGIN_NAMESPACE
#define DTC_END_NAMESPACE
#define DTC_USING_NAMESPACE

#endif //end DTC_HAS_VERSIONED_NAMESPACE

#endif
