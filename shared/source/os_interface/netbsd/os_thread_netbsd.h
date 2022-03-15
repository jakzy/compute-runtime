#include "shared/source/os_interface/os_thread.h"

#include <pthread.h>

namespace NEO {
class ThreadNetBSD : public Thread {
  public:
    ThreadNetBSD(pthread_t threadId);
    void join() override;
    void yield() override;
    ~ThreadNetBSD() override = default;

  protected:
    pthread_t threadId;
};
} // namespace NEO