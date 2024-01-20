// Copyright 2024 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#pragma once

// JSONB signature that begins every jsonb object
#define JSONB_HEADER                "{:"
#define JSONB_TRAILER               ":}"
#define JSONB_TERMINATOR            '\n'
#define jsonbPresent(x,y)             ((y) > sizeof(JSONB_HEADER-1) && memcmp((x), JSONB_HEADER, sizeof(JSONB_HEADER)-1) == 0)

// JSONB opcodes used for formatting and parsing
#define JSONB_INVALID               0x00

#define JSONB_BEGIN_OBJECT          0x10
#define JSONB_END_OBJECT            0x11
#define JSONB_BEGIN_ARRAY           0x12
#define JSONB_END_ARRAY             0x13

#define JSONB_NULL                  0x20
#define JSONB_TRUE                  0x21
#define JSONB_FALSE                 0x22

// A UTF-8 JSON item name, null-terminated
#define JSONB_ITEM                  0x30

// A UTF-8 JSON string, null-terminated
#define JSONB_STRING                0x40

// A binary buffer, prefixed by its length
#define JSONB_BIN8                  0x51
#define JSONB_BIN16                 0x52
#define JSONB_BIN24                 0x53
#define JSONB_BIN32                 0x54

// Signed integers, occupying JSON_OPCODE_LEN bytes
#define JSONB_INT8                  0x61
#define JSONB_INT16                 0x62
#define JSONB_INT32                 0x64
#define JSONB_INT64                 0x68

// Unsigned integers, occupying JSON_OPCODE_LEN bytes
#define JSONB_UINT8                 0x71
#define JSONB_UINT16                0x72
#define JSONB_UINT32                0x74
#define JSONB_UINT64                0x78

// IEEE Reals, occupying JSON_OPCODE_LEN bytes
#define JSONB_FLOAT                 0x84
#define JSONB_DOUBLE                0x88

typedef bool (*bufGrowFn) (uint8_t **buf, uint32_t *buflen, uint32_t growBytes);

typedef struct {
    bool overrun;
    bool error;
    uint8_t opcode;
    // If growFn is supplied, the caller can retrieve the pointer and size of
    // the grown buffer directly from these fields.
    bufGrowFn growFn;
    uint8_t *buf;
    uint32_t buflen;
    uint32_t bufused;
} jsonbContext;

void jsonbFormatBegin(jsonbContext *ctx, uint8_t *buf, uint32_t buflen, bufGrowFn bufGrow);
uint32_t jsonbFormatEnd(jsonbContext *ctx);
uint32_t jsonbBuf(jsonbContext *ctx, uint8_t **buf, uint32_t *buflen);

void jsonbObjectBegin(jsonbContext *ctx, uint8_t *buf, uint32_t buflen, bufGrowFn bufGrow);
uint32_t jsonbObjectEnd(jsonbContext *ctx);

void jsonbAddObjectBegin(jsonbContext *ctx);
void jsonbAddObjectEnd(jsonbContext *ctx);

void jsonbAddArrayBegin(jsonbContext *ctx);
void jsonbAddArrayEnd(jsonbContext *ctx);

void jsonbAddString(jsonbContext *ctx, const char *str);
void jsonbAddStringLen(jsonbContext *ctx, const char *str, uint32_t strLen);
void jsonbAddBin(jsonbContext *ctx, uint8_t *bin, uint32_t binLen);
void jsonbAddInt8(jsonbContext *ctx, int8_t v);
void jsonbAddInt16(jsonbContext *ctx, int16_t v);
void jsonbAddInt32(jsonbContext *ctx, int32_t v);
void jsonbAddInt64(jsonbContext *ctx, int64_t v);
void jsonbAddUint8(jsonbContext *ctx, uint8_t v);
void jsonbAddUint16(jsonbContext *ctx, uint16_t v);
void jsonbAddUint32(jsonbContext *ctx, uint32_t v);
void jsonbAddUint64(jsonbContext *ctx, uint64_t v);
void jsonbAddNull(jsonbContext *ctx);
void jsonbAddBool(jsonbContext *ctx, bool tf);
void jsonbAddTrue(jsonbContext *ctx);
void jsonbAddFalse(jsonbContext *ctx);
void jsonbAddFloat(jsonbContext *ctx, float v);
void jsonbAddDouble(jsonbContext *ctx, double v);

void jsonbAddItemToObject(jsonbContext *ctx, const char *itemName);
void jsonbAddStringToObject(jsonbContext *ctx, const char *itemName, const char *str);
void jsonbAddStringWithLenToObject(jsonbContext *ctx, const char *itemName, const char *str, uint32_t strLen);
void jsonbAddBinToObject(jsonbContext *ctx, const char *itemName, uint8_t *bin, uint32_t binLen);
void jsonbAddInt8ToObject(jsonbContext *ctx, const char *itemName, int8_t v);
void jsonbAddInt16ToObject(jsonbContext *ctx, const char *itemName, int16_t v);
void jsonbAddInt32ToObject(jsonbContext *ctx, const char *itemName, int32_t v);
void jsonbAddInt64ToObject(jsonbContext *ctx, const char *itemName, int64_t v);
void jsonbAddUint8ToObject(jsonbContext *ctx, const char *itemName, uint8_t v);
void jsonbAddUint16ToObject(jsonbContext *ctx, const char *itemName, uint16_t v);
void jsonbAddUint32ToObject(jsonbContext *ctx, const char *itemName, uint32_t v);
void jsonbAddUint64ToObject(jsonbContext *ctx, const char *itemName, uint64_t v);
void jsonbAddFloatToObject(jsonbContext *ctx, const char *itemName, float v);
void jsonbAddDoubleToObject(jsonbContext *ctx, const char *itemName, double v);
void jsonbAddNullToObject(jsonbContext *ctx, const char *itemName);
void jsonbAddTrueToObject(jsonbContext *ctx, const char *itemName);
void jsonbAddFalseToObject(jsonbContext *ctx, const char *itemName);
void jsonbAddBoolToObject(jsonbContext *ctx, const char *itemName, bool tf);

bool jsonbParse(jsonbContext *ctx, uint8_t *buf, uint32_t buflen);
void jsonbEnum(jsonbContext *ctx);
bool jsonbEnumNext(jsonbContext *ctx, bool *firstInObjectOrArray, uint8_t *opcode, const char **item, void *v);
bool jsonbGetObjectItem(jsonbContext *ctx, const char *itemName, uint8_t *itemType, void *itemValue);
char *jsonbGetString(jsonbContext *ctx, const char *itemName);
double jsonbGetDouble(jsonbContext *ctx, const char *itemName);
float jsonbGetFloat(jsonbContext *ctx, const char *itemName);
bool jsonbGetBool(jsonbContext *ctx, const char *itemName);
int32_t jsonbGetInt32(jsonbContext *ctx, const char *itemName);
int64_t jsonbGetInt64(jsonbContext *ctx, const char *itemName);
uint32_t jsonbGetUint32(jsonbContext *ctx, const char *itemName);
uint64_t jsonbGetUint64(jsonbContext *ctx, const char *itemName);
char *jsonbGetErr(jsonbContext *ctx);
