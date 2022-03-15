/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_library.h"

namespace NEO {

#ifdef NET_BSD
namespace NetBSD {
#else
namespace Linux {
#endif

void adjustLibraryFlags(int &dlopenFlag);

class OsLibrary : public NEO::OsLibrary {
  private:
    void *handle;

  public:
    OsLibrary(const std::string &name, std::string *errorValue);
    ~OsLibrary() override;

    bool isLoaded() override;
    void *getProcAddress(const std::string &procName) override;
};
} // namespace Linux/NetBSD
} // namespace NEO
