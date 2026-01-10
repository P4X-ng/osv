/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef ARCH_PCI_HH_
#define ARCH_PCI_HH_

// RISC-V PCI support
// Most RISC-V platforms use device tree for device discovery
// PCI support may be limited or not available on all platforms

namespace pci {

void arch_pci_init();

}

#endif /* ARCH_PCI_HH_ */
