/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/mmu.hh>
#include <osv/mempool.hh>
#include "processor.hh"
#include <osv/debug.hh>
#include "exceptions.hh"
#include <string.h>
#include <iterator>
#include "libc/signal.hh"
#include <osv/align.hh>
#include <osv/ilog2.hh>
#include <osv/prio.hh>
#include <safe-ptr.hh>
#include "fs/vfs/vfs.h"
#include <osv/error.h>
#include <osv/trace.hh>
#include <stack>
#include <fs/fs.hh>
#include <osv/file.h>
#include "dump.hh"
#include <osv/rcu.hh>
#include <osv/rwlock.h>
#include <numeric>
#include <set>

#include <osv/kernel_config_memory_debug.h>
#include <osv/kernel_config_lazy_stack.h>
#include <osv/kernel_config_lazy_stack_invariant.h>
#include <osv/kernel_config_memory_jvm_balloon.h>

// FIXME: Without this pragma, we get a lot of warnings that I don't know
// how to explain or fix. For now, let's just ignore them :-(
#pragma GCC diagnostic ignored "-Wstringop-overflow"

extern void* elf_start;
extern size_t elf_size;

extern const char text_start[], text_end[];

namespace mmu {

#if CONF_lazy_stack
// We need to ensure that lazy stack is populated deeply enough (2 pages)
// for all the cases when the vma_list_mutex is taken for write to prevent
// page faults triggered on stack. The page-fault handling logic would
// attempt to take same vma_list_mutex fo read and end up with a deadlock.
#define PREVENT_STACK_PAGE_FAULT \
    arch::ensure_next_two_stack_pages();
#else
#define PREVENT_STACK_PAGE_FAULT
#endif

struct vma_range_compare {
    bool operator()(const vma_range& a, const vma_range& b) const {
        return a.start() < b.start();
    }
};

//Set of all vma ranges - both linear and non-linear ones
__attribute__((init_priority((int)init_prio::vma_range_set)))
std::set<vma_range, vma_range_compare> vma_range_set;
rwlock_t vma_range_set_mutex;

struct linear_vma_compare {
    bool operator()(const linear_vma* a, const linear_vma* b) const {
        return a->_virt_addr < b->_virt_addr;
    }
};

__attribute__((init_priority((int)init_prio::linear_vma_set)))
std::set<linear_vma*, linear_vma_compare> linear_vma_set;
rwlock_t linear_vma_set_mutex;

namespace bi = boost::intrusive;

class vma_compare {
public:
    bool operator ()(const vma& a, const vma& b) const {
        return a.addr() < b.addr();
    }
};

constexpr uintptr_t lower_vma_limit = 0x0;
constexpr uintptr_t upper_vma_limit = 0x400000000000;

typedef boost::intrusive::set<vma,
                              bi::compare<vma_compare>,
                              bi::member_hook<vma,
                                              bi::set_member_hook<>,
                                              &vma::_vma_list_hook>,
                              bi::optimize_size<true>
                              > vma_list_base;

struct vma_list_type : vma_list_base {
    vma_list_type() {
        // insert markers for the edges of allocatable area
        // simplifies searches
        auto lower_edge = new anon_vma(addr_range(lower_vma_limit, lower_vma_limit), 0, 0);
        insert(*lower_edge);
        auto upper_edge = new anon_vma(addr_range(upper_vma_limit, upper_vma_limit), 0, 0);
        insert(*upper_edge);

        WITH_LOCK(vma_range_set_mutex.for_write()) {
            vma_range_set.insert(vma_range(lower_edge));
            vma_range_set.insert(vma_range(upper_edge));
        }
    }
};

__attribute__((init_priority((int)init_prio::vma_list)))
vma_list_type vma_list;

// protects vma list and page table modifications.
// anything that may add, remove, split vma, zaps pte or changes pte permission
// should hold the lock for write
rwlock_t vma_list_mutex;

// A mutex serializing modifications to the high part of the page table
// (linear map, etc.) which are not part of vma_list.
mutex page_table_high_mutex;

#if CONF_memory_jvm_balloon
// List of jvm_balloon_vma objects marked for deferred deletion
// to avoid deadlock during fault handling
std::vector<jvm_balloon_vma*> deferred_deletion_list;
mutex deferred_deletion_mutex;
#endif

// 1's for the bits provided by the pte for this level
// 0's for the bits provided by the virtual address for this level
phys pte_level_mask(unsigned level)
{
    auto shift = level * ilog2_roundup_constexpr(pte_per_page)
        + ilog2_roundup_constexpr(page_size);
    return ~((phys(1) << shift) - 1);
}

#ifdef __x86_64__
static void *elf_phys_start = (void*)OSV_KERNEL_BASE;
#endif

#ifdef __aarch64__
void *elf_phys_start;
extern "C" u64 kernel_vm_shift;
#endif

void* phys_to_virt(phys pa)
{
    void* phys_addr = reinterpret_cast<void*>(pa);
    if ((phys_addr >= elf_phys_start) && (phys_addr < elf_phys_start + elf_size)) {
#ifdef __x86_64__
        return (void*)(phys_addr + OSV_KERNEL_VM_SHIFT);
#endif
#ifdef __aarch64__
        return (void*)(phys_addr + kernel_vm_shift);
#endif
    }

    return phys_mem + pa;
}

phys virt_to_phys_pt(void* virt);

phys virt_to_phys(void *virt)
{
    if ((virt >= elf_start) && (virt < elf_start + elf_size)) {
#ifdef __x86_64__
        return reinterpret_cast<phys>((void*)(virt - OSV_KERNEL_VM_SHIFT));
#endif
#ifdef __aarch64__
        return reinterpret_cast<phys>((void*)(virt - kernel_vm_shift));
#endif
    }

#if CONF_memory_debug
    if (virt > debug_base) {
        return virt_to_phys_pt(virt);
    }
#endif

    // For now, only allow non-mmaped areas.  Later, we can either
    // bounce such addresses, or lock them in memory and translate
    assert(virt >= phys_mem);
    return reinterpret_cast<uintptr_t>(virt) & (mem_area_size - 1);
}

template <int N, typename MakePTE>
phys allocate_intermediate_level(MakePTE make_pte)
{
    phys pt_page = virt_to_phys(memory::alloc_page());
    // since the pt is not yet mapped, we don't need to use hw_ptep
    pt_element<N>* pt = phys_cast<pt_element<N>>(pt_page);
    for (auto i = 0; i < pte_per_page; ++i) {
        pt[i] = make_pte(i);
    }
    return pt_page;
}

template<int N>
void allocate_intermediate_level(hw_ptep<N> ptep, pt_element<N> org)
{
    phys pt_page = allocate_intermediate_level<N>([org](int i) {
        auto tmp = org;
        phys addend = phys(i) << page_size_shift;
        tmp.set_addr(tmp.addr() | addend, false);
        return tmp;
    });
    ptep.write(make_intermediate_pte(ptep, pt_page));
}

template<int N>
void allocate_intermediate_level(hw_ptep<N> ptep)
{
    phys pt_page = allocate_intermediate_level<N>([](int i) {
        return make_empty_pte<N>();
    });
    if (!ptep.compare_exchange(make_empty_pte<N>(), make_intermediate_pte(ptep, pt_page))) {
        memory::free_page(phys_to_virt(pt_page));
    }
}

// only 4k can be cow for now
pt_element<0> pte_mark_cow(pt_element<0> pte, bool cow)
{
    if (cow) {
        pte.set_writable(false);
    }
    pte.set_sw_bit(pte_cow, cow);
    return pte;
}

template<int N>
bool change_perm(hw_ptep<N> ptep, unsigned int perm)
{
    static_assert(pt_level_traits<N>::leaf_capable::value, "non leaf pte");
    pt_element<N> pte = ptep.read();
    unsigned int old = (pte.valid() ? perm_read : 0) |
        (pte.writable() ? perm_write : 0) |
        (pte.executable() ? perm_exec : 0);

    if (pte_is_cow(pte)) {
        perm &= ~perm_write;
    }

    // Note: in x86, if the present bit (0x1) is off, not only read is
    // disallowed, but also write and exec. So in mprotect, if any
    // permission is requested, we must also grant read permission.
    // Linux does this too.
    pte.set_valid(true);
    pte.set_writable(perm & perm_write);
    pte.set_executable(perm & perm_exec);
    pte.set_rsvd_bit(0, !perm);
    ptep.write(pte);

#ifdef __x86_64__
    return old & ~perm;
#endif
#ifdef __aarch64__
    //TODO: This will trigger full tlb flush in slightly more cases than on x64
    //and in future we should investigate more precise and hopefully lighter
    //mechanism. But for now it will do it.
    return old != perm;
#endif
}

template<int N>
void split_large_page(hw_ptep<N> ptep)
{
}

template<>
void split_large_page(hw_ptep<1> ptep)
{
    pt_element<1> pte_orig = ptep.read();
    pte_orig.set_large(false);
    allocate_intermediate_level(ptep, pte_orig);
}

struct page_allocator {
    virtual bool map(uintptr_t offset, hw_ptep<0> ptep, pt_element<0> pte, bool write) = 0;
    virtual bool map(uintptr_t offset, hw_ptep<1> ptep, pt_element<1> pte, bool write) = 0;
    virtual bool unmap(void *addr, uintptr_t offset, hw_ptep<0> ptep) = 0;
    virtual bool unmap(void *addr, uintptr_t offset, hw_ptep<1> ptep) = 0;
    virtual ~page_allocator() {}
};

unsigned long all_vmas_size()
{
    SCOPE_LOCK(vma_list_mutex.for_read());
    return std::accumulate(vma_list.begin(), vma_list.end(), size_t(0), [](size_t s, vma& v) { return s + v.size(); });
}

void clamp(uintptr_t& vstart1, uintptr_t& vend1,
           uintptr_t min, size_t max, size_t slop)
{
    vstart1 &= ~(slop - 1);
    vend1 |= (slop - 1);
    vstart1 = std::max(vstart1, min);
    vend1 = std::min(vend1, max);
}

constexpr unsigned pt_index(uintptr_t virt, unsigned level)
{
    return pt_index(reinterpret_cast<void*>(virt), level);
}

unsigned nr_page_sizes = 2; // FIXME: detect 1GB pages

void set_nr_page_sizes(unsigned nr)
{
    nr_page_sizes = nr;
}

enum class allocate_intermediate_opt : bool {no = true, yes = false};
enum class skip_empty_opt : bool {no = true, yes = false};
enum class descend_opt : bool {no = true, yes = false};
enum class once_opt : bool {no = true, yes = false};
enum class split_opt : bool {no = true, yes = false};
enum class account_opt: bool {no = true, yes = false};

// Parameter descriptions:
//  Allocate - if "yes" page walker will allocate intermediate page if one is missing
//             otherwise it will skip to next address.
//  Skip     - if "yes" page walker will not call leaf page handler on an empty pte.
//  Descend  - if "yes" page walker will descend one level if large page range is mapped
//             by small pages, otherwise it will call huge_page() on intermediate small pte
//  Once     - if "yes" page walker will not loop over range of pages
//  Split    - If "yes" page walker will split huge pages to small pages while walking
template<allocate_intermediate_opt Allocate, skip_empty_opt Skip = skip_empty_opt::yes,
        descend_opt Descend = descend_opt::yes, once_opt Once = once_opt::no, split_opt Split = split_opt::yes>
class page_table_operation {
protected:
    template<typename T>  bool opt2bool(T v) { return v == T::yes; }
public:
    bool allocate_intermediate(void) { return opt2bool(Allocate); }
    bool skip_empty(void) { return opt2bool(Skip); }
    bool descend(void) { return opt2bool(Descend); }
    bool once(void) { return opt2bool(Once); }
    template<int N>
    bool split_large(hw_ptep<N> ptep, int level) { return opt2bool(Split); }
    unsigned nr_page_sizes(void) { return mmu::nr_page_sizes; }

