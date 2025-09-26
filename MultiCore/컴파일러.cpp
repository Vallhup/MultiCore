#include <iostream>
#include <thread>
#include <mutex>

volatile bool g_ready{ false };
volatile int g_data{ 0 };

void Sender() 
{
	int temp{ 0 };
	std::cin >> temp;
	g_data = temp;
	g_ready = true;
}

void Receiver()
{
	long long count{ 0 };

	while (false == g_ready) {
		count++;
	}

	std::cout << "I got " << g_data << std::endl;
	std::cout << "count = " << count << std::endl;
}

int main()
{
	std::thread t2{ Receiver };
	std::thread t1{ Sender };
	
	t1.join();
	t2.join();
}