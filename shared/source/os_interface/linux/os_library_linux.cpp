/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef NET_BSD
    #include "shared/source/os_interface/netbsd/os_library_netbsd.h"
#else
    #include "shared/source/os_interface/linux/os_library_linux.h"
#endif
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include <dlfcn.h>

namespace NEO {
OsLibrary *OsLibrary::load(const std::string &name) {
    return load(name, nullptr);
}

OsLibrary *OsLibrary::load(const std::string &name, std::string *errorValue) {
    
    #ifdef NET_BSD
    auto ptr = new (std::nothrow) NetBSD::OsLibrary(name, errorValue);
    #else
    auto ptr = new (std::nothrow) Linux::OsLibrary(name, errorValue);
    #endif

    if (ptr == nullptr)
        return nullptr;

    if (!ptr->isLoaded()) {
        delete ptr;
        return nullptr;
    }
    return ptr;
}

const std::string OsLibrary::createFullSystemPath(const std::string &name) {
    return name;
}

#ifdef NET_BSD
namespace NetBSD {
#else
namespace Linux {
#endif

OsLibrary::OsLibrary(const std::string &name, std::string *errorValue) {
    if (name.empty()) {
        this->handle = SysCalls::dlopen(0, RTLD_LAZY);
    } else {
#ifdef SANITIZER_BUILD
        auto dlopenFlag = RTLD_LAZY;
#else
        auto dlopenFlag = RTLD_LAZY | RTLD_DEEPBIND;
        /* Background: https://github.com/intel/compute-runtime/issues/122 */
#endif
        adjustLibraryFlags(dlopenFlag);
        this->handle = SysCalls::dlopen(name.c_str(), dlopenFlag);
        if (!this->handle && (errorValue != nullptr)) {
            errorValue->assign(dlerror());
        }
    }
}

OsLibrary::~OsLibrary() {
    if (this->handle != nullptr) {
        dlclose(this->handle);
        this->handle = nullptr;
    }
}

bool OsLibrary::isLoaded() {
    return this->handle != nullptr;
}

void *OsLibrary::getProcAddress(const std::string &procName) {
    DEBUG_BREAK_IF(this->handle == nullptr);

    return dlsym(this->handle, procName.c_str());
}
} // namespace Linux
} // namespace NEO
