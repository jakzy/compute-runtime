#define NET_BSD
#include "shared/source/os_interface/linux/sys_calls_linux.cpp"


namespace NEO {

namespace SysCalls {

int getDevicePath(int deviceFd, char *buf, size_t &bufSize) {
    struct stat st;
    // same
    if (::fstat(deviceFd, &st)) {
        return -1;
    }

    snprintf(buf, bufSize, "/sys/dev/char/%d:%d", // ?????????????????????
             major(st.st_rdev), minor(st.st_rdev));

    return 0;
}

} // namespace SysCalls
} // namespace NEO