    template<int N>
    pt_element<N> ptep_read(hw_ptep<N> ptep) { return ptep.read(); }

    // page() function is called on leaf ptes. Each page table operation
    // have to provide its own version.
    template<int N>
    bool page(hw_ptep<N> ptep, uintptr_t offset) { assert(0); }
    // if huge page range is covered by smaller pages some page table operations
    // may want to have special handling for level 1 non leaf pte. intermediate_page_pre()
    // is called just before descending into the next level, while intermediate_page_post()
    // is called just after.
    void intermediate_page_pre(hw_ptep<1> ptep, uintptr_t offset) {}
    void intermediate_page_post(hw_ptep<1> ptep, uintptr_t offset) {}
    // Page walker calls page() when it a whole leaf page need to be handled, but if it
    // has 2M pte and less then 2M of virt memory to operate upon and split is disabled
    // sup_page is called instead. So if you are here it means that page walker encountered
    // 2M pte and page table operation wants to do something special with sub-region of it
    // since it disabled splitting.
    void sub_page(hw_ptep<1> ptep, int level, uintptr_t offset) { return; }
};

template<typename PageOps, int N>
static inline typename std::enable_if<pt_level_traits<N>::large_capable::value>::type
sub_page(PageOps& pops, hw_ptep<N> ptep, int level, uintptr_t offset)
{
    pops.sub_page(ptep, level, offset);
}

template<typename PageOps, int N>
static inline typename std::enable_if<!pt_level_traits<N>::large_capable::value>::type
sub_page(PageOps& pops, hw_ptep<N> ptep, int level, uintptr_t offset)
{
}

template<typename PageOps, int N>
static inline typename std::enable_if<pt_level_traits<N>::leaf_capable::value, bool>::type
page(PageOps& pops, hw_ptep<N> ptep, uintptr_t offset)
{
    return pops.page(ptep, offset);
}

template<typename PageOps, int N>
static inline typename std::enable_if<!pt_level_traits<N>::leaf_capable::value, bool>::type
page(PageOps& pops, hw_ptep<N> ptep, uintptr_t offset)
{
    assert(0);
    return false;
}

template<typename PageOps, int N>
static inline typename std::enable_if<pt_level_traits<N>::large_capable::value>::type
intermediate_page_pre(PageOps& pops, hw_ptep<N> ptep, uintptr_t offset)
{
    pops.intermediate_page_pre(ptep, offset);
}

template<typename PageOps, int N>
static inline typename std::enable_if<!pt_level_traits<N>::large_capable::value>::type
intermediate_page_pre(PageOps& pops, hw_ptep<N> ptep, uintptr_t offset)
{
}

template<typename PageOps, int N>
static inline typename std::enable_if<pt_level_traits<N>::large_capable::value>::type
intermediate_page_post(PageOps& pops, hw_ptep<N> ptep, uintptr_t offset)
{
    pops.intermediate_page_post(ptep, offset);
}

template<typename PageOps, int N>
static inline typename std::enable_if<!pt_level_traits<N>::large_capable::value>::type
intermediate_page_post(PageOps& pops, hw_ptep<N> ptep, uintptr_t offset)
{
}

template<typename PageOp, int ParentLevel> class map_level;

template<typename PageOp>
        void map_range(uintptr_t vma_start, uintptr_t vstart, size_t size, PageOp& page_mapper, size_t slop = page_size)
{
    map_level<PageOp, 4> pt_mapper(vma_start, vstart, size, page_mapper, slop);
    pt_mapper(hw_ptep<4>::force(mmu::get_root_pt(vstart)));
    // On some architectures with weak memory model it is necessary
    // to force writes to page table entries complete and instruction pipeline
    // flushed so that new mappings are properly visible when relevant newly mapped
    // virtual memory areas are accessed right after this point.
    // So let us call arch-specific function to execute the logic above if
    // applicable for given architecture.
    synchronize_page_table_modifications();
}

template<typename PageOp, int ParentLevel> class map_level {
private:
    uintptr_t vma_start;
    uintptr_t vcur;
    uintptr_t vend;
    size_t slop;
    PageOp& page_mapper;
    static constexpr int level = ParentLevel - 1;

    friend void map_range<PageOp>(uintptr_t, uintptr_t, size_t, PageOp&, size_t);
    friend class map_level<PageOp, ParentLevel + 1>;

    map_level(uintptr_t vma_start, uintptr_t vcur, size_t size, PageOp& page_mapper, size_t slop) :
        vma_start(vma_start), vcur(vcur), vend(vcur + size - 1), slop(slop), page_mapper(page_mapper) {}
    pt_element<ParentLevel> read(const hw_ptep<ParentLevel>& ptep) const {
        return page_mapper.ptep_read(ptep);
    }
    pt_element<level> read(const hw_ptep<level>& ptep) const {
        return page_mapper.ptep_read(ptep);
    }
    hw_ptep<level> follow(hw_ptep<ParentLevel> ptep)
    {
        return hw_ptep<level>::force(phys_cast<pt_element<level>>(read(ptep).next_pt_addr()));
    }
    bool skip_pte(hw_ptep<level> ptep) {
        return page_mapper.skip_empty() && read(ptep).empty();
    }
    bool descend(hw_ptep<level> ptep) {
        return page_mapper.descend() && !read(ptep).empty() && !read(ptep).large();
    }
    template<int N>
    typename std::enable_if<N == 0>::type
    map_range(uintptr_t vcur, size_t size, PageOp& page_mapper, size_t slop,
            hw_ptep<N> ptep, uintptr_t base_virt)
    {
    }
    template<int N>
    typename std::enable_if<N == level && N != 0>::type
    map_range(uintptr_t vcur, size_t size, PageOp& page_mapper, size_t slop,
            hw_ptep<N> ptep, uintptr_t base_virt)
    {
        map_level<PageOp, level> pt_mapper(vma_start, vcur, size, page_mapper, slop);
        pt_mapper(ptep, base_virt);
    }
    void operator()(hw_ptep<ParentLevel> parent, uintptr_t base_virt = 0) {
        if (!read(parent).valid()) {
            if (!page_mapper.allocate_intermediate()) {
                return;
            }
            allocate_intermediate_level(parent);
        } else if (read(parent).large()) {
            if (page_mapper.split_large(parent, ParentLevel)) {
                // We're trying to change a small page out of a huge page (or
                // in the future, potentially also 2 MB page out of a 1 GB),
                // so we need to first split the large page into smaller pages.
                // Our implementation ensures that it is ok to free pieces of a
                // alloc_huge_page() with free_page(), so it is safe to do such a
                // split.
                split_large_page(parent);
            } else {
                // If page_mapper does not want to split, let it handle subpage by itself
                sub_page(page_mapper, parent, ParentLevel, base_virt - vma_start);
                return;
            }
        }
        auto pt = follow(parent);
        phys step = phys(1) << (page_size_shift + level * pte_per_page_shift);
        auto idx = pt_index(vcur, level);
        auto eidx = pt_index(vend, level);
        base_virt += idx * step;
        base_virt = (int64_t(base_virt) << 16) >> 16; // extend 47th bit

        do {
            auto ptep = pt.at(idx);
            uintptr_t vstart1 = vcur, vend1 = vend;
            clamp(vstart1, vend1, base_virt, base_virt + step - 1, slop);
            if (unsigned(level) < page_mapper.nr_page_sizes() && vstart1 == base_virt && vend1 == base_virt + step - 1) {
                uintptr_t offset = base_virt - vma_start;
                if (level) {
                    if (!skip_pte(ptep)) {
                        if (descend(ptep) || !page(page_mapper, ptep, offset)) {
                            intermediate_page_pre(page_mapper, ptep, offset);
                            map_range(vstart1, vend1 - vstart1 + 1, page_mapper, slop, ptep, base_virt);
                            intermediate_page_post(page_mapper, ptep, offset);
                        }
                    }
                } else {
                    if (!skip_pte(ptep)) {
                        page(page_mapper, ptep, offset);
                    }
                }
            } else {
                map_range(vstart1, vend1 - vstart1 + 1, page_mapper, slop, ptep, base_virt);
            }
            base_virt += step;
            ++idx;
        } while(!page_mapper.once() && idx <= eidx);
    }
};

class linear_page_mapper :
        public page_table_operation<allocate_intermediate_opt::yes, skip_empty_opt::no, descend_opt::no> {
    phys start;
    phys end;
    mattr mem_attr;
public:
    linear_page_mapper(phys start, size_t size, mattr mem_attr = mattr_default) :
        start(start), end(start + size), mem_attr(mem_attr) {}
    template<int N>
    bool page(hw_ptep<N> ptep, uintptr_t offset) {
        phys addr = start + offset;
        assert(addr < end);
        ptep.write(make_leaf_pte(ptep, addr, mmu::perm_rwx, mem_attr));
        return true;
    }
};

template<allocate_intermediate_opt Allocate, skip_empty_opt Skip = skip_empty_opt::yes,
         account_opt Account = account_opt::no>
class vma_operation :
        public page_table_operation<Allocate, Skip, descend_opt::yes, once_opt::no, split_opt::yes> {
public:
    // returns true if tlb flush is needed after address range processing is completed.
    bool tlb_flush_needed(void) { return false; }
    // this function is called at the very end of operate_range(). vma_operation may do
    // whatever cleanup is needed here.
    void finalize(void) { return; }

    ulong account_results(void) { return _total_operated; }
    void account(size_t size) { if (this->opt2bool(Account)) _total_operated += size; }
private:
    // We don't need locking because each walk will create its own instance, so
    // while two instances can operate over the same linear address (therefore
    // all the cmpxcghs), the same instance will go linearly over its duty.
    ulong _total_operated = 0;
};

/*
 * populate() populates the page table with the entries it is (assumed to be)
 * missing to span the given virtual-memory address range, and then pre-fills
 * (using the given fill function) these pages and sets their permissions to
 * the given ones. This is part of the mmap implementation.
 */
template <account_opt T = account_opt::no>
class populate : public vma_operation<allocate_intermediate_opt::yes, skip_empty_opt::no, T> {
private:
    page_allocator* _page_provider;
    unsigned int _perm;
    bool _write;
    bool _map_dirty;
    template<int N>
    bool skip(pt_element<N> pte) {
        if (pte.empty()) {
            return false;
        }
        return !_write || pte.writable();
    }
    template<int N>
    inline pt_element<N> dirty(pt_element<N> pte) {
        pte.set_dirty(_map_dirty || _write);
        return pte;
    }
public:
    populate(page_allocator* pops, unsigned int perm, bool write = false, bool map_dirty = true) :
        _page_provider(pops), _perm(perm), _write(write), _map_dirty(map_dirty) { }
    template<int N>
    bool page(hw_ptep<N> ptep, uintptr_t offset) {
        auto pte = ptep.read();
        if (skip(pte)) {
            return true;
        }

        pte = dirty(make_leaf_pte(ptep, 0, _perm));

        try {
            if (_page_provider->map(offset, ptep, pte, _write)) {
                this->account(pt_level_traits<N>::size::value);
            }
        } catch(std::exception&) {
            return false;
        }
        return true;
    }
};

template <account_opt Account = account_opt::no>
class populate_small : public populate<Account> {
public:
    populate_small(page_allocator* pops, unsigned int perm, bool write = false, bool map_dirty = true) :
        populate<Account>(pops, perm, write, map_dirty) { }
    template<int N>
    bool page(hw_ptep<N> ptep, uintptr_t offset) {
        assert(!pt_level_traits<N>::large_capable::value);
        return populate<Account>::page(ptep, offset);
    }
    unsigned nr_page_sizes(void) { return 1; }
};

class splithugepages : public vma_operation<allocate_intermediate_opt::no, skip_empty_opt::yes, account_opt::no> {
public:
    splithugepages() { }
    template<int N>
    bool page(hw_ptep<N> ptep, uintptr_t offset)
    {
        assert(!pt_level_traits<N>::large_capable::value);
        return true;
    }
    unsigned nr_page_sizes(void) { return 1; }
};

struct tlb_gather {
    static constexpr size_t max_pages = 20;
    struct tlb_page {
        void* addr;
        size_t size;
    };
    size_t nr_pages = 0;
    tlb_page pages[max_pages];
    bool push(void* addr, size_t size) {
        bool flushed = false;
        if (nr_pages == max_pages) {
            flush();
            flushed = true;
        }
        pages[nr_pages++] = { addr, size };
        return flushed;
    }
    bool flush() {
        if (!nr_pages) {
            return false;
        }
        mmu::flush_tlb_all();
        for (auto i = 0u; i < nr_pages; ++i) {
            auto&& tp = pages[i];
            if (tp.size == page_size) {
                memory::free_page(tp.addr);
            } else {
                memory::free_huge_page(tp.addr, tp.size);
            }
        }
        nr_pages = 0;
        return true;
    }
};

/*
 * Undo the operation of populate(), freeing memory allocated by populate()
 * and marking the pages non-present.
 */
template <account_opt T = account_opt::no>
class unpopulate : public vma_operation<allocate_intermediate_opt::no, skip_empty_opt::yes, T> {
private:
    tlb_gather _tlb_gather;
    page_allocator* _pops;
    bool do_flush = false;
public:
    unpopulate(page_allocator* pops) : _pops(pops) {}
    template<int N>
    bool page(hw_ptep<N> ptep, uintptr_t offset) {
        void* addr = phys_to_virt(ptep.read().addr());
        size_t size = pt_level_traits<N>::size::value;
        // Note: we free the page even if it is already marked "not present".
        // evacuate() makes sure we are only called for allocated pages, and
        // not-present may only mean mprotect(PROT_NONE).
        if (_pops->unmap(addr, offset, ptep)) {
            do_flush = !_tlb_gather.push(addr, size);
        } else {
            do_flush = true;
        }
        this->account(size);
        return true;
    }
    void intermediate_page_post(hw_ptep<1> ptep, uintptr_t offset) {
        osv::rcu_defer([](void *page) { memory::free_page(page); }, phys_to_virt(ptep.read().addr()));
        ptep.write(make_empty_pte<1>());
    }
    bool tlb_flush_needed(void) {
        return !_tlb_gather.flush() && do_flush;
    }
    void finalize(void) {}
};

class protection : public vma_operation<allocate_intermediate_opt::no, skip_empty_opt::yes> {
private:
    unsigned int perm;
    bool do_flush;
public:
    protection(unsigned int perm) : perm(perm), do_flush(false) { }
    template<int N>
    bool page(hw_ptep<N> ptep, uintptr_t offset) {
        do_flush |= change_perm(ptep, perm);
        return true;
    }
    bool tlb_flush_needed(void) {return do_flush;}
};

template <typename T, account_opt Account = account_opt::no>
class dirty_cleaner : public vma_operation<allocate_intermediate_opt::no, skip_empty_opt::yes, Account> {
private:
    bool do_flush;
    T handler;
public:
    dirty_cleaner(T handler) : do_flush(false), handler(handler) {}

