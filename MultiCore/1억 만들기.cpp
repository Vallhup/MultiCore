#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>

std::atomic<int> atomic_sum;

volatile int sum;
std::mutex mtx;

const int MAX_THREADS{ 16 };
const int CACHE_LINE_SIZE_INT{ 16 };

struct NUM {
	alignas(64) volatile int value;
};

NUM array_sum[MAX_THREADS];

void func(const int thread_id, const int count)
{
	for (int i = 0; i < count; ++i) {
		array_sum[thread_id].value += 2;
	}
}

int main()
{
	using namespace std::chrono;
	{
		auto start = high_resolution_clock::now();

		for (int i = 0; i < 50'000'000; ++i) {
			sum += 2;
		}

		auto end = high_resolution_clock::now();
		std::cout << "Single Thread Duration : " << duration_cast<milliseconds>(end - start).count() << " ms, ";
		std::cout << "Sum = " << sum << std::endl;
	}

	for (int num_threads = 1; num_threads <= 16; num_threads *= 2) {
		sum = 0;
		std::vector<std::thread> workers;

		auto start = high_resolution_clock::now();

		for (int i = 0; i < num_threads; ++i) {
			workers.emplace_back(func, i, 50'000'000 / num_threads);
		}

		for (int i = 0; i < num_threads; ++i) {
			workers[i].join();
			sum += array_sum[i].value;
			array_sum[i].value = 0;
		}

		auto end = high_resolution_clock::now();
		std::cout << num_threads << " Thread Duration : " << duration_cast<milliseconds>(end - start).count() << " ms, ";
		std::cout << "Sum = " << sum << std::endl;
	}
}