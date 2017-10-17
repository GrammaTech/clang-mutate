#define APR_HOOK_STRUCT(members) \
static struct { members } _hooks;

/** macro to link the hook structure */
#define APR_HOOK_LINK(name) \
    int *link_##name;