    template<int N>
    bool page(hw_ptep<N> ptep, uintptr_t offset) {
        pt_element<N> pte = ptep.read();
        if (!pte.dirty()) {
            return true;
        }
        do_flush |= true;
        pte.set_dirty(false);
        ptep.write(pte);
        handler(ptep.read().addr(), offset, pt_level_traits<N>::size::value);
        return true;
    }

    bool tlb_flush_needed(void) {return do_flush;}
    void finalize() {
        handler.finalize();
    }
};

class dirty_page_sync {
    friend dirty_cleaner<dirty_page_sync, account_opt::yes>;
    friend file_vma;
private:
    file *_file;
    f_offset _offset;
    uint64_t _size;
    struct elm {
        iovec iov;
        off_t offset;
    };
    std::stack<elm> queue;
    dirty_page_sync(file *file, f_offset offset, uint64_t size) : _file(file), _offset(offset), _size(size) {}
    void operator()(phys addr, uintptr_t offset, size_t size) {
        off_t off = _offset + offset;
        size_t len = std::min(size, _size - off);
        queue.push(elm{{phys_to_virt(addr), len}, off});
    }
    void finalize() {
        while(!queue.empty()) {
            elm w = queue.top();
            uio data{&w.iov, 1, w.offset, ssize_t(w.iov.iov_len), UIO_WRITE};
            int error = _file->write(&data, FOF_OFFSET);
            if (error) {
                throw make_error(error);
            }
            queue.pop();
        }
    }
};

class virt_to_phys_map :
        public page_table_operation<allocate_intermediate_opt::no, skip_empty_opt::yes,
        descend_opt::yes, once_opt::yes, split_opt::no> {
private:
    uintptr_t v;
    phys result;
    static constexpr phys null = ~0ull;
    virt_to_phys_map(uintptr_t v) : v(v), result(null) {}

    phys addr(void) {
        assert(result != null);
        return result;
    }
public:
    friend phys virt_to_phys_pt(void* virt);
    template<int N>
    bool page(hw_ptep<N> ptep, uintptr_t offset) {
        assert(result == null);
        result = ptep.read().addr() | (v & ~pte_level_mask(N));
        return true;
    }
    void sub_page(hw_ptep<1> ptep, int l, uintptr_t offset) {
        assert(ptep.read().large());
        page(ptep, offset);
    }
};

class cleanup_intermediate_pages
    : public page_table_operation<
          allocate_intermediate_opt::no,
          skip_empty_opt::yes,
          descend_opt::yes,
          once_opt::no,
          split_opt::no> {
public:
    template<int N>
    bool page(hw_ptep<N> ptep, uintptr_t offset) {
        if (!pt_level_traits<N>::large_capable::value) {
            ++live_ptes;
        }
        return true;
    }
    void intermediate_page_pre(hw_ptep<1> ptep, uintptr_t offset) {
        live_ptes = 0;
    }
    void intermediate_page_post(hw_ptep<1> ptep, uintptr_t offset) {
        if (!live_ptes) {
            auto old = ptep.read();
            auto v = phys_cast<u64*>(old.addr());
            for (unsigned i = 0; i < 512; ++i) {
                assert(v[i] == 0);
            }
            ptep.write(make_empty_pte<1>());
            osv::rcu_defer([](void *page) { memory::free_page(page); }, phys_to_virt(old.addr()));
            do_flush = true;
        }
    }
    bool tlb_flush_needed() { return do_flush; }
    void finalize() {}
    ulong account_results(void) { return 0; }
private:
    unsigned live_ptes;
    bool do_flush = false;
};

class virt_to_pte_map_rcu :
        public page_table_operation<allocate_intermediate_opt::no, skip_empty_opt::yes,
        descend_opt::yes, once_opt::yes, split_opt::no> {
private:
    virt_pte_visitor& _visitor;
    virt_to_pte_map_rcu(virt_pte_visitor& visitor) : _visitor(visitor) {}

public:
    friend void virt_visit_pte_rcu(uintptr_t, virt_pte_visitor&);
    template<int N>
    pt_element<N> ptep_read(hw_ptep<N> ptep) {
        return ptep.ll_read();
    }
    template<int N>
    bool page(hw_ptep<N> ptep, uintptr_t offset) {
        auto pte = ptep_read(ptep);
        _visitor.pte(pte);
        assert(pt_level_traits<N>::large_capable::value == pte.large());
        return true;
    }
    void sub_page(hw_ptep<1> ptep, int l, uintptr_t offset) {
        page(ptep, offset);
    }
};

template<typename T> ulong operate_range(T mapper, void *vma_start, void *start, size_t size)
{
    start = align_down(start, page_size);
    size = std::max(align_up(size, page_size), page_size);
    uintptr_t virt = reinterpret_cast<uintptr_t>(start);
    map_range(reinterpret_cast<uintptr_t>(vma_start), virt, size, mapper);

    // TODO: consider if instead of requesting a full TLB flush, we should
    // instead try to make more judicious use of INVLPG - e.g., in
    // split_large_page() and other specific places where we modify specific
    // page table entries.
    if (mapper.tlb_flush_needed()) {
        mmu::flush_tlb_all();
    }
    mapper.finalize();
    return mapper.account_results();
}

template<typename T> ulong operate_range(T mapper, void *start, size_t size)
{
    return operate_range(mapper, start, start, size);
}

phys virt_to_phys_pt(void* virt)
{
    auto v = reinterpret_cast<uintptr_t>(virt);
    auto vbase = align_down(v, page_size);
    virt_to_phys_map v2p_mapper(v);
    map_range(vbase, vbase, page_size, v2p_mapper);
    return v2p_mapper.addr();
}

void virt_visit_pte_rcu(uintptr_t virt, virt_pte_visitor& visitor)
{
    auto vbase = align_down(virt, page_size);
    virt_to_pte_map_rcu v2pte_mapper(visitor);
    WITH_LOCK(osv::rcu_read_lock) {
        map_range(vbase, vbase, page_siz;
}

void file_vma::fault(uintptr_t addr, exception_frame *ef)
{
    auto hp_start = align_up(_range.start(), huge_page_size);
    auto hp_end = align_down(_range.end(), huge_page_size);
    auto fsize = ::size(_file);
    if (offset(addr) >= fsize) {
        vm_sigbus(addr, ef);
        return;
    }
    size_t size;
    if (!has_flags(mmap_small) && (hp_start <= addr && addr < hp_end) && offset(hp_end) < fsize) {
        addr = align_down(addr, huge_page_size);
        size = huge_page_size;
    } else {
        size = page_size;
    }

    populate_vma<account_opt::no>(this, (void*)addr, size,
            mmu::is_page_fault_write(ef->get_error()));
}

file_vma::~file_vma()
{
    delete _page_ops;
}

void file_vma::split(uintptr_t edge)
{
    if (edge <= _range.start() || edge >= _range.end()) {
        return;
    }
    auto off = offset(edge);
    vma *n = _file->mmap(addr_range(edge, _range.end()), _flags, _perm, off).release();
    set(_range.start(), edge);
    vma_list.insert(*n);
    WITH_LOCK(vma_range_set_mutex.for_write()) {
        vma_range_set.insert(vma_range(n));
    }
}

error file_vma::sync(uintptr_t start, uintptr_t end)
{
    if (!has_flags(mmap_shared))
        return make_error(ENOMEM);

    // Called when ZFS arc cache is not present.
    if (_page_ops && dynamic_cast<map_file_page_read *>(_page_ops)) {
        start = std::max(start, _range.start());
        end = std::min(end, _range.end());
        uintptr_t size = end - start;

        dirty_page_sync sync(_file.get(), _offset, ::size(_file));
        error err = no_error();
        try {
            if (operate_range(dirty_cleaner<dirty_page_sync, account_opt::yes>(sync), (void*)start, size) != 0) {
                err = make_error(sys_fsync(_file.get()));
            }
        } catch (error e) {
            err = e;
        }
        return err;
    }

    try {
        _file->sync(_offset + start - _range.start(), _offset + end - _range.start());
    } catch (error& err) {
        return err;
    }

    return make_error(sys_fsync(_file.get()));
}

int file_vma::validate_perm(unsigned perm)
{
    // fail if mapping a file that is not opened for reading.
    if (!(_file->f_flags & FREAD)) {
        return EACCES;
    }
    if (perm & perm_write) {
        if (has_flags(mmap_shared) && !(_file->f_flags & FWRITE)) {
            return EACCES;
        }
    }
    // fail if prot asks for PROT_EXEC and the underlying FS was
    // mounted no-exec.
    if (perm & perm_exec && (_file->f_dentry->d_mount->m_flags & MNT_NOEXEC)) {
        return EPERM;
    }
    return 0;
}

f_offset file_vma::offset(uintptr_t addr)
{
    return _offset + (addr - _range.start());
}

std::unique_ptr<file_vma> shm_file::mmap(addr_range range, unsigned flags, unsigned perm, off_t offset)
{
    return map_file_mmap(this, range, flags, perm, offset);
}

void* shm_file::page(uintptr_t hp_off)
{
    void *addr;

    auto p = _pages.find(hp_off);
    if (p == _pages.end()) {
        addr = memory::alloc_huge_page(huge_page_size);
        memset(addr, 0, huge_page_size);
        _pages.emplace(hp_off, addr);
    } else {
        addr = p->second;
    }

    return addr;
}

bool shm_file::map_page(uintptr_t offset, hw_ptep<0> ptep, pt_element<0> pte, bool write, bool shared)
{
    uintptr_t hp_off = align_down(offset, huge_page_size);

    return write_pte(static_cast<char*>(page(hp_off)) + offset - hp_off, ptep, pte);
}

bool shm_file::map_page(uintptr_t offset, hw_ptep<1> ptep, pt_element<1> pte, bool write, bool shared)
{
    uintptr_t hp_off = align_down(offset, huge_page_size);

    assert(hp_off == offset);

    return write_pte(static_cast<char*>(page(hp_off)) + offset - hp_off, ptep, pte);
}

bool shm_file::put_page(void *addr, uintptr_t offset, hw_ptep<0> ptep) {return false;}
bool shm_file::put_page(void *addr, uintptr_t offset, hw_ptep<1> ptep) {return false;}

shm_file::shm_file(size_t size, int flags) : special_file(flags, DTYPE_UNSPEC), _size(size) {}

int shm_file::stat(struct stat* buf)
{
    buf->st_size = _size;
    return 0;
}

int shm_file::close()
{
    for (auto& i : _pages) {
        memory::free_huge_page(i.second, huge_page_size);
    }
    _pages.clear();
    return 0;
}

linear_vma::linear_vma(void* virt, phys phys, size_t size, mattr mem_attr, const char* name) {
    _virt_addr = virt;
    _phys_addr = phys;
    _size = size;
    _mem_attr = mem_attr;
    _name = name;
}

linear_vma::~linear_vma() {
}

std::string sysfs_linear_maps() {
    std::string output;
    WITH_LOCK(linear_vma_set_mutex.for_read()) {
        for(auto *vma : linear_vma_set) {
            char mattr = vma->_mem_attr == mmu::mattr::normal ? 'n' : 'd';
            output += osv::sprintf("%18p %18p %12x rwxp %c %s\n",
                vma->_virt_addr, (void*)vma->_phys_addr, vma->_size, mattr, vma->_name.c_str());
        }
    }
    return output;
}

void linear_map(void* _virt, phys addr, size_t size, const char* name,
                size_t slop, mattr mem_attr)
{
    uintptr_t virt = reinterpret_cast<uintptr_t>(_virt);
    slop = std::min(slop, page_size_level(nr_page_sizes - 1));
    assert((virt & (slop - 1)) == (addr & (slop - 1)));
    linear_page_mapper phys_map(addr, size, mem_attr);
    map_range(virt, virt, size, phys_map, slop);
    auto _vma = new linear_vma(_virt, addr, size, mem_attr, name);
    WITH_LOCK(linear_vma_set_mutex.for_write()) {
       linear_vma_set.insert(_vma);
    }
    WITH_LOCK(vma_range_set_mutex.for_write()) {
       vma_range_set.insert(vma_range(_vma));
    }
}

void free_initial_memory_range(uintptr_t addr, size_t size)
{
    if (!size) {
        return;
    }
    // Most of the time the kernel code references memory using
    // virtual addresses. However some allocated system structures
    // like page tables use physical addresses.
    // For that reason we skip the very 1st page of physical memory
    // so that allocated memory areas NEVER map to physical address 0.
    if (!addr) {
        ++addr;
        --size;
    }
    memory::free_initial_memory_range(phys_cast<void>(addr), size);
}

error mprotect(const void *addr, size_t len, unsigned perm)
{
    PREVENT_STACK_PAGE_FAULT
    SCOPE_LOCK(vma_list_mutex.for_write());

    if (!ismapped(addr, len)) {
        return make_error(ENOMEM);
    }

    return protect(addr, len, perm);
}

error munmap(const void *addr, size_t length)
{
    PREVENT_STACK_PAGE_FAULT
    SCOPE_LOCK(vma_list_mutex.for_write());

    length = align_up(length, mmu::page_size);
    if (!ismapped(addr, length)) {
        return make_error(EINVAL);
    }
    sync(addr, length, 0);
    unmap(addr, length);
    return no_error();
}

error msync(const void* addr, size_t length, int flags)
{
    SCOPE_LOCK(vma_list_mutex.for_read());

    if (!ismapped(addr, length)) {
        return make_error(ENOMEM);
    }
    return sync(addr, length, flags);
}

error mincore(const void *addr, size_t length, unsigned char *vec)
{
    char *end = align_up((char *)addr + length, page_size);
    char tmp;
    SCOPE_LOCK(vma_list_mutex.for_read());
    if (!is_linear_mapped(addr, length) && !ismapped(addr, length)) {
        return make_error(ENOMEM);
    }
    for (char *p = (char *)addr; p < end; p += page_size) {
        if (safe_load(p, tmp)) {
            *vec++ = 0x01;
        } else {
            *vec++ = 0x00;
        }
    }
    return no_error();
}

std::string procfs_maps()
{
    std::string output;
    WITH_LOCK(vma_list_mutex.for_read()) {
        for (auto& vma : vma_list) {
            char read    = vma.perm() & perm_read  ? 'r' : '-';
            char write   = vma.perm() & perm_write ? 'w' : '-';
            char execute = vma.perm() & perm_exec  ? 'x' : '-';
            char priv    = 'p';
            output += osv::sprintf("%lx-%lx %c%c%c%c ", vma.start(), vma.end(), read, write, execute, priv);
            if (vma.flags() & mmap_file) {
                const file_vma &f_vma = static_cast<file_vma&>(vma);
                unsigned dev_id_major = major(f_vma.file_dev_id());
                unsigned dev_id_minor = minor(f_vma.file_dev_id());
                output += osv::sprintf("%08x %02x:%02x %ld %s\n", f_vma.offset(), dev_id_major, dev_id_minor, f_vma.file_inode(), f_vma.file()->f_dentry->d_path);
            } else {
                output += osv::sprintf("00000000 00:00 0\n");
            }
        }
    }
    return output;
}

}

extern "C" bool is_linear_mapped(const void *addr)
{
    return addr >= mmu::phys_mem;
}
