// core::Thread, Mutex and SyncEvent over pthreads (inlined into AudioEngineAL.cpp in the
// original, 0x080f5c50-0x080f72d0).
#pragma once
#include <pthread.h>

namespace core
{

class Mutex
{
  public:
    Mutex()
    {
        pthread_mutex_init(&m_mutex, 0);
    }
    ~Mutex()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    void lock()
    {
        pthread_mutex_lock(&m_mutex);
    }
    void unlock()
    {
        pthread_mutex_unlock(&m_mutex);
    }

  private:
    pthread_mutex_t m_mutex;
};

// Condition variable with a sticky signalled flag (0x4c bytes in the original).
class SyncEvent
{
  public:
    SyncEvent() : m_signalled(false)
    {
        pthread_cond_init(&m_cond, 0);
        pthread_mutex_init(&m_mutex, 0);
    }
    ~SyncEvent()
    {
        pthread_cond_destroy(&m_cond);
        pthread_mutex_destroy(&m_mutex);
    }
    void signal()
    {
        pthread_mutex_lock(&m_mutex);
        m_signalled = true;
        pthread_mutex_unlock(&m_mutex);
        pthread_cond_signal(&m_cond);
    }
    void wait()
    {
        pthread_mutex_lock(&m_mutex);
        while (!m_signalled)
            pthread_cond_wait(&m_cond, &m_mutex);
        m_signalled = false;
        pthread_mutex_unlock(&m_mutex);
    }

  private:
    pthread_cond_t m_cond;
    pthread_mutex_t m_mutex;
    bool m_signalled;
};

class Thread
{
  public:
    Thread() : m_thread(0) {}
    virtual ~Thread() {}
    virtual void run() = 0;
    // Returns false when the thread could not be created.
    bool start()
    {
        return pthread_create(&m_thread, 0, threadFunc, this) == 0;
    }
    void join()
    {
        if (m_thread)
        {
            pthread_join(m_thread, 0);
            m_thread = 0;
        }
    }
    // 0x080f72d0
    static void* threadFunc(void* arg)
    {
        ((Thread*)arg)->run();
        return 0;
    }

  protected:
    pthread_t m_thread;
};

} // namespace core
