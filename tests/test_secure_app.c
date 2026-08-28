#include "secure_app.h"

#include <assert.h>
#include <stdio.h>

struct fake_context
{
    uint32_t now;
    uint32_t remaining;
    int starts;
    int stops;
    bool present;
};

static int ok_init(void *context) { (void)context; return 0; }
static int ok_auth(void *context) { (void)context; return 0; }
static int ok_record(void *context, const struct secure_session_record *record)
{
    (void)context;
    return record == NULL ? -1 : 0;
}
static int read_quota(void *context, struct secure_quota *quota)
{
    struct fake_context *fake = context;
    quota->limit = 10U;
    quota->remaining = fake->remaining;
    quota->used = quota->limit - quota->remaining;
    return 0;
}
static int consume(void *context, struct secure_quota *quota)
{
    struct fake_context *fake = context;
    if (fake->remaining == 0U) return -1;
    fake->remaining--;
    return read_quota(context, quota);
}
static uint32_t now_s(void *context) { return ((struct fake_context *)context)->now; }
static bool present(void *context) { return ((struct fake_context *)context)->present; }
static int start(void *context) { ((struct fake_context *)context)->starts++; return 0; }
static int stop(void *context) { ((struct fake_context *)context)->stops++; return 0; }

int main(void)
{
    struct fake_context fake = {0U, 3U, 0, 0, true};
    struct secure_chip_ops chip = {
        ok_init, ok_auth, NULL, ok_record, read_quota, consume, &fake
    };
    struct secure_platform_ops platform = {
        now_s, present, start, stop, NULL, &fake
    };
    struct secure_app_config config = {5U};
    struct secure_app_snapshot snapshot;
    struct secure_app app;

    assert(secure_app_init(&app, &config, &chip, &platform) == SECURE_APP_OK);
    assert(app.state == SECURE_APP_READY);
    assert(secure_app_dispatch(&app, SECURE_EVENT_START) == SECURE_APP_OK);
    assert(app.state == SECURE_APP_ACTIVE);
    assert(fake.remaining == 2U);
    assert(fake.starts == 1);

    fake.now = 4U;
    assert(secure_app_dispatch(&app, SECURE_EVENT_TICK) == SECURE_APP_OK);
    assert(app.state == SECURE_APP_ACTIVE);
    assert(secure_app_get_snapshot(&app, &snapshot) == SECURE_APP_OK);
    assert(snapshot.session_remaining_s == 1U);

    fake.now = 5U;
    assert(secure_app_dispatch(&app, SECURE_EVENT_TICK) == SECURE_APP_OK);
    assert(app.state == SECURE_APP_AUTHORIZED);
    assert(fake.stops == 1);
    assert(secure_app_dispatch(&app, SECURE_EVENT_RESOURCE_REMOVED) == SECURE_APP_OK);
    assert(app.state == SECURE_APP_DETACHED);

    puts("secure_app tests: PASS");
    return 0;
}
