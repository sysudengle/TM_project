#ifndef __HRTIME_H__
#define __HRTIME_H__

// gethrtime implementation by Kai Shen for x86 Linux

#include <stdio.h>
#include <string.h>

// get the number of CPU cycles per microsecond from Linux /proc filesystem
// return < 0 on error
inline  __attribute__((always_inline))
double getMHZ_x86(void)
{
#ifdef plum
	return 2666.6;
#else

    double mhz = -1;
    char line[1024], *s, search_str[] = "cpu MHz";
    FILE* fp;

    // open proc/cpuinfo
    if ((fp = fopen("/proc/cpuinfo", "r")) == NULL)
        return -1;

    // ignore all lines until we reach MHz information
    while (fgets(line, 1024, fp) != NULL) {
        if (strstr(line, search_str) != NULL) {
            // ignore all characters in line up to :
            for (s = line; *s && (*s != ':'); ++s);
            // get MHz number
            if (*s && (sscanf(s+1, "%lf", &mhz) == 1))
                break;
        }
    }

    if (fp != NULL)
        fclose(fp);
    return mhz;
#endif
}

// get the number of CPU cycles since startup using rdtsc instruction
inline  __attribute__((always_inline))
unsigned long long get_c()
{
    unsigned int tmp[2];

    asm volatile("rdtsc"
         : "=a" (tmp[1]), "=d" (tmp[0])
         : "c" (0x10) );
    return ( ( ((unsigned long long)tmp[0]) << 32 ) | tmp[1]);
}

inline  __attribute__((always_inline))
void _get_c(unsigned int* tmp)
{
    asm volatile("rdtsc" : "=a" (tmp[0]), "=d" (tmp[1]) : "c" (0x10) );
    //return ( ( ((unsigned long long)tmp[0]) << 32 ) | tmp[1]);
}

// get the elapsed time (in nanoseconds) since startup
inline  __attribute__((always_inline))
unsigned long long get_t()
{
    static double CPU_MHZ = 0;
    if (CPU_MHZ == 0)
	{
        CPU_MHZ = getMHZ_x86();
	}
    return (unsigned long long)(get_c() * 1000 / CPU_MHZ);
}

// get the elapsed time (in seconds) since startup
inline  __attribute__((always_inline))
double get_td()
{
    static double CPU_HZ = 0;
    if (CPU_HZ == 0)
	{
        CPU_HZ = getMHZ_x86()*1000000;
	}
    return (double)(get_c() / CPU_HZ);
}

inline  __attribute__((always_inline))
double c_to_t(unsigned long long _c)
{
    static double CPU_HZ = 0;
    if (CPU_HZ == 0)
	{
	CPU_HZ = getMHZ_x86()*1000000;;
	}
    return (_c / CPU_HZ);
}

inline  __attribute__((always_inline))
unsigned long long t_to_c(unsigned int _t)
{
    static double CPU_HZ = 0;
    if (CPU_HZ == 0)
	{
        CPU_HZ = getMHZ_x86()*1000000;;
	}
    return (unsigned long long)(_t * CPU_HZ);
}

#endif // __HRTIME_H__
