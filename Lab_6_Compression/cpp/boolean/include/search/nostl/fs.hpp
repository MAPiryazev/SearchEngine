#pragma once

#include <cstddef>
#include <cstring>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace nostl {

inline int mkdir_p(const char* path) {
    if (!path || !*path) return 0;

    char buf[4096];
    std::size_t len = std::strlen(path);
    if (len >= sizeof(buf)) return -1;

    std::memcpy(buf, path, len + 1);
    if (len && buf[len - 1] == '/') buf[len - 1] = 0;

    for (char* p = buf + 1; *p; ++p) {
        if (*p == '/') {
            *p = 0;
            if (::mkdir(buf, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (::mkdir(buf, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

}
