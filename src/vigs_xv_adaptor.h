#ifndef _VIGS_XV_ADAPTOR_H_
#define _VIGS_XV_ADAPTOR_H_

#include "vigs_config.h"
#include "xf86.h"
#include "xf86xv.h"

struct vigs_xv_adaptor
{
    XF86VideoAdaptorRec base;

    void (*destroy)(struct vigs_xv_adaptor */*adaptor*/);
};

#endif
