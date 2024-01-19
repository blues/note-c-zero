// Copyright 2024 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "jsonb.h"

// Forwards
#define jbAppend8(ctx, opcode, v) jbAppendBytes(ctx, opcode, (uint8_t *) &(v), 1)
#define jbAppend16(ctx, opcode, v) jbAppendBytes(ctx, opcode, (uint8_t *) &(v), 2)
#define jbAppend24(ctx, opcode, v) jbAppendBytes(ctx, opcode, (uint8_t *) &(v), 3)
#define jbAppend32(ctx, opcode, v) jbAppendBytes(ctx, opcode, (uint8_t *) &(v), 4)
#define jbAppend64(ctx, opcode, v) jbAppendBytes(ctx, opcode, (uint8_t *) &(v), 8)
void jbAppendBytes(jsonbContext *ctx, uint8_t opcode, uint8_t *buf, uint32_t buflen);
uint32_t jbCobsEncode(uint8_t *ptr, uint32_t length, uint8_t xor, uint8_t *dst);
uint32_t jbCobsEncodedLength(uint8_t *ptr, uint32_t length);
uint32_t jbCobsDecode(uint8_t *ptr, uint32_t length, uint8_t xor, uint8_t *dst);
uint32_t jbCobsGuaranteedFit(uint32_t buflen);

///
/// JSONB FORMATTING METHODS
///

// Begin building a JSONB buffer.  If dynamic expansion may be done, supply a method to
// grow the buffer (potentially changing its address), else just supply NULL for bufGrow
// Note that over the course of the formatting the "ctx.bufused" will change.  If a
// grow function has been specified, the "ctx.buf" and "ctx.buflen" will also change.
void jsonbFormatBegin(jsonbContext *ctx, uint8_t *buf, uint32_t buflen, bufGrowFn bufGrow)
{
    ctx->growFn = bufGrow;
    ctx->buf = buf;
    ctx->buflen = buflen;
    ctx->bufused = 0;
    ctx->overrun = false;
    ctx->error = false;
}

// End the cobs encoding, returning how many bytes are in the buffer
void jsonbFormatEnd(jsonbContext *ctx)
{

    // Exit if overrun
    if (ctx->overrun || ctx->error) {
        return;
    }

    // The length must account for the JSONB_HEADER, JSONB_TRAILER, \n
    uint32_t siglen = (sizeof(JSONB_HEADER)-1) + (sizeof(JSONB_TRAILER)-1) + 1;
    uint32_t buflenWithoutSig = ctx->buflen - siglen;

    // Compute the amount that we'll need for an encoded buffer
    uint32_t maxExpansionByEncoding = buflenWithoutSig - jbCobsGuaranteedFit(buflenWithoutSig);
    if (ctx->bufused + maxExpansionByEncoding > buflenWithoutSig) {
        return;
    }

    // Shift the entire buffer higher in memory so that it can be encoded downward
    uint8_t *movedPayload = &ctx->buf[maxExpansionByEncoding + siglen];
    memmove(movedPayload, ctx->buf, ctx->bufused);

    // COBS-encode the subset of the buffer in a way that removes all terminator bytes from the binary
    memcpy(ctx->buf, JSONB_HEADER, sizeof(JSONB_HEADER)-1);
    int32_t cobslen = (int32_t) jbCobsEncode(movedPayload, ctx->bufused, (uint8_t) JSONB_TERMINATOR, &ctx->buf[sizeof(JSONB_HEADER)-1]);

    // Newline--terminate the COBS-encoded buffer, and we're done
    memcpy(&ctx->buf[(sizeof(JSONB_HEADER)-1)+cobslen], JSONB_TRAILER, sizeof(JSONB_TRAILER)-1);
    ctx->bufused = (sizeof(JSONB_HEADER)-1) + cobslen + (sizeof(JSONB_TRAILER)-1);
    ctx->buf[ctx->bufused++] = JSONB_TERMINATOR;

}

// Get buffer info
uint32_t jsonbBuf(jsonbContext *ctx, uint8_t **buf, uint32_t *buflen)
{
    if (buf != NULL) {
        *buf = ctx->buf;
    }
    if (buflen != NULL) {
        *buflen = ctx->buflen;
    }
    return ctx->bufused;
}

// Append an object
void jsonbAddObjectBegin(jsonbContext *ctx)
{
    jbAppendBytes(ctx, JSONB_BEGIN_OBJECT, NULL, 0);
}
void jsonbAddObjectEnd(jsonbContext *ctx)
{
    jbAppendBytes(ctx, JSONB_END_OBJECT, NULL, 0);
}

