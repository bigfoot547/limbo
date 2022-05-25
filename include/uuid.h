#ifndef LIMBO_UUID_H_INCLUDED
#define LIMBO_UUID_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* UUID high long:
    FFFFFFFF00000000 - time_low
    00000000FFFF0000 - time_mid
    000000000000F000 - version
    0000000000000FFF - time_hi
   low long:
    C000000000000000 - variant
    3FFF000000000000 - clock_seq
    0000FFFFFFFFFFFF - node
 */

struct uuid {
    uint64_t mostsig, leastsig;
};

int uuid_version(struct uuid *id);

void uuid_gen_name(struct uuid *dest, const char *ns, const char *name);
int uuid_parse(struct uuid *id, const char *str, bool lenient);
int uuid_format(struct uuid *id, char *str, size_t len);

#define UUID_STRLEN (36)

#endif // include guard
