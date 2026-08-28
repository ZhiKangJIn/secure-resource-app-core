#ifndef SECURE_APP_H
#define SECURE_APP_H

#include <stdbool.h>
#include <stdint.h>
#include "secure_chip.h"

#ifdef __cplusplus
extern "C" {
#endif
enum secure_app_state
{
    SECURE_APP_DETACHED = 0,
    SECURE_APP_READY,
    SECURE_APP_AUTHORIZED,
    SECURE_APP_ACTIVE,
    SECURE_APP_ERROR
};

enum secure_app_event
{
    SECURE_EVENT_RESOURCE_PRESENT = 1,
    SECURE_EVENT_RESOURCE_REMOVED,
    SECURE_EVENT_START,
    SECURE_EVENT_STOP,
    SECURE_EVENT_TICK,
    SECURE_EVENT_CLEAR_ERROR
};

enum secure_app_status
{
    SECURE_APP_OK = 0,
    SECURE_APP_EINVAL = -1,
    SECURE_APP_ESTATE = -2,
    SECURE_APP_EAUTH = -3,
    SECURE_APP_EQUOTA = -4,
    SECURE_APP_ECHIP = -5,
    SECURE_APP_EHW = -6
};

struct secure_platform_ops
{
    uint32_t (*now_s)(void *context);
    bool (*resource_present)(void *context);
    int (*action_start)(void *context);
    int (*action_stop)(void *context);
    void (*state_changed)(void *context, enum secure_app_state state, int reason);
    void *context;
};

struct secure_app_config
{
    uint32_t session_duration_s;
};

struct secure_app_snapshot
{
    enum secure_app_state state;
    uint32_t remaining;
    uint32_t session_remaining_s;
    int last_error;
};

struct secure_app
{
    const struct secure_chip_ops *chip;
    const struct secure_platform_ops *platform;
    struct secure_app_config config;
    enum secure_app_state state;
    uint32_t session_started_at_s;
    uint32_t remaining;
    int last_error;
};

int secure_app_init(struct secure_app *app,
                    const struct secure_app_config *config,
                    const struct secure_chip_ops *chip,
                    const struct secure_platform_ops *platform);
int secure_app_dispatch(struct secure_app *app, enum secure_app_event event);
int secure_app_get_snapshot(const struct secure_app *app,
                            struct secure_app_snapshot *snapshot);
const char *secure_app_state_name(enum secure_app_state state);

#ifdef __cplusplus
}
#endif

#endif
