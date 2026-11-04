#include "cpu.h"
#include <stdint.h>

int cpu_vendor_id_is_intel(const char vendor_id[13]) {
    static const char intel_vendor_id[13] = "GenuineIntel";
    if (vendor_id == 0) return 0;

    for (uint32_t i = 0; i < 13; i++) {
        if (vendor_id[i] != intel_vendor_id[i]) return 0;
    }
    return 1;
}

int cpu_is_intel(void) {
#if !defined(__x86_64__)
    return 0;
#else
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "0"(0)
        : "memory");

    char vendor_id[13];
    vendor_id[0] = (char)(ebx & 0xffu);
    vendor_id[1] = (char)((ebx >> 8) & 0xffu);
    vendor_id[2] = (char)((ebx >> 16) & 0xffu);
    vendor_id[3] = (char)((ebx >> 24) & 0xffu);
    vendor_id[4] = (char)(edx & 0xffu);
    vendor_id[5] = (char)((edx >> 8) & 0xffu);
    vendor_id[6] = (char)((edx >> 16) & 0xffu);
    vendor_id[7] = (char)((edx >> 24) & 0xffu);
    vendor_id[8] = (char)(ecx & 0xffu);
    vendor_id[9] = (char)((ecx >> 8) & 0xffu);
    vendor_id[10] = (char)((ecx >> 16) & 0xffu);
    vendor_id[11] = (char)((ecx >> 24) & 0xffu);
    vendor_id[12] = '\0';
    return cpu_vendor_id_is_intel(vendor_id);
#endif
}
