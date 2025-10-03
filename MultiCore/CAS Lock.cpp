#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

const int MAX_THREADS{ 8 };

bool CAS(std::atomic_bool* lock_flag, bool old_value, bool new_value)
{
	return std::atomic_compare_exchange_strong(
		lock_flag, &old_value, new_value);
}

bool CAS(volatile bool* lock_flag, bool old_value, bool new_value)
{
	return std::atomic_compare_exchange_strong(
		reinterpret_cast<volatile std::atomic_bool*>(lock_flag),
		&old_value, new_value);
}

volatile int sum{ 0 };
volatile bool lock_flag{ false };
//std::atomic<bool> lock_flag{ false };

void CAS_lock()
{
	while (not CAS(&lock_flag, false, true));
}

void CAS_unlock()
{
	std::atomic_thread_fence(std::memory_order_acquire);
	lock_flag = false;
}

void worker_func(const int thread_id, const int loop_count)
{
	for (int i = 0; i < loop_count; ++i) {
		CAS_lock();
		sum += 2;
		CAS_unlock();
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
		std::cout << "Single Thread Exec Time = " << duration_cast<milliseconds>(end - start).count();
		std::cout << "  Sum = " << sum << std::endl;
	}

	{
		for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
			sum = 0;
			auto start = high_resolution_clock::now();

			std::vector<std::thread> workers(num_threads);
			for (int i = 0; i < workers.size(); ++i) {
				workers[i] = std::thread(worker_func, i, 50'000'000 / num_threads);
			}

			for (int i = 0; i < workers.size(); ++i) {
				workers[i].join();
			}

			auto end = high_resolution_clock::now();
			std::cout << num_threads << " Threads Exec Time = " << duration_cast<milliseconds>(end - start).count();
			std::cout << "  Sum = " << sum << std::endl;
		}
	}
}