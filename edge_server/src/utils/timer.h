#pragma once

#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>


class Timer
{
public:
    using Callback = std::function<void()>;

    Timer();
    ~Timer();
    // 禁止拷贝
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    // 启动定时器 
    // immediate: 是否立马执行, true为立马执行
    // repeat: 是否循环，false只执行一次
    void start(std::chrono::milliseconds interval, Callback callback, bool immediate = true, bool repeat = true);

    // 停止定时器
    void stop();

    // 是否运行
    bool isRunning() const;

private:
    void runLoop();

private:
    std::chrono::milliseconds m_interval;
    std::atomic<bool> m_repeat{false};
    std::atomic<bool> m_running{false};
    bool m_immediate{false};  // 是否立马执行
    Callback m_callback;
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};
