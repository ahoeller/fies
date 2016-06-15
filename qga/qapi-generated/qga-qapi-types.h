/* AUTOMATICALLY GENERATED, DO NOT MODIFY */

/*
 * schema-defined QAPI types
 *
 * Copyright IBM, Corp. 2011
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */

#ifndef QGA_QAPI_TYPES_H
#define QGA_QAPI_TYPES_H

#include <stdbool.h>
#include <stdint.h>


#ifndef QAPI_TYPES_BUILTIN_STRUCT_DECL_H
#define QAPI_TYPES_BUILTIN_STRUCT_DECL_H


typedef struct strList
{
    union {
        char * value;
        uint64_t padding;
    };
    struct strList *next;
} strList;

typedef struct intList
{
    union {
        int64_t value;
        uint64_t padding;
    };
    struct intList *next;
} intList;

typedef struct numberList
{
    union {
        double value;
        uint64_t padding;
    };
    struct numberList *next;
} numberList;

typedef struct boolList
{
    union {
        bool value;
        uint64_t padding;
    };
    struct boolList *next;
} boolList;

typedef struct int8List
{
    union {
        int8_t value;
        uint64_t padding;
    };
    struct int8List *next;
} int8List;

typedef struct int16List
{
    union {
        int16_t value;
        uint64_t padding;
    };
    struct int16List *next;
} int16List;

typedef struct int32List
{
    union {
        int32_t value;
        uint64_t padding;
    };
    struct int32List *next;
} int32List;

typedef struct int64List
{
    union {
        int64_t value;
        uint64_t padding;
    };
    struct int64List *next;
} int64List;

typedef struct uint8List
{
    union {
        uint8_t value;
        uint64_t padding;
    };
    struct uint8List *next;
} uint8List;

typedef struct uint16List
{
    union {
        uint16_t value;
        uint64_t padding;
    };
    struct uint16List *next;
} uint16List;

typedef struct uint32List
{
    union {
        uint32_t value;
        uint64_t padding;
    };
    struct uint32List *next;
} uint32List;

typedef struct uint64List
{
    union {
        uint64_t value;
        uint64_t padding;
    };
    struct uint64List *next;
} uint64List;

#endif /* QAPI_TYPES_BUILTIN_STRUCT_DECL_H */



typedef struct GuestAgentCommandInfo GuestAgentCommandInfo;

typedef struct GuestAgentCommandInfoList
{
    union {
        GuestAgentCommandInfo *value;
        uint64_t padding;
    };
    struct GuestAgentCommandInfoList *next;
} GuestAgentCommandInfoList;


typedef struct GuestAgentInfo GuestAgentInfo;

typedef struct GuestAgentInfoList
{
    union {
        GuestAgentInfo *value;
        uint64_t padding;
    };
    struct GuestAgentInfoList *next;
} GuestAgentInfoList;


typedef struct GuestFileRead GuestFileRead;

typedef struct GuestFileReadList
{
    union {
        GuestFileRead *value;
        uint64_t padding;
    };
    struct GuestFileReadList *next;
} GuestFileReadList;


typedef struct GuestFileWrite GuestFileWrite;

typedef struct GuestFileWriteList
{
    union {
        GuestFileWrite *value;
        uint64_t padding;
    };
    struct GuestFileWriteList *next;
} GuestFileWriteList;


typedef struct GuestFileSeek GuestFileSeek;

typedef struct GuestFileSeekList
{
    union {
        GuestFileSeek *value;
        uint64_t padding;
    };
    struct GuestFileSeekList *next;
} GuestFileSeekList;

extern const char *GuestFsfreezeStatus_lookup[];
typedef enum GuestFsfreezeStatus
{
    GUEST_FSFREEZE_STATUS_THAWED = 0,
    GUEST_FSFREEZE_STATUS_FROZEN = 1,
    GUEST_FSFREEZE_STATUS_MAX = 2,
} GuestFsfreezeStatus;

typedef struct GuestFsfreezeStatusList
{
    union {
        GuestFsfreezeStatus value;
        uint64_t padding;
    };
    struct GuestFsfreezeStatusList *next;
} GuestFsfreezeStatusList;

extern const char *GuestIpAddressType_lookup[];
typedef enum GuestIpAddressType
{
    GUEST_IP_ADDRESS_TYPE_IPV4 = 0,
    GUEST_IP_ADDRESS_TYPE_IPV6 = 1,
    GUEST_IP_ADDRESS_TYPE_MAX = 2,
} GuestIpAddressType;

