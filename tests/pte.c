#include "ktest.h"
#include <stddef.h>
#include <stdint.h>

// Test PTE forge/unforge round-trip from x86_64/memory.h
// These are declared in x86_64/memory.h but we test the logic inline
// to avoid pulling in arch-specific headers on non-x86 test hosts.

static size_t test_phy_to_pte(size_t phys) { return phys | 0x8000000000000067; }

static size_t test_pte_to_phy(size_t pte) { return pte & 0x000ffffffffff000; }

KTEST(pte, roundtrip_aligned) {
  size_t phys = 0x1234000;
  size_t pte = test_phy_to_pte(phys);
  size_t back = test_pte_to_phy(pte);
  KT_ASSERT_EQ(back, phys);
}

KTEST(pte, flags_present) {
  size_t pte = test_phy_to_pte(0x1000);
  KT_ASSERT(pte & 1);            // Present
  KT_ASSERT(pte & 2);            // R/W
  KT_ASSERT(pte & 4);            // User
  KT_ASSERT(pte & (1ULL << 63)); // NX
}

KTEST(pte, zero_phys) {
  size_t pte = test_phy_to_pte(0);
  size_t back = test_pte_to_phy(pte);
  KT_ASSERT_EQ(back, 0);
}

KTEST(pte, high_phys) {
  size_t phys = 0xFFFFF000ULL; // ~4GB boundary
  size_t pte = test_phy_to_pte(phys);
  size_t back = test_pte_to_phy(pte);
  KT_ASSERT_EQ(back, phys);
}
