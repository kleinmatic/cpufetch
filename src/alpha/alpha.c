#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include "alpha.h"
#include "uarch.h"
#include "../common/global.h"
#include "../common/udev.h"

#define DMESG_BOOT_PATH "/var/run/dmesg.boot"

// Read the 'implver' (implementation version) instruction, which returns
// the coarse Alpha generation: 0=EV4, 1=EV5, 2=EV6, 3=EV7.
static inline uint64_t read_implver(void) {
  uint64_t implver;
  __asm__ volatile ("implver %0" : "=r"(implver));
  return implver;
}

// Read the implemented architecture-extension bits. The 'amask' instruction
// clears, in its input mask, every feature bit the CPU *does* implement and
// returns the rest. So the set of implemented features is ~amask(~0).
static inline uint64_t read_amask_features(void) {
  uint64_t not_implemented;
  __asm__ volatile ("amask %1, %0" : "=r"(not_implemented) : "r"(~0UL));
  return (~not_implemented) & 0xffffUL;
}

// Fetch a string sysctl by name (e.g. "hw.model"). Returns a newly
// allocated string, or NULL on failure.
static char* get_sysctl_string(const char* name) {
  size_t len = 0;
  if(sysctlbyname(name, NULL, &len, NULL, 0) != 0 || len == 0) {
    printWarn("sysctlbyname(%s): unable to determine length", name);
    return NULL;
  }

  char* buf = emalloc(sizeof(char) * len);
  if(sysctlbyname(name, buf, &len, NULL, 0) != 0) {
    printWarn("sysctlbyname(%s): read failed", name);
    free(buf);
    return NULL;
  }

  return buf;
}

static int32_t get_ncores(void) {
  int ncpu;
  size_t len = sizeof(ncpu);
  if(sysctlbyname("hw.ncpu", &ncpu, &len, NULL, 0) != 0) {
    // Fall back to the portable interface
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if(n < 1) {
      printWarn("Unable to determine the number of cores, assuming 1");
      return 1;
    }
    return (int32_t) n;
  }
  return (int32_t) ncpu;
}

// There is no sysctl exposing the CPU clock on NetBSD/alpha, and measuring
// the cycle counter is unreliable under emulation, so we recover the value
// the kernel printed at boot, e.g. "AlphaServer ES40, 357MHz, s/n ".
static int32_t get_frequency_from_dmesg(void) {
  int len;
  char* dmesg = read_file((char*) DMESG_BOOT_PATH, &len);
  if(dmesg == NULL) {
    printWarn("Could not read '%s' to determine frequency", DMESG_BOOT_PATH);
    return UNKNOWN_DATA;
  }

  int32_t freq = UNKNOWN_DATA;
  char* p = strstr(dmesg, "MHz");
  while(p != NULL && freq == UNKNOWN_DATA) {
    // Walk backwards over the digits immediately preceding "MHz"
    char* start = p;
    while(start > dmesg && isdigit((unsigned char) *(start - 1))) start--;
    if(start != p) {
      freq = (int32_t) strtol(start, NULL, 10);
    }
    p = strstr(p + 3, "MHz");
  }

  free(dmesg);
  return freq;
}

struct cache* get_cache_info(struct cpuInfo* cpu) {
  struct cache* cach = emalloc(sizeof(struct cache));
  init_cache_struct(cach);

  int32_t l1i = get_l1i_KB(cpu->arch);
  int32_t l1d = get_l1d_KB(cpu->arch);
  int32_t l2 = get_l2_KB(cpu->arch);

  if(l1i > 0) {
    cach->L1i->size = l1i * 1024;
    cach->L1i->exists = true;
    cach->L1i->num_caches = 1;
    cach->max_cache_level = 1;
  }
  if(l1d > 0) {
    cach->L1d->size = l1d * 1024;
    cach->L1d->exists = true;
    cach->L1d->num_caches = 1;
    cach->max_cache_level = 2;
  }
  if(l2 > 0) {
    cach->L2->size = l2 * 1024;
    cach->L2->exists = true;
    cach->L2->num_caches = 1;
    cach->max_cache_level = 3;
  }

