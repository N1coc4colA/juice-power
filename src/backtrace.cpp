#include "backtrace.h"

#include <execinfo.h>
#include <cxxabi.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fmt/printf.h>


void BackTrace::easyPrint(uint maxFrames)
{
	assert(maxFrames != 0);

	// Array to store the stack frames
	void *addrlist[maxFrames + 1];

	// Get the backtrace frames
	const auto num_frames = backtrace(addrlist, static_cast<int>(sizeof(addrlist) / sizeof(void *)));

	if (!num_frames) {
		std::cerr << "No stack trace available\n";
		return;
	}

	// Get symbolic names for the frames
	char **symbollist = backtrace_symbols(addrlist, num_frames);

	for (int i = 1; i < num_frames; i++) { // Skip the first frame as it is this function itself
		char *func_name = nullptr;
		char *begin_name = nullptr;
		char *begin_offset = nullptr;
		char *end_offset = nullptr;

		// Find parentheses and the + offset surrounded by parentheses
		for (char *p = symbollist[i]; *p; p++) {
			if (*p == '(') {
				begin_name = p;
			} else if (*p == '+') {
				begin_offset = p;
			} else if (*p == ')') {
				end_offset = p;
				break;
			}
		}

		if (begin_name && begin_offset && end_offset && begin_name < begin_offset) {
			*begin_name++ = '\0';
			*begin_offset++ = '\0';
			*end_offset = '\0';

			// Demangle the function name
			int status;
			func_name = abi::__cxa_demangle(begin_name, nullptr, nullptr, &status);

			if (!status) {
				fmt::print("[{}] {} : {}+{}\n", i, symbollist[i], func_name, begin_offset);
			} else {
				// If demangling failed, print the mangled function name
				fmt::print("[{}] {} : {}+{}\n", i, symbollist[i], begin_name, begin_offset);
			}
			free(func_name);
		} else {
			// Print the whole line if no parentheses or + sign found
			fmt::print("[{}] {}\n", i, symbollist[i]);
		}
	}

	// From the docs, no need to free the elements pointed-to within the array.
	free(symbollist);
}

std::span<BackTraceEntry> backtrace_entries(uint max_frames)
{
	assert(max_frames > 0);

	// Array to store the stack frames
	void *addrlist[max_frames + 1];

	// Get the backtrace frames
	const int num_frames = backtrace(addrlist, static_cast<int>(sizeof(addrlist) / sizeof(void *)));

	if (!num_frames) {
		std::cout << "Failed to generate required BT.\n";

		return std::span<BackTraceEntry, 0>();
	}

	// Get symbolic names for the frames
	char **symbollist = backtrace_symbols(addrlist, num_frames);
	const auto entries = std::span<BackTraceEntry>(new BackTraceEntry[num_frames], num_frames);

	for (int i = 1; i < num_frames; i++) { // Skip the first frame as it is this function itself
		char *func_name = nullptr;
		char *begin_name = nullptr;
		char *begin_offset = nullptr;
		char *end_offset = nullptr;

		// Find parentheses and the + offset surrounded by parentheses
		for (char *p = symbollist[i]; *p; p++) {
			if (*p == '(') {
				begin_name = p;
			} else if (*p == '+') {
				begin_offset = p;
			} else if (*p == ')') {
				end_offset = p;
				break;
			}
		}

		const int j = i-1;
		entries[j].frame = j;
		entries[j].source = symbollist[i];

		if (begin_name && begin_offset && end_offset && begin_name < begin_offset) {
			*begin_name++ = '\0';
			*begin_offset++ = '\0';
			*end_offset = '\0';

			// Demangle the function name
			int status;
			func_name = abi::__cxa_demangle(begin_name, nullptr, nullptr, &status);

			entries[j].offset = std::stoll(begin_offset);

			if (!status) {
				//printf("[%d] %s : %s+%s\n", i, symbollist[i], func_name, begin_offset);

				entries[j].symbol = func_name;
			} else {
				// If demangling failed, print the mangled function name
				//printf("[%d] %s : %s+%s\n", i, symbollist[i], begin_name, begin_offset);

				entries[j].symbol = begin_name;
			}

			free(func_name);
		} else {
			// Print the whole line if no parentheses or + sign found
			//printf("[%d] %s\n", i, symbollist[i]);
			entries[j].offset = -1;
		}
	}

	// From the docs, no need to free the elements pointed-to within the array.
	free(symbollist);

	return entries;
}

BackTrace::BackTrace(uint maxFrames)
	: entries(backtrace_entries(maxFrames))
	, bt(entries.data(), [](BackTraceEntry *d) {delete [] d;})
{
	assert(maxFrames != 0);
}

BackTrace::BackTrace(const BackTrace &other)
	: entries(other.entries)
	, bt(other.bt)
{
}

BackTrace::BackTrace(BackTrace &&other) noexcept
	: entries(other.entries)
	, bt(std::move(other.bt))
{
}

BackTrace::~BackTrace()
{
}

void BackTrace::print() const
{
	for (const auto &e : entries) {
		std::cout << '[' << e.frame << "] " << e.source << " : " << e.symbol << std::hex << std::showpos << std::showbase << e.offset << '\n';
	}
}

BackTrace &BackTrace::operator =(const BackTrace &other)
{
	if (&other == this) {
		return *this;
	}

	entries = other.entries;
	bt = other.bt;

	return *this;
}

BackTrace &BackTrace::operator =(BackTrace &&other) noexcept
{
	if (&other == this) {
		return *this;
	}

	entries = other.entries;
	bt = std::move(other.bt);

	return *this;
}