// Append an array
void jsonbAddArrayBegin(jsonbContext *ctx)
{
    jbAppendBytes(ctx, JSONB_BEGIN_ARRAY, NULL, 0);
}
void jsonbAddArrayEnd(jsonbContext *ctx)
{
    jbAppendBytes(ctx, JSONB_END_ARRAY, NULL, 0);
}

// Append a null-terminated string to an array
void jsonbAddString(jsonbContext *ctx, const char *str)
{
    jbAppendBytes(ctx, JSONB_STRING, (uint8_t *) str, strlen(str)+1);
}

// Append a counted string to an array
void jsonbAddStringLen(jsonbContext *ctx, const char *str, uint32_t strLen)
{
    jbAppendBytes(ctx, JSONB_STRING, (uint8_t *) str, strLen);
    uint8_t zerobyte = 0;
    jbAppendBytes(ctx, JSONB_INVALID, &zerobyte, 1);
}

// Append a binary payload (which, like golang, should ultimately be rendered as B64 in a JSON string)
void jsonbAddBin(jsonbContext *ctx, uint8_t *bin, uint32_t binLen)
{
    if (binLen < 0x00000100) {
        jbAppend8(ctx, JSONB_BIN8, binLen);
    } else if (binLen < 0x00010000) {
        jbAppend16(ctx, JSONB_BIN16, binLen);
    } else if (binLen < 0x01000000) {
        jbAppend24(ctx, JSONB_BIN24, binLen);
    } else {
        jbAppend32(ctx, JSONB_BIN32, binLen);
    }
    jbAppendBytes(ctx, JSONB_INVALID, bin, binLen);
}

// Append integers to an array
void jsonbAddInt8(jsonbContext *ctx, uint8_t v)
{
    jbAppend8(ctx, JSONB_INT8, v);
}
void jsonbAddInt16(jsonbContext *ctx, uint16_t v)
{
    jbAppend16(ctx, JSONB_INT16, v);
}
void jsonbAddInt32(jsonbContext *ctx, uint32_t v)
{
    jbAppend32(ctx, JSONB_INT32, v);
}
void jsonbAddInt64(jsonbContext *ctx, uint32_t v)
{
    jbAppend64(ctx, JSONB_INT64, v);
}

// Append unsigned integers to an array
void jsonbAddUint8(jsonbContext *ctx, uint8_t v)
{
    jbAppend8(ctx, JSONB_UINT8, v);
}
void jsonbAddUint16(jsonbContext *ctx, uint16_t v)
{
    jbAppend16(ctx, JSONB_UINT16, v);
}
void jsonbAddUint32(jsonbContext *ctx, uint32_t v)
{
    jbAppend32(ctx, JSONB_UINT32, v);
}
void jsonbAddUint64(jsonbContext *ctx, uint32_t v)
{
    jbAppend64(ctx, JSONB_UINT64, v);
}

// Append a null or bool to an array
void jsonbAddNull(jsonbContext *ctx)
{
    jbAppendBytes(ctx, JSONB_NULL, NULL, 0);
}
void jsonbAddBool(jsonbContext *ctx, bool tf)
{
    jbAppendBytes(ctx, tf ? JSONB_TRUE : JSONB_FALSE, NULL, 0);
}
void jsonbAddTrue(jsonbContext *ctx)
{
    jbAppendBytes(ctx, JSONB_TRUE, NULL, 0);
}
void jsonbAddFalse(jsonbContext *ctx)
{
    jbAppendBytes(ctx, JSONB_FALSE, NULL, 0);
}

// Append a real to an array
void jsonbAddFloat(jsonbContext *ctx, float v)
{
    if (sizeof(float) == 4) {
        jbAppendBytes(ctx, JSONB_FLOAT, (uint8_t *) &v, 4);
    } else if (sizeof(float) == 8) {
        jbAppendBytes(ctx, JSONB_DOUBLE, (uint8_t *) &v, 8);
    }
}
void jsonbAddDouble(jsonbContext *ctx, double v)
{
    if (sizeof(double) == 4) {
        jbAppendBytes(ctx, JSONB_FLOAT, (uint8_t *) &v, 4);
    } else if (sizeof(double) == 8) {
        jbAppendBytes(ctx, JSONB_DOUBLE, (uint8_t *) &v, 8);
    }
}

