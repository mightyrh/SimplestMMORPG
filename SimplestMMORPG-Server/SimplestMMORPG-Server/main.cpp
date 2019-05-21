#include "simplestMMORPG.h"


int main()
{
	tbb::concurrent_priority_queue<int> testObj;
	int i;
	testObj.push(101);
	testObj.try_pop(i);
	std::cout << i << std::endl;

	system("pause");
}