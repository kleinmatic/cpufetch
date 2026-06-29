#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uarch.h"
#include "alpha.h"
#include "../common/global.h"

typedef uint32_t MICROARCH;

// Unknown manufacturing process / unknown cache size
#define UNK -1

enum {
  UARCH_UNKNOWN,
  UARCH_EV4,    // 21064
  UARCH_EV45,   // 21064A
  UARCH_EV5,    // 21164
  UARCH_EV56,   // 21164A
  UARCH_PCA56,  // 21164PC
  UARCH_EV6,    // 21264
  UARCH_EV67,   // 21264A
  UARCH_EV68,   // 21264B/21264C
  UARCH_EV7,    // 21364
  UARCH_EV79,   // 21364A
};

struct uarch {
  MICROARCH uarch;
  char* uarch_str; // e.g. "EV68 (21264C)"
  int32_t process; // manufacturing process, in nanometers
  int32_t l1i;     // L1 instruction cache, in KB (UNK if unknown)
  int32_t l1d;     // L1 data cache, in KB (UNK if unknown)
  int32_t l2;      // on-chip L2/Scache, in KB (UNK if external/unknown)
};

static void fill_uarch(struct uarch* arch, MICROARCH u, const char* str,
                       int32_t process, int32_t l1i, int32_t l1d, int32_t l2) {
  arch->uarch = u;
  arch->uarch_str = emalloc(sizeof(char) * (strlen(str) + 1));
  strcpy(arch->uarch_str, str);
  arch->process = process;
  arch->l1i = l1i;
  arch->l1d = l1d;
  arch->l2 = l2;
}

/*
 * Alpha microarchitectures are identified by the 'implver' instruction,
 * which returns a coarse generation number, refined by the feature bits
 * returned by 'amask':
 *
 *   implver=0  EV4 family   21064 / 21064A
 *   implver=1  EV5 family   21164 (none), 21164A (BWX), 21164PC (BWX+MVI)
 *   implver=2  EV6 family   21264 (BWX+FIX+MVI), 21264A (+CIX), 21264C (+PAT+PMI)
 *   implver=3  EV7 family   21364
 *
 * Within a generation, 'amask' cannot always distinguish every silicon
 * stepping (e.g. EV68A vs EV68CB), so we report the generation-level part.
 * References: Alpha Architecture Reference Manual (implver/amask), and the
 * NetBSD alpha cpu_attach feature decoding.
 */
struct uarch* get_uarch_from_alpha(uint64_t implver, uint64_t amask_features) {
  struct uarch* arch = emalloc(sizeof(struct uarch));

  bool bwx = amask_features & ALPHA_AMASK_BWX;
  bool cix = amask_features & ALPHA_AMASK_CIX;
  bool mvi = amask_features & ALPHA_AMASK_MVI;
  bool pat = amask_features & ALPHA_AMASK_PAT;
  bool pmi = amask_features & ALPHA_AMASK_PMI;

  switch(implver) {
    case 0: // EV4 family. amask reports no features for either chip.
      // 21064 and 21064A are indistinguishable via amask; report 21064A
      // since it is the more common variant still in service.
      fill_uarch(arch, UARCH_EV45, "EV45 (21064A)", 500, 16, 16, UNK);
      break;
    case 1: // EV5 family
      if(bwx && mvi)
        fill_uarch(arch, UARCH_PCA56, "PCA56 (21164PC)", 350, 16, 8, UNK);
      else if(bwx)
        fill_uarch(arch, UARCH_EV56, "EV56 (21164A)", 350, 8, 8, 96);
      else
        fill_uarch(arch, UARCH_EV5, "EV5 (21164)", 500, 8, 8, 96);
      break;
    case 2: // EV6 family
      if(pat || pmi)
        fill_uarch(arch, UARCH_EV68, "EV68 (21264C)", 180, 64, 64, UNK);
      else if(cix)
        fill_uarch(arch, UARCH_EV67, "EV67 (21264A)", 250, 64, 64, UNK);
      else
        fill_uarch(arch, UARCH_EV6, "EV6 (21264)", 350, 64, 64, UNK);
      break;
    case 3: // EV7 family
      fill_uarch(arch, UARCH_EV7, "EV7 (21364)", 180, 64, 64, 1792);
      break;
    default:
      printBug("Unknown Alpha implver value: %lu", (unsigned long) implver);
      fill_uarch(arch, UARCH_UNKNOWN, STRING_UNKNOWN, UNK, UNK, UNK, UNK);
      break;
  }

  return arch;
}

char* get_str_uarch(struct cpuInfo* cpu) {
  return cpu->arch->uarch_str;
}

char* get_str_process(struct cpuInfo* cpu) {
  char* str = emalloc(sizeof(char) * (strlen(STRING_UNKNOWN) + 1));
  int32_t process = cpu->arch->process;

  if(process == UNK) {
    snprintf(str, strlen(STRING_UNKNOWN) + 1, STRING_UNKNOWN);
  }
  else if(process > 0) {
    free(str);
    str = emalloc(sizeof(char) * 8);
    sprintf(str, "%dnm", process);
  }
  else {
    snprintf(str, strlen(STRING_UNKNOWN) + 1, STRING_UNKNOWN);
    printBug("Found invalid process: '%d'", process);
  }

  return str;
}

int32_t get_l1i_KB(struct uarch* arch) { return arch->l1i; }
int32_t get_l1d_KB(struct uarch* arch) { return arch->l1d; }
int32_t get_l2_KB(struct uarch* arch)  { return arch->l2;  }

void free_uarch_struct(struct uarch* arch) {
  free(arch->uarch_str);
  free(arch);
}
