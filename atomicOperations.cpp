#include <iostream>
#include <thread>
//#include "hpctimer.c"



using namespace std;

static const long long testRuns=10000000;
static const int num_threads = 2;

uint64_t oldValue1 = 5;
uint64_t oldValue2 = 4;
uint64_t newValue = 0;
uint64_t *pAddr = &oldValue1;
uint64_t value = 1;

pthread_barrier_t open_barrier;

      void threadFunctionCAS() {
		  uint64_t startTime, finishTime, resultTime, average = 0;
		  pthread_barrier_wait(&open_barrier); // с этого места одновременный старт потоков
          for (int i = 0; i < testRuns; ++i )
		{
			__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (startTime) :: "rdx");
		
			__sync_val_compare_and_swap(pAddr,oldValue2,newValue); // cas C++
		
			__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (finishTime) :: "rdx");
			resultTime = finishTime - startTime;
			average += resultTime;
		}
		cout << "Average CAS C++ = " << average/testRuns << " ticks" << endl;
		average = 0;
//=============================================================================================
//=============================================================================================
		pthread_barrier_wait(&open_barrier); // с этого места одновременный старт потоков
		for (int i = 0; i < testRuns; ++i )
		{
			__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (startTime) :: "rdx");
		
			// ============== нужен ли здесь барьер?
			asm volatile("lock\n\tcmpxchgq %1, %2" // cas assembler
				: "=a" (newValue)
				: "r"  (oldValue1), "m"(*pAddr), "0"(oldValue2)
				: "memory"
				);
			// ============== нужен ли здесь барьер ?
		
			__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (finishTime) :: "rdx");
			resultTime = finishTime - startTime;
			average += resultTime;
		}	
	cout << "Average CAS Assembler = " << average/testRuns << " ticks" << endl;
     }
     
     
 //==============================================================================================
	void threadFunctionFAA()
	{
		uint64_t startTime, finishTime, resultTime, average = 0;
		pthread_barrier_wait(&open_barrier); // с этого места одновременный старт потоков
		for (int i = 0; i < testRuns; ++i )
		{

			__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (startTime) :: "rdx");
		
			__sync_fetch_and_add(pAddr, value); // faa C++
		
			__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (finishTime) :: "rdx");
			resultTime = finishTime - startTime;
			average += resultTime;
		}
		cout << "Average FAA C++ = " << average/testRuns << " ticks" << endl;
//===============================================================================================
		average = 0;
		pthread_barrier_wait(&open_barrier); // с этого места одновременный старт потоков
		for (int i = 0; i < testRuns; ++i )
		{
			__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (startTime) :: "rdx");
			
			// ============== нужен ли здесь барьер?
			asm volatile ("lock\n\tcmpxchgq %0, %1" // faa assembler
			: "+r" (value), "+m" (*pAddr) : : "memory");
			// ============== нужен ли здесь барьер ?
		
			__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (finishTime) :: "rdx");
			resultTime = finishTime - startTime;
			average += resultTime;
		}
		cout << "Average FAA Assembler = " << average/testRuns << " ticks" << endl;
		
}
 
int main() 
{
         thread t[num_threads];
		 cpu_set_t mask;
		 pthread_barrier_init(&open_barrier, NULL, num_threads );
		// hpctimer_t myTimer1;
		// myTimer1.initialize();
		// double time1 = myTimer1.wtime();
		 int error = 0;
		// double time2 = myTimer1.wtime();
		// cout << "Result time = " << time2 - time1 << endl;
         for (int i = 0; i < num_threads; ++i) 
         {
			 CPU_ZERO(&mask);
			 CPU_SET(i, &mask);
			 error = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
             t[i] = thread(threadFunctionCAS);
         }
         for (int i = 0; i < num_threads; ++i) 
         {
             t[i].join();
         }
   return 0;
}
