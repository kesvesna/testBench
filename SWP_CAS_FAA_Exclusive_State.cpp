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

const unsigned long long startNRuns = 100000;
const unsigned long long startCycleStep = 32;
const unsigned long long startDataSize = 1;
const unsigned long long cashLineSize = 64;

atomic<int> atvar(0);
atomic<int> tempVar(0);
int exp = 0;
int des = 1;

atomic<bool> flagIState(false);
atomic<bool> flagSState(false);
atomic<bool> flagMState(false);
atomic<bool> flagEState(false);

struct threadsData {
	int threadNumber;
	threadsData() : threadNumber(0){};
} testData;

unsigned long long testSizeBuffer = startDataSize + (268435456 )/1; // 1073741824 byte = 1Gb, 2147483648 = 2Gb
atomic<int> * buffer = (atomic<int> *) malloc(testSizeBuffer*sizeof(atomic<int>));

ofstream fout;
//======================================================================
//======================================================================
unsigned long long dataSizeScaleTune (unsigned long long dataSize) // dataSize change for test speed
{
	unsigned long long cycleStep = startCycleStep;
	if (dataSize >= 33554432) cycleStep = 117440512;                  // if data => 128M
	if (dataSize < 33554432 && dataSize >= 4194304) cycleStep = 14680064; // 16M <= data < 128M
	if (dataSize < 4194304 && dataSize >= 262144)   cycleStep = 983040;   //  1M <= data < 16M
	if (dataSize < 262144 && dataSize >= 32768)     cycleStep = 28672;   // 128K <= data < 1M
	if (dataSize < 32768 && dataSize >= 4096)       cycleStep = 3584;     // 16K <= data < 128K
	if (dataSize < 4096 && dataSize >= 256)         cycleStep = 480;      //  1K <= data < 16K
	return cycleStep;
}
//======================================================================
int nrunsScaleTune (unsigned long long dataSize) // data runs change for test speed
{
	int nruns = startNRuns;
	if (dataSize >= 33554432) nruns = 2;                                // if data => 128M
	if (dataSize < 33554432 && dataSize >= 4194304) nruns = 2;              // 16M <= data < 128M
	if (dataSize < 4194304 && dataSize >= 262144)   nruns = 8;              //  1M <= data < 16M
	if (dataSize < 262144 && dataSize >= 32768)     nruns = 10;            // 128K <= data < 1M
	if (dataSize < 32768 && dataSize >= 4096)       nruns = 100;          //   16K <= data < 128K
	if (dataSize < 4096 && dataSize >= 256)         nruns = 10000;          //  1K <= data < 16K
	return nruns;
}
//=======================================================================
void inline clflush(atomic<int>* buffer, unsigned long long testSizeBuffer) // flush all cashes
{
  unsigned long long addr,passes,linesize;

  addr = (unsigned long long) buffer;
  linesize = cashLineSize;

  __asm__ __volatile__("mfence;"::: "memory"); 

  for(passes = (sizeof(atomic<int>)*testSizeBuffer/linesize);passes>0;passes--){
      __asm__ __volatile__("clflush (%%rax);":: "a" (addr));
      addr+=linesize;
  }

  __asm__ __volatile__("mfence;"::: "memory"); 

}
//======================================================================
// Original CAS Asm ====================================================
//uint64_t *ptr;
//uint64_t ret_val,old_val,new_val;

//asm volatile("lock\n\tcmpxchgq %1,%2"
 // :"=a"(ret_val)
 // :"r"(new_val),"m"(*ptr),"0"(old_val)
 // :"memory"
 // );
