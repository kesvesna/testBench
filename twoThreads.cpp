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

const auto startNRuns = 100'000;
const auto startCycleStep = 32;
const auto startDataSize = 1;

atomic<int> atvar(0);
atomic<int> tempVar(0);
int exp = 0;
int des = 1;

atomic<bool> flagIState(false);
atomic<bool> flagSState(false);
atomic<bool> flagMState(false);


//struct notAlignedData {
	//atomic<int> atvar;
	//notAlignedData() : atvar(0){};
//} testData;


unsigned long long testSizeBuffer = (268435456 + startDataSize)/1; // 1073741824 byte = 1Gb, 2147483648 = 2Gb
atomic<int> * buffer = (atomic<int> *) malloc(testSizeBuffer*sizeof(atomic<int>));

pthread_barrier_t open_barrier;
ofstream fout;
//======================================================================
//======================================================================

unsigned long long dataSizeScaleTune (unsigned long long dataSize)
{
	unsigned long long cycleStep = startCycleStep;
	if (dataSize >= 33554432) cycleStep = 117440512; // if data => 128M
	if (dataSize < 33554432 && dataSize >= 4194304) cycleStep = 14680064; // 16M <= data < 128M
	if (dataSize < 4194304 && dataSize >= 262144)   cycleStep = 983040;  //  1M <= data < 16M
	if (dataSize < 262144 && dataSize >= 32768)     cycleStep = 28672;   // 128K <= data < 1M
	if (dataSize < 32768 && dataSize >= 4096)       cycleStep = 3584;    // 16K <= data < 128K
	if (dataSize < 4096 && dataSize >= 256)         cycleStep = 480;     //  1K <= data < 16K
	return cycleStep;
}

int nrunsScaleTune (unsigned long long dataSize)
{
	int nruns = startNRuns;
	if (dataSize >= 33554432) nruns = 2;                                  // if data => 128M
	if (dataSize < 33554432 && dataSize >= 4194304) nruns = 2;             // 16M <= data < 128M
	if (dataSize < 4194304 && dataSize >= 262144)   nruns = 2;             //  1M <= data < 16M
	if (dataSize < 262144 && dataSize >= 32768)     nruns = 4;      // 128K <= data < 1M
	if (dataSize < 32768 && dataSize >= 4096)       nruns = 100;     //   16K <= data < 128K
	if (dataSize < 4096 && dataSize >= 256)         nruns = 1000;     //  1K <= data < 16K
	return nruns;
}
void inline clflush(atomic<int>* buffer, unsigned long long testSizeBuffer)
{
  unsigned long long addr,passes,linesize;

  addr = (unsigned long long) buffer;
  linesize = 64;

  __asm__ __volatile__("mfence;"::: "memory"); 

  for(passes = (sizeof(atomic<int>)*testSizeBuffer/linesize);passes>0;passes--){
      __asm__ __volatile__("clflush (%%rax);":: "a" (addr));
      addr+=linesize;
  }

  __asm__ __volatile__("mfence;"::: "memory"); 

}
//======================================================================
//======================================================================
void meas_M()
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
		for (auto i = 0; i < nruns; i++) {
        
			srand(time(0));
			for (auto j = 0; j < dataSize; j++){
				buffer[j].store(1+rand()%100);
			}
			
			start = get_time();
			for (auto j = 0; j < dataSize; j++){
				buffer[j].compare_exchange_weak(exp, des);
			}
			end = get_time();
			elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
			sumTime+= elapsed;
		}
			fout << "arrData = [arrData " << dataSize*sizeof(atomic<int>) << "];" << endl;
			fout << "arrCasTimeStateM = [arrCasTimeStateM " << dataSize/(sumTime/nruns) << "];" << endl;
			cout << "Закончен тест для состояния M с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
			dataSize+= dataSizeScaleTune(dataSize);
			nruns = nrunsScaleTune(dataSize);
			sumTime = 0.0;
	} 
}
//=====================================================================================
void prep_M ()
{
	unsigned long long cycleStep = startCycleStep;
	unsigned long long dataSize = startDataSize;
	int nruns = startNRuns;
	while (dataSize <= testSizeBuffer)
	{
		for (auto i = 0; i < nruns; i++) {
			for (auto j = 0; j < dataSize; j += cycleStep){
				buffer[j].store(100); // in Modified state
			}
			flagMState.store(true);
			while (flagMState.load() == true) {}
		}
		dataSize+= dataSizeScaleTune(dataSize);
		nruns = nrunsScaleTune(dataSize);

	}
}
// в Owned можно попасть только из Modified попаданием при чтении (probe read hit / Bus Read)
// если делать read hit, то будет петля на Modified
// probe write hit this is Bus Read X
void meas_O()
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
		for (auto i = 0; i < nruns; i++) {
			while (flagMState.load() == false) {}
			srand(time(0));
			for (auto j = 0; j < dataSize; j++){
				tempVar.store(buffer[j]); // in Owned state
			}
			
			start = get_time();
			for (auto j = 0; j < dataSize; j++){
				buffer[j].compare_exchange_weak(exp, des);
			}
			end = get_time();
			elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
			sumTime += elapsed;
			flagMState.store(false);
		}
		fout << "arrCasTimeStateO = [arrCasTimeStateO " << dataSize/(sumTime/nruns) << "];" << endl;
		cout << "Закончен тест для состояния O с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
		dataSize+= dataSizeScaleTune(dataSize);
		nruns = nrunsScaleTune(dataSize);
		sumTime = 0.0;
	}
}
//=================================================================================
void meas_E()
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
		for (auto i = 0; i < nruns; i++) {
			for (auto j = 0; j < dataSize; j++){
				tempVar.store(buffer[j]);
			}
			
			start = get_time();
			for (auto j = 0; j < dataSize; j++){
				buffer[j].compare_exchange_weak(exp, des);
			}
			end = get_time();
			elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
			sumTime+= elapsed;
		}
		fout << "arrCasTimeStateE = [arrCasTimeStateE " << dataSize/(sumTime/nruns) << "];" << endl;
		cout << "Закончен тест для состояния Е с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
		dataSize+= dataSizeScaleTune(dataSize);
		nruns = nrunsScaleTune(dataSize);
		sumTime = 0.0;
	}
    
}
//===================================================================================


