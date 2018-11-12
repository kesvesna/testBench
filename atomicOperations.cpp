#include <iostream>
#include <unistd.h>
#include "hpctimer.c"
#include <atomic>
#include <time.h>

using namespace std;

static const long long testRuns = 10000000;
static const int num_threads = 2;
long long dataSize = 0;
pthread_barrier_t open_barrier;

void warmUpTLB ()
{
	
}

inline void functionCASC (char  value, atomic<char> *counter) 
{
	counter->compare_exchange_weak(value,value+'1'); // cas c++
}

inline void functionCASAsm (char * pAddr, char * oldValue, char * newValue, char * returnValue) 
{
	asm volatile("lock\n\tcmpxchgq %1, %2" // cas assembler
				: "=a" (returnValue) : "r"  (newValue), "m"(pAddr), "0"(oldValue) : "memory"
				);
}

inline void functionFAAC(atomic<char> *counter)
{
	counter->fetch_add(1); // faa c++
}	

inline void functionFAAAsm(char * value, char *pAddr)
{
	asm volatile ("lock\n\tcmpxchgq %0, %1" // faa assembler
				: "+r" (value), "+m" (pAddr) : : "memory");	
}

inline double getTime ()
{
	double timeLabel = hpctimer_getwtime();
	return timeLabel;
}

void* test (void* args)
{
	char * buffer = (char*) malloc(dataSize);
	if (buffer == NULL) 
	{
		cout << "Не удалось выделить память" << endl;
		exit (1);
	}
	cout << "Идет заполнение буфера" << endl;
	for (int ix = 0; ix < dataSize; ix++)
    //buffer[ix] = rand() % 26 + 'a';
    buffer[ix] = 'a';
    cout << "Буфер заполнен" << endl;
	char returnValue = 'a';
	atomic <char> counter;
	buffer[0] = counter.load(memory_order_relaxed);
	double startTime, finishTime, resultTime;
	pthread_barrier_wait(&open_barrier);
	warmUpTLB();
//======================================================================
	pthread_barrier_wait(&open_barrier);
	startTime = getTime();
		for ( int j = 0; j < dataSize; ++j)
		{
			functionCASC(buffer[j], &counter);
		}
	finishTime = getTime();
	resultTime = finishTime - startTime;
	cout << "CAS C++ resultTime = " << resultTime << " sec" << endl;
//======================================================================
	pthread_barrier_wait(&open_barrier); 
	startTime = getTime();
		for ( int j = 0; j < dataSize; ++j)
		{
			functionCASAsm(buffer, &buffer[j], &buffer[j], &returnValue);
		}
	finishTime = getTime();
	resultTime = finishTime - startTime;
	cout << "CAS Asm resultTime = " << resultTime << " sec" << endl;
//======================================================================
	counter.store(0);
	pthread_barrier_wait(&open_barrier); 
	startTime = getTime();
	for ( int j = 0; j < dataSize; ++j)
	{
		functionFAAC(&counter);
	}
	finishTime = getTime();
	resultTime = finishTime - startTime;
	cout << "FAA C++ resultTime = " << resultTime << " sec" << endl;
//======================================================================
	pthread_barrier_wait(&open_barrier); 
	startTime = getTime();
		for ( int j = 0; j < dataSize; ++j)
		{
			functionFAAAsm(&buffer[j], buffer);
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
	time_t myTime;
	myTime = time(NULL);
	cout << "Test start: " << ctime(&myTime) << endl;
	cout << "============================================" << endl << endl;
	long mempagesize = sysconf(_SC_PAGESIZE);
    cout << "Threads : " << num_threads << endl;
    //cout << "One test cycles : " << testRuns << endl;
    cout << "Memory pagesize on this machine : " << mempagesize << " bytes" << endl << endl;
	cout << "Укажите размер буфера данных в байтах (max 2147483647 (2Gb)): " ;
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
    time_t myTime2;
	myTime2 = time(NULL);
	cout << endl << "=============================================" << endl;
	cout << "Test finish: " << ctime(&myTime2) << endl;
	return 0;
}