// Append the start of an item
void jsonbAddItemToObject(jsonbContext *ctx, const char *itemName)
{
    jbAppendBytes(ctx, JSONB_ITEM, (uint8_t *) itemName, strlen(itemName)+1);
}

// Append a string item to an object
void jsonbAddStringToObject(jsonbContext *ctx, const char *itemName, const char *str)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddString(ctx, str);
}

// Append a counted string item to an object
void jsonbAddStringWithLenToObject(jsonbContext *ctx, const char *itemName, const char *str, uint32_t strLen)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddStringLen(ctx, str, strLen);
}

// Append a binary payload item to an object
void jsonbAddBinToObject(jsonbContext *ctx, const char *itemName, uint8_t *bin, uint32_t binLen)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddBin(ctx, bin, binLen);
}

// Append integer items to an object
void jsonbAddInt8ToObject(jsonbContext *ctx, const char *itemName, int8_t v)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddInt8(ctx, v);
}
void jsonbAddInt16ToObject(jsonbContext *ctx, const char *itemName, int16_t v)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddInt16(ctx, v);
}
void jsonbAddInt32ToObject(jsonbContext *ctx, const char *itemName, int32_t v)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddInt32(ctx, v);
}
void jsonbAddInt64ToObject(jsonbContext *ctx, const char *itemName, int64_t v)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddInt64(ctx, v);
}

// Append unsigned integer items to an object
void jsonbAddUint8ToObject(jsonbContext *ctx, const char *itemName, uint8_t v)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddUint8(ctx, v);

}
void jsonbAddUint16ToObject(jsonbContext *ctx, const char *itemName, uint16_t v)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddUint16(ctx, v);
}
void jsonbAddUint32ToObject(jsonbContext *ctx, const char *itemName, uint32_t v)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddUint32(ctx, v);
}
void jsonbAddUint64ToObject(jsonbContext *ctx, const char *itemName, uint32_t v)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddUint64(ctx, v);
}

// Append a null or bool items to an object
void jsonbAddNullToObject(jsonbContext *ctx, const char *itemName)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddNull(ctx);
}
void jsonbAddBoolToObject(jsonbContext *ctx, const char *itemName, bool tf)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddBool(ctx, tf);
}
void jsonbAddTrueToObject(jsonbContext *ctx, const char *itemName)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddTrue(ctx);
}
void jsonbAddFalseToObject(jsonbContext *ctx, const char *itemName)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddFalse(ctx);
}

// Append a real item to an object
void jsonbAddFloatToObject(jsonbContext *ctx, const char *itemName, float v)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddFloat(ctx, v);
}
void jsonbAddDoubleToObject(jsonbContext *ctx, const char *itemName, double v)
{
    jsonbAddItemToObject(ctx, itemName);
    jsonbAddDouble(ctx, v);
}

///
/// JSONB PARSING METHODS
///

// Begin parsing a binary object
bool jsonbParse(jsonbContext *ctx, uint8_t *buf, uint32_t buflen)
{

    // Trim the control characters off both ends
    while (buflen > 0 && buf[0] < ' ') {
        buf++;
        buflen--;
    }
    while (buflen > 0 && buf[buflen-1] < ' ') {
        buflen--;
    }

    // Exit if it's not JSONB
    if (buflen == 0) {
        return false;
    }
    if (buflen < sizeof(JSONB_HEADER)-1 || memcmp(buf, JSONB_HEADER, sizeof(JSONB_HEADER)-1) != 0) {
        return false;
    }
    buf += sizeof(JSONB_HEADER)-1;
    buflen -= sizeof(JSONB_HEADER)-1;
    if (buflen < sizeof(JSONB_TRAILER)-1 || memcmp(&buf[buflen-(sizeof(JSONB_TRAILER)-1)], JSONB_TRAILER, sizeof(JSONB_TRAILER)-1) != 0) {
        return false;
    }
    buflen -= sizeof(JSONB_TRAILER)-1;

    // Decode the COBS object in-place
    ctx->buflen = jbCobsDecode(buf, buflen, JSONB_TERMINATOR, buf);
    ctx->buf = buf;
    ctx->bufused = 0;
    return true;

}

// Begin enumerating a binary object
void jsonbEnum(jsonbContext *ctx)
{
    ctx->bufused = 0;
    ctx->opcode = JSONB_INVALID;
}