//======================================================================
inline void unsuccessfulCASAsm(atomic<int> *ptr, atomic<int> *old_val, atomic<int> *new_val, atomic<int> *ret_val)
{
	//cout << "=======================" << endl;
	//cout << "Before CASAsm : " << endl;
	//cout << "ptr = " << ptr << endl;
	//cout << "*ptr = " << *ptr << endl;
	//cout << "old_val = " << *old_val << endl;
	//cout << "new_val = " << *new_val << endl;
	//cout << "ret_val = " << *ret_val << endl;
	asm volatile("lock\n\tcmpxchg %1,%2"
					:"=a"(*ret_val)
					:"r"(*new_val),"m"(ptr),"0"(*old_val)
					:"memory"
					);
	//cout << "=======================" << endl;
	//cout << "After CASAsm : " << endl;
	//cout << "ptr = " << ptr << endl;
	//cout << "*ptr = " << *ptr << endl;
	//cout << "old_val = " << *old_val << endl;
	//cout << "new_val = " << *new_val << endl;
	//cout << "ret_val = " << *ret_val << endl;
}
//==================================================================================
//static inline void fetch_and_add(int *variable, int value)
//{
//	__asm__ volatile ("lock; xaddl %0, %1"
//						:"+r" (value), "+m" (*variable) // input + output
//						: // No input-only
//						: "memory"
//					);			
//}
//======================================================================
// Exclusive state for SWP measuring ===================================
//======================================================================
void * meas_E_local_SWP(void * arg) // measure in state E for one local thread
{
	auto get_time = std::chrono::steady_clock::now;
    decltype(get_time()) start, end;
    unsigned long long cycleStep = startCycleStep; 
    unsigned long long dataSize = startDataSize;
	double elapsed = 0.0;
	double sumTime = 0.0;
	int nruns = startNRuns;
	while (dataSize <= testSizeBuffer)
	{
		
		for (auto i = 0; i < nruns; i++) { // clear all level cashes
			clflush(buffer,dataSize);
			for (auto j = 0; j < dataSize; j++){ // switch in Exclusive state
				tempVar.store(buffer[j]);
			}
			start = get_time();
			for (auto j = 0; j < dataSize; j++){ // measure swp for E state
				buffer[j].exchange(des);
			}
			end = get_time();
			elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
			sumTime += elapsed;
		}
		fout << "arrData = [arrData " << dataSize*sizeof(atomic<int>) << "];" << endl;
		fout << "latencyTimeSWPOneLocalThreadE = [latencyTimeSWPOneLocalThreadE " << sumTime/nruns/(dataSize*sizeof(atomic<int>)) << "];" << endl;
		cout << "Local thread SWP for E с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
		dataSize += dataSizeScaleTune(dataSize);
		nruns = nrunsScaleTune(dataSize);
		sumTime = 0.0;
	}
}
//======================================================================
// prep before state E clear all cashes
void * prep_flush_cashes(void * arg) 
{
	unsigned long long dataSize = startDataSize;
	int nruns = startNRuns;
	while (dataSize <= testSizeBuffer)
	{
		for (auto i = 0; i < nruns; i++) {
		    clflush(buffer,dataSize);
			flagEState.store(true);
			while (flagEState.load() == true) {}
		}
		dataSize += dataSizeScaleTune(dataSize);
		nruns = nrunsScaleTune(dataSize);
	}
}
//=========================================================================================
// measure in state E for two threads
void * meas_E_SWP(void * arg) 
{
	threadsData *threadsNumber = (threadsData*) arg;
	auto get_time = std::chrono::steady_clock::now;
    decltype(get_time()) start, end;
    unsigned long long cycleStep = startCycleStep; 
    unsigned long long dataSize = startDataSize;
	double elapsed = 0.0;
	double sumTime = 0.0;
	int nruns = startNRuns;
	while (dataSize <= testSizeBuffer)
	{
		for (auto i = 0; i < nruns; i++) {
			while (flagEState.load() == false) {}
			for (auto j = 0; j < dataSize; j++){
				tempVar.store(buffer[j]);
			}
			
			start = get_time();
			for (auto j = 0; j < dataSize; j++){
				buffer[j].exchange(des);
				//buffer[j].fetch_add(des);
				//buffer[j].compare_exchange_weak(exp, des);
			}
			end = get_time();
			elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
			sumTime += elapsed;
			flagEState.store(false);
		}
		if (threadsNumber->threadNumber == 1) {
			fout << "latencyTimeSWPTwoNearThreadsE = [latencyTimeSWPTwoNearThreadsE " << sumTime/nruns/(dataSize*sizeof(atomic<int>)) << "];" << endl;
			cout << "Two near threads SWP for E с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
		}
		if (threadsNumber->threadNumber == 3) {
			fout << "latencyTimeSWPTwoRemoteThreadsE = [latencyTimeSWPTwoRemoteThreadsE " << sumTime/nruns/(dataSize*sizeof(atomic<int>)) << "];" << endl;
			cout << "Two remote threads SWP for E с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
		}
		dataSize += dataSizeScaleTune(dataSize);
		nruns = nrunsScaleTune(dataSize);
		sumTime = 0.0;
	}
}
//======================================================================
// Exclusive state for CAS measuring ===================================
//======================================================================
void * meas_E_local_CAS(void * arg) // measure in state E for one local thread
{
	auto get_time = std::chrono::steady_clock::now;
    decltype(get_time()) start, end;
    unsigned long long cycleStep = startCycleStep; 
    unsigned long long dataSize = startDataSize;
	double elapsed = 0.0;
	double sumTime = 0.0;
	int nruns = startNRuns;
	atomic<int> new_val(1);
	atomic<int> ret_val(0);
	while (dataSize <= testSizeBuffer)
	{
		for (auto i = 0; i < nruns; i++) { // clear all level cashes
			clflush(buffer,dataSize);
			for (auto j = 0; j < dataSize; j++){ // switch in Exclusive state
				tempVar.store(buffer[j]);
			}
			start = get_time();
			for (auto j = 0; j < dataSize; j++){ // measure swp for E state
				buffer[j].compare_exchange_weak(exp, des);
				//unsuccessfulCASAsm(&buffer[j], &buffer[j], &new_val, &ret_val);
			}
			end = get_time();
			elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
			sumTime += elapsed;
		}
		//fout << "arrData = [arrData " << dataSize*sizeof(atomic<int>) << "];" << endl;
		fout << "latencyTimeCASOneLocalThreadE = [latencyTimeCASOneLocalThreadE " << sumTime/nruns/(dataSize*sizeof(atomic<int>)) << "];" << endl;
		cout << "Local thread CAS for E с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
		dataSize += dataSizeScaleTune(dataSize);
		nruns = nrunsScaleTune(dataSize);
		sumTime = 0.0;
	}
}
//=========================================================================================
// measure in state E for two threads
void * meas_E_CAS(void * arg) 
{
	threadsData *threadsNumber = (threadsData*) arg;
	auto get_time = std::chrono::steady_clock::now;
    decltype(get_time()) start, end;
    unsigned long long cycleStep = startCycleStep; 
    unsigned long long dataSize = startDataSize;
	double elapsed = 0.0;
	double sumTime = 0.0;
	int nruns = startNRuns;
	atomic<int> new_val(1);
	atomic<int> ret_val(0);
	while (dataSize <= testSizeBuffer)
	{
		for (auto i = 0; i < nruns; i++) {
			while (flagEState.load() == false) {}
			for (auto j = 0; j < dataSize; j++){
				tempVar.store(buffer[j]);
			}
			
			start = get_time();
			for (auto j = 0; j < dataSize; j++){
				//buffer[j].exchange(des);
				//buffer[j].fetch_add(des);
				buffer[j].compare_exchange_weak(exp, des);
				//unsuccessfulCASAsm(&buffer[j], &buffer[j], &new_val, &ret_val);
			}
			end = get_time();
			elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
			sumTime += elapsed;
			flagEState.store(false);
		}
		if (threadsNumber->threadNumber == 1) {
			fout << "latencyTimeCASTwoNearThreadsE = [latencyTimeCASTwoNearThreadsE " << sumTime/nruns/(dataSize*sizeof(atomic<int>)) << "];" << endl;
			cout << "Two near threads CAS for E с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
		}
		if (threadsNumber->threadNumber == 3) {
			fout << "latencyTimeCASTwoRemoteThreadsE = [latencyTimeCASTwoRemoteThreadsE " << sumTime/nruns/(dataSize*sizeof(atomic<int>)) << "];" << endl;
			cout << "Two remote threads CAS for E с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
		}
		dataSize += dataSizeScaleTune(dataSize);
		nruns = nrunsScaleTune(dataSize);
		sumTime = 0.0;
	}
}
//======================================================================
// Exclusive state for FAA measuring ===================================
//======================================================================
void * meas_E_local_FAA(void * arg) // measure in state E for one local thread
{
	auto get_time = std::chrono::steady_clock::now;
    decltype(get_time()) start, end;
    unsigned long long cycleStep = startCycleStep; 
    unsigned long long dataSize = startDataSize;
	double elapsed = 0.0;
	double sumTime = 0.0;
	int nruns = startNRuns;
	while (dataSize <= testSizeBuffer)
	{
		for (auto i = 0; i < nruns; i++) { // clear all level cashes
			clflush(buffer,dataSize);
			for (auto j = 0; j < dataSize; j++){ // switch in Exclusive state
				tempVar.store(buffer[j]);
			}
			start = get_time();
			for (auto j = 0; j < dataSize; j++){ // measure swp for E state
				buffer[j].fetch_add(des);
			}
			end = get_time();
			elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
			sumTime += elapsed;
		}
		//fout << "arrData = [arrData " << dataSize*sizeof(atomic<int>) << "];" << endl;
		fout << "latencyTimeFAAOneLocalThreadE = [latencyTimeFAAOneLocalThreadE " << sumTime/nruns/(dataSize*sizeof(atomic<int>)) << "];" << endl;
		cout << "Local thread FAA for E с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
		dataSize += dataSizeScaleTune(dataSize);
		nruns = nrunsScaleTune(dataSize);
		sumTime = 0.0;
	}
}
//=========================================================================================
// measure in state E for two threads
void * meas_E_FAA(void * arg) 
{
	threadsData *threadsNumber = (threadsData*) arg;
	auto get_time = std::chrono::steady_clock::now;
    decltype(get_time()) start, end;
    unsigned long long cycleStep = startCycleStep; 
    unsigned long long dataSize = startDataSize;
	double elapsed = 0.0;
	double sumTime = 0.0;
	int nruns = startNRuns;
	while (dataSize <= testSizeBuffer)
	{
		for (auto i = 0; i < nruns; i++) {
			while (flagEState.load() == false) {}
			for (auto j = 0; j < dataSize; j++){
				tempVar.store(buffer[j]);
			}
			
			start = get_time();
			for (auto j = 0; j < dataSize; j++){
				//buffer[j].exchange(des);
				buffer[j].fetch_add(des);
				//buffer[j].compare_exchange_weak(exp, des);
			}
			end = get_time();
			elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
			sumTime += elapsed;
			flagEState.store(false);
		}
		if (threadsNumber->threadNumber == 1) {
			fout << "latencyTimeFAATwoNearThreadsE = [latencyTimeFAATwoNearThreadsE " << sumTime/nruns/(dataSize*sizeof(atomic<int>)) << "];" << endl;
			cout << "Two near threads FAA for E с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
		}
		if (threadsNumber->threadNumber == 3) {
			fout << "latencyTimeFAATwoRemoteThreadsE = [latencyTimeFAATwoRemoteThreadsE " << sumTime/nruns/(dataSize*sizeof(atomic<int>)) << "];" << endl;
			cout << "Two remote threads FAA for E с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
		}
		dataSize += dataSizeScaleTune(dataSize);
		nruns = nrunsScaleTune(dataSize);
		sumTime = 0.0;
	}
}
//===================================================================================
int main(int argc, const char *argv[])
{
	if (buffer == NULL) 
	{
		cout << "Не удалось выделить память" << endl;
		exit (1);
	}
	srand(time(0));
	for (auto j = 0; j <= testSizeBuffer; j++){
		buffer[j].store(1+rand()%100);
	}
	//==================================================================
	fout.open("SWP_CAS_FAA_Exclusive_State.m");
	long mempagesize = sysconf(_SC_PAGESIZE);
	cout << "Memory pagesize on this machine : " << mempagesize << " bytes" << endl;
	cout << "Test data size : " << testSizeBuffer*sizeof(atomic<int>)/1024 << " Kilobytes" << endl;
	fout << "arrData = [];" << endl;
	//==================================================================
	// Exclusive State of cash
	// SWP time for one local thread, two near threads, two remote threads
	fout << "latencyTimeSWPOneLocalThreadE = [];" << endl;
	fout << "latencyTimeSWPTwoNearThreadsE = [];" << endl;
	fout << "latencyTimeSWPTwoRemoteThreadsE = [];" << endl;
	//==================================================================
	// Exclusive State of cash
	// CAS time for one local thread, two near threads, two remote threads
	fout << "latencyTimeCASOneLocalThreadE = [];" << endl;
	fout << "latencyTimeCASTwoNearThreadsE = [];" << endl;
	fout << "latencyTimeCASTwoRemoteThreadsE = [];" << endl;
	//==================================================================
	// Exclusive State of cash
	// FAA time for one local thread, two near threads, two remote threads
	fout << "latencyTimeFAAOneLocalThreadE = [];" << endl;
	fout << "latencyTimeFAATwoNearThreadsE = [];" << endl;
	fout << "latencyTimeFAATwoRemoteThreadsE = [];" << endl;
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
	// SWP latency =====================================================
	//==================================================================
	int i = 0;	  // one local thread on core 1
	CPU_ZERO(&mask);
	CPU_SET(i, &mask);
	pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
	sched_setaffinity(0,sizeof(cpu_set_t),&mask);
	testData.threadNumber = i;
	clflush(buffer,testSizeBuffer);
	pthread_create(&threads[i],&attr, meas_E_local_SWP, (void*)&testData);
	pthread_join(threads[i],NULL);
	//==================================================================
	flagEState.store(false); // two threads on near cores 1 and 2
	clflush(buffer,testSizeBuffer);
	for (int i = 0; i <= 1; i++) 
	{	  
		CPU_ZERO(&mask);
		CPU_SET(i, &mask);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
		sched_setaffinity(0,sizeof(cpu_set_t),&mask);
		if ( i == 0 )
		{
			pthread_create(&threads[i],&attr, prep_flush_cashes, NULL);
		}
		if ( i == 1 )
		{
			testData.threadNumber = i;
			pthread_create(&threads[i],&attr, meas_E_SWP, (void*)&testData);
		}
	}
	for (int i = 0; i <= 1; i++) 
	{
		pthread_join(threads[i],NULL);
	}
	//==================================================================
	flagEState.store(false); // two threads on remote cores 1 and 4
	clflush(buffer,testSizeBuffer);
	for (int i = 0; i <= 3; i += 3) 
	{	  
		CPU_ZERO(&mask);
		CPU_SET(i, &mask);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
		sched_setaffinity(0,sizeof(cpu_set_t),&mask);
		if ( i == 0 )
		{
			pthread_create(&threads[i],&attr, prep_flush_cashes, NULL);
		}
		if ( i == 3 )
		{
			testData.threadNumber = i;
			pthread_create(&threads[i],&attr, meas_E_SWP, (void*)&testData);
		}
	}
	for (int i = 0; i <= 3; i += 3) 
	{
		pthread_join(threads[i],NULL);
	}
	//==================================================================
	// CAS latency =====================================================
	//==================================================================
	i = 0;	  // one local thread on core 1
	CPU_ZERO(&mask);
	CPU_SET(i, &mask);
	pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
	sched_setaffinity(0,sizeof(cpu_set_t),&mask);
	testData.threadNumber = i;
	clflush(buffer,testSizeBuffer);
	pthread_create(&threads[i],&attr, meas_E_local_CAS, (void*)&testData);
	pthread_join(threads[i],NULL);
	//==================================================================
	flagEState.store(false); // two threads on near cores 1 and 2
	clflush(buffer,testSizeBuffer);
	for (int i = 0; i <= 1; i++) 
	{	  
		CPU_ZERO(&mask);
		CPU_SET(i, &mask);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
		sched_setaffinity(0,sizeof(cpu_set_t),&mask);
		if ( i == 0 )
		{
			pthread_create(&threads[i],&attr, prep_flush_cashes, NULL);
		}
		if ( i == 1 )
		{
			testData.threadNumber = i;
			pthread_create(&threads[i],&attr, meas_E_CAS, (void*)&testData);
		}
	}
	for (int i = 0; i <= 1; i++) 
	{
		pthread_join(threads[i],NULL);
	}
	//==================================================================
	flagEState.store(false); // two threads on remote cores 1 and 4
	clflush(buffer,testSizeBuffer);
	for (int i = 0; i <= 3; i += 3) 
	{	  
		CPU_ZERO(&mask);
		CPU_SET(i, &mask);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
		sched_setaffinity(0,sizeof(cpu_set_t),&mask);
		if ( i == 0 )
		{
			pthread_create(&threads[i],&attr, prep_flush_cashes, NULL);
		}
		if ( i == 3 )
		{
			testData.threadNumber = i;
			pthread_create(&threads[i],&attr, meas_E_CAS, (void*)&testData);
		}
	}
	for (int i = 0; i <= 3; i += 3) 
	{
		pthread_join(threads[i],NULL);
	}
	//==================================================================
	// FAA latency =====================================================
	//==================================================================
	i = 0;	  // one local thread on core 1
	CPU_ZERO(&mask);
	CPU_SET(i, &mask);
	pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
	sched_setaffinity(0,sizeof(cpu_set_t),&mask);
	testData.threadNumber = i;
	clflush(buffer,testSizeBuffer);
	pthread_create(&threads[i],&attr, meas_E_local_FAA, (void*)&testData);
	pthread_join(threads[i],NULL);
	//==================================================================
	flagEState.store(false); // two threads on near cores 1 and 2
	clflush(buffer,testSizeBuffer);
	for (int i = 0; i <= 1; i++) 
	{	  
		CPU_ZERO(&mask);
		CPU_SET(i, &mask);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
		sched_setaffinity(0,sizeof(cpu_set_t),&mask);
		if ( i == 0 )
		{
			pthread_create(&threads[i],&attr, prep_flush_cashes, NULL);
		}
		if ( i == 1 )
		{
			testData.threadNumber = i;
			pthread_create(&threads[i],&attr, meas_E_FAA, (void*)&testData);
		}
	}
	for (int i = 0; i <= 1; i++) 
	{
		pthread_join(threads[i],NULL);
	}
	//==================================================================
	flagEState.store(false); // two threads on remote cores 1 and 4
	clflush(buffer,testSizeBuffer);
	for (int i = 0; i <= 3; i += 3) 
	{	  
		CPU_ZERO(&mask);
		CPU_SET(i, &mask);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
		sched_setaffinity(0,sizeof(cpu_set_t),&mask);
		if ( i == 0 )
		{
			pthread_create(&threads[i],&attr, prep_flush_cashes, NULL);
		}
		if ( i == 3 )
		{
			testData.threadNumber = i;
			pthread_create(&threads[i],&attr, meas_E_FAA, (void*)&testData);
		}
	}
	for (int i = 0; i <= 3; i += 3) 
	{
		pthread_join(threads[i],NULL);
	}
	//==================================================================
	//==================================================================
	fout << "figure" << endl;
	fout << "hold on" << endl;
	fout << "arrData = log(arrData);" << endl;
	fout << "titleString = sprintf('latency atomic<int> data for E state SWP/CAS/FAA local/near/remote cores');" << endl;
	fout << "title (titleString);" << endl;
	fout << "ylabel('latency [nsec]')" << endl;
	fout << "xlabel('data size [bytes], XScale log')" << endl;
	fout << "plot(arrData,latencyTimeSWPOneLocalThreadE,'h-')" << endl;
	fout << "plot(arrData, latencyTimeSWPTwoNearThreadsE, '*-')" << endl;
	fout << "plot(arrData,latencyTimeSWPTwoRemoteThreadsE,'v-')" << endl;
	//==================================================================
	fout << "plot(arrData,latencyTimeCASOneLocalThreadE,'>-')" << endl;
	fout << "plot(arrData, latencyTimeCASTwoNearThreadsE, '<-')" << endl;
	fout << "plot(arrData,latencyTimeCASTwoRemoteThreadsE,'x-')" << endl;
	//==================================================================
	fout << "plot(arrData,latencyTimeFAAOneLocalThreadE,'s-')" << endl;
	fout << "plot(arrData, latencyTimeFAATwoNearThreadsE, 'p-')" << endl;
	fout << "plot(arrData,latencyTimeFAATwoRemoteThreadsE,'o-')" << endl;
	//==================================================================
	fout << "legend('SWP latency for E state local core', 'SWP latency for E state near cores', 'SWP latency for E state remote cores', 'CAS latency for E state local core', 'CAS latency for E state near cores', 'CAS latency for E state remote cores', 'FAA latency for E state local core', 'FAA latency for E state near cores', 'FAA latency for E state remote cores', 'Location','North')" << endl;
	fout << "grid on" << endl;
	//==================================================================
	time_t myTime2;
	myTime2 = time(NULL);
	cout << "Test finish: " << ctime(&myTime2) << endl;
	fout << "%Test finish: " << ctime(&myTime2) << endl;
	fout.close();
    return 0;
}
