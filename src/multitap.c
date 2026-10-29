/*
 * Behavior kieu Nokia (multi-tap): bam lien tiep CUNG MOT PHIM trong "tapping-term-ms"
 * de doi ky tu. Ky tu hien ngay khi bam: moi lan bam them thi gui Backspace roi gui
 * ky tu ke tiep trong danh sach "keys". Bam phim khac hoac het thoi gian thi ket thuc.
 */

#define DT_DRV_COMPAT zmk_behavior_multitap

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define BSPC_ENC ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE)

struct mt_config {
    const uint32_t *keys;
    uint8_t len;
    uint16_t term_ms;
};

static const struct device *cur_dev;
static uint8_t cur_idx;
static bool sending;
static bool inited;
static struct k_work_delayable reset_work;

static void tap(uint32_t enc) {
    sending = true;
    raise_zmk_keycode_state_changed_from_encoded(enc, true, k_uptime_get());
    raise_zmk_keycode_state_changed_from_encoded(enc, false, k_uptime_get());
    sending = false;
}

static void reset_fn(struct k_work *work) {
    ARG_UNUSED(work);
    cur_dev = NULL;
}

static int on_pressed(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct mt_config *cfg = dev->config;

    if (cur_dev == dev) {
        tap(BSPC_ENC); /* xoa ky tu vua go, thay bang ky tu ke tiep */
        cur_idx = (cur_idx + 1) % cfg->len;
    } else {
        cur_dev = dev;
        cur_idx = 0;
    }
    tap(cfg->keys[cur_idx]);
    k_work_reschedule(&reset_work, K_MSEC(cfg->term_ms));
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_released(struct zmk_behavior_binding *binding,
                       struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int mt_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);

    /* phim khac (khong phai do minh gui) duoc bam -> ket thuc chuoi, tranh xoa nham */
    if (ev != NULL && ev->state && !sending) {
        cur_dev = NULL;
        k_work_cancel_delayable(&reset_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(multitap, mt_listener);
ZMK_SUBSCRIPTION(multitap, zmk_keycode_state_changed);

static const struct behavior_driver_api mt_api = {
    .binding_pressed = on_pressed,
    .binding_released = on_released,
};

static int mt_init(const struct device *dev) {
    ARG_UNUSED(dev);
    if (!inited) {
        k_work_init_delayable(&reset_work, reset_fn);
        inited = true;
    }
    return 0;
}

#define MT_INST(n)                                                                                 \
    static const uint32_t mt_keys_##n[] = DT_INST_PROP(n, keys);                                   \
    static const struct mt_config mt_cfg_##n = {                                                   \
        .keys = mt_keys_##n,                                                                       \
        .len = DT_INST_PROP_LEN(n, keys),                                                          \
        .term_ms = DT_INST_PROP(n, tapping_term_ms),                                               \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, mt_init, NULL, NULL, &mt_cfg_##n, POST_KERNEL,                      \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &mt_api);

DT_INST_FOREACH_STATUS_OKAY(MT_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY */
