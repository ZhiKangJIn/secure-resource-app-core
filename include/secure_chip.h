#ifndef SECURE_CHIP_H
#define SECURE_CHIP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
enum secure_record_tag
{
    SECURE_RECORD_EMPTY = 0,
    SECURE_RECORD_ACTIVE = 1,
    SECURE_RECORD_COMPLETE = 2
};

struct secure_session_record
{
    uint8_t tag;
    uint32_t started_at_s;
    uint32_t duration_s;
};

struct secure_quota
{
    uint32_t used;
    uint32_t limit;
    uint32_t remaining;
};

struct secure_chip_ops
{
    int (*init)(void *context);
    int (*authenticate)(void *context);
    int (*read_record)(void *context, struct secure_session_record *record);
    int (*write_record)(void *context, const struct secure_session_record *record);
    int (*read_quota)(void *context, struct secure_quota *quota);
    int (*consume_once)(void *context, struct secure_quota *quota_after);
    void *context;
};

#ifdef __cplusplus
}
#endif

#endif
