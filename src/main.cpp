#include <iostream>

#include "vk/engine.h"


int main(int argc, char **argv)
{
	std::cout << "Hell-O World!" << std::endl;

	Engine engine;

	engine.init();

	engine.run();

	engine.cleanup();

	return 0;
}
