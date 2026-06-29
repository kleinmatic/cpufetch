#ifndef __ALPHA_UARCH__
#define __ALPHA_UARCH__

#include <stdint.h>
#include "alpha.h"

struct uarch;

// Identify the microarchitecture from the 'implver' generation value
// (0=EV4, 1=EV5, 2=EV6, 3=EV7 family) and the implemented 'amask'
// feature bits, which disambiguate the chip within a generation.
struct uarch* get_uarch_from_alpha(uint64_t implver, uint64_t amask_features);

char* get_str_uarch(struct cpuInfo* cpu);
char* get_str_process(struct cpuInfo* cpu);

int32_t get_l1i_KB(struct uarch* arch);
int32_t get_l1d_KB(struct uarch* arch);
int32_t get_l2_KB(struct uarch* arch);

void free_uarch_struct(struct uarch* arch);

#endif
