#include <iostream>
#include <thread>
#include <atomic>

const int LOOP_COUNT{ 50'000'000 };
std::atomic<int> x, y;
int trace_x[LOOP_COUNT], trace_y[LOOP_COUNT];

void ThreadFuncX()
{
	for (int i = 0; i < LOOP_COUNT; ++i) {
		x = i;
		trace_y[i] = y;
	}
}

void ThreadFuncY()
{
	for (int i = 0; i < LOOP_COUNT; ++i) {
		y = i;
		trace_x[i] = x;
	}
}

int main()
{
	std::thread t1{ ThreadFuncX };
	std::thread t2{ ThreadFuncY };

	t1.join();
	t2.join();

	int count{ 0 };
	for (int i = 0; i < LOOP_COUNT - 1; ++i) {
		if (trace_x[i] == trace_x[i + 1]) {
			if (trace_y[trace_x[i]] == trace_y[trace_x[i] + 1]) {
				if (trace_y[trace_x[i]] == i) count++;
			}
		}
	}

	std::cout << "Memory Inconsistency : " << count << std::endl;
}