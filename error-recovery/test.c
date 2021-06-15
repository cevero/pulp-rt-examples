/*
 * This example shows how to use the performance API on fabric controller
 * side in order to get performance values out of the HW counters.
 * There are 2 ways to access counters, either directly to get the value 
 * directly out of the HW counter or indirectly where HW counter values are first
 * cumulated to the performance structure and then be read.
 * This example shows the direct way.
 */

#include "rt/rt_api.h"
#include "stdio.h"

static inline void enable_error(int enable)
{ 
  // add zero, r0, r1 is a no-operation instruction
  // because writes to the zero register are ignored

  // This instructions are used by the cevero core to detect when
  // the user wants to enable error insertion or not
  if (enable) asm volatile("add zero, zero, a0");
  else asm volatile("add zero, zero, a1");
}

/* This algorithm is mentioned in the ISO C standard, here extended
   for 32 bits.  */
int rand_r (unsigned int *seed)
{
  unsigned int next = *seed;
  int result;
  next *= 1103515245;
  next += 12345;
  result = (unsigned int) (next / 65536) % 2048;
  next *= 1103515245;
  next += 12345;
  result <<= 10;
  result ^= (unsigned int) (next / 65536) % 1024;
  next *= 1103515245;
  next += 12345;
  result <<= 10;
  result ^= (unsigned int) (next / 65536) % 1024;
  *seed = next;
  return result;
}

// This benchmark is a single shot so we can read the value directly out of the
// HW counter using the function rt_perf_read
static int* do_it(rt_perf_t *perf, int events, int _error)
{
  printf("Generating RANDOM Numbers !\n");
  int seed = 0;
  int* array = rt_alloc(RT_ALLOC_FC_DATA, sizeof(int[10]));

  // Activate specified events
  rt_perf_conf(perf, events);

  // Reset HW counters now and start and stop counters so that we benchmark
  // only around the intended code
  rt_perf_reset(perf);
  rt_perf_start(perf);
  //if (_error) enable_error(1);

  // Benchmark this code

  // Enable FTM
  asm volatile("add zero, zero, a2");

  for (int i = 0; i < 10; i++) {
    int result;
    for (int j = 0; j < 10; j++) {
      if (_error) enable_error(1);
      result = rand_r(&seed);
      if (_error) enable_error(0);
    }
    array[i] = result;
  }

  // Disable FTM
  asm volatile("add zero, zero, a3");

  // End of benchmarking

  //if (_error) enable_error(0);
  rt_perf_stop(perf);

  printf("Random numbers: \n");
  for (int i = 0; i < 10; i++) {
    printf("%d\n", array[i]);
  }

  return array;
}

int main()
{
  // This tructure will hold the configuration and also the results in the
  // cumulative mode
  rt_perf_t perf;

  // It must be initiliazed at least once, this will set all values in the
  // structure to zero.
  rt_perf_init(&perf);

  // To be compatible with all platforms, we can count only 1 event at the
  // same time (the silicon as only 1 HW counter), but the total number of cyles
  // is reported by a timer, we can activate it at the same time.

  printf("\n\n");
  printf(" ==== REFERENCE RUN ====\n");
  int* reference_run = do_it(&perf, (1<<RT_PERF_ACTIVE_CYCLES) | (1<<RT_PERF_INSTR), 0);
  printf("Total cycles: %d\n", rt_perf_read(RT_PERF_ACTIVE_CYCLES));
  printf("Instructions: %d\n", rt_perf_read(RT_PERF_INSTR));

  printf("\n\n");
  printf(" ==== ERROR INSERTION RUN ====\n");
  int* error_run = do_it(&perf, (1<<RT_PERF_ACTIVE_CYCLES) | (1<<RT_PERF_INSTR), 1);
  printf("Total cycles: %d\n", rt_perf_read(RT_PERF_ACTIVE_CYCLES));
  printf("Instructions: %d\n", rt_perf_read(RT_PERF_INSTR));

  printf("\n\n");
  printf(" ==== COMPARE RESULTS ====\n");
  int different = 0;

  for (int i = 0; i < 10; i++) {
    if ( *(error_run + i) != *(reference_run + i) ) {
      printf("COMPARISON ERROR: error_run[%d]:%d != reference_run[%d]:%d\n", i, *(error_run + i), i, *(reference_run + i));
      different = 1;
    }
  }
  
  if (!different) printf("ALL random numbers are equal!!\n");
  return 0;
}
