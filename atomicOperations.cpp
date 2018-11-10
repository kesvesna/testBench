#include <iostream>
#include <unistd.h>
#include "hpctimer.c"
#include <atomic>

using namespace std;

static const long long testRuns = 10000000;
static const int num_threads = 2;
int dataSize = 0;
pthread_barrier_t open_barrier;

void warmUpTLB ()
{
	
}

inline void functionCASC (int value, atomic<int> *counter) 
{
	counter->compare_exchange_weak(value,value+1); // cas c++
}

inline void functionCASAsm (uint64_t*pAddr, uint64_t oldValue1, uint64_t oldValue2, uint64_t newValue) 
{
	asm volatile("lock\n\tcmpxchgq %1, %2" // cas assembler
				: "=a" (newValue) : "r"  (oldValue1), "m"(*pAddr), "0"(oldValue2) : "memory"
				);
}

inline void functionFAAC(atomic<int> *counter)
{
	counter->fetch_add(1); // faa c++
}	

inline void functionFAAAsm(uint64_t value, uint64_t *pAddr)
{
	asm volatile ("lock\n\tcmpxchgq %0, %1" // faa assembler
				: "+r" (value), "+m" (*pAddr) : : "memory");	
}

inline double getTime ()
{
	double timeLabel = hpctimer_getwtime();
	return timeLabel;
}

void* test (void* args)
{
	char * buffer = (char*) malloc(dataSize);
	if (buffer==NULL) 
	{
		cout << "Не удалось выделить память" << endl;
		exit (1);
	}
	//for (int ix = 0; ix < dataSize; ix++)
    //buffer[ix] = rand() % 26 + 'a';
	uint64_t oldValue1 = 5;
	uint64_t oldValue2 = 4;
	uint64_t newValue = 0;
	uint64_t *pAddr = &oldValue1;
	atomic <int> counter;
	int value = counter.load(memory_order_relaxed);
	double startTime, finishTime, resultTime;
	warmUpTLB();
//======================================================================
	pthread_barrier_wait(&open_barrier);
	startTime = getTime();
	for (int i = 0; i < testRuns; ++i)
	{
		functionCASC(value, &counter);
	}
	finishTime = getTime();
	resultTime = finishTime - startTime;
	cout << "CAS C++ resultTime = " << resultTime << " sec" << endl;
//======================================================================
	pthread_barrier_wait(&open_barrier); 
	startTime = getTime();
	for (int i = 0; i < testRuns; ++i)
	{
		functionCASAsm(pAddr, oldValue1, oldValue2, newValue);
	}
	finishTime = getTime();
	resultTime = finishTime - startTime;
	cout << "CAS Asm resultTime = " << resultTime << " sec" << endl;
//======================================================================
	counter.store(0);
	pthread_barrier_wait(&open_barrier); 
	startTime = getTime();
	for (int i = 0; i < testRuns; ++i)
	{
		functionFAAC(&counter);
	}
	finishTime = getTime();
	resultTime = finishTime - startTime;
	cout << "FAA C++ resultTime = " << resultTime << " sec" << endl;
//======================================================================
	int valueFaa = 1;
	pthread_barrier_wait(&open_barrier); 
	startTime = getTime();
	for (int i = 0; i < testRuns; ++i)
	{
		functionFAAAsm(valueFaa, pAddr);
	}
	finishTime = getTime();
	resultTime = finishTime - startTime;
	cout << "FAA Asm resultTime = " << resultTime << " sec" << endl;
//======================================================================
	free(buffer);
	return 0;
}

//======================================================================
int main() 
{
	long mempagesize = sysconf(_SC_PAGESIZE);
    cout << "Threads : " << num_threads << endl;
    cout << "One test cycles : " << testRuns << endl;
    cout << "Memory pagesize on this machine : " << mempagesize << " bytes" << endl << endl;
	cout << "Укажите размер данных в байтах, максимум 2100000000: ";
	cin >> dataSize;
	cout << endl;
    pthread_t threads[num_threads];
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_barrier_init(&open_barrier, NULL, num_threads);
	cpu_set_t mask;
	CPU_ZERO(&mask);
    for (int i = 0; i < num_threads; ++i) 
    {	 
		CPU_SET(i, &mask);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
        pthread_create(&threads[i], &attr, test, NULL);
    }
    for (int i = 0; i < num_threads; ++i) 
    {
        pthread_join(threads[i],NULL);
    }
	return 0;
}