  return cach;
}

struct topology* get_topology_info(struct cache* cach) {
  struct topology* topo = emalloc(sizeof(struct topology));
  init_topology_struct(topo, cach);

  topo->total_cores = get_ncores();
  topo->sockets = 1;
  topo->physical_cores = topo->total_cores;
  topo->logical_cores = topo->total_cores;
  topo->smt_supported = 1;

  return topo;
}

struct frequency* get_frequency_info(void) {
  struct frequency* freq = emalloc(sizeof(struct frequency));

  freq->measured = false;
  freq->base = UNKNOWN_DATA;
  freq->max = get_frequency_from_dmesg();

  return freq;
}

int64_t get_peak_performance(struct cpuInfo* cpu, int32_t freq) {
  if(freq == UNKNOWN_DATA) {
    return -1;
  }

  // Every Alpha generation has independent FP add and FP multiply pipes,
  // giving a peak of 2 FLOP/cycle/core. Peak performance is, as cpufetch
  // notes elsewhere, only a theoretical estimate.
  int64_t flops = (int64_t) cpu->topo->total_cores * (int64_t) freq * 1000000 * 2;
  return flops;
}

struct cpuInfo* get_cpu_info(void) {
  struct cpuInfo* cpu = emalloc(sizeof(struct cpuInfo));
  struct features* feat = emalloc(sizeof(struct features));
  cpu->feat = feat;

  bool* ptr = &(feat->AES);
  for(uint32_t i = 0; i < sizeof(struct features)/sizeof(bool); i++, ptr++) {
    *ptr = false;
  }

  cpu->implver = read_implver();
  cpu->amask = read_amask_features();

  feat->BWX = cpu->amask & ALPHA_AMASK_BWX;
  feat->FIX = cpu->amask & ALPHA_AMASK_FIX;
  feat->CIX = cpu->amask & ALPHA_AMASK_CIX;
  feat->MVI = cpu->amask & ALPHA_AMASK_MVI;
  feat->PAT = cpu->amask & ALPHA_AMASK_PAT;
  feat->PMI = cpu->amask & ALPHA_AMASK_PMI;

  cpu->cpu_name = get_sysctl_string("hw.model");
  cpu->hv = emalloc(sizeof(struct hypervisor));
  cpu->hv->present = false;
  cpu->arch = get_uarch_from_alpha(cpu->implver, cpu->amask);
  cpu->cach = get_cache_info(cpu);
  cpu->topo = get_topology_info(cpu->cach);
  cpu->freq = get_frequency_info();
  cpu->peak_performance = get_peak_performance(cpu, get_freq(cpu->freq));

  return cpu;
}

char* get_str_topology(struct topology* topo, bool dual_socket) {
  UNUSED(dual_socket);
  uint32_t size = 3 + 7 + 1;
  char* string = emalloc(sizeof(char) * size);
  snprintf(string, size, "%d cores", topo->total_cores);
  return string;
}

char* get_str_features(struct cpuInfo* cpu) {
  struct features* feat = cpu->feat;
  char* string = emalloc(sizeof(char) * 48);
  string[0] = '\0';

  bool first = true;
  #define APPEND_FEAT(cond, name) \
    if(cond) { \
      if(!first) strcat(string, ", "); \
      strcat(string, name); \
      first = false; \
    }

  APPEND_FEAT(feat->BWX, "BWX")
  APPEND_FEAT(feat->FIX, "FIX")
  APPEND_FEAT(feat->CIX, "CIX")
  APPEND_FEAT(feat->MVI, "MVI")
  APPEND_FEAT(feat->PAT, "PAT")
  APPEND_FEAT(feat->PMI, "PMI")
  #undef APPEND_FEAT

  if(first) {
    free(string);
    return NULL;
  }

  return string;
}

void print_debug(struct cpuInfo* cpu) {
  printf("implver: %lu\n", (unsigned long) cpu->implver);
  printf("amask (implemented features): 0x%lx\n", (unsigned long) cpu->amask);
}
