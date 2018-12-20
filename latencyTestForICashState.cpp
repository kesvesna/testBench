#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <mutex>
#include <string.h>

using namespace std;
using namespace std::chrono;

long long warmUpOperations = 10000;
long long iterations = 10000;
long long testRuns = 1;
long long threadsNumber = 1;
long long maxThreadsNumber = 1;//thread::hardware_concurrency();
bool timeResultsIsStored = false;
bool dataSizeIsStored = false;
mutex threadsMutex;
ofstream fout;
pthread_barrier_t open_barrier;


struct alignData {
	int structInt;
	char paddingData[60] = {0};
};
//int globalVariable = 4;
//int * ptrGlobalVariable = &globalVariable;

long long testSizeBuffer = 500; // 2Gb = 33554432 testSizeBuffer, 2Mb = 32768 testSizeBuffer, 16Kb = 256
alignData * buffer = (alignData *) malloc(testSizeBuffer*sizeof(alignData));
int cashLineSize = 64;


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

void warmUp()
{
	int forWarmUp = 9; // рандомное значение
	int * pForWarmUp = &forWarmUp;
	for ( long long i = 0; i < warmUpOperations; ++i )
	{
		functionCASAsm (pForWarmUp,&forWarmUp,&forWarmUp,&forWarmUp);
	}
}

static __inline__ unsigned long long rdtsc(void) // для замера времени (абстрактного)
{
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long )hi) << 32 );
}

inline void invalidateCash(volatile alignData *ptr)
{
	//asm volatile("mfence;"::: "memory"); 
	for(int i = 0; i < testSizeBuffer; ++i)
	{
      //asm volatile("clflush (%%rax);":: "a" (ptr));
      asm volatile("clflush (%0);":: "a" (ptr));
      ptr++;
	}
	//asm volatile("mfence;"::: "memory"); 
}

inline void modifideCash (volatile alignData *ptr)
{
	for ( int i = 0; i < testSizeBuffer; ++i)
	{
		ptr->structInt = 5;
		ptr++;
	}
}

inline void exclusiveCash (volatile alignData *ptr)
{
	int tmpValue;
	for ( int i = 0; i < testSizeBuffer; ++i)
	{
		tmpValue = ptr->structInt;
		ptr++;
	}
}