typedef struct GuestIpAddressTypeList
{
    union {
        GuestIpAddressType value;
        uint64_t padding;
    };
    struct GuestIpAddressTypeList *next;
} GuestIpAddressTypeList;


typedef struct GuestIpAddress GuestIpAddress;

typedef struct GuestIpAddressList
{
    union {
        GuestIpAddress *value;
        uint64_t padding;
    };
    struct GuestIpAddressList *next;
} GuestIpAddressList;


typedef struct GuestNetworkInterface GuestNetworkInterface;

typedef struct GuestNetworkInterfaceList
{
    union {
        GuestNetworkInterface *value;
        uint64_t padding;
    };
    struct GuestNetworkInterfaceList *next;
} GuestNetworkInterfaceList;


typedef struct GuestLogicalProcessor GuestLogicalProcessor;

typedef struct GuestLogicalProcessorList
{
    union {
        GuestLogicalProcessor *value;
        uint64_t padding;
    };
    struct GuestLogicalProcessorList *next;
} GuestLogicalProcessorList;

#ifndef QAPI_TYPES_BUILTIN_CLEANUP_DECL_H
#define QAPI_TYPES_BUILTIN_CLEANUP_DECL_H

void qapi_free_strList(strList * obj);
void qapi_free_intList(intList * obj);
void qapi_free_numberList(numberList * obj);
void qapi_free_boolList(boolList * obj);
void qapi_free_int8List(int8List * obj);
void qapi_free_int16List(int16List * obj);
void qapi_free_int32List(int32List * obj);
void qapi_free_int64List(int64List * obj);
void qapi_free_uint8List(uint8List * obj);
void qapi_free_uint16List(uint16List * obj);
void qapi_free_uint32List(uint32List * obj);
void qapi_free_uint64List(uint64List * obj);

#endif /* QAPI_TYPES_BUILTIN_CLEANUP_DECL_H */


struct GuestAgentCommandInfo
{
    char * name;
    bool enabled;
    bool success_response;
};

void qapi_free_GuestAgentCommandInfoList(GuestAgentCommandInfoList * obj);
void qapi_free_GuestAgentCommandInfo(GuestAgentCommandInfo * obj);

struct GuestAgentInfo
{
    char * version;
    GuestAgentCommandInfoList * supported_commands;
};

void qapi_free_GuestAgentInfoList(GuestAgentInfoList * obj);
void qapi_free_GuestAgentInfo(GuestAgentInfo * obj);

struct GuestFileRead
{
    int64_t count;
    char * buf_b64;
    bool eof;
};

void qapi_free_GuestFileReadList(GuestFileReadList * obj);
void qapi_free_GuestFileRead(GuestFileRead * obj);

struct GuestFileWrite
{
    int64_t count;
    bool eof;
};

void qapi_free_GuestFileWriteList(GuestFileWriteList * obj);
void qapi_free_GuestFileWrite(GuestFileWrite * obj);

struct GuestFileSeek
{
    int64_t position;
    bool eof;
};

void qapi_free_GuestFileSeekList(GuestFileSeekList * obj);
void qapi_free_GuestFileSeek(GuestFileSeek * obj);

void qapi_free_GuestFsfreezeStatusList(GuestFsfreezeStatusList * obj);

void qapi_free_GuestIpAddressTypeList(GuestIpAddressTypeList * obj);

struct GuestIpAddress
{
    char * ip_address;
    GuestIpAddressType ip_address_type;
    int64_t prefix;
};

void qapi_free_GuestIpAddressList(GuestIpAddressList * obj);
void qapi_free_GuestIpAddress(GuestIpAddress * obj);

struct GuestNetworkInterface
{
    char * name;
    bool has_hardware_address;
    char * hardware_address;
    bool has_ip_addresses;
    GuestIpAddressList * ip_addresses;
};

void qapi_free_GuestNetworkInterfaceList(GuestNetworkInterfaceList * obj);
void qapi_free_GuestNetworkInterface(GuestNetworkInterface * obj);

struct GuestLogicalProcessor
{
    int64_t logical_id;
    bool online;
    bool has_can_offline;
    bool can_offline;
};

void qapi_free_GuestLogicalProcessorList(GuestLogicalProcessorList * obj);
void qapi_free_GuestLogicalProcessor(GuestLogicalProcessor * obj);

#endif
