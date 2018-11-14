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
FILE *outputResult;


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
// не уверен, что FAA правильно сделал
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
	bool outInFile = (bool*) args;
	int scale = 64;
	int cycleStep = 1;
	if ( dataSize > 4096 && dataSize <= 1000000 ) { scale = 5000; }
	if ( dataSize > 1000000 && dataSize <= 1000000000 ) { scale = 90000; cycleStep = 1024; }
	if ( dataSize > 1000000000 ) { scale = 1000000; cycleStep = 8192; }
	for ( long long testSizeBuffer = 0; testSizeBuffer <= dataSize; testSizeBuffer += scale )
	{
		char * buffer = (char*) malloc(testSizeBuffer);
		if (buffer == NULL) 
		{
			cout << "Не удалось выделить память" << endl;
			exit (1);
		}
		//clflush(buffer,dataSize,cpuinfo);
		//cout << "Идет заполнение буфера" << endl;
		for (int ix = 0; ix < testSizeBuffer; ix++)
		{
			//buffer[ix] = rand() % 26 + 'a'; // раскомментить после отладки
			buffer[ix] = 'a'; // для отладки, быстрое заполнение буфера, закоментить после отладки
		}
		// cout << "Буфер заполнен" << endl;
		char returnValue = 'a';
		atomic <char> counter;
		buffer[0] = counter.load(memory_order_relaxed);
		double startTime, finishTime, resultTime;
		//fputs("%Размер буфера данных = ", outputResult);
		//fprintf(outputResult,"%lld", testSizeBuffer);
		//fputs(" byte", outputResult);
		// fputs("\n",outputResult);
		//pthread_barrier_wait(&open_barrier);
		//warmUpTLB();
//======================================================================
		pthread_barrier_wait(&open_barrier);
		startTime = getTime();
		for ( int j = 0; j < testSizeBuffer; j = j + cycleStep )
		{
			functionCASC(buffer[j], &counter);
		}
		finishTime = getTime();
		resultTime = finishTime - startTime;
		//cout << "CAS C++ resultTime = " << resultTime << " sec" << endl;
		if (outInFile)
		{
			fputs("arrCasCTime = [arrCasCTime ", outputResult);
			fprintf(outputResult,"%f", resultTime);
			fputs("];", outputResult);
			fputs("\n",outputResult);
			fputs("arrData = [arrData ", outputResult);
			fprintf(outputResult,"%lld", testSizeBuffer);
			fputs("];", outputResult);
			fputs("\n",outputResult);
		}
//======================================================================
		//pthread_barrier_wait(&open_barrier); 
		startTime = getTime();
		for ( int j = 0; j < testSizeBuffer; j = j + cycleStep )
		{
			functionCASAsm(buffer, &buffer[j], &buffer[j], &returnValue);
		}
		finishTime = getTime();
		resultTime = finishTime - startTime;
		if (outInFile)
		{
			fputs("arrCasAsmTime = [arrCasAsmTime ", outputResult);
			fprintf(outputResult,"%f", resultTime);
			fputs("];", outputResult);
			fputs("\n",outputResult);
		}
//======================================================================
		counter.store(0);
		//pthread_barrier_wait(&open_barrier); 
		startTime = getTime();
		for ( int j = 0; j < testSizeBuffer; j = j + cycleStep )
		{
			functionFAAC(&counter); // не уверен, что правильно сделан FAA
		}
		finishTime = getTime();
		resultTime = finishTime - startTime;
		if (outInFile)
		{
			fputs("arrFaaCTime = [arrFaaCTime ", outputResult);
			fprintf(outputResult,"%f", resultTime);
			fputs("];", outputResult);
			fputs("\n",outputResult);
		}
//======================================================================
		//pthread_barrier_wait(&open_barrier); 
		startTime = getTime();
		for ( int j = 0; j < testSizeBuffer; j = j + cycleStep )
		{
			functionFAAAsm(&buffer[j], buffer);
		}
		finishTime = getTime();
		resultTime = finishTime - startTime;
		if (outInFile)
		{
			fputs("arrFaaAsmTime = [arrFaaAsmTime ", outputResult);
			fprintf(outputResult,"%f", resultTime);
			fputs("];", outputResult);
			fputs("\n",outputResult);
		}
//======================================================================
		free(buffer);
	}
	return 0;
}

