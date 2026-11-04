#ifndef VOS_CPU_H
#define VOS_CPU_H

/* The target currently accepts Intel x86-64 CPUs only. */
int cpu_is_intel(void);

/* Pure helper kept separate so the vendor check can be tested without CPUID. */
int cpu_vendor_id_is_intel(const char vendor_id[13]);

#endif