// Continue enumerating
bool jsonbEnumNext(jsonbContext *ctx, bool *firstInObjectOrArray, uint8_t *opcode, const char **item, void *v)
{
    if (ctx->bufused >= ctx->buflen) {
        return false;
    }
    if (firstInObjectOrArray != NULL) {
        *firstInObjectOrArray = (ctx->opcode == JSONB_BEGIN_OBJECT || ctx->opcode == JSONB_BEGIN_ARRAY || ctx->opcode == JSONB_INVALID);
    }
    ctx->opcode = ctx->buf[ctx->bufused++];
    if (opcode != NULL) {
        *opcode = ctx->opcode;
    }
    *item = NULL;
    if (ctx->opcode == JSONB_ITEM) {
        *item = (const char *) &ctx->buf[ctx->bufused];
        uint32_t namelen = 0;
        bool nullTerminated = false;
        for (int i=0; i<(ctx->buflen-ctx->bufused); i++) {
            namelen++;
            if (ctx->buf[ctx->bufused+i] == '\0') {
                nullTerminated = true;
                break;
            }
        }
        if (!nullTerminated) {
            return false;
        }
        ctx->bufused += namelen;
        ctx->opcode = ctx->buf[ctx->bufused++];
        if (opcode != NULL) {
            *opcode = ctx->opcode;
        }
    }
    uint32_t len = 0;
    switch (ctx->opcode) {
    case JSONB_BEGIN_OBJECT:
        break;
    case JSONB_END_OBJECT:
        break;
    case JSONB_BEGIN_ARRAY:
        break;
    case JSONB_END_ARRAY:
        break;
    case JSONB_NULL:
        break;
    case JSONB_TRUE:
        break;
    case JSONB_FALSE:
        break;
    case JSONB_STRING: {
        bool nullTerminated = false;
        for (int i=0; i<(ctx->buflen-ctx->bufused); i++) {
            len++;
            if (ctx->buf[ctx->bufused+i] == '\0') {
                nullTerminated = true;
                break;
            }
        }
        if (!nullTerminated) {
            return false;
        }
        break;
    }
    case JSONB_BIN8:
        len = ctx->buf[ctx->bufused++];
        break;
    case JSONB_BIN16:
        len = ctx->buf[ctx->bufused++];
        len |= (ctx->buf[ctx->bufused++] << 8);
        break;
    case JSONB_BIN24:
        len = ctx->buf[ctx->bufused++];
        len |= (ctx->buf[ctx->bufused++] << 8);
        len |= (ctx->buf[ctx->bufused++] << 16);
        break;
    case JSONB_BIN32:
        len = ctx->buf[ctx->bufused++];
        len |= (ctx->buf[ctx->bufused++] << 8);
        len |= (ctx->buf[ctx->bufused++] << 16);
        len |= (ctx->buf[ctx->bufused++] << 24);
        break;
    case JSONB_INT8:
        len = 1;
        break;
    case JSONB_INT16:
        len = 2;
        break;
    case JSONB_INT32:
        len = 4;
        break;
    case JSONB_INT64:
        len = 8;
        break;
    case JSONB_UINT8:
        len = 1;
        break;
    case JSONB_UINT16:
        len = 2;
        break;
    case JSONB_UINT32:
        len = 4;
        break;
    case JSONB_UINT64:
        len = 8;
        break;
    case JSONB_FLOAT:
        len = 8;
        break;
    case JSONB_DOUBLE:
        len = 16;
        break;
    default:
        return false;
    }
    * (void **) v = &ctx->buf[ctx->bufused];
    ctx->bufused += len;
    return true;
}

// Find an item by name in the current item
bool jsonbGetObjectItem(jsonbContext *ctx, const char *itemName, uint8_t *itemType, void *itemValue)
{
    uint8_t type;
    const char *key;
    void *value;
    int nesting = 0;
    jsonbEnum(ctx);
    while (jsonbEnumNext(ctx, NULL, &type, &key, &value)) {
        switch (type) {
        case JSONB_BEGIN_OBJECT:
            nesting++;
            break;
        case JSONB_END_OBJECT:
            nesting--;
            break;
        }
        if (nesting == 0) {
            break;
        }
        if (nesting != 1) {
            continue;
        }
        if (key != NULL) {
            int l1 = strlen(itemName);
            int l2 = strlen(key);
            if (l1 == l2 && memcmp(itemName, key, l1) == 0) {
                *itemType = type;
                * ((void **)itemValue) = value;
                return true;
            }
        }
    }
    return false;
}

