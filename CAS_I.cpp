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

atomic<bool> flagMState(false); // threads sync flag

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
//=======================================================================
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
/// measuring for CPU2, local cash of CPU2
void * invalid_local (void * arg)
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
			for (auto i = 0; i < nruns; i++) // test runs
				{
					for (auto j = 0; j < dataSize; j++) // test data size
						{
							buffer[j].store(1+rand()%100); // modified state
						}
					clflush(buffer,dataSize);				// invalid state
					start = get_time();						// get start time
					for (auto j = 0; j < dataSize; j++)
						{ 									// operations bank
							buffer[j].compare_exchange_weak(exp, des); // CAS
							//buffer[j].fetch_add(des); // FAA
							//buffer[j].store(10011);	// store
							//buffer[j].load();			// load
							//buffer[j].exchange(des);  // SWP
							//unsuccessfulCASAsm(&buffer[j],&buffer[j],&exp,&des);  // unCAS
						}
					end = get_time(); // get finish time
					elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(); // test time
					sumTime += elapsed; // summ all tests time
				}
			fout << "latencyLocalCore = [latencyLocalCore " << sumTime/nruns/(dataSize*sizeof(atomic<int>)) << "];" << endl; // write to the file latency
			fout << "bandwidthLocalCore = [bandwidthLocalCore " << (((dataSize*sizeof(atomic<int>))/(sumTime/nruns))*1000000000)/1048576 << "];" << endl; // write bandwidth
			cout << "Local data = " << dataSize*sizeof(atomic<int>) << endl;
			dataSize += dataSizeScaleTune(dataSize); // data + data step, next bigger size
			nruns = nrunsScaleTune(dataSize);	// change test runs to smaller size
			sumTime = 0.0;
		}
}
//======================================================================
//======================================================================
void * prep_I (void * arg) // prepair for I state 
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
					for (auto j = 0; j < dataSize; j++)
						{
							buffer[j].store(1+rand()%100); // modified state
						}
					clflush(buffer,dataSize);				// invalid state
					flagMState.store(true);
					while (flagMState.load() == true) {} // wait while another thread measuring
				}
			dataSize += dataSizeScaleTune(dataSize);
			nruns = nrunsScaleTune(dataSize);
		}
}
//======================================================================
// measure in state I for two threads
void * meas_cpu3(void * arg) 
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
					while (flagMState.load() == false) {} // wait ready for state I
					start = get_time();
					for (auto j = 0; j < dataSize; j++)
						{
							buffer[j].compare_exchange_weak(exp, des); // CAS
							//buffer[j].fetch_add(des); // FAA
							//buffer[j].store(10011);	// store
							//buffer[j].load();			// load
							//buffer[j].exchange(des);  // SWP
							//unsuccessfulCASAsm(&buffer[j],&buffer[j],&exp,&des);  // unCAS
						}
					end = get_time();
					elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
					sumTime += elapsed;			// total time
					flagMState.store(false);	// sync flag
				}
			// write latency time nanoseconds to the file for Matlab
			fout << "latencyCASMNearCore = [latencyCASMNearCore " << sumTime/nruns/(dataSize*sizeof(atomic<int>)) << "];" << endl;
			// transformation to the bandwidth Mbyte/sec and write to the file for Matlab
			fout << "bandwidthCASMNearCore = [bandwidthCASMNearCore " << (((dataSize*sizeof(atomic<int>))/(sumTime/nruns))*1000000000)/1048576 << "];" << endl;
			// the program is not frozen
			cout << "Near  с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
			// increase test data buffer size
			dataSize += dataSizeScaleTune(dataSize);
			// stay or decrease nruns for test speed up
			nruns = nrunsScaleTune(dataSize);
			// reset total time
			sumTime = 0.0;
		}
}
//======================================================================
void * meas_cpu2(void * arg) 
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
					while (flagMState.load() == false) {}
					start = get_time();
					for (auto j = 0; j < dataSize; j++)
						{
							buffer[j].compare_exchange_weak(exp, des);	// CAS
							//buffer[j].fetch_add(des); // FAA
							//buffer[j].store(10011);	// store
							//buffer[j].load();			// load
							//buffer[j].exchange(des);  // SWP
							//unsuccessfulCASAsm(&buffer[j],&buffer[j],&exp,&des);  // unCAS
						}
					end = get_time();
					elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
					sumTime += elapsed;
					flagMState.store(false);
				}
			fout << "latencyCASMNearCore2 = [latencyCASMNearCore2 " << sumTime/nruns/(dataSize*sizeof(atomic<int>)) << "];" << endl;
			fout << "bandwidthCASMNearCore2 = [bandwidthCASMNearCore2 " << (((dataSize*sizeof(atomic<int>))/(sumTime/nruns))*1000000000)/1048576 << "];" << endl;
			cout << "Remote  с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
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
	fout.open("CAS_I_cpu2_cpu3_cpu1.m"); // open stream for writing in file
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
	fout << "latencyCASMNearCore2 = [];" << endl;
	fout << "bandwidthCASMNearCore2 = [];" << endl;
	//==================================================================
	time_t myTime;
	myTime = time(NULL);
	cout << "Test start: " << ctime(&myTime) << endl;	// test start time
	fout << "%Test start: " << ctime(&myTime) << endl;	// write test start time to the file
	//==================================================================
	pthread_t threads[2];
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	cpu_set_t mask;
	//==================================================================
	// Local core ======================================================
	//==================================================================
	int i=2;
	CPU_ZERO(&mask);
	CPU_SET(i, &mask); // set mask for thread to cpu2
	pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask); 
	sched_setaffinity(0,sizeof(cpu_set_t),&mask);
	pthread_create(&threads[i],&attr, invalid_local, NULL);
	pthread_join(threads[i],NULL);
	//==================================================================
	//  two threads=====================================================
	//==================================================================
	// test on cpu2 and cpu3
	flagMState.store(false); 
	clflush(buffer,testSizeBuffer);
	for (int i = 0; i <= 1; i++) 
		{	  
			CPU_ZERO(&mask);
			CPU_SET(i + 2, &mask); // i + 2 shift to cpu2, because cpu0 always have some system proccess on it
			pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
			sched_setaffinity(0,sizeof(cpu_set_t),&mask);
			if ( i == 0 )
				{
					pthread_create(&threads[i],&attr, prep_I, NULL); // thread on cpu2
				}
			if ( i == 1 )
				{
					pthread_create(&threads[i],&attr, meas_cpu3, NULL); // thread on cpu3
				}
		}
	for (int i = 0; i <= 1; i++) 
		{
			pthread_join(threads[i],NULL); // destroy threads
		}
	//==================================================================
	// test on cpu2 and cpu1
	flagMState.store(false);
	clflush(buffer,testSizeBuffer);
	for (int i = 0; i <= 1; i++) 
		{	  
			CPU_ZERO(&mask);
			CPU_SET(i + 1, &mask); // shift to cpu1
			pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
			sched_setaffinity(0,sizeof(cpu_set_t),&mask);
			if ( i == 1 ) 
				{
					pthread_create(&threads[i],&attr, prep_I, NULL); // thread on cpu2
				}
			if ( i == 0 )
				{
					pthread_create(&threads[i],&attr, meas_cpu2, NULL); // thread on cpu1
				}
		}
	for (int i = 0; i <= 1; i++) 
		{
			pthread_join(threads[i],NULL);
		}
	//==================================================================
	//================================================================== 
	// write to the file for plotting
	// latency
	fout << "FigH = figure ('Position',get(0,'Screensize'));" << endl;
	fout << "semilogx(arrData, latencyLocalCore ,'s-',arrData, latencyCASMNearCore ,'>-',arrData, latencyCASMNearCore2 ,'o-','LineWidth',3)" << endl;
	fout << "titleString = sprintf('CAS, I, CPU2 (local), CPU3, CPU1, L1 = 64Kb, L2 = 512Kb');" << endl;
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
	fout << "titleString = sprintf('CAS, I, CPU2 (local), CPU3, CPU1, L1 = 64Kb, L2 = 512Kb');" << endl;
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
