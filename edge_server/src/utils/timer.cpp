#include "utils/timer.h"

Timer::Timer()
{
}

Timer::~Timer()
{
    stop();
}

void Timer::start(std::chrono::milliseconds interval, Callback callback, bool immediate, bool repeat)
{
    if (m_thread.joinable()) 
    {
        // 先尝试停止（外部调用会 join，内部调用会设置标志并唤醒）
        stop();
        // 如果仍然 joinable，说明未能 join（内部调用），此时禁止继续
        if (m_thread.joinable()) {
            throw std::runtime_error("Cannot restart timer from its own callback");
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_interval = interval;
        m_callback = std::move(callback);
        m_repeat = repeat;
        m_running = true;
        m_immediate = immediate;
    }
    
    m_thread = std::thread(&Timer::runLoop, this);
}

void Timer::stop()
{
    m_running = false;    
    m_cv.notify_all();

    if(m_thread.joinable() && std::this_thread::get_id() != m_thread.get_id())
    {
        m_thread.join();
    }
    else
    {
        m_thread.detach();
    }
}

bool Timer::isRunning() const
{
    return m_running.load();
}

void Timer::runLoop()
{
    while (m_running)
    {
        if(m_immediate)
        {
            m_immediate = false;

            auto callback = m_callback;

            if(callback)
            {
                try
                {
                    callback();
                }
                catch(...)
                {

                }
            }

            if(!m_repeat)
            {
                m_running=false;
                break;
            }
        }

        std::unique_lock<std::mutex> lock(m_mutex);
        bool stopped = m_cv.wait_for(
                        lock,
                        m_interval,
                        [this]()
                        {
                            return !m_running;
                        });

        if(stopped)
            break;
        
        auto callback = m_callback;
        lock.unlock();

        if(callback)
        {
            try
            {
                callback();
            }
            catch(...)
            {

            }
        }

        if(!m_repeat)
        {
            m_running=false;
            break;
        }
    }
}