void * testFunction(void * arg)
{
		unsigned long long int startRdtsc, endRdtsc;
		vector <double> casTimeVector, faaTimeVector;
		int bufferSize = 1;	
//======================================================================
		do {
		for ( long long i = 0; i < iterations; ++i )
		{
			//invalidateCash(buffer);
			//exclusiveCash(buffer);
			modifideCash(buffer);
			pthread_barrier_wait(&open_barrier);
			//warmUp();
			startRdtsc = rdtsc();
			for ( long long j = 0; j < bufferSize; ++j )
			{
				functionCASAsm (&buffer[j].structInt, &buffer[j].structInt, &buffer[j].structInt, &buffer[j].structInt);
			}
			endRdtsc = rdtsc();
			casTimeVector.push_back((double)(endRdtsc-startRdtsc)/iterations);
			if ( !dataSizeIsStored )
			{
				fout << "arrDataSize = [ arrDataSize " << bufferSize * cashLineSize << " ]; " << endl;
				dataSizeIsStored = true;
			}
//======================================================================
			//invalidateCash(buffer);
			//exclusiveCash(buffer);
			modifideCash(buffer);
			pthread_barrier_wait(&open_barrier);
			//warmUp();
			startRdtsc = rdtsc();
			for ( long long j = 0; j < bufferSize; ++j )
			{
				functionFAAAsm (&buffer[j].structInt, &buffer[j].structInt);
			}
			endRdtsc = rdtsc();
			faaTimeVector.push_back((double)(endRdtsc-startRdtsc)/iterations);
		}
		//cout << "test for data size " << bufferSize * 64 << " bytes finished" << endl;
		if ( bufferSize <= 256 )
		{
		bufferSize += 2;
		}
		else if ( bufferSize > 256 && bufferSize <= 32768 )
		{ bufferSize += 4; }
		else
		{
			bufferSize += 10;
		}
		double tempCasTime = 0;
		for ( double n : casTimeVector )
		{
			tempCasTime += n;
		}
		casTimeVector.clear();
		fout << "arrCasAsmTime = [ arrCasAsmTime " << tempCasTime << " ]; " << endl;
		double tempFaaTime = 0;
		for ( double n : faaTimeVector )
		{
			tempFaaTime += n;
		}
		faaTimeVector.clear();
		fout << "arrFaaAsmTime = [ arrFaaAsmTime " << tempFaaTime << " ]; " << endl;
		pthread_barrier_wait(&open_barrier);
		dataSizeIsStored = false;
	} 
	while ( bufferSize <= testSizeBuffer );
//======================================================================
	return 0;
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
		buffer[ix].structInt = 8; // для отладки, быстрое заполнение буфера, закоментить после отладки
		//cout << buffer[ix].structInt << endl;
	}
	fout.open("latencyTestForMCashState.m");
	time_t myTime;
	myTime = time(NULL);
	cout << "Test start: " << ctime(&myTime) << endl;
	fout << "%Test start: " << ctime(&myTime) << endl;
	long mempagesize = sysconf(_SC_PAGESIZE);
	cout << "Memory pagesize on this machine : " << mempagesize << " bytes" << endl << endl;
	cout << "Max threads number: " << maxThreadsNumber << endl;
	cout << "Test runs: " << testRuns << endl;
	cout << "Iterations in one test: " << iterations << endl;
	cout << "Buffer size: " << testSizeBuffer*sizeof(alignData) << " bytes" << endl;
	fout << "arrDataSize = [];" << endl;
	fout << "arrCasAsmTime = [];" << endl;
	fout << "arrFaaAsmTime = [];" << endl;
	//fout << "arrTotalResultCasTime = [];" << endl;
	//fout << "arrTotalResultFaaTime = [];" << endl;
	for ( long long i = 1; i <= testRuns; ++i )
	{
		do { 
		pthread_t threads[threadsNumber];
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_barrier_init(&open_barrier, NULL, threadsNumber);
		cpu_set_t mask;
		for (int i = 0; i < threadsNumber; ++i) 
		{	 
			CPU_ZERO(&mask);
			CPU_SET(i, &mask);
			pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
			sched_setaffinity(0,sizeof(cpu_set_t),&mask);
			pthread_create(&threads[i], &attr, testFunction, NULL);
		}
		for (int i = 0; i < threadsNumber; ++i) 
		{
			pthread_join(threads[i],NULL);
		}
		threadsNumber ++;
		timeResultsIsStored = false;
		} while ( threadsNumber <= maxThreadsNumber );// от одного до max возможного числа потоков
		threadsNumber = 0;
		cout << "test " << i << " finish" << endl;
	}
	time_t myTime2;
	myTime2 = time(NULL);
	cout << endl << "Test finish: " << ctime(&myTime2) << endl;
	fout << "%Test finish: " << ctime(&myTime2) << endl;
	fout << "%Memory pagesize on this machine : " << mempagesize << " bytes" << endl << endl;
	fout << "bufferSize = " << testSizeBuffer*sizeof(alignData) << ";" << endl;
	fout << "maxThreadsNumber = " << maxThreadsNumber << " ; " <<  endl;
	fout << "testRuns = " << testRuns << " ; " <<  endl;
	fout << "iterations = " << iterations << " ; " << endl;
	fout << "hold on" << endl;
	//fout << "axis([-1000 70000 -2 2])" << endl;
	fout << "plot(arrDataSize,arrCasAsmTime,'*-')" << endl;
	fout << "plot(arrDataSize,arrFaaAsmTime,'o-')" << endl;
	fout << "grid on" << endl;
	fout << "titleString = sprintf('latency test for Modified Cash, testRuns = %d, iterations = %d, buffer size = %d bytes', testRuns, iterations, bufferSize);" << endl;
	fout << "title (titleString);" << endl;
	fout << "legend('CasAsmTime','FaaAsmTime','Location','North')" << endl;
    fout << "ylabel('latency [ticks]')" << endl;
    fout << "xlabel('data size [bytes]')" << endl;
	fout.close();
	return 0;
}
