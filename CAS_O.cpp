#include <iostream> // input - output
#include <thread>	// for threads work
#include <chrono>	// time
#include <fstream>	// file input - output
#include <unistd.h>	// data type
#include <atomic>	// for atomic

using namespace std;

const unsigned long long startNRuns = 100000; 								// test repeats for data size L1
unsigned long long cashL1LineSize = sysconf (_SC_LEVEL1_DCACHE_LINESIZE); 	// get info L1d line size
unsigned long long cashL1Size = sysconf (_SC_LEVEL1_DCACHE_SIZE);			// get info L1d size
unsigned long long cashL2Size = sysconf (_SC_LEVEL2_CACHE_SIZE);			// get info L2 size
unsigned long long cashL3Size = sysconf (_SC_LEVEL3_CACHE_SIZE);			// get info L3 size
const unsigned long long startDataSize = 1536;								// 1536 * sizeof(int) == 6Kb 
const unsigned long long startCycleStep = (cashL1Size - startDataSize*sizeof(atomic<int>))/8/sizeof(atomic<int>); // for 8 sections

int exp = 0;	// atomic functions argument
int des = 1;	// atomic functions argument

atomic<bool> flagCPU1(false);	// sync flag for cpu1
atomic<bool> flagCPU2(false);	// sync flag for cpu2
atomic<bool> flagCPU3(false);	// sync flag for cpu3

unsigned long long testSizeBuffer = 33554432; // 33554432 * sizeof(int) / 1024 / 1024 = 128 Mb
atomic<int> * buffer = (atomic<int> *) malloc(testSizeBuffer*sizeof(atomic<int>)); // buffer with tested data

