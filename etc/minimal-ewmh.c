#include "minimal_xcb_ewmh.h"

#include<xcb/xcb.h>

#define DO_REPLY_SINGLE_VALUE(fname)					   \
  uint8_t xcb_ewmh_get_##fname##_from_reply() {				   \
    return 1;                                                              \
  }                                                                        

/** Define reply functions for common WINDOW Atom */
DO_REPLY_SINGLE_VALUE(window)
