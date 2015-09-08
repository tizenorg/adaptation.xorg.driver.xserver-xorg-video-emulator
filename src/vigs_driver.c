/*
 * X.Org X server driver for VIGS
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact :
 * Stanislav Vorobiov <s.vorobiov@samsung.com>
 * Jinhyung Jo <jinhyung.jo@samsung.com>
 * YeongKyoon Lee <yeongkyoon.lee@samsung.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */

#include "vigs_config.h"
#include "xf86.h"
#include "xf86_OSproc.h"
#include "vigs_version.h"
#include "vigs_screen.h"
#include "vigs_options.h"
#include "vigs_log.h"

#define VIGS_NAME "VIGS"
#define VIGS_DRIVER_NAME "vigs"

#define PCI_VENDOR_ID_VIGS 0x19B2
#define PCI_DEVICE_ID_VIGS 0x1011

#ifdef XSERVER_LIBPCIACCESS
static const struct pci_id_match vigs_device_match[] =
{
    {
        PCI_VENDOR_ID_VIGS, PCI_DEVICE_ID_VIGS, PCI_MATCH_ANY, PCI_MATCH_ANY,
        0, 0, 0
    },
    { 0, 0, 0, 0, 0, 0, 0 }
};
#endif

static SymTabRec vigs_chips[] =
{
    { PCI_DEVICE_ID_VIGS, "vigs" },
    { -1, NULL }
};

static PciChipsets vigs_pci_chips[] =
{
    { PCI_DEVICE_ID_VIGS, PCI_DEVICE_ID_VIGS, RES_EXCLUSIVE_VGA },
    { -1, -1, RES_UNDEFINED }
};

static void vigs_scrn_fill(ScrnInfoPtr scrn)
{
    scrn->driverVersion = 0;
    scrn->driverName = VIGS_DRIVER_NAME;
    scrn->name = VIGS_NAME;
    scrn->PreInit = vigs_screen_pre_init;
    scrn->ScreenInit = vigs_screen_init;
    scrn->SwitchMode = vigs_screen_switch_mode;
    scrn->AdjustFrame = vigs_screen_adjust_frame;
    scrn->ValidMode = vigs_screen_valid_mode;
    scrn->EnterVT = vigs_screen_enter_vt;
    scrn->LeaveVT = vigs_screen_leave_vt;
    scrn->FreeScreen = vigs_screen_free;
}

static void vigs_identify(int flags)
{
    xf86PrintChipsets(VIGS_NAME, "Driver for VIGS chipsets", vigs_chips);
}

static Bool vigs_probe(DriverPtr drv, int flags)
{
    int i, num_used;
    int num_dev_sections;
    int *used_chips;
    GDevPtr *dev_sections;

    VIGS_LOG_TRACE("enter");

    if ((num_dev_sections = xf86MatchDevice(VIGS_DRIVER_NAME, &dev_sections)) <= 0) {
        return FALSE;
    }

#ifdef XSERVER_LIBPCIACCESS
    free(dev_sections);

    return FALSE;
#else
    if (!xf86GetPciVideoInfo()) {
        xfree(dev_sections);
        return FALSE;
    }

    num_used = xf86MatchPciInstances(VIGS_DRIVER_NAME,
                                     PCI_VENDOR_ID_VIGS,
                                     vigs_chips, vigs_pci_chips,
                                     dev_sections, num_dev_sections,
                                     drv,
                                     &used_chips);

    xfree(dev_sections);

    if (num_used <= 0) {
        xfree(used_chips);

        return FALSE;
    }

    if (flags & PROBE_DETECT) {
        xfree(used_chips);

        return TRUE;
    }

    for (i = 0; i < num_used; i++) {
        ScrnInfoPtr scrn = NULL;
        if ((scrn = xf86ConfigPciEntity(scrn, 0, used_chips[i],
                                        vigs_pci_chips, 0, 0, 0, 0, 0))) {
            vigs_scrn_fill(scrn);
        }
    }

    xfree(used_chips);

    return TRUE;
#endif
}

#ifdef XSERVER_LIBPCIACCESS
static Bool vigs_pci_probe(DriverPtr drv,
                           int entity_num,
                           struct pci_device *dev,
                           intptr_t match_data)
{
    VIGS_LOG_TRACE("Probing entity_num = %d", entity_num);

    ScrnInfoPtr scrn = xf86ConfigPciEntity(NULL, 0, entity_num, NULL, NULL,
                                           NULL, NULL, NULL, NULL);

    if (!scrn) {
        VIGS_LOG_TRACE("Probe failed");
        return FALSE;
    }

    VIGS_LOG_TRACE("Success");

    vigs_scrn_fill(scrn);

    return TRUE;
}
#endif

static const OptionInfoRec *vigs_available_options(int chipid, int busid)
{
    return g_vigs_options;
}

_X_EXPORT DriverRec vigs_driver =
{
    VIGS_VERSION,
    VIGS_DRIVER_NAME,
    vigs_identify,
#ifdef XSERVER_LIBPCIACCESS
    NULL,
#else
    vigs_probe,
#endif
    vigs_available_options,
    NULL,
    0,
    NULL,
#ifdef XSERVER_LIBPCIACCESS
    vigs_device_match,
    vigs_pci_probe,
#endif
#ifdef XSERVER_PLATFORM_BUS
    NULL,
#endif
};

#ifdef XFree86LOADER

MODULESETUPPROTO(vigs_setup);

static XF86ModuleVersionInfo vigs_module_info =
{
    VIGS_DRIVER_NAME,
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    VIGS_VERSION_MAJOR, VIGS_VERSION_MINOR, VIGS_PATCHLEVEL,
    ABI_CLASS_VIDEODRV,
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    { 0, 0, 0, 0 }
};

pointer vigs_setup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setup_done = FALSE;

    VIGS_LOG_TRACE("enter");

    if (!setup_done) {
        setup_done = TRUE;
        xf86AddDriver(&vigs_driver, module, HaveDriverFuncs);
        return (pointer)1;
    } else {
        if (errmaj) {
            *errmaj = LDR_ONCEONLY;
        }
        return NULL;
    }
}

_X_EXPORT XF86ModuleData vigsModuleData =
{
    &vigs_module_info,
    vigs_setup,
    NULL
};

#endif
