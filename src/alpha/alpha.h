#ifndef __CPUFETCH_ALPHA__
#define __CPUFETCH_ALPHA__

#include <stdint.h>
#include "../common/cpu.h"

// Architecture extension bits as reported by the 'amask' instruction.
// amask returns the set of features that are NOT implemented, so the
// implemented set is computed as ~amask(~0). These bit positions match
// the values decoded by the NetBSD kernel (see sys/arch/alpha).
#define ALPHA_AMASK_BWX (1UL << 0)  // Byte/Word extension
#define ALPHA_AMASK_FIX (1UL << 1)  // Float<->int moves and sqrt
#define ALPHA_AMASK_CIX (1UL << 2)  // Count extension (ctlz/ctpop/cttz)
#define ALPHA_AMASK_MVI (1UL << 8)  // Motion Video Instructions
#define ALPHA_AMASK_PAT (1UL << 9)  // Precise arithmetic trap reporting
#define ALPHA_AMASK_PMI (1UL << 12) // Prefetch with modify intent

struct cpuInfo* get_cpu_info(void);
char* get_str_topology(struct topology* topo, bool dual_socket);
char* get_str_features(struct cpuInfo* cpu);
void print_debug(struct cpuInfo* cpu);

#endif
