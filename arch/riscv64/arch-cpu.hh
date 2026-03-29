/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef ARCH_CPU_HH_
#define ARCH_CPU_HH_

#include "processor.hh"
#include <osv/types.h>

namespace sched {
    class cpu;
    class thread;
}

void arch_init_premain();
void arch_init_drivers();

#endif /* ARCH_CPU_HH_ */
