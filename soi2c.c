// Copyright 2024 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "soi2c.h"

// Reset the state4 of things by sending a \n to flush anything pending
// on the notecard from before this host was reset.  This ensures that
// our first transaction will be received cleanly.
int soi2cReset(soi2cContext_t *ctx)
{
    uint8_t resetReq[25];
    resetReq[0] = '\n';
    return soi2cTransaction(ctx, SOI2C_IGNORE_RESPONSE, resetReq, sizeof(resetReq));
}

// Get buffer info
uint32_t soi2cBuf(soi2cContext_t *ctx, uint8_t **buf, uint32_t *buflen)
{
    if (buf != NULL) {
        *buf = ctx->buf;
    }
    if (buflen != NULL) {
        *buflen = ctx->buflen;
    }
    return ctx->bufused;
}

// Perform a transaction.  Note that the transmit buffer will be turned to
// garbage, as it will be used as an I/O buffer for both TX and RX operations.
// The request within the input buf must always be terminated with \n, and the
// buflen on input should be the current full allocated size of that buf.
int soi2cTransaction(soi2cContext_t *ctx, uint32_t flags, uint8_t *buf, uint32_t buflen)
{

    // Default i2c address to the notecard
    if (ctx->addr == 0) {
        ctx->addr = 0x17;
    }

    // Exit if not configured
    if (ctx->tx == NULL || ctx->rx == NULL || ctx->delay == NULL || buflen < 5) {
        return SOI2C_CONFIG;
    }

    // Exit if request isn't newline-terminated
    ctx->buf = buf;
    ctx->buflen = buflen;
    ctx->bufused = 0;
    for (int i=0; i<buflen; i++) {
        ctx->bufused++;
        if (buf[i] == '\n') {
            break;
        }
    }
    if (ctx->bufused == 0) {
        return SOI2C_TERMINATOR;
    }

    // Begin by shifting the req in the buf to allow space for the transmit header
    if ((ctx->buflen - ctx->bufused) < 1) {
        return SOI2C_TX_BUFFER_OVERFLOW;
    }
    memmove(&ctx->buf[1], ctx->buf, ctx->bufused);

    // Loop, transmitting at most 250 bytes per chunk every 250 milliseconds
    uint32_t left = ctx->bufused;
    while (left) {

        uint8_t chunklen = 250;
        if (left < chunklen) {
            chunklen = (uint8_t) left;
        }

        ctx->buf[0] = chunklen;
        if (!ctx->tx(ctx->port, ctx->addr, ctx->buf, 1+chunklen)) {
            return SOI2C_IO_TRANSMIT;
        }
        ctx->delay(250);

        left -= chunklen;
        memmove(&ctx->buf[1], &ctx->buf[1+chunklen], left);

    }

    // Exit if a "cmd" was sent and no response is expected.
    if ((flags & SOI2C_NO_RESPONSE) != 0) {
        return SOI2C_OK;
    }

    // Go into a receive loop, using the txbuf as a (potentially-growing) rxbuf.
    uint32_t msLeftToWait = 5000;
    uint8_t chunklen = 0;
    while (true) {
        uint8_t hdrlen = 2;

        // First, attempt to grow the buffer to ensure we have enough
        if (ctx->growFn != NULL) {
            if (ctx->bufused + hdrlen + chunklen > ctx->buflen) {
                ctx->growFn(&ctx->buf, &ctx->buflen, ctx->bufused + hdrlen + chunklen);
            }
        }

        // Constrain by our buffer size
        if (ctx->bufused + hdrlen + chunklen > ctx->buflen) {
            chunklen = (ctx->buflen - ctx->bufused) - hdrlen;
        }

        // Issue special write transaction that is a 'read will come next' transaction
        ctx->buf[ctx->bufused+0] = 0;
        ctx->buf[ctx->bufused+1] = chunklen;
        if (!ctx->tx(ctx->port, ctx->addr, &ctx->buf[ctx->bufused], hdrlen)) {
            return SOI2C_IO_TRANSMIT;
        }
        ctx->delay(1);

        // Receive the chunk of data
        if (!ctx->rx(ctx->port, ctx->addr, &ctx->buf[ctx->bufused], chunklen + hdrlen)) {
            return SOI2C_IO_TRANSMIT;
        }
        ctx->delay(5);

        // Verify size
        uint8_t availableBytes = ctx->buf[ctx->bufused+0];
        uint8_t returnedBytes = ctx->buf[ctx->bufused+1];
        if (returnedBytes != chunklen) {
            return SOI2C_IO_BAD_SIZE_RETURNED;
        }

        // Look at what has just been received for a terminator, and stop if found
        bool receivedNewline = (memchr(&ctx->buf[ctx->bufused+2], '\n', chunklen) != NULL);

        // Only move bytes into the response buffer if a nonzero length specified,
        // else just flush it.
        if ((flags & SOI2C_IGNORE_RESPONSE) == 0 && chunklen > 0) {
            memmove(&ctx->buf[ctx->bufused], &ctx->buf[ctx->bufused+2], chunklen);
            ctx->bufused += chunklen;
        }

        // Attempt to receive all available bytes in the next chunk
        chunklen = availableBytes;

        // If more to receive, do it
        if (chunklen > 0) {
            continue;
        }

        // If there's nothing available AND we've received a newline, we're done
        if (receivedNewline) {
            break;
        }

        // If no time left to process the transaction, give up
        uint32_t pollMs = 50;
        if (msLeftToWait < pollMs) {
            return SOI2C_IO_TIMEOUT;
        }

        // Delay, and subtract from what's left
        ctx->delay(pollMs);
        msLeftToWait -= pollMs;

    }

    // Done
    return SOI2C_OK;

}
