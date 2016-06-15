/* AUTOMATICALLY GENERATED, DO NOT MODIFY */

/*
 * deallocation functions for schema-defined QAPI types
 *
 * Copyright IBM, Corp. 2011
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *  Michael Roth      <mdroth@linux.vnet.ibm.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */

#include "qapi/dealloc-visitor.h"
#include "qga-qapi-types.h"
#include "qga-qapi-visit.h"

const char *GuestFsfreezeStatus_lookup[] = {
    "thawed",
    "frozen",
    NULL,
};

const char *GuestIpAddressType_lookup[] = {
    "ipv4",
    "ipv6",
    NULL,
};


void qapi_free_GuestAgentCommandInfoList(GuestAgentCommandInfoList * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestAgentCommandInfoList(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestAgentCommandInfo(GuestAgentCommandInfo * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestAgentCommandInfo(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestAgentInfoList(GuestAgentInfoList * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestAgentInfoList(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestAgentInfo(GuestAgentInfo * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestAgentInfo(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestFileReadList(GuestFileReadList * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestFileReadList(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestFileRead(GuestFileRead * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestFileRead(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestFileWriteList(GuestFileWriteList * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestFileWriteList(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestFileWrite(GuestFileWrite * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestFileWrite(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestFileSeekList(GuestFileSeekList * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestFileSeekList(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestFileSeek(GuestFileSeek * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestFileSeek(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestFsfreezeStatusList(GuestFsfreezeStatusList * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestFsfreezeStatusList(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestIpAddressTypeList(GuestIpAddressTypeList * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestIpAddressTypeList(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestIpAddressList(GuestIpAddressList * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestIpAddressList(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestIpAddress(GuestIpAddress * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestIpAddress(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestNetworkInterfaceList(GuestNetworkInterfaceList * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestNetworkInterfaceList(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestNetworkInterface(GuestNetworkInterface * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestNetworkInterface(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestLogicalProcessorList(GuestLogicalProcessorList * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestLogicalProcessorList(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}


void qapi_free_GuestLogicalProcessor(GuestLogicalProcessor * obj)
{
    QapiDeallocVisitor *md;
    Visitor *v;

    if (!obj) {
        return;
    }

    md = qapi_dealloc_visitor_new();
    v = qapi_dealloc_get_visitor(md);
    visit_type_GuestLogicalProcessor(v, &obj, NULL, NULL);
    qapi_dealloc_visitor_cleanup(md);
}

