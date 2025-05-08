#ifndef BACKTRACE_H
#define BACKTRACE_H

#include <memory>
#include <span>
#include <string>


struct BackTraceEntry
{
	/// @brief ID of the frame from the bt, i.e.: 0, 1, 2...
	int frame;
	/// @brief Source of the function, i.e.: /usr/lib/mylib.so.7
	std::string source;
	/// @brief The symbol corresponding to the call, may be empty or mangled.
	std::string symbol;
	/// @brief offset from the stack.
	int64_t offset;
};

/**
 * @brief Class used to generate backtraces at runtime.
 * If you need to debug stuff at runtime, without a debugger, or for log report,
 * you may want to use this class.
 */
class BackTrace
{
public:
	explicit BackTrace(uint maxFrames);
	BackTrace(const BackTrace &other);
	BackTrace(BackTrace &&other) noexcept;
	~BackTrace();

	std::span<BackTraceEntry> entries;

	void print() const;

	BackTrace &operator =(const BackTrace &other);
	BackTrace &operator =(BackTrace &&other) noexcept;

	static void easyPrint(uint maxFrames);

private:
	std::shared_ptr<BackTraceEntry> bt;
};


#endif // BACKTRACE_H
