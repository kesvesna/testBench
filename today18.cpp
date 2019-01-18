#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <mutex>
#include <string.h>
#include <atomic>

using namespace std;
using namespace std::chrono;

long long warmUpOperations = 10000;
long long iterations = 1000;
long long testRuns = 5;
long long threadsNumber = 2;
long long maxThreadsNumber = thread::hardware_concurrency();
bool timeResultsIsStored = false;
mutex threadsMutex;
ofstream fout;
pthread_barrier_t open_barrier;
atomic<bool> flag (false);

//struct alignData {
//	int structInt =3;
//	char paddingData[60] = {0};
//}firstAlignData;
//int globalVariable = 4;
//int * ptrGlobalVariable = &globalVariable;

long long testSizeBuffer = 1; // 2Gb = 33554432 testSizeBuffer, 2Mb = 32768 testSizeBuffer, 16Kb = 256
int * buffer = (int*) malloc(testSizeBuffer*sizeof(int));


template <typename T>
inline void functionCASAsm (const T * pAddr1, const T * oldValue1, const T * newValue1, const T * returnValue1)
{
	asm volatile ("lock\n\tcmpxchgq %1, %2" //cas assembler
					: "=a" (returnValue1) : "r" (newValue1), "m" (pAddr1), "0" (oldValue1) : "memory"
					);
} 

template <typename T2>
inline void functionFAAAsm(const T2 * oldValue1, const T2 * pAddr1)
{
	asm volatile ("lock\n\tcmpxchgq %0, %1" // faa assembler
				: "+r" (oldValue1), "+m" (pAddr1) : : "memory");
}


static __inline__ unsigned long long rdtsc(void) // для замера времени (абстрактного)
{
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long )hi) << 32 );
}




void * testFunction1(void * arg)
{
	unsigned long long int startRdtsc1 =0, endRdtsc1=0, time1=0;
	for (int i = 0; i < iterations ; i++) 
	{
		while (flag.load() == false) {}
		startRdtsc1 = rdtsc();
		functionCASAsm(&buffer, &buffer, &buffer, &buffer);
		endRdtsc1 = rdtsc();
		time1 = time1 + (endRdtsc1 - startRdtsc1);
		flag.store(false);
	}
	cout << "Time I state = " << time1/iterations << endl;
}

void * testFunction2(void * arg)
{
	unsigned long long int startRdtsc2=0, endRdtsc2=0, time2=0;
	for (int i = 0; i < iterations ; i++) 
	{
		*buffer = 5;
		startRdtsc2 = rdtsc();
		functionCASAsm(&buffer, &buffer, &buffer, &buffer);
		endRdtsc2 = rdtsc();
		time2 = time2 + (endRdtsc2 - startRdtsc2);
		flag.store(true);
		while (flag.load() == true) {}
	}
	cout << "Time M state = " << time2/iterations << endl;
}



int main ()
{
	
	if (buffer == NULL) 
	{
		cout << "Не удалось выделить память" << endl;
		exit (1);
	}
	//memset(buffer, 3, sizeof(buffer));
	for (int ix = 0; ix < testSizeBuffer; ix++)
	{
		//buffer[ix] = rand() % 26 + 'a'; // раскомментить после отладки
		buffer[ix] = 'a'; // для отладки, быстрое заполнение буфера, закоментить после отладки
	}
	time_t myTime;
	myTime = time(NULL);
	cout << "Test start: " << ctime(&myTime) << endl;
	fout << "%Test start: " << ctime(&myTime) << endl;
	long mempagesize = sysconf(_SC_PAGESIZE);
	cout << "Memory pagesize on this machine : " << mempagesize << " bytes" << endl << endl;
	cout << "Max threads number: " << maxThreadsNumber << endl;
	cout << "Test runs: " << testRuns << endl;
	cout << "Iterations in one test: " << iterations << endl;
	cout << "Buffer size: " << testSizeBuffer*sizeof(int) << " bytes" << endl;
	cout << "==============================" << endl;
	for ( long long i = 1; i <= testRuns; ++i )
	{
		pthread_t threads[threadsNumber];
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_barrier_init(&open_barrier, NULL, threadsNumber);
		cpu_set_t mask;
		CPU_ZERO(&mask);
		CPU_SET(2, &mask);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
		sched_setaffinity(0,sizeof(cpu_set_t),&mask);
		pthread_create(&threads[0], &attr, testFunction1, NULL);
		pthread_create(&threads[1], &attr, testFunction2, NULL);
		pthread_join(threads[0],NULL);
		pthread_join(threads[1],NULL);
		//cout << "test " << i << " finished" << endl;
		cout << "==================================" << endl;
	}
	return 0;

}

///** flushes content of buffer from all cache-levels
 //* @param buffer pointer to the buffer
 //* @param size size of buffer in Bytes
 //* @return 0 if successful
 //*         -1 if not available
 //*/
//int inline clflush(void* buffer,unsigned long long size,cpu_info_t cpuinfo)
//{
  //#if defined (__x86_64__)
  //unsigned long long addr,passes,linesize;

  //if(!(cpuinfo.features&CLFLUSH) || !cpuinfo.clflush_linesize) return -1;
  
  //addr = (unsigned long long) buffer;
  //linesize = (unsigned long long) cpuinfo.clflush_linesize;

  //__asm__ __volatile__("mfence;"::: "memory"); 

  //for(passes = (size/linesize);passes>0;passes--){
      //__asm__ __volatile__("clflush (%%rax);":: "a" (addr));
      //addr+=linesize;
  //}

  //__asm__ __volatile__("mfence;"::: "memory"); 

  //#endif

  //return 0;
//}



