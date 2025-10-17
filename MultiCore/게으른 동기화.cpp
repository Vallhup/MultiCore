#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <queue>

const int MAX_THREADS{ 32 };

class NODE {
public:
	int value;
	volatile bool removed;
	NODE* volatile next;
	std::mutex mtx;

	NODE(int v) 
		: next(nullptr), value(v), removed(false) {}

	void lock() { mtx.lock(); }
	void unlock() { mtx.unlock(); }
};

class NODE_SP {
public:
	int value;
	volatile bool removed;
	std::shared_ptr<NODE_SP> next;
	std::mutex mtx;

	NODE_SP(int v)
		: next(nullptr), value(v), removed(false) {
	}

	void lock() { mtx.lock(); }
	void unlock() { mtx.unlock(); }
};

class NODE_ATOMIC_SP {
public:
	int value;
	volatile bool removed;
	std::atomic<std::shared_ptr<NODE_ATOMIC_SP>> next;
	std::mutex mtx;

	NODE_ATOMIC_SP(int v)
		: next(nullptr), value(v), removed(false) {
	}

	void lock() { mtx.lock(); }
	void unlock() { mtx.unlock(); }
};

class L_SET {
public:
	L_SET()
	{
		head = new NODE(std::numeric_limits<int>::min());
		tail = new NODE(std::numeric_limits<int>::max());
		head->next = tail;
	}

