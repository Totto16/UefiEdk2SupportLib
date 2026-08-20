
#include <SupportLib/nanosleep.h>


#include <errno.h>


int nanosleep(const struct timespec* __req, struct timespec* __rem) {
    errno = ENOTSUP;
    return -1;
}
