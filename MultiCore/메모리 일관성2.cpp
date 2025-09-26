#include <iostream>
#include <thread>
#include <atomic>

const int LOOP_COUNT{ 25'000'000 };
volatile int* ptr{ nullptr };
volatile bool done{ false };

void update_ptr()
{
	for (int i = 0; i < LOOP_COUNT; ++i) {
		*ptr = -(1 + *ptr);
	}
	done = true;
}

void watch_ptr()
{
	int error_count{ 0 };

	while (not done) {
		int v = *ptr;
		if ((v != 0) and (v != -1)) {
			printf("%X, ", v);
			error_count++;
		}
	}

	std::cout << "Error count = " << error_count << std::endl;
}

int main()
{
	int value[32];
	long long addr = reinterpret_cast<long long>(&value[31]);
	addr = addr - (addr % 64);
	addr = addr - 1;
	ptr = reinterpret_cast<int*>(addr);
	*ptr = 0;

	std::thread t1{ watch_ptr };
	std::thread t2{ update_ptr };

	t1.join();
	t2.join();
}