	~L_SET()
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
		while (true) {
			auto prev = head;
			auto curr = prev->next;

			while (curr->value < v) {
				prev = curr;
				curr = curr->next;
			}

			prev->lock();
			curr->lock();
			if (false == validate(prev, curr)) {
				prev->unlock();
				curr->unlock();
				continue;
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
	}

	bool remove(int v)
	{
		while (true) {
			auto prev = head;
			auto curr = prev->next;

			while (curr->value < v) {
				prev = curr;
				curr = curr->next;
			}

			prev->lock();
			curr->lock();
			if (false == validate(prev, curr)) {
				prev->unlock();
				curr->unlock();
				continue;
			}

			if (curr->value == v) {
				curr->removed = true;
				std::atomic_thread_fence(std::memory_order_seq_cst);
				prev->next = curr->next;

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
	}

	bool contains(int v)
	{
		NODE* curr = head;

		while (curr->value < v) {
			curr = curr->next;
		}

		return curr->value == v and not curr->removed;
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
	bool validate(NODE* p, NODE* c)
	{
		return (p->removed == false)
			and (c->removed == false)
			and (p->next == c);
	}

private:
	NODE* head;
	NODE* tail;
};

class L_SET_FL {
public:
	L_SET_FL()
	{
		head = new NODE(std::numeric_limits<int>::min());
		tail = new NODE(std::numeric_limits<int>::max());
		head->next = tail;
	}

	~L_SET_FL()
	{
		clear();
		delete head;
		delete tail;
	}

	void my_delete(NODE* node)
	{
		std::lock_guard<std::mutex> lg{ fl_mtx };
		free_list.push(node);
	}

	void recycle()
	{
		while (false == free_list.empty()) {
			auto node = free_list.front();
			free_list.pop();
			delete node;
		}
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
		while (true) {
			auto prev = head;
			auto curr = prev->next;

			while (curr->value < v) {
				prev = curr;
				curr = curr->next;
			}

			prev->lock();
			curr->lock();
			if (false == validate(prev, curr)) {
				prev->unlock();
				curr->unlock();
				continue;
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
	}

	bool remove(int v)
	{
		while (true) {
			auto prev = head;
			auto curr = prev->next;

			while (curr->value < v) {
				prev = curr;
				curr = curr->next;
			}

			prev->lock();
			curr->lock();
			if (false == validate(prev, curr)) {
				prev->unlock();
				curr->unlock();
				continue;
			}

			if (curr->value == v) {
				curr->removed = true;
				std::atomic_thread_fence(std::memory_order_seq_cst);
				prev->next = curr->next;
				my_delete(curr);

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
	}

	bool contains(int v)
	{
		NODE* curr = head;

		while (curr->value < v) {
			curr = curr->next;
		}

		return curr->value == v and not curr->removed;
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
	bool validate(NODE* p, NODE* c)
	{
		return (p->removed == false)
			and (c->removed == false)
			and (p->next == c);
	}

private:
	NODE* head;
	NODE* tail;
	std::queue<NODE*> free_list;
	std::mutex fl_mtx;
};

class L_SET_SP {
public:
	L_SET_SP()
	{
		head = std::make_shared<NODE_SP>(std::numeric_limits<int>::min());
		tail = std::make_shared<NODE_SP>(std::numeric_limits<int>::max());
		head->next = tail;
	}

	~L_SET_SP() = default;

	void clear()
	{
		head->next = tail;
	}

	bool add(int v)
	{
		while (true) {
			auto prev = head;
			auto curr = prev->next;

			while (curr->value < v) {
				prev = curr;
				curr = curr->next;
			}

			prev->lock();
			curr->lock();
			if (false == validate(prev, curr)) {
				prev->unlock();
				curr->unlock();
				continue;
			}

			if (curr->value == v) {
				prev->unlock();
				curr->unlock();

				return false;
			}

			else {
				auto newNode = std::make_shared<NODE_SP>(v);
				newNode->next = curr;
				prev->next = newNode;

				prev->unlock();
				curr->unlock();

				return true;
			}
		}
	}

	bool remove(int v)
	{
		while (true) {
			auto prev = head;
			auto curr = prev->next;

			while (curr->value < v) {
				prev = curr;
				curr = curr->next;
			}

			prev->lock();
			curr->lock();
			if (false == validate(prev, curr)) {
				prev->unlock();
				curr->unlock();
				continue;
			}

			if (curr->value == v) {
				curr->removed = true;
				std::atomic_thread_fence(std::memory_order_seq_cst);
				prev->next = curr->next;

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
	}

	bool contains(int v)
	{
		auto curr = head->next;

		while (curr->value < v) {
			curr = curr->next;
		}

		return curr->value == v and not curr->removed;
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
	bool validate(const std::shared_ptr<NODE_SP>& p, 
		const std::shared_ptr<NODE_SP>& c)
	{
		return (p->removed == false)
			and (c->removed == false)
			and (p->next == c);
	}

private:
	std::shared_ptr<NODE_SP> head;
	std::shared_ptr<NODE_SP> tail;
};

class L_SET_ATOMIC_SP {
public:
	L_SET_ATOMIC_SP()
	{
		head = std::make_shared<NODE_ATOMIC_SP>(std::numeric_limits<int>::min());
		tail = std::make_shared<NODE_ATOMIC_SP>(std::numeric_limits<int>::max());
		head->next = tail;
	}

	~L_SET_ATOMIC_SP() = default;

	void clear()
	{
		head->next = tail;
	}

	bool add(int v)
	{
		while (true) {
			auto prev = head;
			auto curr = prev->next.load();

			while (curr->value < v) {
				prev = curr;
				curr = curr->next;
			}

			prev->lock();
			curr->lock();
			if (false == validate(prev, curr)) {
				prev->unlock();
				curr->unlock();
				continue;
			}

			if (curr->value == v) {
				prev->unlock();
				curr->unlock();

				return false;
			}

			else {
				auto newNode = std::make_shared<NODE_ATOMIC_SP>(v);
				newNode->next = curr;
				prev->next = newNode;

				prev->unlock();
				curr->unlock();

				return true;
			}
		}
	}

	bool remove(int v)
	{
		while (true) {
			auto prev = head;
			auto curr = prev->next.load();

			while (curr->value < v) {
				prev = curr;
				curr = curr->next;
			}

			prev->lock();
			curr->lock();
			if (false == validate(prev, curr)) {
				prev->unlock();
				curr->unlock();
				continue;
			}

			if (curr->value == v) {
				curr->removed = true;
				std::atomic_thread_fence(std::memory_order_seq_cst);
				prev->next = curr->next.load();

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
	}

	bool contains(int v)
	{
		auto curr = head->next.load();

		while (curr->value < v) {
			curr = curr->next;
		}

		return curr->value == v and not curr->removed;
	}

	void print20()
	{
		auto curr = head->next.load();

		for (int i = 0; i < 20 and curr != tail; ++i) {
			std::cout << curr->value << ", ";
			curr = curr->next;
		}
		std::cout << std::endl;
	}

private:
	bool validate(const std::shared_ptr<NODE_ATOMIC_SP>& p,
		const std::shared_ptr<NODE_ATOMIC_SP>& c)
	{
		return (p->removed == false)
			and (c->removed == false)
			and (p->next.load() == c);
	}

private:
	std::shared_ptr<NODE_ATOMIC_SP> head;
	std::shared_ptr<NODE_ATOMIC_SP> tail;
};

L_SET set;
const int LOOP = 4'000'000;
const int RANGE = 1000;

#include <array>

class HISTORY {
public:
	int op;
	int i_value;
	bool o_value;
	HISTORY(int o, int i, bool re) : op(o), i_value(i), o_value(re) {}
};

std::array<std::vector<HISTORY>, MAX_THREADS> history;

void check_history(int num_threads)
{
	std::array <int, RANGE> survive = {};
	std::cout << "Checking Consistency : ";
	if (history[0].size() == 0) {
		std::cout << "No history.\n";
		return;
	}
	for (int i = 0; i < num_threads; ++i) {
		for (auto& op : history[i]) {
			if (false == op.o_value) continue;
			if (op.op == 3) continue;
			if (op.op == 0) survive[op.i_value]++;
			if (op.op == 1) survive[op.i_value]--;
		}
	}
	for (int i = 0; i < RANGE; ++i) {
		int val = survive[i];
		if (val < 0) {
			std::cout << "ERROR. The value " << i << " removed while it is not in the set.\n";
			exit(-1);
		}
		else if (val > 1) {
			std::cout << "ERROR. The value " << i << " is added while the set already have it.\n";
			exit(-1);
		}
		else if (val == 0) {
			if (set.contains(i)) {
				std::cout << "ERROR. The value " << i << " should not exists.\n";
				exit(-1);
			}
		}
		else if (val == 1) {
			if (false == set.contains(i)) {
				std::cout << "ERROR. The value " << i << " shoud exists.\n";
				exit(-1);
			}
		}
	}
	std::cout << " OK\n";
}

void benchmark_check(int num_threads, int th_id)
{
	for (int i = 0; i < LOOP / num_threads; ++i) {
		int op = rand() % 3;
		switch (op) {
		case 0: {
			int v = rand() % RANGE;
			history[th_id].emplace_back(0, v, set.add(v));
			break;
		}
		case 1: {
			int v = rand() % RANGE;
			history[th_id].emplace_back(1, v, set.remove(v));
			break;
		}
		case 2: {
			int v = rand() % RANGE;
			history[th_id].emplace_back(2, v, set.contains(v));
			break;
		}
		}
	}
}

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

	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
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

	// Consistency check
	std::cout << "\n\nConsistency Check\n";

	for (int num_thread = MAX_THREADS; num_thread >= 1; num_thread /= 2) {
		set.clear();
		std::vector<std::thread> threads;
		for (int i = 0; i < MAX_THREADS; ++i) {
			history[i].clear();
		}

		auto start = high_resolution_clock::now();

		for (int i = 0; i < num_thread; ++i) {
			threads.emplace_back(benchmark_check, num_thread, i);
		}

		for (auto& th : threads) {
			th.join();
		}

		auto stop = high_resolution_clock::now();
		auto duration = duration_cast<milliseconds>(stop - start);
		
		std::cout << "Threads: " << num_thread
			<< ", Duration: " << duration.count() << " ms.\n";
		std::cout << "Set: "; set.print20();
		check_history(num_thread);
	}
}