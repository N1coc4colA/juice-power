#include "backtrace.h"

#include <execinfo.h>
#include <cxxabi.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fmt/printf.h>


void BackTrace::easyPrint(unsigned int maxFrames)
{
	assert(maxFrames != 0);

	// Array to store the stack frames
	void *addrlist[maxFrames + 1];

	// Get the backtrace frames
	const int num_frames = backtrace(addrlist, sizeof(addrlist) / sizeof(void *));

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

	free(symbollist);
}

std::span<BackTraceEntry> backtrace_entries(int max_frames)
{
	assert(max_frames > 0);

	// Array to store the stack frames
	void *addrlist[max_frames + 1];

	// Get the backtrace frames
	int num_frames = backtrace(addrlist, sizeof(addrlist) / sizeof(void *));

	if (!num_frames) {
		std::cout << "Failed to generate required BT." << std::endl;
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

	free(symbollist);

	return entries;
}

BackTrace::BackTrace(unsigned int maxFrames)
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

void BackTrace::print() const
{
	for (const auto &e : entries) {
		std::cout << '[' << e.frame << "] " << e.source << " : " << e.symbol << std::hex << std::showpos << std::showbase << e.offset << std::endl;
	}
}
