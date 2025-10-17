#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <queue>

const int MAX_THREADS{ 32 };
int num_thread{ 0 };

class LF_NODE;
class AMR { // Atomic Markable Reference
	volatile long long ptr_and_mark;
public:
	AMR(LF_NODE* ptr = nullptr, bool mark = false)
	{
		long long val = reinterpret_cast<long long>(ptr);
		if (mark) val |= 1;
		ptr_and_mark = val;
	}

	LF_NODE* GetPtr()
	{
		long long val = ptr_and_mark;
		return reinterpret_cast<LF_NODE*>(val & ~1ULL);
	}

	bool GetMark()
	{
		return (ptr_and_mark & 1) == 1;
	}

	LF_NODE* GetPtrAndMark(bool* mark)
	{
		long long val = ptr_and_mark;
		*mark = (val & 1) == 1;
		return reinterpret_cast<LF_NODE*>(val & ~1ULL);
	}

	bool AttemptMark(LF_NODE* expected_ptr, bool new_mark)
	{
		return CAS(expected_ptr, expected_ptr, false, new_mark);
	}

	bool CAS(LF_NODE* expected_ptr, LF_NODE* new_ptr, bool expected_mark, bool new_mark)
	{
		long long expected_val = reinterpret_cast<long long>(expected_ptr);
		if (expected_mark) expected_val |= 1;

		long long new_val = reinterpret_cast<long long>(new_ptr);
		if (new_mark) new_val |= 1;	

		return std::atomic_compare_exchange_strong(
			reinterpret_cast<volatile std::atomic<long long>*>(&ptr_and_mark),
			&expected_val, new_val);
	}
};

thread_local int threadId{ 0 };

class LF_NODE {
public:
	int value;
	AMR next;
	int epoch; // For EBR

	LF_NODE(int v) : value(v), epoch(0) {}
};

class EBR { // Epoch Based Reclamation
	struct ThreadCounter {
		alignas(64) std::atomic<int> localEpoch;
	};

public:
	~EBR()
	{
		recycle();
	}

public:
	void recycle()
	{
		for (int i = 0; i < MAX_THREADS; ++i) {
			while (not freeList[i].empty()) {
				auto node = freeList[i].front();
				freeList[i].pop();
				delete node;
			}
		}
	}

	LF_NODE* newNode(int v)
	{
		if (not freeList[threadId].empty()) {
			auto node = freeList[threadId].front();

			bool canReuse{ true };
			for (int i = 0; i < num_thread; ++i) {
				if (i == threadId) continue;
				if (threadCounter[i].localEpoch <= node->epoch) {
					canReuse = false;
				}

				break;
			}

			if (canReuse) {
				freeList[threadId].pop();
				node->value = v;
				node->next = nullptr;
				return node;
			}
		}

		return new LF_NODE(v);
	}

	void deleteNode(LF_NODE* node)
	{
		node->epoch = epochCounter;
		freeList[threadId].push(node);
	}

	void StartOp()
	{
		threadCounter[threadId].localEpoch = epochCounter.fetch_add(1);
	}

	void EndOp()
	{
		threadCounter[threadId].localEpoch = std::numeric_limits<int>::max();
	}

private:
	std::queue<LF_NODE*> freeList[MAX_THREADS];
	std::atomic<int> epochCounter;
	ThreadCounter threadCounter[MAX_THREADS];
};

class LF_SET {
public:
	LF_SET()
	{
		head = new LF_NODE(std::numeric_limits<int>::min());
		tail = new LF_NODE(std::numeric_limits<int>::max());
		head->next = tail;
	}

	~LF_SET()
	{
		clear();
		delete head;
		delete tail;
	}

	void clear()
	{
		LF_NODE* curr = head->next.GetPtr();

		while (curr != tail) {
			LF_NODE* temp = curr;
			curr = curr->next.GetPtr();
			delete temp;
		}

		head->next = tail;
	}

	bool add(int v)
	{
		while (true) {
			LF_NODE* prev{ nullptr };
			LF_NODE* curr{ nullptr };
			find(prev, curr, v);

			if (curr->value == v) {
				return false;
			}

			else {
				auto newNode = new LF_NODE(v);
				newNode->next = curr;
				if (prev->next.CAS(curr, newNode, false, false)) {
					return true;
				}
				delete newNode;
			}
		}
	}

	bool remove(int v)
	{
		while (true) {
			LF_NODE* prev{ nullptr };
			LF_NODE* curr{ nullptr };
			find(prev, curr, v);

			if (curr->value != v) {
				return false;
			}

			else {
				LF_NODE* succ = curr->next.GetPtr();
				if(not curr->next.AttemptMark(succ, true)) {
					continue;
				}

				prev->next.CAS(curr, succ, false, false);
				return true;
			}
		}
	}

	bool contains(int v)
	{
		LF_NODE* curr = head;

		while (curr->value < v) {
			curr = curr->next.GetPtr();
		}

		return curr->value == v and not curr->next.GetMark();
	}

