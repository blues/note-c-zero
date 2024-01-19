// Copyright 2024 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#pragma once

#define SOI2C_OK                    0
#define SOI2C_CONFIG                1
#define SOI2C_TERMINATOR            2
#define SOI2C_TX_BUFFER_OVERFLOW    3
#define SOI2C_RX_BUFFER_OVERFLOW    4
#define SOI2C_IO_TRANSMIT           5
#define SOI2C_IO_RECEIVE            6
#define SOI2C_IO_TIMEOUT            7
#define SOI2C_IO_BAD_SIZE_RETURNED  8
typedef int soiStatus_t;

typedef bool (*i2cTransmitFn) (uint16_t devAddr, uint8_t *buf, uint16_t buflen);
typedef bool (*i2cReceiveFn) (uint16_t devAddr, uint8_t *buf, uint16_t buflen);
typedef void (*i2cDelayFn) (uint32_t ms);
typedef bool (*i2cBufGrowFn) (uint8_t **buf, uint32_t *buflen, uint32_t neededBytes);
typedef struct {
    uint16_t addr;
    i2cTransmitFn tx;
    i2cReceiveFn rx;
    i2cDelayFn delay;
    i2cBufGrowFn grow;
    // If growFn is supplied, the caller can retrieve the pointer and size of
    // the grown buffer directly from these fields.
    i2cBufGrowFn growFn;
    uint8_t *buf;
    uint32_t buflen;
    uint32_t bufused;
} soi2cContext_t;

#define SOI2C_NO_RESPONSE           0x0001
#define SOI2C_IGNORE_RESPONSE       0x0002
int soi2cTransaction(soi2cContext_t *ctx, uint32_t flags, uint8_t *buf, uint32_t buflen);
#define soi2cRequestResponse(ctx, buf, buflen) soi2cTransaction(ctx, 0, buf, buflen)
#define soi2cRequest(ctx, buf, buflen) soi2cTransaction(ctx, SOI2C_IGNORE_RESPONSE, buf, buflen)
#define soi2cCommand(ctx, buf, buflen) soi2cTransaction(ctx, SOI2C_NO_RESPONSE, buf, buflen)
int soi2cReset(soi2cContext_t *ctx);
uint32_t soi2cBuf(soi2cContext_t *ctx, uint8_t **buf, uint32_t *buflen);

