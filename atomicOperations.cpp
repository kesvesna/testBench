#include <iostream>
#include <unistd.h>
#include "hpctimer.c"
#include <atomic>


using namespace std;

static const long long testRuns=10000000;
static const int num_threads = 2;
pthread_barrier_t open_barrier;




void warmUpTLB ()
{
	
}


inline void functionCASC (int value, atomic<int> *counter) 
{
	do
	{}
	while(!counter->compare_exchange_weak(value,value+1)); // cas c++
}

inline void functionCASAsm (uint64_t*pAddr, uint64_t oldValue1, uint64_t oldValue2, uint64_t newValue) 
{
	asm volatile("lock\n\tcmpxchgq %1, %2" // cas assembler
				: "=a" (newValue) : "r"  (oldValue1), "m"(*pAddr), "0"(oldValue2) : "memory"
				);
}

inline void functionFAAC()
{
	
}	

inline void functionFAAAsm()
{
	
}

inline double getTime ()
{
	double timeLabel = hpctimer_getwtime();
	return timeLabel;
}


void* test (void* args)
{
	uint64_t oldValue1 = 5;
	uint64_t oldValue2 = 4;
	uint64_t newValue = 0;
	uint64_t *pAddr = &oldValue1;
	atomic <int> counter;
	int value = counter.load(std::memory_order_relaxed);
	warmUpTLB();
//======================================================================
	pthread_barrier_wait(&open_barrier); // с этого места одновременный старт потоков
	double startTime = getTime();
	for (int i = 0; i < testRuns; ++i)
	{
		functionCASC(value, &counter);
	}
	double finishTime = getTime();
	double resultTime = finishTime - startTime;
	cout << "function CASC resultTime = " << resultTime << " sec" << endl;
//======================================================================
	pthread_barrier_wait(&open_barrier); 
	startTime = getTime();
	for (int i = 0; i < testRuns; ++i)
	{
		functionCASAsm(pAddr, oldValue1, oldValue2, newValue);
	}
	finishTime = getTime();
	resultTime = finishTime - startTime;
	cout << "function CASAsm resultTime = " << resultTime << " sec" << endl;
//======================================================================
	functionFAAC();
	functionFAAAsm();
	hpctimer_free();
	return 0;
}
//======================================================================
int main() 
{
	long mempagesize = sysconf(_SC_PAGESIZE);
    cout << "Memory pagesize on this machine : " << mempagesize << " bytes" << endl;
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









//uint64_t oldValue1 = 5;
//uint64_t oldValue2 = 4;
//uint64_t newValue = 0;
//uint64_t *pAddr = &oldValue1;
//uint64_t value = 1;

		//void threadFunctionCAS() 
		//{
		  //uint64_t startTime, finishTime, resultTime, average = 0;
		  //pthread_barrier_wait(&open_barrier); // с этого места одновременный старт потоков
          //for (int i = 0; i < testRuns; ++i )
			//{
				//__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (startTime) :: "rdx");
		
				//functionCASC();
		
				//__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (finishTime) :: "rdx");
				//resultTime = finishTime - startTime;
				//average += resultTime;
			//}
			//cout << "Average CAS C++ = " << average/testRuns << " ticks" << endl;
			//average = 0;

////=============================================================================================
			//pthread_barrier_wait(&open_barrier); // с этого места одновременный старт потоков
			//for (int i = 0; i < testRuns; ++i )
			//{
				//__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (startTime) :: "rdx");
		
				//// ============== нужен ли здесь барьер?
				//asm volatile("lock\n\tcmpxchgq %1, %2" // cas assembler
					//: "=a" (newValue)
					//: "r"  (oldValue1), "m"(*pAddr), "0"(oldValue2)
					//: "memory"
					//);
				//// ============== нужен ли здесь барьер ?
		
				//__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (finishTime) :: "rdx");
				//resultTime = finishTime - startTime;
				//average += resultTime;
			 //}	
		//cout << "Average CAS Assembler = " << average/testRuns << " ticks" << endl;
     //}
     
     
 ////==============================================================================================
	//void threadFunctionFAA()
	//{
		//uint64_t startTime, finishTime, resultTime, average = 0;
		//pthread_barrier_wait(&open_barrier); // с этого места одновременный старт потоков
		//for (int i = 0; i < testRuns; ++i )
		//{

			//__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (startTime) :: "rdx");
		
			//__sync_fetch_and_add(pAddr, value); // faa C++
		
			//__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (finishTime) :: "rdx");
			//resultTime = finishTime - startTime;
			//average += resultTime;
		//}
		//cout << "Average FAA C++ = " << average/testRuns << " ticks" << endl;
////===============================================================================================
		//average = 0;
		//pthread_barrier_wait(&open_barrier); // с этого места одновременный старт потоков
		//for (int i = 0; i < testRuns; ++i )
		//{
			//__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (startTime) :: "rdx");
			//// ============== нужен ли здесь барьер?
			//asm volatile ("lock\n\tcmpxchgq %0, %1" // faa assembler
			//: "+r" (value), "+m" (*pAddr) : : "memory");
			//// ============== нужен ли здесь барьер ?
		
			//__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (finishTime) :: "rdx");
			//resultTime = finishTime - startTime;
			//average += resultTime;
		//}
		//cout << "Average FAA Assembler = " << average/testRuns << " ticks" << endl;
//}