// Get a bool
bool jsonbGetBool(jsonbContext *ctx, const char *itemName)
{
    uint8_t itemType;
    char *itemValue;
    if (!jsonbGetObjectItem(ctx, itemName, &itemType, &itemValue)) {
        return false;
    }
    if (itemType == JSONB_TRUE) {
        return true;
    }
    return false;
}

// Get a string
char *jsonbGetString(jsonbContext *ctx, const char *itemName)
{
    uint8_t itemType;
    char *itemValue;
    if (!jsonbGetObjectItem(ctx, itemName, &itemType, &itemValue)) {
        return (char *) "";
    }
    if (itemType != JSONB_STRING) {
        return (char *) "";
    }
    return itemValue;
}

// Get an error string
char *jsonbGetErr(jsonbContext *ctx)
{
    return jsonbGetString(ctx, "err");
}

// Get a float
float jsonbGetFloat(jsonbContext *ctx, const char *itemName)
{
    return (float) jsonbGetDouble(ctx, itemName);
}

// Get a double
double jsonbGetDouble(jsonbContext *ctx, const char *itemName)
{
    uint8_t itemType;
    char *itemValue;
    if (!jsonbGetObjectItem(ctx, itemName, &itemType, &itemValue)) {
        return (double) 0;
    }
    switch (itemType) {

    case JSONB_FLOAT: {
        float v;
        memcpy(&v, itemValue, sizeof(v));
        return (double) v;
    }

    case JSONB_DOUBLE: {
        double v;
        memcpy(&v, itemValue, sizeof(v));
        return v;
    }

    case JSONB_UINT8: {
        uint8_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (double) v;
    }

    case JSONB_UINT16: {
        uint16_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (double) v;
    }

    case JSONB_UINT32: {
        uint32_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (double) v;
    }

    case JSONB_UINT64: {
        uint64_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (double) v;
    }

    case JSONB_INT8: {
        int8_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (double) v;
    }

    case JSONB_INT16: {
        int16_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (double) v;
    }

    case JSONB_INT32: {
        int32_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (double) v;
    }

    case JSONB_INT64: {
        int64_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (double) v;
    }

    }

    return (double) 0.0;

}

// Get an int32
int32_t jsonbGetInt32(jsonbContext *ctx, const char *itemName)
{
    return (int32_t) jsonbGetInt64(ctx, itemName);
}

// Get an int64
int64_t jsonbGetInt64(jsonbContext *ctx, const char *itemName)
{
    uint8_t itemType;
    char *itemValue;
    if (!jsonbGetObjectItem(ctx, itemName, &itemType, &itemValue)) {
        return (int32_t) 0;
    }
    switch (itemType) {

    case JSONB_FLOAT: {
        float v;
        memcpy(&v, itemValue, sizeof(v));
        return (int64_t) v;
    }

    case JSONB_DOUBLE: {
        double v;
        memcpy(&v, itemValue, sizeof(v));
        return (int64_t) v;
    }

    case JSONB_UINT8: {
        uint8_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (int64_t) v;
    }

    case JSONB_UINT16: {
        uint16_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (int64_t) v;
    }

    case JSONB_UINT32: {
        uint32_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (int64_t) v;
    }

    case JSONB_UINT64: {
        uint64_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (int64_t) v;
    }

    case JSONB_INT8: {
        int8_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (int64_t) v;
    }

    case JSONB_INT16: {
        int16_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (int64_t) v;
    }

    case JSONB_INT32: {
        int32_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (int64_t) v;
    }

    case JSONB_INT64: {
        int64_t v;
        memcpy(&v, itemValue, sizeof(v));
        return v;
    }

    }

    return (int64_t) 0;

}

// Get a uint32
uint32_t jsonbGetUint32(jsonbContext *ctx, const char *itemName)
{
    return (uint32_t) jsonbGetUint64(ctx, itemName);
}

// Get a uint64
uint64_t jsonbGetUint64(jsonbContext *ctx, const char *itemName)
{
    uint8_t itemType;
    char *itemValue;
    if (!jsonbGetObjectItem(ctx, itemName, &itemType, &itemValue)) {
        return (uint64_t) 0;
    }
    switch (itemType) {

    case JSONB_FLOAT: {
        float v;
        memcpy(&v, itemValue, sizeof(v));
        return (uint64_t) v;
    }

    case JSONB_DOUBLE: {
        double v;
        memcpy(&v, itemValue, sizeof(v));
        return (uint64_t) v;
    }

    case JSONB_UINT8: {
        uint8_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (uint64_t) v;
    }

    case JSONB_UINT16: {
        uint16_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (uint64_t) v;
    }

    case JSONB_UINT32: {
        uint32_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (uint64_t) v;
    }

    case JSONB_UINT64: {
        uint64_t v;
        memcpy(&v, itemValue, sizeof(v));
        return v;
    }

    case JSONB_INT8: {
        int8_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (uint64_t) v;
    }

    case JSONB_INT16: {
        int16_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (uint64_t) v;
    }

    case JSONB_INT32: {
        int32_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (uint64_t) v;
    }

    case JSONB_INT64: {
        int64_t v;
        memcpy(&v, itemValue, sizeof(v));
        return (uint64_t) v;
    }

    }

    return (uint64_t) 0;

}

