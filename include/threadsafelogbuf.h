#pragma once

// 线程安全的日志缓冲区
// 替代裸 std::stringstream 作为 std::cout / std::cerr 的重定向目标：
// 工作线程（QtConcurrent）写日志、主线程（QTimer 定时器）读+清空时，
// 用同一把 mutex 保护内部 buffer，避免数据竞争（未定义行为）。
//
// 容量护栏：内部 buffer 超过 MAX_CAPACITY 后丢弃【新】日志（保留旧的），
// 防止 PCL 等库的警告刷屏（如 ISS e3c<0 每点一条 PCL_WARN）撑爆内存。

#include <streambuf>
#include <string>
#include <mutex>

class ThreadSafeLogBuf : public std::streambuf
{
public:
    ThreadSafeLogBuf() = default;
    ~ThreadSafeLogBuf() override = default;

    // 容量上限：超出后丢弃新写入（日志不再是无限增长）
    static constexpr std::size_t MAX_CAPACITY = 200 * 1024;   // 200 KB

protected:
    // 单字符写入路径（std::cout 逐字符输出时触发）
    int_type overflow(int_type c) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (c != traits_type::eof() && m_buffer.size() < MAX_CAPACITY)
            m_buffer.push_back(static_cast<char>(c));
        return c;
    }

    // 批量写入路径（"LOG_INFO(...)" 的 << 流的主要输出方式）
    std::streamsize xsputn(const char* s, std::streamsize n) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_buffer.size() < MAX_CAPACITY)
        {
            std::size_t room = MAX_CAPACITY - m_buffer.size();
            m_buffer.append(s, (std::size_t)n < room ? (std::size_t)n : room);
        }
        return n;
    }

public:
    // 取出全部内容并清空（主线程定时器调用）
    std::string take()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string out;
        out.swap(m_buffer);
        return out;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_buffer.empty();
    }

private:
    mutable std::mutex m_mutex;
    std::string m_buffer;
};
