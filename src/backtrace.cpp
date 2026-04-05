#include "backtrace.h"

#include <cassert>
#include <cstdlib>
#include <cxxabi.h>
#include <execinfo.h>
#include <fmt/printf.h>
#include <iostream>
#include <span>
#include <vector>

#include <gsl-lite/gsl-lite.hpp>

#include "pointer.h"

void BackTrace::easyPrint(const uint maxFrames)
{
	assert(maxFrames != 0);

	// Array to store the stack frames
    std::vector<void *> addrlist(maxFrames + 1);

    // Get the backtrace frames
    const auto num_frames = backtrace(addrlist.data(), static_cast<int>(addrlist.size()));

    if (!num_frames) {
        std::cerr << "No stack trace available\n";
        return;
    }

    // Get symbolic names for the frames
    AutoFreePtr symbollist_ = gsl::owner<char **>(backtrace_symbols(addrlist.data(), num_frames));
    std::span symbollist(symbollist_, num_frames);

    for (int i = 1; i < num_frames; i++) { // Skip the first frame as it is this function itself
        char *begin_name = nullptr;
        char *begin_offset = nullptr;
        char *end_offset = nullptr;

        // Find parentheses and the + offset surrounded by parentheses
        for (char p : std::span(symbollist[i], std::strlen(symbollist[i]))) {
            if (p == '(') {
                begin_name = &p;
            } else if (p == '+') {
                begin_offset = &p;
            } else if (p == ')') {
                end_offset = &p;
                break;
            }
        }

        if (begin_name && begin_offset && end_offset && begin_name < begin_offset) {
            *begin_name++ = '\0';
            *begin_offset++ = '\0';
            *end_offset = '\0';

            // Demangle the function name
            int status = -4;
            AutoFreePtr func_name = gsl::owner<char *>(abi::__cxa_demangle(begin_name, nullptr, nullptr, &status));

            if (!status) {
                fmt::print("[{}] {} : {}+{}\n", i, symbollist[i], func_name.get(), begin_offset);
            } else {
                // If demangling failed, print the mangled function name
                fmt::print("[{}] {} : {}+{}\n", i, symbollist[i], begin_name, begin_offset);
            }
        } else {
            // Print the whole line if no parentheses or + sign found
            fmt::print("[{}] {}\n", i, symbollist[i]);
        }
    }
}

auto backtraceEntries(const uint max_frames) -> std::span<BackTraceEntry>
{
	assert(max_frames > 0);

	// Array to store the stack frames
    std::vector<void *> addrlist(max_frames + 1);

    // Get the backtrace frames
    const int num_frames = backtrace(addrlist.data(), static_cast<int>(addrlist.size()));

    if (!num_frames) {
        std::cout << "Failed to generate required BT.\n";

        return std::span<BackTraceEntry, 0>();
    }

    // Get symbolic names for the frames
    AutoFreePtr<char *> symbollist_(backtrace_symbols(addrlist.data(), num_frames));
    std::span<char *> symbollist(symbollist_, num_frames);

    const auto entries = std::span<BackTraceEntry>(new BackTraceEntry[num_frames], num_frames);

    for (int i = 1; i < num_frames; i++) { // Skip the first frame as it is this function itself
        char *begin_name = nullptr;
        char *begin_offset = nullptr;
        char *end_offset = nullptr;

        // Find parentheses and the + offset surrounded by parentheses
        for (char p : std::span(symbollist[i], std::strlen(symbollist[i]))) {
            if (p == '(') {
                begin_name = &p;
            } else if (p == '+') {
                begin_offset = &p;
            } else if (p == ')') {
                end_offset = &p;
                break;
            }
        }

        const int j = i - 1;
        entries[j].frame = j;
        entries[j].source = symbollist[i];

        if (begin_name && begin_offset && end_offset && begin_name < begin_offset) {
            *begin_name++ = '\0';
            *begin_offset++ = '\0';
            *end_offset = '\0';

            // Demangle the function name
            int status = -4;
            auto func_name = AutoFreePtr(abi::__cxa_demangle(begin_name, nullptr, nullptr, &status));

            entries[j].offset = std::stoll(begin_offset);

            if (!status) {
                //printf("[%d] %s : %s+%s\n", i, symbollist[i], func_name, begin_offset);

                entries[j].symbol = func_name;
            } else {
                // If demangling failed, print the mangled function name
                //printf("[%d] %s : %s+%s\n", i, symbollist[i], begin_name, begin_offset);

                entries[j].symbol = begin_name;
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
    , m_bt(m_entries.data(), [](gsl::owner<BackTraceEntry *> d) -> void { delete[] d; })
{
	assert(maxFrames != 0);
}

BackTrace::BackTrace(BackTrace &&other) noexcept
    : m_entries(other.m_entries)
    , m_bt(std::move(other.m_bt))
{}

void BackTrace::print() const
{
    for (const auto &e : m_entries) {
        std::cout << '[' << e.frame << "] " << e.source << " : " << e.symbol << std::hex << std::showpos << std::showbase << e.offset << '\n';
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
