#include "src/backtrace.h"

#include <fmt/printf.h>

#include <gsl-lite/gsl-lite.hpp>

#include <cassert>
#include <cxxabi.h>
#include <execinfo.h>
#include <iostream>
#include <span>
#include <vector>

#include "src/pointer.h"

void BackTrace::easyPrint(const uint maxFrames)
{
	assert(maxFrames != 0);

	// Array to store the stack frames
    std::vector<void *> addressesList(maxFrames + 1);

    // Get the backtrace frames
    const auto framesCount = backtrace(addressesList.data(), static_cast<int>(addressesList.size()));

    if (!framesCount) {
        std::cerr << "No stack trace available\n";
        return;
    }

    // Get symbolic names for the frames
    const auto symbolsListPtr = AutoFreePtr(backtrace_symbols(addressesList.data(), framesCount));
    std::span symbolsList(symbolsListPtr, framesCount);

    for (int i = 1; i < framesCount; i++) { // Skip the first frame as it is this function itself
        char *beginName = nullptr;
        char *beginOffset = nullptr;
        char *endOffset = nullptr;

        // Find parentheses and the + offset surrounded by parentheses
        for (char p : std::span(symbolsList[i], std::strlen(symbolsList[i]))) {
            if (p == '(') {
                beginName = &p;
            } else if (p == '+') {
                beginOffset = &p;
            } else if (p == ')') {
                endOffset = &p;
                break;
            }
        }

        if (beginName && beginOffset && endOffset && beginName < beginOffset) {
            *beginName++ = '\0';
            *beginOffset++ = '\0';
            *endOffset = '\0';

            // Demangle the function name
            int status = -4;
            auto funcName = AutoFreePtr(abi::__cxa_demangle(beginName, nullptr, nullptr, &status));

            if (!status) {
                fmt::print("[{}] {} : {}+{}\n", i, symbolsList[i], funcName.get(), beginOffset);
            } else {
                // If demangling failed, print the mangled function name
                fmt::print("[{}] {} : {}+{}\n", i, symbolsList[i], beginName, beginOffset);
            }
        } else {
            // Print the whole line if no parentheses or + sign found
            fmt::print("[{}] {}\n", i, symbolsList[i]);
        }
    }
}

auto backtraceEntries(const uint maxFrames) -> std::vector<BackTraceEntry>
{
    assert(maxFrames > 0);

    // Array to store the stack frames
    std::vector<void *> addressesList(maxFrames + 1);

    // Get the backtrace frames
    const int framesCount = backtrace(addressesList.data(), static_cast<int>(addressesList.size()));

    if (!framesCount) {
        std::cout << "Failed to generate required BT.\n";

        return {};
    }

    // Get symbolic names for the frames
    const AutoFreePtr symbolsListPtr(backtrace_symbols(addressesList.data(), framesCount));
    const std::span symbolsList(symbolsListPtr, framesCount);

    auto entries = std::vector<BackTraceEntry>(framesCount);

    for (int i = 1; i < framesCount; i++) { // Skip the first frame as it is this function itself
        char *beginName = nullptr;
        char *beginOffset = nullptr;
        char *endOffset = nullptr;

        // Find parentheses and the + offset surrounded by parentheses
        for (char p : std::span(symbolsList[i], std::strlen(symbolsList[i]))) {
            if (p == '(') {
                beginName = &p;
            } else if (p == '+') {
                beginOffset = &p;
            } else if (p == ')') {
                endOffset = &p;
                break;
            }
        }

        const int j = i - 1;
        entries[j].frame = j;
        entries[j].source = symbolsList[i];

        if (beginName && beginOffset && endOffset && beginName < beginOffset) {
            *beginName++ = '\0';
            *beginOffset++ = '\0';
            *endOffset = '\0';

            // Demangle the function name
            int status = -4;
            const auto funcName = AutoFreePtr(abi::__cxa_demangle(beginName, nullptr, nullptr, &status));

            entries[j].offset = std::stoll(beginOffset);

            if (!status) {
                //printf("[%d] %s : %s+%s\n", i, symbollist[i], func_name, begin_offset);

                entries[j].symbol = funcName;
            } else {
                // If demangling failed, print the mangled function name
                //printf("[%d] %s : %s+%s\n", i, symbollist[i], begin_name, begin_offset);

                entries[j].symbol = beginName;
            }
        } else {
            // Print the whole line if no parentheses or + sign found
            //printf("[%d] %s\n", i, symbollist[i]);
            entries[j].offset = -1;
        }
    }

    return entries;
}

BackTrace::BackTrace(const uint maxFrames)
    : m_entries(backtraceEntries(maxFrames))
    , m_bt(m_entries.data(), [](const gsl::owner<BackTraceEntry *> d) -> void { delete[] d; })
{
	assert(maxFrames != 0);
}

BackTrace::BackTrace(BackTrace &&other) noexcept
    : m_entries(std::move(other.m_entries))
    , m_bt(std::move(other.m_bt))
{}

void BackTrace::print() const
{
    for (const auto & [frame, source, symbol, offset] : m_entries) {
        std::cout << '[' << frame << "] " << source << " : " << symbol << std::hex << std::showpos << std::showbase << offset << '\n';
    }
}

auto BackTrace::operator=(const BackTrace &other) -> BackTrace &
{
	if (&other == this) {
		return *this;
	}

    m_entries = other.m_entries;
    m_bt = other.m_bt;

	return *this;
}

auto BackTrace::operator=(BackTrace &&other) noexcept -> BackTrace &
{
	if (&other == this) {
		return *this;
	}

    m_entries = other.m_entries;
    m_bt = std::move(other.m_bt);

	return *this;
}
