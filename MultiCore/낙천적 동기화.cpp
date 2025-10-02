#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>

const int MAX_THREADS{ 32 };

class NODE {
public:
	int value;
	NODE* next;
	std::mutex mtx;

	NODE(int v) : next(nullptr), value(v) {}

	void lock() { mtx.lock(); }
	void unlock() { mtx.unlock(); }
};

class F_SET {
public:
	F_SET()
	{
		head = new NODE(std::numeric_limits<int>::min());
		tail = new NODE(std::numeric_limits<int>::max());
		head->next = tail;
	}

	~F_SET()
	{
		clear();
		delete head;
		delete tail;
	}

	void clear()
	{
		NODE* curr = head->next;
		while (curr != tail) {
			NODE* temp = curr;
			curr = curr->next;
			delete temp;
		}

		head->next = tail;
	}

	bool add(int v)
	{
		auto prev = head;
		prev->lock();

		auto curr = prev->next;
		curr->lock();

		while (curr->value < v) {
			prev->unlock();
			prev = curr;
			curr = curr->next;
			curr->lock();
		}

		if (curr->value == v) {
			prev->unlock();
			curr->unlock();

			return false;
		}

		else {
			auto newNode = new NODE(v);
			newNode->next = curr;
			prev->next = newNode;

			prev->unlock();
			curr->unlock();

			return true;
		}
	}

	bool remove(int v)
	{
		auto prev = head;
		prev->lock();

		auto curr = prev->next;
		curr->lock();

		while (curr->value < v) {
			prev->unlock();
			prev = curr;
			curr = curr->next;
			curr->lock();
		}

		if (curr->value == v) {
			prev->next = curr->next;

			prev->unlock();
			curr->unlock();

			delete curr;
			return true;
		}

		else {
			prev->unlock();
			curr->unlock();

			return false;
		}
	}

	bool contains(int v)
	{
		auto prev = head;
		prev->lock();

		auto curr = prev->next;
		curr->lock();

		while (curr->value < v) {
			prev->unlock();
			prev = curr;
			curr = curr->next;
			curr->lock();
		}

		if (curr->value == v) {
			prev->unlock();
			curr->unlock();

			return true;
		}

		else {
			prev->unlock();
			curr->unlock();

			return false;
		}
	}

	void print20()
	{
		auto curr = head->next;

		for (int i = 0; i < 20 and curr != tail; ++i) {
			std::cout << curr->value << ", ";
			curr = curr->next;
		}
		std::cout << std::endl;
	}

private:
	NODE* head;
	NODE* tail;
};

F_SET set;

void benchmark(const int num_threads)
{
	const int LOOP_COUNT{ 4'000'000 / num_threads };
	const int RANGE{ 1'000 };

	for (int i = 0; i < LOOP_COUNT; ++i) {
		int value = rand() % RANGE;
		int op = rand() % 3;

		if (op == 0) set.add(value);
		else if (op == 1) set.remove(value);
		else set.contains(value);
	}
}

int main()
{
	using namespace std::chrono;

	for (int num_threads = MAX_THREADS; num_threads >= 1; num_threads /= 2) {
		set.clear();
		std::vector<std::thread> workers;

		auto start = high_resolution_clock::now();

		for (int i = 0; i < num_threads; ++i) {
			workers.emplace_back(benchmark, num_threads);
		}

		for (int i = 0; i < num_threads; ++i) {
			workers[i].join();
		}

		auto end = high_resolution_clock::now();
		std::cout << num_threads << " Threads, Duration : "
			<< duration_cast<milliseconds>(end - start).count() << "ms\n";
		std::cout << "Set : ";
		set.print20();
	}
}