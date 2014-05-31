#ifndef _VIGS_VERSION_H_
#define _VIGS_VERSION_H_

#include "vigs_protocol.h"

/*
 * Bump this whenever driver changes.
 * Whenever protocol changes this should be reset to 0.
 */
#define VIGS_PATCHLEVEL 1

#define VIGS_VERSION_MAJOR VIGS_PROTOCOL_VERSION
#define VIGS_VERSION_MINOR 0

#define VIGS_VERSION \
    ((VIGS_VERSION_MAJOR << 20) | \
     (VIGS_VERSION_MINOR << 10) | \
     (VIGS_PATCHLEVEL))

#endif
