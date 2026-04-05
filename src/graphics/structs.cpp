#include "structs.h"


namespace Graphics
{

void DeletionQueue::flush()
{
	// reverse iterate the deletion queue to execute all the functions
	const auto rend = deleters.rend();
	for (auto it = deleters.rbegin(); it != rend; it++) {
		(*it)();
	}

	deleters.clear();
}


}
