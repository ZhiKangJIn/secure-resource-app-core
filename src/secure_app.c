#include "secure_app.h"

#include <stddef.h>
#include <string.h>

static uint32_t elapsed_s(uint32_t now, uint32_t then)
{
    return now - then;
}
static void publish(struct secure_app *app, int reason)
{
    if (app->platform->state_changed != NULL)
    {
        app->platform->state_changed(app->platform->context, app->state, reason);
    }
}

static int fail(struct secure_app *app, int error)
{
    app->last_error = error;
    app->state = SECURE_APP_ERROR;
    publish(app, error);
    return error;
}

static int stop_action(struct secure_app *app)
{
    if ((app->state == SECURE_APP_ACTIVE) &&
        (app->platform->action_stop(app->platform->context) != 0))
    {
        return fail(app, SECURE_APP_EHW);
    }
    return SECURE_APP_OK;
}

static int authenticate(struct secure_app *app)
{
    struct secure_quota quota;

    if (app->chip->authenticate(app->chip->context) != 0)
    {
        return fail(app, SECURE_APP_EAUTH);
    }
    if (app->chip->read_quota(app->chip->context, &quota) != 0)
    {
        return fail(app, SECURE_APP_ECHIP);
    }
    if ((quota.used > quota.limit) ||
        (quota.remaining != quota.limit - quota.used))
    {
        return fail(app, SECURE_APP_ECHIP);
    }

    app->remaining = quota.remaining;
    app->last_error = SECURE_APP_OK;
    app->state = SECURE_APP_AUTHORIZED;
    publish(app, SECURE_APP_OK);
    return SECURE_APP_OK;
}

static int start_action(struct secure_app *app)
{
    struct secure_quota quota;
    struct secure_session_record record;

    if (app->state != SECURE_APP_AUTHORIZED)
    {
        return SECURE_APP_ESTATE;
    }
    if (app->remaining == 0U)
    {
        return fail(app, SECURE_APP_EQUOTA);
    }
    if (app->chip->consume_once(app->chip->context, &quota) != 0)
    {
        return fail(app, SECURE_APP_ECHIP);
    }
    if ((quota.used > quota.limit) ||
        (quota.remaining != quota.limit - quota.used))
    {
        return fail(app, SECURE_APP_ECHIP);
    }

    app->remaining = quota.remaining;
    app->session_started_at_s = app->platform->now_s(app->platform->context);
    record.tag = SECURE_RECORD_ACTIVE;
    record.started_at_s = app->session_started_at_s;
    record.duration_s = app->config.session_duration_s;
    if (app->chip->write_record(app->chip->context, &record) != 0)
    {
        return fail(app, SECURE_APP_ECHIP);
    }
    if (app->platform->action_start(app->platform->context) != 0)
    {
        return fail(app, SECURE_APP_EHW);
    }

    app->state = SECURE_APP_ACTIVE;
    app->last_error = SECURE_APP_OK;
    publish(app, SECURE_APP_OK);
    return SECURE_APP_OK;
}

static int complete_action(struct secure_app *app)
{
    struct secure_session_record record;
    int result = stop_action(app);

    if (result != SECURE_APP_OK)
    {
        return result;
    }
    memset(&record, 0, sizeof(record));
    record.tag = SECURE_RECORD_COMPLETE;
    if (app->chip->write_record(app->chip->context, &record) != 0)
    {
        return fail(app, SECURE_APP_ECHIP);
    }

    app->session_started_at_s = 0U;
    app->state = SECURE_APP_AUTHORIZED;
    app->last_error = SECURE_APP_OK;
    publish(app, SECURE_APP_OK);
    return SECURE_APP_OK;
}

