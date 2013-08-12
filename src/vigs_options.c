#include "vigs_options.h"

const OptionInfoRec g_vigs_options[vigs_option_count + 1] =
{
    {
        vigs_option_max_execbuffer_size,
        "MaxExecbufferSize",
        OPTV_INTEGER,
        { 100000 },
        FALSE
    },
    {
        vigs_option_steal_fb,
        "StealFb",
        OPTV_BOOLEAN,
        { 1 },
        FALSE
    },
    {
        vigs_option_no_accel,
        "NoAccel",
        OPTV_BOOLEAN,
        { 0 },
        FALSE
    },
    { -1, NULL, OPTV_NONE, {0}, FALSE }
};