ofstream fout; // stream write results to the file
//======================================================================
//======================================================================
unsigned long long dataSizeScaleTune (unsigned long long dataSize) // dataSize change for test speed
{
	unsigned long long cycleStep = startCycleStep;							
	if (dataSize >= 33554432) cycleStep = 117440512;  // > 128M  
	if (dataSize >= cashL3Size/(sizeof(atomic<int>)) && dataSize < 33554432) cycleStep = (33554432 - cashL3Size/(sizeof(atomic<int>)))/4;     // L3 -> 128M
	if (dataSize >= cashL2Size/(sizeof(atomic<int>)) && dataSize < cashL3Size/(sizeof(atomic<int>))) cycleStep = (cashL3Size/(sizeof(atomic<int>)) - cashL2Size/(sizeof(atomic<int>)))/8; // L2 -> L3
	if (dataSize >= cashL1Size/(sizeof(atomic<int>)) && dataSize < cashL2Size/(sizeof(atomic<int>))) cycleStep = (cashL2Size/(sizeof(atomic<int>)) - cashL1Size/(sizeof(atomic<int>)))/8; // L1 -> L2
	return cycleStep;
}
//======================================================================
int nrunsScaleTune (unsigned long long dataSize) // data runs change for test speed
{
	int nruns = startNRuns;
	if (dataSize > 33554432) nruns = 2; 		//  > 128M  
	if (dataSize <= 33554432 && dataSize > cashL3Size/(sizeof(atomic<int>))) nruns = 10;    		// L3 -> 128M
	if (dataSize <= cashL3Size/(sizeof(atomic<int>)) && dataSize > cashL2Size/(sizeof(atomic<int>))) nruns = 10;     	// L2 -> L3
	if (dataSize <= cashL2Size/(sizeof(atomic<int>)) && dataSize > cashL1Size/(sizeof(atomic<int>))) nruns = 1000;      // L1 -> L2
	return nruns;
}
//======================================================================
void storeData(void)  // write test buffer data size to the file
{
	unsigned long long dataSize = startDataSize;
	while (dataSize <= testSizeBuffer)
		{
			fout << "arrData = [arrData " << dataSize*sizeof(atomic<int>) << "];" << endl;
			dataSize += dataSizeScaleTune(dataSize); // data + data step
		}
}
//======================================================================
//======================================================================
void inline clflush(atomic<int>* buffer, unsigned long long testSizeBuffer) // flush all cashes
{
  unsigned long long addr,passes,linesize;
  addr = (unsigned long long) buffer;
  linesize = cashL1LineSize;
  __asm__ __volatile__("mfence;"::: "memory"); 
  for(passes = (sizeof(atomic<int>)*testSizeBuffer/linesize);passes>0;passes--)
	{
	  __asm__ __volatile__("clflush (%%rax);":: "a" (addr));
	  addr+=linesize;
	}
  __asm__ __volatile__("mfence;"::: "memory"); 
}
//======================================================================
/// unCAS function, always unsuccessful CAS
inline void unsuccessfulCASAsm(atomic<int> *ptr, atomic<int> *old_val, int *new_val, int *ret_val)
{
	asm volatile("lock\n\tcmpxchg %1,%2"
					:"=a"(*ret_val)
					:"r"(*new_val),"m"(ptr),"0"(*old_val)
					:"memory"
					);
}
//======================================================================
//======================================================================
void * owned_local (void * arg) // prepair for state 
{
	auto get_time = std::chrono::steady_clock::now;
    decltype(get_time()) start, end;
    unsigned long long dataSize = startDataSize;
	double elapsed = 0.0;
	double sumTime = 0.0;
	int nruns = startNRuns;
	srand(time(0));
	while (dataSize <= testSizeBuffer)
		{   
			for (auto i = 0; i < nruns; i++) 
				{
					for (auto j = 0; j < dataSize; j++)
						{
							buffer[j].store(1+rand()%100);  // local modified, another cashes invalid
						}
					flagCPU1.store(true);
					while(flagCPU1.load() == true) {}
					start = get_time();
					for (auto j = 0; j < dataSize; j++)
						{
							buffer[j].compare_exchange_weak(exp, des);  // CAS
							//buffer[j].fetch_add(des); 				// FAA
							//buffer[j].store(10011);					// store
							//buffer[j].load();							// load
							//buffer[j].exchange(des);  				// SWP
							//unsuccessfulCASAsm(&buffer[j],&buffer[j],&exp,&des);  // unCAS
						}
					end = get_time();
					elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
					sumTime += elapsed;
				}
			fout << "latencyLocalCore = [latencyLocalCore " << sumTime/nruns/(dataSize*sizeof(atomic<int>)) << "];" << endl;
			fout << "bandwidthLocalCore = [bandwidthLocalCore " << (((dataSize*sizeof(atomic<int>))/(sumTime/nruns))*1000000000)/1048576 << "];" << endl;
			cout << "Local data = " << dataSize*sizeof(atomic<int>) << endl;
			dataSize += dataSizeScaleTune(dataSize);
			nruns = nrunsScaleTune(dataSize);
			sumTime = 0.0;
		}
}
//======================================================================
//CPU1 read, cpu1 cash Shared, cpu2 cash Owned
void * cpu1_read (void * arg) 
{
	auto get_time = std::chrono::steady_clock::now;
    decltype(get_time()) start, end;
    unsigned long long dataSize = startDataSize;
	int nruns = startNRuns;
	while (dataSize <= testSizeBuffer)
		{
			for (auto i = 0; i < nruns; i++) 
				{
					while (flagCPU1.load() == false) {}
					for (auto j = 0; j < dataSize; j++)
						{
							buffer[j].load();	// read, cpu1 cash Shared, cpu2 cash Owned
						}
					flagCPU1.store(false);
				}
			dataSize += dataSizeScaleTune(dataSize);
			nruns = nrunsScaleTune(dataSize);
		}
}
//=======================================================================
//======================================================================
//CPU2
void * owned_local2 (void * arg) // prepair for state 
{
	auto get_time = std::chrono::steady_clock::now;
    decltype(get_time()) start, end;
    unsigned long long dataSize = startDataSize;
	int nruns = startNRuns;
	srand(time(0));
	while (dataSize <= testSizeBuffer)
		{   
			for (auto i = 0; i < nruns; i++) 
				{
					while (flagCPU2.load() == false) {}
					for (auto j = 0; j < dataSize; j++)
						{
							buffer[j].store(1+rand()%100);  // modified, another invalid
						}
					flagCPU1.store(true);
					flagCPU2.store(false);
				}
			dataSize += dataSizeScaleTune(dataSize);
			nruns = nrunsScaleTune(dataSize);
		}
}
//======================================================================
//CPU1
void * meas_cpu1(void * arg) 
{
	auto get_time = std::chrono::steady_clock::now;
    decltype(get_time()) start, end;
    unsigned long long dataSize = startDataSize;
	int nruns = startNRuns;
	while (dataSize <= testSizeBuffer)
		{
			for (auto i = 0; i < nruns; i++) 
				{
					while (flagCPU1.load() == false) {}
					for (auto j = 0; j < dataSize; j++)
						{
							buffer[j].load(); // cpu2 Owned
						}
					flagCPU3.store(true);
					flagCPU1.store(false);
				}
			dataSize += dataSizeScaleTune(dataSize);
			nruns = nrunsScaleTune(dataSize);
		}
}
//======================================================================
//CPU3
void * meas_cpu3(void * arg) 
{
	auto get_time = std::chrono::steady_clock::now;
    decltype(get_time()) start, end;
    unsigned long long dataSize = startDataSize;
	double elapsed = 0.0;
	double sumTime = 0.0;
	int nruns = startNRuns;
	while (dataSize <= testSizeBuffer)
		{
			for (auto i = 0; i < nruns; i++) 
				{
					while (flagCPU3.load() == false) {}
					start = get_time();
					for (auto j = 0; j < dataSize; j++)
						{
							buffer[j].compare_exchange_weak(exp, des);  // CAS
							//buffer[j].fetch_add(des); 				// FAA
							//buffer[j].store(10011);					// store
							//buffer[j].load();							// load
							//buffer[j].exchange(des);  				// SWP
							//unsuccessfulCASAsm(&buffer[j],&buffer[j],&exp,&des);  // unCAS
						}
					end = get_time();
					elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
					sumTime += elapsed;
					flagCPU2.store(true);
					flagCPU3.store(false);
				}
			fout << "latencyCASMNearCore = [latencyCASMNearCore " << sumTime/nruns/(dataSize*sizeof(atomic<int>)) << "];" << endl;
			fout << "bandwidthCASMNearCore = [bandwidthCASMNearCore " << (((dataSize*sizeof(atomic<int>))/(sumTime/nruns))*1000000000)/1048576 << "];" << endl;
			cout << "Near  с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
			dataSize += dataSizeScaleTune(dataSize);
			nruns = nrunsScaleTune(dataSize);
			sumTime = 0.0;
		}
}
//======================================================================
//======================================================================
int main(int argc, const char *argv[])
{
	if (buffer == NULL) // can we get memory 
		{
			cout << "Не удалось выделить память" << endl;
			exit (1);
		}
	srand(time(0));
	for (auto j = 0; j <= testSizeBuffer; j++)
		{
			buffer[j].store(1+rand()%100); // fill the buffer random values from 1 to 100
		}
	//==================================================================
	fout.open("CAS_O_cpu2_cpu3_cpu1.m"); // open stream for writing in file
	// Architecture =====================================================
	long mempagesize = sysconf(_SC_PAGESIZE); // get info
	cout << "Memory pagesize on this machine : " << mempagesize << " bytes" << endl;
	if ( testSizeBuffer*sizeof(atomic<int>) >= (1024*1024*1024)) 
		{
			cout << "Test data size : " << testSizeBuffer*sizeof(atomic<int>)/(1024*1024*1024) << " Gbytes" << endl;
			fout << "%Test data size : " << testSizeBuffer*sizeof(atomic<int>)/(1024*1024*1024) << " Gbytes" << endl;
		} 
	else if (testSizeBuffer*sizeof(atomic<int>) >= (1024*1024))
		{
			cout << "Test data size : " << testSizeBuffer*sizeof(atomic<int>)/(1024*1024) << " Mbytes" << endl;
			fout << "%Test data size : " << testSizeBuffer*sizeof(atomic<int>)/(1024*1024) << " Mbytes" << endl;
		}
	else 
		{
			cout << "Test data size : " << testSizeBuffer*sizeof(atomic<int>)/1024 << " Kbytes" << endl;
			fout << "%Test data size : " << testSizeBuffer*sizeof(atomic<int>)/1024 << " Kbytes" << endl;
		}
	//==================================================================
	cout << "======== Cash L1 =====================" << endl;
	cout << "Cash level L1 string size : " << cashL1LineSize << " bytes" << endl; // get info
	cout << "Cash  L1  size : " << cashL1Size / 1024 << " Kilobytes" << endl;
	fout << "%L1 = " << cashL1Size/1024 << " Kb" << endl;
	cout << "Cash  L1  associativity : " << sysconf (_SC_LEVEL1_DCACHE_ASSOC) << endl;
	cout << "======== Cash L2 =====================" << endl;
	int cashL2LineSize = sysconf (_SC_LEVEL2_CACHE_LINESIZE);
	cout << "Cash level L2 string size : " << cashL2LineSize << " bytes" << endl;
	cout << "Cash level L2 size : " << cashL2Size/1024 << " Kilobytes" << endl;
	fout << "%L2 = " << cashL2Size/1024 << " Kb" << endl;
	cout << "Cash level L2 associativity : " << sysconf (_SC_LEVEL2_CACHE_ASSOC) << endl;
	cout << "======================================" << endl;
	if (cashL3Size == 0) 
		{
			cout << "This machine without cash L3!" << endl;
			cashL3Size = 8388608;
		}
	else 
		{ 
			cout << "Cash level L3 size : " << cashL3Size/1024*1024 << " Mbytes" << endl;
			fout << "%L3 = " << cashL3Size/1024*1024 << " Mb" << endl;
		}
	//==================================================================
	fout << "arrData = [];" << endl;
	storeData(); // store data sizes array in matlab file
	//==================================================================
	fout << "latencyLocalCore = [];" << endl;
	fout << "bandwidthLocalCore = [];" << endl;
	fout << "latencyCASMNearCore = [];" << endl;
	fout << "bandwidthCASMNearCore = [];" << endl;
	//==================================================================
	time_t myTime;
	myTime = time(NULL);
	cout << "Test start: " << ctime(&myTime) << endl;
	fout << "%Test start: " << ctime(&myTime) << endl;
	//==================================================================
	pthread_t threads[2];
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	cpu_set_t mask;
	//==================================================================
	//==================================================================
	//  two threads=====================================================
	// measuring for CPU2 local core
	flagCPU1.store(false);
	clflush(buffer,testSizeBuffer);
	for (int i = 1; i <= 2; i++) 
	{	  
		CPU_ZERO(&mask);
		CPU_SET(i, &mask);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
		sched_setaffinity(0,sizeof(cpu_set_t),&mask);
		if ( i == 1 )
		{
			
			pthread_create(&threads[i],&attr, cpu1_read, NULL);
		}
		if ( i == 2 )
		{
			pthread_create(&threads[i],&attr, owned_local, NULL);
		}
	}
	for (int i = 1; i <= 2; i++) 
	{
		pthread_join(threads[i],NULL);
	}
	//==================================================================
	//  two threads=====================================================
	// measuring for cpu3 near core
	flagCPU1.store(false);
	flagCPU2.store(true);
	flagCPU3.store(false);
	clflush(buffer,testSizeBuffer);
	//int i = 0;
	for (int i = 1; i <= 3; i++) 
	{	  
		CPU_ZERO(&mask);
		CPU_SET(i, &mask);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
		sched_setaffinity(0,sizeof(cpu_set_t),&mask);
		if ( i == 1 )
		{
			
			pthread_create(&threads[i],&attr, meas_cpu1, NULL);
		}
		if ( i == 2 )
		{
			pthread_create(&threads[i],&attr, owned_local2, NULL);
		}
		if ( i == 3 )
		{
			pthread_create(&threads[i],&attr, meas_cpu3, NULL);
		}
	}
	for (int i = 1; i <= 3; i++) 
	{
		pthread_join(threads[i],NULL);
	}
	//==================================================================
	//  two threads=====================================================
	// measuring for cpu1 remote core
	flagCPU1.store(false);
	flagCPU2.store(true);
	flagCPU3.store(false);
	clflush(buffer,testSizeBuffer);
	//int i = 0;
	for (int i = 1; i <= 3; i++) 
	{	  
		CPU_ZERO(&mask);
		CPU_SET(i, &mask);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
		sched_setaffinity(0,sizeof(cpu_set_t),&mask);
		if ( i == 1 )
		{
			
			pthread_create(&threads[i],&attr, meas_cpu3, NULL);
		}
		if ( i == 2 )
		{
			pthread_create(&threads[i],&attr, owned_local2, NULL);
		}
		if ( i == 3 )
		{
			pthread_create(&threads[i],&attr, meas_cpu1, NULL);
		}
	}
	for (int i = 1; i <= 3; i++) 
	{
		pthread_join(threads[i],NULL);
	}
	//==================================================================
	//================================================================== 
	// write to the file for plotting
	// latency
	fout << "FigH = figure ('Position',get(0,'Screensize'));" << endl;
	fout << "semilogx(arrData, latencyLocalCore ,'s-',arrData, latencyCASMNearCore ,'>-',arrData, latencyCASMNearCore2 ,'o-','LineWidth',3)" << endl;
	fout << "titleString = sprintf('CAS, O, CPU2 (local), CPU3, CPU1');" << endl;
	fout << "title (titleString);" << endl;
	fout << "ylabel('latency [ns]')" << endl;
	fout << "%ylim([5.5 7.5]);" << endl;
	fout << "xlabel('data size [bytes]')" << endl;
	fout << "%ylim([10 20]);" << endl;
	fout << "legend('CAS  CPU2', 'CAS  CPU3',  'CAS CPU1',  'Location', 'Best')  " << endl;
	fout << "grid on" << endl;
	//==================================================================
	// bandwidth
	fout << "FigH = figure ('Position',get(0,'Screensize'));" << endl;
	fout << "semilogx(arrData, bandwidthLocalCore ,'s-',arrData, bandwidthCASMNearCore ,'>-',arrData, bandwidthCASMNearCore2 ,'o-','LineWidth',3)" << endl;
	fout << "titleString = sprintf('CAS, O, CPU2 (local), CPU3, CPU1');" << endl;
	fout << "title (titleString);" << endl;
	fout << "ylabel('bandwidth [Mb/s]')" << endl;
	fout << "%ylim([0 600]);" << endl;
	fout << "xlabel('data size [bytes]')" << endl;
	fout << "%ylim([10 20]);" << endl;
	fout << "legend('CAS  CPU2', 'CAS  CPU3',  'CAS CPU1',  'Location', 'Best')  " << endl;
	fout << "grid on" << endl;
	//==================================================================
	time_t myTime2;
	myTime2 = time(NULL);
	cout << "Test finish: " << ctime(&myTime2) << endl;
	fout << "%Test finish: " << ctime(&myTime2) << endl;
	fout.close();
	return 0;
}