int secure_app_init(struct secure_app *app,
                    const struct secure_app_config *config,
                    const struct secure_chip_ops *chip,
                    const struct secure_platform_ops *platform)
{
    if ((app == NULL) || (config == NULL) || (chip == NULL) ||
        (platform == NULL) || (config->session_duration_s == 0U) ||
        (chip->init == NULL) || (chip->authenticate == NULL) ||
        (chip->write_record == NULL) || (chip->read_quota == NULL) ||
        (chip->consume_once == NULL) || (platform->now_s == NULL) ||
        (platform->resource_present == NULL) ||
        (platform->action_start == NULL) || (platform->action_stop == NULL))
    {
        return SECURE_APP_EINVAL;
    }

    memset(app, 0, sizeof(*app));
    app->chip = chip;
    app->platform = platform;
    app->config = *config;
    app->state = platform->resource_present(platform->context) ?
                 SECURE_APP_READY : SECURE_APP_DETACHED;
    if (chip->init(chip->context) != 0)
    {
        return fail(app, SECURE_APP_ECHIP);
    }
    publish(app, SECURE_APP_OK);
    return SECURE_APP_OK;
}

int secure_app_dispatch(struct secure_app *app, enum secure_app_event event)
{
    if ((app == NULL) || (app->chip == NULL) || (app->platform == NULL))
    {
        return SECURE_APP_EINVAL;
    }

    switch (event)
    {
    case SECURE_EVENT_RESOURCE_PRESENT:
        if (app->state != SECURE_APP_DETACHED)
        {
            return SECURE_APP_ESTATE;
        }
        app->state = SECURE_APP_READY;
        publish(app, SECURE_APP_OK);
        return authenticate(app);

    case SECURE_EVENT_RESOURCE_REMOVED:
        if (stop_action(app) != SECURE_APP_OK)
        {
            return app->last_error;
        }
        app->state = SECURE_APP_DETACHED;
        app->remaining = 0U;
        app->session_started_at_s = 0U;
        app->last_error = SECURE_APP_OK;
        publish(app, SECURE_APP_OK);
        return SECURE_APP_OK;

    case SECURE_EVENT_START:
        if (app->state == SECURE_APP_READY)
        {
            int result = authenticate(app);
            if (result != SECURE_APP_OK)
            {
                return result;
            }
        }
        return start_action(app);

    case SECURE_EVENT_STOP:
        return (app->state == SECURE_APP_ACTIVE) ?
               complete_action(app) : SECURE_APP_ESTATE;

    case SECURE_EVENT_TICK:
        if ((app->state == SECURE_APP_ACTIVE) &&
            (elapsed_s(app->platform->now_s(app->platform->context),
                       app->session_started_at_s) >= app->config.session_duration_s))
        {
            return complete_action(app);
        }
        return SECURE_APP_OK;

    case SECURE_EVENT_CLEAR_ERROR:
        if (app->state != SECURE_APP_ERROR)
        {
            return SECURE_APP_ESTATE;
        }
        app->last_error = SECURE_APP_OK;
        app->state = app->platform->resource_present(app->platform->context) ?
                     SECURE_APP_READY : SECURE_APP_DETACHED;
        publish(app, SECURE_APP_OK);
        return SECURE_APP_OK;

    default:
        return SECURE_APP_EINVAL;
    }
}

int secure_app_get_snapshot(const struct secure_app *app,
                            struct secure_app_snapshot *snapshot)
{
    uint32_t elapsed;

    if ((app == NULL) || (snapshot == NULL) || (app->platform == NULL))
    {
        return SECURE_APP_EINVAL;
    }
    snapshot->state = app->state;
    snapshot->remaining = app->remaining;
    snapshot->last_error = app->last_error;
    snapshot->session_remaining_s = 0U;
    if (app->state == SECURE_APP_ACTIVE)
    {
        elapsed = elapsed_s(app->platform->now_s(app->platform->context),
                            app->session_started_at_s);
        snapshot->session_remaining_s =
            (elapsed >= app->config.session_duration_s) ? 0U :
            app->config.session_duration_s - elapsed;
    }
    return SECURE_APP_OK;
}

const char *secure_app_state_name(enum secure_app_state state)
{
    switch (state)
    {
    case SECURE_APP_DETACHED: return "DETACHED";
    case SECURE_APP_READY: return "READY";
    case SECURE_APP_AUTHORIZED: return "AUTHORIZED";
    case SECURE_APP_ACTIVE: return "ACTIVE";
    case SECURE_APP_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}
