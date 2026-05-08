/* runtime.c — built-in implementations linked into every competitive-lang
 * executable. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----- I/O ----- */

void __rt_print_int(long long x) { printf("%lld\n", x); }
void __rt_print_float(double x) { printf("%g\n", x); }
void __rt_print_str(const char *s) { printf("%s\n", s); }
void __rt_print_bool(long long x) { printf("%s\n", x ? "true" : "false"); }

long long __rt_read_int(void) {
    long long x;
    if (scanf("%lld", &x) != 1)
        x = 0;
    return x;
}
double __rt_read_float(void) {
    double x;
    if (scanf("%lf", &x) != 1)
        x = 0.0;
    return x;
}
/* read(string): caller passes a fixed 256-byte buffer (allocated by codegen for
 * string locals). */
void __rt_read_str(char *buf) {
    if (scanf("%255s", buf) != 1)
        buf[0] = '\0';
}

/* ----- Sorting and searching ----- */

static int __rt_cmp_ll(const void *a, const void *b) {
    long long x = *(const long long *)a, y = *(const long long *)b;
    return (x > y) - (x < y);
}
void __rt_sort_int(long long *a, long long n) {
    if (n > 1)
        qsort(a, (size_t)n, sizeof(long long), __rt_cmp_ll);
}
void __rt_reverse_int(long long *a, long long n) {
    for (long long i = 0, j = n - 1; i < j; ++i, --j) {
        long long t = a[i];
        a[i] = a[j];
        a[j] = t;
    }
}
long long __rt_binary_search_int(long long *a, long long n, long long key) {
    long long lo = 0, hi = n;
    while (lo < hi) {
        long long m = (lo + hi) / 2;
        if (a[m] < key)
            lo = m + 1;
        else
            hi = m;
    }
    return (lo < n && a[lo] == key) ? lo : -1;
}

/* ----- Arithmetic helpers ----- */

long long __rt_gcd(long long a, long long b) {
    if (a < 0)
        a = -a;
    if (b < 0)
        b = -b;
    while (b) {
        long long t = a % b;
        a = b;
        b = t;
    }
    return a;
}
long long __rt_lcm(long long a, long long b) {
    if (a == 0 || b == 0)
        return 0;
    return (a / __rt_gcd(a, b)) * b;
}
long long __rt_max_int(long long a, long long b) { return a > b ? a : b; }
long long __rt_min_int(long long a, long long b) { return a < b ? a : b; }
long long __rt_abs_int(long long x) { return x < 0 ? -x : x; }
double __rt_max_float(double a, double b) { return a > b ? a : b; }
double __rt_min_float(double a, double b) { return a < b ? a : b; }
double __rt_abs_float(double x) { return x < 0 ? -x : x; }

/* ----- Swap (8-byte slot, type-erased) ----- */

void __rt_swap_i64(long long *a, long long *b) {
    long long t = *a;
    *a = *b;
    *b = t;
}

/* ----- Strings ----- */

long long __rt_strlen(const char *s) { return (long long)strlen(s); }
long long __rt_streq(const char *a, const char *b) { return strcmp(a, b) == 0; }

/* Concat into a caller-supplied 256-byte destination buffer. Truncates if too
 * long. */
void __rt_strconcat(char *dst, const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    if (la > 255)
        la = 255;
    if (la + lb > 255)
        lb = 255 - la;
    memcpy(dst, a, la);
    memcpy(dst + la, b, lb);
    dst[la + lb] = '\0';
}
