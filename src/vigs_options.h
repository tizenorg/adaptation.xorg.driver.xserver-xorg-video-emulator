#ifndef _VIGS_OPTIONS_H_
#define _VIGS_OPTIONS_H_

#include "vigs_config.h"
#include "xf86.h"

typedef enum
{
    vigs_option_max_execbuffer_size = 0,
    vigs_option_steal_fb,
    vigs_option_no_accel,
    vigs_option_count,
} vigs_option;

extern const OptionInfoRec g_vigs_options[vigs_option_count + 1];

#endif
