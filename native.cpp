#include <cstdio>

extern "C"
void put_int(long long val)
{
    printf("%lld\n", val);
}