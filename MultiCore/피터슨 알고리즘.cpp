#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>
#include <atomic>

const int MAX_THREADS{ 2 };

volatile int sum{ 0 };

std::atomic<int> victim{ 0 };
std::atomic<bool> flag[MAX_THREADS]{ false, false };

std::mutex mtx;

void p_lock(int thread_id)
{
	int other = 1 - thread_id;
	flag[thread_id] = true;
	victim = thread_id;
	//std::atomic_thread_fence(std::memory_order_seq_cst);
	while (flag[other] && (victim == thread_id));
}

void p_unlock(int thread_id)
{
	flag[thread_id] = false;
}

void worker_func(const int thread_id, const int loop_count)
{
	for (int i = 0; i < loop_count; ++i) {
		p_lock(thread_id);
		sum += 2;
		p_unlock(thread_id);
	}
}

void worker_func2(const int loop_count)
{
	for (int i = 0; i < loop_count; ++i) {
		mtx.lock();
		sum += 2;
		mtx.unlock();
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

	{
		for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
			sum = 0;
			auto start = high_resolution_clock::now();

			std::vector<std::thread> workers(num_threads);
			for (int i = 0; i < workers.size(); ++i) {
				workers[i] = std::thread(worker_func2, 50'000'000 / num_threads);
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