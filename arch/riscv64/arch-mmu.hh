/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef ARCH_MMU_HH
#define ARCH_MMU_HH

#include <osv/debug.hh>
#include <cassert>

/* RISC-V MMU
 *
 * RISC-V uses the Sv39, Sv48, or Sv57 paging schemes.
 * For now, we'll target Sv39 (39-bit virtual addressing).
 */

namespace mmu {
constexpr int max_phys_addr_size = 56; // RISC-V supports up to 56-bit physical addresses
extern u64 mem_addr;
extern void *elf_phys_start;

enum class mattr {
    normal,
    dev
};
constexpr mattr mattr_default = mattr::normal;

template<int N>
class pt_element : public pt_element_common<N> {
public:
    constexpr pt_element() noexcept : pt_element_common<N>(0) {}
    explicit pt_element(u64 x) noexcept : pt_element_common<N>(x) {}

    // RISC-V PTE bits
    // Bit 0: Valid
    // Bit 1: Read
    // Bit 2: Write  
    // Bit 3: Execute
    // Bit 4: User
    // Bit 5: Global
    // Bit 6: Accessed
    // Bit 7: Dirty
};

/* common interface implementation */
template<int N>
inline bool pt_element_common<N>::empty() const { return !x; }
template<int N>
inline bool pt_element_common<N>::valid() const { return x & 0x1; }
template<int N>
inline bool pt_element_common<N>::writable() const { return x & (1ul << 2); } // W bit
template<int N>
inline bool pt_element_common<N>::executable() const { return x & (1ul << 3); } // X bit
template<int N>
inline bool pt_element_common<N>::dirty() const { return x & (1ul << 7); } // D bit
template<int N>
inline bool pt_element_common<N>::large() const {
    // Large pages have R/W/X bits set (leaf PTE)
    return pt_level_traits<N>::large_capable::value && ((x & 0xE) != 0);
}
template<int N>
inline bool pt_element_common<N>::user() { return x & (1 << 4); } // U bit
template<int N>
inline bool pt_element_common<N>::accessed() { return x & (1 << 6); } // A bit

template<int N>
inline bool pt_element_common<N>::sw_bit(unsigned off) const {
    assert(off < 2);
    return (x >> (8 + off)) & 1; // Bits 8-9 are reserved for software
}

template<int N>
inline bool pt_element_common<N>::rsvd_bit(unsigned off) const {
    return false;
}

template<int N>
inline phys pt_element_common<N>::addr() const {
    // PPN (Physical Page Number) is in bits 10-53
    u64 ppn = (x >> 10) & ((1ul << 44) - 1);
    return ppn << page_size_shift;
}

template<int N>
inline u64 pt_element_common<N>::pfn() const { return addr() >> page_size_shift; }
template<int N>
inline phys pt_element_common<N>::next_pt_addr() const {
    assert(!large());
    return addr();
}
template<int N>
inline u64 pt_element_common<N>::next_pt_pfn() const {
    assert(!large());
    return pfn();
}

template<int N>
inline void pt_element_common<N>::set_valid(bool v) { set_bit(0, v); }
template<int N>
inline void pt_element_common<N>::set_writable(bool v) { set_bit(2, v); } // W bit
template<int N>
inline void pt_element_common<N>::set_executable(bool v) { set_bit(3, v); } // X bit
template<int N>
inline void pt_element_common<N>::set_dirty(bool v) { set_bit(7, v); } // D bit
template<int N>
inline void pt_element_common<N>::set_large(bool v) { 
    // Large pages are leaf PTEs with R/W/X bits set
    // For non-leaf PTEs, R/W/X should be 0
    if (!v) {
        x &= ~0xEul; // Clear R/W/X bits for non-leaf
    }
}
template<int N>
inline void pt_element_common<N>::set_user(bool v) { set_bit(4, v); } // U bit
template<int N>
inline void pt_element_common<N>::set_accessed(bool v) { set_bit(6, v); } // A bit

template<int N>
inline void pt_element_common<N>::set_sw_bit(unsigned off, bool v) {
    assert(off < 2);
    set_bit(8 + off, v);
}

template<int N>
inline void pt_element_common<N>::set_rsvd_bit(unsigned off, bool v) {
}

template<int N>
inline void pt_element_common<N>::set_addr(phys addr, bool large) {
    u64 ppn = addr >> page_size_shift;
    x = (x & 0x3FF) | (ppn << 10); // Keep low 10 bits, set PPN
    if (large) {
        x |= 0x1; // Set valid bit
    } else {
        x |= 0x1; // Set valid bit for non-leaf too
    }
}

template<int N>
inline void pt_element_common<N>::set_pfn(u64 pfn, bool large) {
    set_addr(pfn << page_size_shift, large);
}

template<int N>
pt_element<N> make_pte(phys addr, bool leaf, unsigned perm = perm_rwx,
                       mattr mem_attr = mattr_default)
{
    assert(pt_level_traits<N>::leaf_capable::value || !leaf);
    bool large = pt_level_traits<N>::large_capable::value && leaf;
    pt_element<N> pte;
    pte.set_valid(perm != 0);
    pte.set_writable(perm & perm_write);
    pte.set_executable(perm & perm_exec);
    pte.set_dirty(true);
    pte.set_large(large);
    pte.set_addr(addr, large);

    // Set Read bit for readable memory
    if (perm & (perm_read | perm_write | perm_exec)) {
        pte.x |= (1ul << 1); // R bit
    }

    pte.set_user(false);
    pte.set_accessed(true);

    // RISC-V doesn't have explicit memory attribute bits like ARM
    // Memory ordering is controlled by fence instructions

    return pte;
}

// On RISC-V, ensure page table modifications are visible
inline void synchronize_page_table_modifications() {
    asm volatile("sfence.vma");
}

}
#endif /* ARCH_MMU_HH */