void meas_I()
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
		for (auto i = 0; i < nruns; i++) {
			while (flagIState.load() == false) {}
			srand(time(0));
			start = get_time();
			for (auto j = 0; j < dataSize; j++){
				buffer[j].compare_exchange_weak(exp, des);
			}
			end = get_time();
			elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
			sumTime+= elapsed;
			flagIState.store(false);
		}
		fout << "arrCasTimeStateI = [arrCasTimeStateI " << dataSize/(sumTime/nruns) << "];" << endl;
		cout << "Закончен тест для состояния I с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
		dataSize+= dataSizeScaleTune(dataSize);
		nruns = nrunsScaleTune(dataSize);
		sumTime = 0.0;
	}
}
//==================================================================================
void * meas_S1(void * arg)
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
		for (auto i = 0; i < nruns; i++) {
			while (flagSState.load() == false) {}
			for (auto j = 0; j < dataSize; j++){
				tempVar.store(buffer[j]);
			}
			start = get_time();
			for (auto j = 0; j < dataSize; j++){
				buffer[j].compare_exchange_weak(exp, des);
			}
			end = get_time();
			elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
			sumTime += elapsed;
			flagSState.store(false);
		}
		fout << "arrData = [arrData " << dataSize*sizeof(atomic<int>) << "];" << endl;
		fout << "arrCasBandwidthTimeStateS = [arrCasBandwidthTimeStateS " << dataSize/(sumTime/nruns) << "];" << endl;
		fout << "arrCasLatencyTimeStateS = [arrCasLatencyTimeStateS " << sumTime/nruns << "];" << endl;
		cout << "Закончен тест для состояния S с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
		dataSize+= dataSizeScaleTune(dataSize);
		nruns = nrunsScaleTune(dataSize);
		sumTime = 0.0;
	}
}
//==================================================================================
void * meas_S2(void * arg)
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
		for (auto i = 0; i < nruns; i++) {
			while (flagSState.load() == false) {}
			for (auto j = 0; j < dataSize; j++){
				tempVar.store(buffer[j]);
			}
			start = get_time();
			for (auto j = 0; j < dataSize; j++){
				buffer[j].compare_exchange_weak(exp, des);
			}
			end = get_time();
			elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
			sumTime += elapsed;
			flagSState.store(false);
		}
		//fout << "arrData = [arrData " << dataSize*sizeof(atomic<int>) << "];" << endl;
		fout << "arrCasBandwidthTimeStateSNearCores = [arrCasBandwidthTimeStateSNearCores " << dataSize/(sumTime/nruns) << "];" << endl;
		fout << "arrCasLatencyTimeStateSNearCores = [arrCasLatencyTimeStateSNearCores " << sumTime/nruns << "];" << endl;
		cout << "Закончен тест 2 для состояния S с размером данных = " << dataSize*sizeof(atomic<int>) << endl;
		dataSize+= dataSizeScaleTune(dataSize);
		nruns = nrunsScaleTune(dataSize);
		sumTime = 0.0;
	}
}
//==================================================================================
void * prep_S(void * arg)
{
	unsigned long long cycleStep = startCycleStep; 
	unsigned long long dataSize = startDataSize;
	int nruns = startNRuns;
	while (dataSize <= testSizeBuffer)
	{
		for (auto i = 0; i < nruns; i++) {
			for (auto j = 0; j < dataSize; j++){
				tempVar.store(buffer[j]);
			}
			flagSState.store(true);
			while (flagSState.load() == true) {}
		}
		dataSize+= dataSizeScaleTune(dataSize);
		nruns = nrunsScaleTune(dataSize);

	}
}
//==================================================================================
void prep_I()
{
	unsigned long long cycleStep = startCycleStep; 
	unsigned long long dataSize = startDataSize;
	int nruns = startNRuns;
	while (dataSize <= testSizeBuffer)
	{
		for (auto i = 0; i < nruns; i++) {
		    srand(time(0));
			for (auto j = 0; j < dataSize; j++){
				buffer[j].store(1+rand()%100);
			}
			flagIState.store(true);
			while (flagIState.load() == true) {}
		}
		dataSize+= dataSizeScaleTune(dataSize);
		nruns = nrunsScaleTune(dataSize);

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
		//cout << "Data in buffer : №" << j<< " : " << buffer[j] << endl;
	}
	
	fout.open("at22.m");
	long mempagesize = sysconf(_SC_PAGESIZE);
	cout << "Memory pagesize on this machine : " << mempagesize << " bytes" << endl;
	cout << "Test data size : " << testSizeBuffer*sizeof(atomic<int>)/1024 << " Kilobytes" << endl;
	fout << "arrData = [];" << endl;
	//fout << "arrCasTimeStateI = [];" << endl;
	//fout << "arrCasTimeStateE = [];" << endl;
	fout << "arrCasBandwidthTimeStateS = [];" << endl;
	fout << "arrCasLatencyTimeStateS = [];" << endl;
	fout << "arrCasBandwidthTimeStateSNearCores = [];" << endl;
	fout << "arrCasLatencyTimeStateSNearCores = [];" << endl;
	//fout << "arrCasTimeStateO = [];" << endl;
	//fout << "arrCasTimeStateS = [];" << endl;
	time_t myTime;
	myTime = time(NULL);
	cout << "Test start: " << ctime(&myTime) << endl;
	fout << "%Test start: " << ctime(&myTime) << endl;
	//==================================================================
	pthread_t threads[2];
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	//pthread_barrier_init(&open_barrier, NULL, 2);
	cpu_set_t mask;
	flagSState.store(false);
	for (int i = 0; i <= 3; i += 3) 
	{	  
		CPU_ZERO(&mask);
		CPU_SET(i, &mask);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
		sched_setaffinity(0,sizeof(cpu_set_t),&mask);
		if ( i == 0 )
		{
			pthread_create(&threads[i],&attr, prep_S, NULL);
		}
		if ( i == 3 )
		{
			pthread_create(&threads[i],&attr, meas_S1, NULL);
		}
	}
	for (int i = 0; i <= 3; i += 3) 
	{
		pthread_join(threads[i],NULL);
	}
	//=================================================================
	flagSState.store(false);
	for (int i = 0; i <= 1; i++) 
	{	  
		CPU_ZERO(&mask);
		CPU_SET(i, &mask);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask);
		sched_setaffinity(0,sizeof(cpu_set_t),&mask);
		if ( i == 0 )
		{
			pthread_create(&threads[i],&attr, prep_S, NULL);
		}
		if ( i == 1 )
		{
			pthread_create(&threads[i],&attr, meas_S2, NULL);
		}
	}
	for (int i = 0; i <= 1; i++) 
	{
		pthread_join(threads[i],NULL);
	}
	//=================================================================
	//clflush(buffer,testSizeBuffer);
	//meas_E();
	//time_t myTimeE;
	//myTimeE = time(NULL);
    //cout << "E state bandwidth finished : " << ctime(&myTimeE)<< endl;
    
    //meas_M();
    //time_t myTimeM;
	//myTimeM = time(NULL);
    //cout << "M state bandwidth finished : " << ctime(&myTimeM)<< endl;
    
    // measure for O state
    //flagMState.store(false);
    //::thread meas_O_thr(meas_O), prep_M_thr(prep_M);
    //meas_O_thr.join();
    //prep_M_thr.join();
    //time_t myTimeO;
	//myTimeO = time(NULL);
    //cout << "O state bandwidth finished : " << ctime(&myTimeO)<< endl;
    
    // measure for I state
    //flagIState.store(false);
    //std::thread meas_I_thr(meas_I), prep_I_thr(prep_I);
    //meas_I_thr.join();
    //prep_I_thr.join();
	//time_t myTimeI;
	//myTimeI = time(NULL);
    //cout << "I state bandwidth finished : " << ctime(&myTimeI)<< endl;
  
    //measure for S state
    
    //flagSState.store(false);
    //std::thread meas_S_thr(meas_S), prep_S_thr(prep_S);
    //meas_S_thr.join();
    //prep_S_thr.join();
	//==================================================================
	fout << "figure" << endl;
	fout << "hold on" << endl;
	fout << "arrData = log(arrData);" << endl;
	fout << "titleString = sprintf('latency atomic<int> data for S state CAS remote/near cores');" << endl;
	fout << "title (titleString);" << endl;
	fout << "ylabel('latency [nsec]')" << endl;
	fout << "xlabel('data size [bytes], XScale log')" << endl;
	fout << "plot(arrData,arrCasLatencyTimeStateS,'v-')" << endl;
	fout << "plot(arrData, arrCasLatencyTimeStateSNearCores, '*-')" << endl;
	fout << "legend('CAS latency for S state remote cores', 'CAS latency for S state near cores', 'Location','NorthWest')" << endl;
	fout << "grid on" << endl;
	//==========================================================================
	fout << "figure" << endl;//axis([4 15 0.0055 0.006]);
	fout << "hold on" << endl;
	fout << "arrData = log(arrData);" << endl;
	fout << "titleString = sprintf('bandwidth atomic<int> data for S state CAS remote/near cores');" << endl;
	fout << "title (titleString);" << endl;
	fout << "ylabel('bandwidth [bytes/time]')" << endl;
	fout << "xlabel('data size [bytes], XScale log')" << endl;
	fout << "plot(arrData,arrCasBandwidthTimeStateS,'v-')" << endl;
	fout << "plot(arrData,arrCasBandwidthTimeStateSNearCores, '*-')" << endl;
	fout << "legend('CAS bandwidth for S state remote cores', 'CAS bandwidth for S state near cores', 'Location','NorthWest')" << endl;
	fout << "grid on" << endl;
	//===========================================================================
	time_t myTime2;
	myTime2 = time(NULL);
	cout << "Test finish: " << ctime(&myTime2) << endl;
	fout << "%Test finish: " << ctime(&myTime2) << endl;
	fout.close();
    return 0;
}


/*

pthread_barrier_t open_barrier;


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
		//x=logspace(2,8); loglog(x,arrCasBandwidthTimeStateS); grid on
*/