	void print20()
	{
		auto curr = head->next.GetPtr();

		for (int i = 0; i < 20 and curr != tail; ++i) {
			std::cout << curr->value << ", ";
			curr = curr->next.GetPtr();
		}
		std::cout << std::endl;
	}

private:
	void find(LF_NODE*& prev, LF_NODE*& curr, int v)
	{
		while (true) {
			retry:
			prev = head;
			curr = prev->next.GetPtr();

			while (true) {
				bool currMark;
				auto succ = curr->next.GetPtrAndMark(&currMark);

				while (currMark) {
					if (not prev->next.CAS(curr, succ, false, false)) {
						goto retry;
					}

					curr = succ;
					succ = curr->next.GetPtrAndMark(&currMark);
				}

				if (curr->value >= v) {
					return;
				}

				prev = curr;
				curr = succ;
			}
		}
	}

private:
	LF_NODE* head;
	LF_NODE* tail;
};

class LF_SET_EBR {
public:
	LF_SET_EBR()
	{
		head = new LF_NODE(std::numeric_limits<int>::min());
		tail = new LF_NODE(std::numeric_limits<int>::max());
		head->next = tail;
	}

	~LF_SET_EBR()
	{
		clear();
		delete head;
		delete tail;
	}

	void clear()
	{
		LF_NODE* curr = head->next.GetPtr();

		while (curr != tail) {
			LF_NODE* temp = curr;
			curr = curr->next.GetPtr();
			delete temp;
		}

		head->next = tail;
	}

	bool add(int v)
	{
		ebr.StartOp();

		while (true) {
			LF_NODE* prev{ nullptr };
			LF_NODE* curr{ nullptr };
			find(prev, curr, v);

			if (curr->value == v) {
				ebr.EndOp();
				return false;
			}

			else {
				auto newNode = ebr.newNode(v);
				newNode->next = curr;
				if (prev->next.CAS(curr, newNode, false, false)) {
					ebr.EndOp();
					return true;
				}
				ebr.deleteNode(newNode);
			}
		}
	}

	bool remove(int v)
	{
		ebr.StartOp();

		while (true) {
			LF_NODE* prev{ nullptr };
			LF_NODE* curr{ nullptr };
			find(prev, curr, v);

			if (curr->value != v) {
				ebr.EndOp();
				return false;
			}

			else {
				LF_NODE* succ = curr->next.GetPtr();
				if (not curr->next.AttemptMark(succ, true)) {
					continue;
				}

				if (prev->next.CAS(curr, succ, false, false)) {
					ebr.deleteNode(curr);
				}

				ebr.EndOp();
				return true;
			}
		}
	}

	bool contains(int v)
	{
		ebr.StartOp();

		LF_NODE* curr = head;

		while (curr->value < v) {
			curr = curr->next.GetPtr();
		}

		bool result = curr->value == v and not curr->next.GetMark();

		ebr.EndOp();
		return result;
	}

	void print20()
	{
		auto curr = head->next.GetPtr();

		for (int i = 0; i < 20 and curr != tail; ++i) {
			std::cout << curr->value << ", ";
			curr = curr->next.GetPtr();
		}
		std::cout << std::endl;
	}

private:
	void find(LF_NODE*& prev, LF_NODE*& curr, int v)
	{
		while (true) {
		retry:
			prev = head;
			curr = prev->next.GetPtr();

			while (true) {
				bool currMark;
				auto succ = curr->next.GetPtrAndMark(&currMark);

				while (currMark) {
					if (not prev->next.CAS(curr, succ, false, false)) {
						goto retry;
					}

					ebr.deleteNode(curr);
					curr = succ;
					succ = curr->next.GetPtrAndMark(&currMark);
				}

				if (curr->value >= v) {
					return;
				}

				prev = curr;
				curr = succ;
			}
		}
	}

private:
	LF_NODE* head;
	LF_NODE* tail;

	EBR ebr;
};

LF_SET_EBR set;
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
	threadId = th_id;

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

void benchmark(const int num_threads, int thread_id)
{
	threadId = thread_id;

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

	for (num_thread = 1; num_thread <= MAX_THREADS; num_thread *= 2) {
		set.clear();
		std::vector<std::thread> workers;

		auto start = high_resolution_clock::now();

		for (int i = 0; i < num_thread; ++i) {
			workers.emplace_back(benchmark, num_thread, i);
		}

		for (int i = 0; i < num_thread; ++i) {
			workers[i].join();
		}

		auto end = high_resolution_clock::now();
		std::cout << num_thread << " Threads, Duration : "
			<< duration_cast<milliseconds>(end - start).count() << "ms\n";
		std::cout << "Set : ";
		set.print20();
	}

	// Consistency check
	std::cout << "\n\nConsistency Check\n";

	for (num_thread = MAX_THREADS; num_thread >= 1; num_thread /= 2) {
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