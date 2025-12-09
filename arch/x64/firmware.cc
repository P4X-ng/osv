/*
 * Copyright (C) 2014 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/firmware.hh>
#include <osv/uefi.hh>

#include "dmi.hh"

namespace osv {

void firmware_probe()
{
    if (uefi::is_uefi_boot()) {
        // UEFI firmware detection
        // SMBIOS tables are available through UEFI configuration tables
        // DMI information will be extracted from SMBIOS tables
        dmi_probe_uefi();
    } else {
        // Legacy BIOS firmware detection
        dmi_probe();
    }
}

std::string firmware_vendor()
{
    if (uefi::is_uefi_boot()) {
        return "UEFI";
    }
    return dmi_bios_vendor;
}

}
