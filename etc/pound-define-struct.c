typedef struct _zend_arg_info {
    const char *name;
} zend_arg_info;

#define ZEND_ARG_INFO_EX(name)  \
  static const zend_arg_info name[] = { \
    { "" }, \
  };

ZEND_ARG_INFO_EX(arginfo)

// expands to:
/*
static const zend_arg_info arginfo[] = {
    { "" },
};
*/
