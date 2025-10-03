#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>
#include <chrono>

/*--------------------------------------------------------------------------------*/
// 멀티코어 프로그래밍 과제1 - 빵집 알고리즘 구현								  //
//  - 벤치마크 프로그램으로 천만 만들기 프로그램 사용							  //
//  - 쓰레드 1, 2, 4, 8개 일 때의 실행시간 측정									  //
//  - 아무것도 사용하지 않을 때 / <mutex> 사용할 때 / 빵집 알고리즘 사용할 때     //
//    결과 값과 속도 비교														  //
//  - Volatile을 atomic으로 변경 후 결과값과 속도 비교							  //
/*--------------------------------------------------------------------------------*/

constexpr int MAX_THREADS{ 8 };

class Bakery {
public:
	Bakery() = delete;
	Bakery(int n) : _threadNum(n)
	{
		_flag = new volatile bool[n];
		_label = new volatile int[n];

		for (int i = 0; i < n; ++i) {
			_flag[i] = false;
			_label[i] = 0;
		}
	}

	~Bakery()
	{
		delete[] _flag;
		delete[] _label;
	}

public:
	void lock(int id)
	{
		_flag[id] = true;

		int maxLabel{ -10000 };
		for (int i = 0; i < _threadNum; ++i) {
			if (maxLabel < _label[i]) {
				maxLabel = _label[i];
			}
		}

		_label[id] = maxLabel + 1;
		WaitLoop(id);
	}

	void unlock(int id)
	{
		_flag[id] = false;
	}

private:
	void WaitLoop(int id)
	{
		for (int i = 0; i < _threadNum; ++i) {
			if (i != id) {
				while (_flag[i] and 
					((_label[i] < _label[id]) or 
					((_label[i] == _label[id]) and (i < id))));
			}
		}
	}

private:
	int _threadNum;

	volatile bool* _flag;
	volatile int* _label;
};

class AtomicBakery {
public:
	AtomicBakery() = delete;
	AtomicBakery(int n) : _threadNum(n)
	{
		_flag = new std::atomic<bool>[n];
		_label = new std::atomic<int>[n];

		for (int i = 0; i < n; ++i) {
			_flag[i] = false;
			_label[i] = 0;
		}
	}

	~AtomicBakery()
	{
		delete[] _flag;
		delete[] _label;
	}

public:
	void lock(int id)
	{
		_flag[id] = true;

		int maxLabel{ -10000 };
		for (int i = 0; i < _threadNum; ++i) {
			if (maxLabel < _label[i]) {
				maxLabel = _label[i];
			}
		}

		_label[id] = maxLabel + 1;
		WaitLoop(id);
	}

	void unlock(int id)
	{
		_flag[id] = false;
	}

private:
	void WaitLoop(int id)
	{
		for (int i = 0; i < _threadNum; ++i) {
			if (i != id) {
				while (_flag[i] and 
					((_label[i] < _label[id]) or
					((_label[i] == _label[id]) and (i < id))));
			}
		}
	}

private:
	int _threadNum;

	std::atomic<bool>* _flag;
	std::atomic<int>* _label;
};

volatile int sum{ 0 };

std::mutex mtx;

void NoLock(const int loop_count)
{
	for (int i = 0; i < loop_count; ++i) {
		sum += 2;
	}
}

void UseMutex(const int loop_count)
{
	for (int i = 0; i < loop_count; ++i) {
		mtx.lock();
		sum += 2;
		mtx.unlock();
	}
}

void UseBakery(Bakery* b, const int thread_id, const int loop_count)
{
	for (int i = 0; i < loop_count; ++i) {
		b->lock(thread_id);
		sum += 2;
		b->unlock(thread_id);
	}
}

void UseAtomicBakery(AtomicBakery* b, const int thread_id, const int loop_count)
{
	for (int i = 0; i < loop_count; ++i) {
		b->lock(thread_id);
		sum += 2;
		b->unlock(thread_id);
	}
}

int main()
{
	using namespace std::chrono;
	{
		auto start = high_resolution_clock::now();

		for (int i = 0; i < 5'000'000; ++i) {
			sum += 2;
		}

		auto end = high_resolution_clock::now();
		std::cout << "Single Thread Duration : " << duration_cast<milliseconds>(end - start).count() << " ms, ";
		std::cout << "Sum = " << sum << std::endl;
	}

	std::cout << "\n------------------ No Lock -------------------\n\n";

	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		sum = 0;
		std::vector<std::thread> workers;
		workers.reserve(num_threads);

		auto start = high_resolution_clock::now();

		for (int i = 0; i < num_threads; ++i) {
			workers.emplace_back(NoLock, 5'000'000 / num_threads);
		}

		for (int i = 0; i < num_threads; ++i) {
			workers[i].join();
		}

		auto end = high_resolution_clock::now();
		std::cout << num_threads << " Thread Duration : " << duration_cast<milliseconds>(end - start).count() << " ms, ";
		std::cout << "Sum = " << sum << std::endl;
	}

	std::cout << "\n------------------ Use Mutex -------------------\n\n";

	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		sum = 0;
		std::vector<std::thread> workers;
		workers.reserve(num_threads);

		auto start = high_resolution_clock::now();

		for (int i = 0; i < num_threads; ++i) {
			workers.emplace_back(UseMutex, 5'000'000 / num_threads);
		}

		for (int i = 0; i < num_threads; ++i) {
			workers[i].join();
		}

		auto end = high_resolution_clock::now();
		std::cout << num_threads << " Thread Duration : " << duration_cast<milliseconds>(end - start).count() << " ms, ";
		std::cout << "Sum = " << sum << std::endl;
	}

	std::cout << "\n------------------ Use Bakery -------------------\n\n";

	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		sum = 0;
		Bakery bakery{ num_threads };
		std::vector<std::thread> workers;
		workers.reserve(num_threads);

		auto start = high_resolution_clock::now();

		for (int i = 0; i < num_threads; ++i) {
			workers.emplace_back(UseBakery, &bakery, i, 5'000'000 / num_threads);
		}

		for (int i = 0; i < num_threads; ++i) {
			workers[i].join();
		}

		auto end = high_resolution_clock::now();
		std::cout << num_threads << " Thread Duration : " << duration_cast<milliseconds>(end - start).count() << " ms, ";
		std::cout << "Sum = " << sum << std::endl;
	}

	std::cout << "\n------------------ Use AtomicBakery -------------------\n\n";

	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		sum = 0;
		AtomicBakery bakery{ num_threads };
		std::vector<std::thread> workers;
		workers.reserve(num_threads);

		auto start = high_resolution_clock::now();

		for (int i = 0; i < num_threads; ++i) {
			workers.emplace_back(UseAtomicBakery, &bakery, i, 5'000'000 / num_threads);
		}

		for (int i = 0; i < num_threads; ++i) {
			workers[i].join();
		}

		auto end = high_resolution_clock::now();
		std::cout << num_threads << " Thread Duration : " << duration_cast<milliseconds>(end - start).count() << " ms, ";
		std::cout << "Sum = " << sum << std::endl;
	}
}