///
/// JSONB INTERNAL UTILITY METHODS
///

void jbAppendBytes(jsonbContext *ctx, uint8_t opcode, uint8_t *buf, uint32_t buflen)
{
    uint32_t needed = buflen;
    if (opcode != JSONB_INVALID) {
        needed++;
    }
    if (ctx->bufused + needed > ctx->buflen) {
        if (ctx->growFn == NULL || !ctx->growFn(&ctx->buf, &ctx->buflen, needed)) {
            ctx->overrun = true;
        }
    }
    if (!ctx->overrun) {
        if (opcode != JSONB_INVALID) {
            ctx->buf[ctx->bufused++] = opcode;
        }
        if (buflen > 0) {
            memcpy(&ctx->buf[ctx->bufused], buf, buflen);
            ctx->bufused += buflen;
        }
    }
}

// jbCobsEncode encodes "length" bytes of data
// at the location pointed to by "ptr", writing
// the output to the location pointed to by "dst".
// Returns the length of the encoded data.
// Default behavior (with xor == 0) is to eliminate
// all '\0' from input data, but if a different value
// is specified, the output is XOR'ed such that THAT
// byte is the one that won't be contained in output.
uint32_t jbCobsEncode(uint8_t *ptr, uint32_t length, uint8_t xor, uint8_t *dst)
{
    uint8_t ch;
    uint8_t *start = dst;
    uint8_t code = 1;
    uint8_t *code_ptr = dst++;          // Where to insert the leading count
    while (length--) {
        ch = *ptr++;
        if (ch != 0) {                  // Input byte not zero
            *dst++ = ch ^ xor;
            code++;
        }
        if (ch == 0 || code == 0xFF) {  // Input is zero or complete block
            *code_ptr = code ^ xor;
            code = 1;
            code_ptr = dst++;
        }
    }
    *code_ptr = code ^ xor;             // Final code
    return (dst - start);
}

// jbCobsEncode computes encoding length
uint32_t jbCobsEncodedLength(uint8_t *ptr, uint32_t length)
{
    uint8_t ch;
    uint32_t dst = 1;
    uint8_t code = 1;
    while (length--) {
        ch = *ptr++;
        if (ch != 0) {                  // Input byte not zero
            dst++;
            code++;
        }
        if (ch == 0 || code == 0xFF) {  // Input is zero or complete block
            code = 1;
            dst++;
        }
    }
    return dst;
}

// jbCobsDecode decodes "length" bytes of data at
// the location pointed to by "ptr", writing the
// output to the location pointed to by "dst".
// Returns the length of the decoded data.
// Because the decoded length is guaranteed to be
// <= length, the decode may be done in-place.
// Default behavior (with xor == 0) is to restore
// all '\0' into output data, but if a different value
// is specified, the input is XOR'ed such that THAT
// byte is the one that is assumed can't be in the input.
uint32_t jbCobsDecode(uint8_t *ptr, uint32_t length, uint8_t xor, uint8_t *dst)
{
    const uint8_t *start = dst, *end = ptr + length;
    uint8_t code = 0xFF, copy = 0;
    for (; ptr < end; copy--) {
        if (copy != 0) {
            *dst++ = (*ptr++) ^ xor;
        } else {
            if (code != 0xFF) {
                *dst++ = 0;
            }
            copy = code = (*ptr++) ^ xor;
            if (code == 0) {
                break;
            }
        }
    }
    return dst - start;
}

// Compute the maximum length of an object that can fit into the specified buffer.
// Note that the way we compute it may leave a bit of slop at the end, including
// one byte for a null terminator.
uint32_t jbCobsGuaranteedFit(uint32_t buflen)
{
    uint32_t cobsOverhead = 1 + (buflen / 254) + 1;
    return (cobsOverhead > buflen ? 0 : (buflen - cobsOverhead));
}