//======================================================================
int main() 
{
	outputResult = fopen ("resultOutput.m", "w");
	if (outputResult == NULL)
	{
		cout << "Не удалось открыть файл для записи" << endl;
	}
	time_t myTime;
	myTime = time(NULL);
	cout << "Test start: " << ctime(&myTime) << endl;
	fputs("%Test start: ", outputResult);
	fputs(ctime(&myTime), outputResult);
	cout << "============================================" << endl << endl;
	long mempagesize = sysconf(_SC_PAGESIZE);
    cout << "Threads : " << num_threads << endl;
    fputs("%Threads: ", outputResult);
	fprintf(outputResult,"%d",num_threads);
	fputs("\n",outputResult);
    //cout << "One test cycles : " << testRuns << endl;
    cout << "Memory pagesize on this machine : " << mempagesize << " bytes" << endl << endl;
    fputs("%Memory pagesize on this machine : ", outputResult);
    fprintf(outputResult,"%ld",mempagesize);
    fputs(" bytes", outputResult);
    fputs("\n",outputResult);
	cout << "Укажите размер буфера данных в байтах (max 2147483647 (2Gb)): " ;
	cin >> dataSize;
	cout << endl;
	fputs("%Buffer size for test : ", outputResult);
    fprintf(outputResult,"%llu", dataSize);
    fputs(" bytes", outputResult);
    fputs("\n",outputResult);
    fputs("arrData = [];", outputResult);
    fputs("\n",outputResult);
    fputs("arrCasCTime = [];", outputResult);
    fputs("\n",outputResult);
   // fputs("arrCasAsmData = [];", outputResult);
   // fputs("\n",outputResult);
    fputs("arrCasAsmTime = [];", outputResult);
    fputs("\n",outputResult);
   // fputs("arrFaaCData = [];", outputResult);
   // fputs("\n",outputResult);
    fputs("arrFaaCTime = [];", outputResult);
    fputs("\n",outputResult);
   // fputs("arrFaaAsmData = [];", outputResult);
    //fputs("\n",outputResult);
    fputs("arrFaaAsmTime = [];", outputResult);
    fputs("\n",outputResult);
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
		if ( i == 0 )
		{
			bool outInFile = true;
			pthread_create(&threads[i], &attr, test, &outInFile);
		}
		else
		{
			pthread_create(&threads[i], &attr, test, NULL);
		}
    }
    for (int i = 0; i < num_threads; ++i) 
    {
        pthread_join(threads[i],NULL);
    }
    time_t myTime2;
	myTime2 = time(NULL);
	cout << endl << "=============================================" << endl;
	cout << "Test finish: " << ctime(&myTime2) << endl;
	fputs("%Test finish: ", outputResult);
	fputs(ctime(&myTime2), outputResult);
	fputs("\n",outputResult);
	fputs("plot(arrData,arrCasCTime)", outputResult);
    fputs("\n",outputResult);
    fputs("hold on", outputResult);
    fputs("\n",outputResult);
    fputs("plot(arrData,arrCasAsmTime)", outputResult);
    fputs("\n",outputResult);
    fputs("plot(arrData,arrFaaCTime)", outputResult);
    fputs("\n",outputResult);
    fputs("plot(arrData,arrFaaAsmTime)", outputResult);
    fputs("\n",outputResult);
    fputs("grid on", outputResult);
    fputs("\n",outputResult);
    fputs("legend('CAS C','CAS Asm','FAA C','FAA Asm','Location','North')", outputResult);
    fputs("\n",outputResult);
    fputs("xlabel('Размер буфера данных в байтах')", outputResult);
    fputs("\n",outputResult);
    fputs("ylabel('Задержка в секундах')", outputResult);
    fputs("\n",outputResult);
	fclose(outputResult);
	return 0;
}


/** flushes content of buffer from all cache-levels
 * @param buffer pointer to the buffer
 * @param size size of buffer in Bytes
 * @return 0 if successful
 *         -1 if not available
 */
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

//CLFLUSH, 
//the WBINVD instruction
//operates on the entire cache, rather than a single cache line.
//The WBINVD instruction first writes back all cache lines that are dirty (in the modified or owned state) to main memory. After writeback is
//complete, the instruction invalidates all cache lines. 
//The INVD instruction is used to invalidate all cache lines. Unlike the WBINVD
//instruction, dirty cache lines are not written to main memory.

/**
 * flushes data from the specified cachelevel
 * @param level the cachelevel that should be flushed
 * @param num_flushes number of accesses to each cacheline
 * @param mode MODE_EXCLUSIVE: fill cache with dummy data in state exclusive
 *             MODE_MODIFIED:  fill cache with dummy data in state modified (causes write backs of dirty data later on)
 *             MODE_INVALID:   invalidate cache (requires clflush)
 *             MODE_RDONLY:    fill cache with valid dummy data, does not perform any write operations, state can be exclusive or shared/forward
 * @param buffer pointer to a memory area, size of the buffer has to be 
 *               has to be larger than 2 x sum of all cachelevels <= level
 */
