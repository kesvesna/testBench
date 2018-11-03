#include <iostream>

using namespace std;

#define testRuns 10000000
#define numberOfProcessors 2

	uint64_t oldValue1 = 5;
	uint64_t oldValue2 = 4;
	uint64_t newValue = 0, average = 0;
	uint64_t *pAddr = &oldValue1;
	uint64_t startTime, finishTime, resultTime;
	uint64_t value = 1;
	
	//=============================================================================================
	void *cas(void* args) {
	for (int i = 0; i < testRuns; ++i )
	{
// с этого места потоки должны запускаться одновременно, как этого достичь
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
	for (int i = 0; i < testRuns; ++i )
	{
// с этого места потоки должны запускаться одновременно, как этого достичь
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
	return 0;
	}
//==============================================================================================	
	
//==============================================================================================
	void *faa(void * args)
	{
		for (int i = 0; i < testRuns; ++i )
	{
// с этого места потоки должны запускаться одновременно, как этого достичь
		__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (startTime) :: "rdx");
		
		__sync_fetch_and_add(pAddr, value); // cas C++
		
		__asm__ volatile ("rdtsc\n\tshl $32, %%rdx\n\tor %%rdx, %%rax" : "=a" (finishTime) :: "rdx");
		resultTime = finishTime - startTime;
		average += resultTime;
	}
	cout << "Average FAA C++ = " << average/testRuns << " ticks" << endl;
//===============================================================================================
		average = 0;
		for (int i = 0; i < testRuns; ++i )
		{
// с этого места потоки должны запускаться одновременно, как этого достичь
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

int main ()
{
	
	pthread_t threads[numberOfProcessors];
	pthread_attr_t attr;
	cpu_set_t cpus;
	pthread_attr_init(&attr);
	CPU_ZERO(&cpus);
	for (int i = 0; i < numberOfProcessors; ++i)
	{
		CPU_SET(i, &cpus);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
		pthread_create(&threads[i], &attr,cas, NULL);
	}
	for (int i = 0; i < numberOfProcessors; ++i)
	{
		pthread_join(threads[i],NULL);
	}	
	cout << "================================================================" << endl;
	for (int i = 0; i < numberOfProcessors; ++i)
	{
		CPU_SET(i, &cpus);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
		pthread_create(&threads[i], &attr,faa, NULL);
	}
	for (int i = 0; i < numberOfProcessors; ++i)
	{
		pthread_join(threads[i],NULL);
	}
	
	return 0;
}

