#include "shared/source/os_interface/netbsd/os_thread_netbsd.h"

#include <sched.h>

namespace NEO {
ThreadNetBSD::ThreadNetBSD(pthread_t threadId) : threadId(threadId){};

std::unique_ptr<Thread> Thread::create(void *(*func)(void *), void *arg) {
    pthread_t threadId;
    pthread_create(&threadId, nullptr, func, arg);
    return std::unique_ptr<Thread>(new ThreadNetBSD(threadId));
}

void ThreadNetBSD::join() {
    pthread_join(threadId, nullptr);
}

void ThreadNetBSD::yield() {
    sched_yield();
}

} // namespace NEO