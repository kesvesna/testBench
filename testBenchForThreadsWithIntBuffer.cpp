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
long long iterations = 1000;
long long testRuns = 50;
long long threadsNumber = 1;
long long maxThreadsNumber = thread::hardware_concurrency();
bool timeResultsIsStored = false;
mutex threadsMutex;
ofstream fout;
pthread_barrier_t open_barrier;

long long testSizeBuffer = 16000; //2147483647; // 2Gb
int * buffer = (int *) malloc(testSizeBuffer*sizeof(int));

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
	int forWarmUp = 5; // рандомное значение
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

void * testFunction(void * arg)
{
	long long operations = iterations;
	unsigned long long int startRdtsc, endRdtsc;
	vector <double> casTimeVector, faaTimeVector;	
//======================================================================
		pthread_barrier_wait(&open_barrier);
		//warmUp();
		startRdtsc = rdtsc();
		for ( long long i = 0; i < operations; ++i )
		{
			for ( long long j = 0; j < testSizeBuffer; ++j )
			{
				functionCASAsm (buffer,&buffer[j],&buffer[j],&buffer[j]);
				buffer++;
			}
		}
		endRdtsc = rdtsc();
		casTimeVector.push_back(operations/((double)(endRdtsc-startRdtsc)));
//======================================================================
		pthread_barrier_wait(&open_barrier);
		//warmUp();
		startRdtsc = rdtsc();
		for ( long long i = 0; i < operations; ++i)
		{
			for ( long long j = 0; j < testSizeBuffer; ++j )
			{
				functionFAAAsm (&buffer[j],buffer);
				buffer++;
			}
		}
		endRdtsc = rdtsc();
		faaTimeVector.push_back(operations/((double)(endRdtsc-startRdtsc)));
//======================================================================
	threadsMutex.lock(); // для поочередной записи, чтобы потоки не портили данные
	double tempCasTime = 0;
    for ( double n : casTimeVector ) // запись результатов замеров CAS от всех потоков
	{
		tempCasTime += n;
    }
    double tempFaaTime = 0;
    for ( double n : faaTimeVector ) // запись результатов замеров FAA от всех потоков
	{
		tempFaaTime += n;
    }
    threadsMutex.unlock();
    pthread_barrier_wait(&open_barrier); // гарантировано, 
    //что все потоки сделали запись времени в переменную temp
    threadsMutex.lock();
    if (!timeResultsIsStored) // запись среднего времени в файл только одним потоком
    {
		tempCasTime /= threadsNumber;
		tempFaaTime /= threadsNumber;
		//fout << "arrDataSize = [arrDataSize " << threadsNumber << "];" << endl;
		fout << "arrCasAsmTime = [arrCasAsmTime " << tempCasTime << "];" << endl;
		fout << "arrFaaAsmTime = [arrFaaAsmTime " << tempFaaTime << "];" << endl;
		timeResultsIsStored = true;
	}
    threadsMutex.unlock();
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
	for (int ix = 0; ix < testSizeBuffer; ++ix)
	{
		//buffer[ix] = rand() % 26 + 'a'; // раскомментить после отладки
		buffer[ix] = 4; // для отладки, быстрое заполнение буфера, закоментить после отладки
	}
	fout.open("testbenchIntBuffer.m");
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
	//fout << "arrDataSize = [];" << endl;
	fout << "arrCasAsmTime = [];" << endl;
	fout << "arrFaaAsmTime = [];" << endl;
	fout << "arrTotalResultCasTime = [];" << endl;
	fout << "arrTotalResultFaaTime = [];" << endl;
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
	fout << "bufferSize = " << testSizeBuffer*sizeof(int) << endl;
	fout << "maxThreadsNumber = " << maxThreadsNumber << " ; " <<  endl;
	fout << "testRuns = " << testRuns << " ; " <<  endl;
	fout << "arrThreadsNumbers = [];" << endl;
	fout << "for m=1:maxThreadsNumber" << endl;
	fout << "arrThreadsNumbers = [arrThreadsNumbers m];" << endl;
	fout << "end" << endl;
	fout << "tempCasTime = 0;" << endl;
	fout << "tempFaaTime = 0;" << endl;
	fout << "startPosition = 0;" << endl;
	fout << "endPosition = testRuns*maxThreadsNumber;" << endl;
	fout << "for m = 1:maxThreadsNumber" << endl;
	fout << "startPosition = startPosition + 1;" << endl;
	fout << "for n = startPosition: + maxThreadsNumber:endPosition" << endl;
	fout << "tempCasTime = tempCasTime + arrCasAsmTime(n);" << endl;
	fout << "tempFaaTime = tempFaaTime + arrFaaAsmTime(n);" << endl;
	fout << "end" << endl;
	fout << "tempCasTime = tempCasTime / testRuns;" << endl;
	fout << "tempFaaTime = tempFaaTime / testRuns;" << endl;
	fout << "arrTotalResultCasTime = [arrTotalResultCasTime tempCasTime];" << endl;
	fout << "arrTotalResultFaaTime = [arrTotalResultFaaTime tempFaaTime];" << endl;
	fout << "tempCasTime = 0;" << endl;
	fout << "tempFaaTime = 0;" << endl;
	fout << "end" << endl;
	fout << "hold on" << endl;
	//fout << "axis([-1000 70000 -2 2])" << endl;
	fout << "plot(arrThreadsNumbers,arrTotalResultCasTime,'*-')" << endl;
	fout << "plot(arrThreadsNumbers,arrTotalResultFaaTime,'o-')" << endl;
	fout << "grid on" << endl;
	fout << "titleString = sprintf('bandwidth test with Int Data Buffer, testRuns = %d, buffer size = %d bytes', testRuns, bufferSize)" << endl;
	fout << "title (titleString);" << endl;
	fout << "legend('CasIntBuffer','FaaIntBuffer')" << endl;
    fout << "ylabel('bandwidth [bytes/cycle]')" << endl;
    fout << "xlabel('threads numbers [pieces]')" << endl;
	fout.close();
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


