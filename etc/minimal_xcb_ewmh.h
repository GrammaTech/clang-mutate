#include <xcb/xcb.h>

typedef struct {
} xcb_ewmh_connection_t;

uint8_t xcb_ewmh_get_window_from_reply(xcb_window_t *window,
                                       xcb_get_property_reply_t *r);
