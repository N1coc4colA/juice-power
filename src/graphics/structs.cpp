#include "structs.h"


void DeletionQueue::flush()
{
	// reverse iterate the deletion queue to execute all the functions
	const auto rend = deletors.rend();
	for (auto it = deletors.rbegin(); it != rend; it++) {
		(*it)();
	}

	deletors.clear();
}
