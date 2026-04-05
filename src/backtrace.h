#ifndef BACKTRACE_H
#define BACKTRACE_H

#include <memory>
#include <span>
#include <string>


/**
 * @brief Decoded information for one stack frame.
 */
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
    /// @brief Captures up to maxFrames stack frames starting from parent caller.
    explicit BackTrace(const uint maxFrames);
    /// @brief Copy constructor.
    BackTrace(const BackTrace &other) = default;
    /// @brief Move constructor.
    BackTrace(BackTrace &&other) noexcept;
    /// @brief Default destructor.
    ~BackTrace() = default;

    /// @brief Prints the captured backtrace to standard output.
    void print() const;

    /// @brief Copy assignment.
    auto operator=(const BackTrace &other) -> BackTrace &;
    /// @brief Move assignment.
    auto operator=(BackTrace &&other) noexcept -> BackTrace &;

    /// @brief Captures and prints a backtrace in a single call.
    static void easyPrint(const uint maxFrames);

    /// @brief Returns the captured backtrace entries.
    inline auto getEntries() const -> std::span<BackTraceEntry> { return m_entries; }

private:
    /// @brief Owning storage for captured entries.
    std::shared_ptr<BackTraceEntry> m_bt;
    /// @brief View over the captured entries.
    std::span<BackTraceEntry> m_entries;
};


#endif // BACKTRACE_H
