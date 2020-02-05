// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the Apache 2.0 License.
#pragma once

#include "tls.h"

#include <assert.h>
#include <string.h>

// Adapted from:
// https://software.intel.com/en-us/articles/intel-digital-random-number-generator-drng-software-implementation-guide

#define DRNG_NO_SUPPORT 0x0
#define DRNG_HAS_RDRAND 0x1
#define DRNG_HAS_RDSEED 0x2

// `It is recommended that applications attempt 10 retries in a tight loop in
// the unlikely event that the RDRAND instruction does not return a random
// number. This number is based on a binomial probability argument: given
// the design margins of the DRNG, the odds of ten failures in a row are
// astronomically small and would in fact be an indication of a larger CPU
// issue.`
#define RDRAND_RETRIES 10

typedef struct cpuid_struct
{
  unsigned int eax;
  unsigned int ebx;
  unsigned int ecx;
  unsigned int edx;
} cpuid_t;

void cpuid(cpuid_t* info, unsigned int leaf, unsigned int subleaf)
{
  asm volatile(
    "cpuid"
    : "=a"(info->eax), "=b"(info->ebx), "=c"(info->ecx), "=d"(info->edx)
    : "a"(leaf), "c"(subleaf));
}

int _is_intel_cpu()
{
  static int intel_cpu = -1;
  cpuid_t info;

  if (intel_cpu == -1)
  {
    cpuid(&info, 0, 0);

    if (
      memcmp((char*)&info.ebx, "Genu", 4) ||
      memcmp((char*)&info.edx, "ineI", 4) ||
      memcmp((char*)&info.ecx, "ntel", 4))
      intel_cpu = 0;
    else
      intel_cpu = 1;
  }

  return intel_cpu;
}

int get_drng_support()
{
  static int drng_features = -1;

  /* So we don't call cpuid multiple times for the same information */

  if (drng_features == -1)
  {
    drng_features = DRNG_NO_SUPPORT;

    if (_is_intel_cpu())
    {
      cpuid_t info;

      cpuid(&info, 1, 0);

      if ((info.ecx & 0x40000000) == 0x40000000)
        drng_features |= DRNG_HAS_RDRAND;

      cpuid(&info, 7, 0);

      if ((info.ebx & 0x40000) == 0x40000)
        drng_features |= DRNG_HAS_RDSEED;
    }
  }

  return drng_features;
}

int rdrand16_step(uint16_t* rand)
{
  unsigned char ok;
  asm volatile("rdrand %0; setc %1" : "=r"(*rand), "=qm"(ok));
  return (int)ok;
}

int rdrand32_step(uint32_t* rand)
{
  unsigned char ok;
  asm volatile("rdrand %0; setc %1" : "=r"(*rand), "=qm"(ok));
  return (int)ok;
}

int rdrand64_step(uint64_t* rand)
{
  unsigned char ok;
  asm volatile("rdrand %0; setc %1" : "=r"(*rand), "=qm"(ok));
  return (int)ok;
}

int rdrand16_retry(unsigned int retries, uint16_t* rand)
{
  unsigned int count = 0;

  while (count <= retries)
  {
    if (rdrand16_step(rand))
      return 1;
    ++count;
  }

  return 0;
}

int rdrand32_retry(unsigned int retries, uint32_t* rand)
{
  unsigned int count = 0;

  while (count <= retries)
  {
    if (rdrand32_step(rand))
      return 1;

    ++count;
  }

  return 0;
}

int rdrand64_retry(unsigned int retries, uint64_t* rand)
{
  unsigned int count = 0;

  while (count <= retries)
  {
    if (rdrand64_step(rand))
      return 1;

    ++count;
  }

  return 0;
}

unsigned int rdrand_get_bytes(unsigned int n, unsigned char* dest)
{
  unsigned char *headstart, *tailstart = NULL;
  uint64_t* blockstart;
  unsigned int count, ltail, lhead, lblock;
  uint64_t i, temprand;

  /* Get the address of the first 64-bit aligned block in the
   * destination buffer. */
  headstart = dest;
  if (((uint64_t)headstart % (uint64_t)8) == 0)
  {
    blockstart = (uint64_t*)headstart;
    lblock = n;
    lhead = 0;
  }
  else
  {
    blockstart =
      (uint64_t*)(((uint64_t)headstart & ~(uint64_t)7) + (uint64_t)8);
    lhead = (unsigned int)((uint64_t)blockstart - (uint64_t)headstart);
    lblock = ((n - lhead) & ~(unsigned int)7); // cwinter: this bit is/as buggy
                                               // in the Intel examples.
  }

  /* Compute the number of 64-bit blocks and the remaining number
   * of bytes (the tail) */
  ltail = n - lblock - lhead;
  count = lblock / 8; /* The number 64-bit rands needed */

  assert(lhead < 8);
  assert(lblock <= n);
  assert(ltail < 8);

  if (ltail)
    tailstart = (unsigned char*)((uint64_t)blockstart + (uint64_t)lblock);

  /* Populate the starting, mis-aligned section (the head) */
  if (lhead)
  {
    if (!rdrand64_retry(RDRAND_RETRIES, &temprand))
      return 0;
    memcpy(headstart, &temprand, lhead);
  }

  /* Populate the central, aligned block */
  for (i = 0; i < count; ++i, ++blockstart)
  {
    if (!rdrand64_retry(RDRAND_RETRIES, blockstart))
      return i * 8 + lhead;
  }

  /* Populate the tail */
  if (ltail)
  {
    if (!rdrand64_retry(RDRAND_RETRIES, &temprand))
      return count * 8 + lhead;
    memcpy(tailstart, &temprand, ltail);
  }

  return n;
}

// The following three functions should be used to generate
// randomness that will be used as seed for another RNG
int rdseed16_step(uint16_t* seed)
{
  unsigned char ok;
  asm volatile("rdseed %0; setc %1" : "=r"(*seed), "=qm"(ok));
  return (int)ok;
}

int rdseed32_step(uint32_t* seed)
{
  unsigned char ok;
  asm volatile("rdseed %0; setc %1" : "=r"(*seed), "=qm"(ok));
  return (int)ok;
}

int rdseed64_step(uint64_t* seed)
{
  unsigned char ok;
  asm volatile("rdseed %0; setc %1" : "=r"(*seed), "=qm"(ok));
  return (int)ok;
}
