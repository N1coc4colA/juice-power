#ifndef BACKTRACE_H
#define BACKTRACE_H

#include <memory>
#include <span>
#include <string>


struct BackTraceEntry
{
	/// @brief ID of the frame from the bt, i.e.: 0, 1, 2...
    int frame = -1;
    /// @brief Source of the function, i.e.: /usr/lib/mylib.so.7
    std::string source;
    /// @brief The symbol corresponding to the call, may be empty or mangled.
    std::string symbol;
    /// @brief offset from the stack.
    int64_t offset = -1;
};

/**
 * @brief Class used to generate backtraces at runtime.
 * If you need to debug stuff at runtime, without a debugger, or for log report,
 * you may want to use this class.
 */
class BackTrace
{
public:
    explicit BackTrace(const uint maxFrames);
    BackTrace(const BackTrace &other) = default;
    BackTrace(BackTrace &&other) noexcept;
    ~BackTrace() = default;

    void print() const;

    auto operator=(const BackTrace &other) -> BackTrace &;
    auto operator=(BackTrace &&other) noexcept -> BackTrace &;

    static void easyPrint(const uint maxFrames);

    inline auto getEntries() -> std::span<BackTraceEntry> { return m_entries; }

private:
    std::shared_ptr<BackTraceEntry> m_bt;
    std::span<BackTraceEntry> m_entries;
};


#endif // BACKTRACE_H
