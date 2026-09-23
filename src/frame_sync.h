#ifndef JP_FRAME_SYNC_H
#define JP_FRAME_SYNC_H

#include <atomic>
#include <condition_variable>
#include <mutex>

/**
 * @brief Lockstep handshake between the graphics thread (consumer) and
 *        one worker thread (producer, e.g. physics).
 *
 * Frame loop, graphics side:
 *     sync.requestFrame();
 *     sync.waitForFrameReady();
 *     // ... now safe to read worker output ...
 *
 * Frame loop, worker side:
 *     sync.waitForFrameRequest();
 *     // ... produce a frame, write output ...
 *     sync.signalFrameReady();
 *
 * stop() wakes both sides. isStopped() is cheap (atomic load) so both
 * loops can poll it once per iteration without contention.
 */
class FrameSync
{
public:
    void requestFrame()
    {
        {
            std::unique_lock lk(m_mtx);
            m_frameRequested = true;
            m_frameReady = false;
        }
        m_cv.notify_all();
    }

    void waitForFrameReady()
    {
        std::unique_lock lk(m_mtx);
        m_cv.wait(lk, [this] { return m_frameReady || m_stopped.load(); });
    }

    void waitForFrameRequest()
    {
        std::unique_lock lk(m_mtx);
        m_cv.wait(lk, [this] { return m_frameRequested || m_stopped.load(); });
        m_frameRequested = false;
    }

    void signalFrameReady()
    {
        {
            std::unique_lock lk(m_mtx);
            m_frameReady = true;
        }
        m_cv.notify_all();
    }

    void stop()
    {
        m_stopped.store(true);
        m_cv.notify_all();
    }

    [[nodiscard]] auto isStopped() const noexcept
    {
        return m_stopped.load(std::memory_order_acquire);
    }

private:
    std::mutex m_mtx;
    std::condition_variable m_cv;
    bool m_frameRequested = false;
    bool m_frameReady = false;
    std::atomic<bool> m_stopped = false;
};

#endif // JP_FRAME_SYNC_H
