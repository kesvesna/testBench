#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <time.h>
#include <unistd.h>

using namespace std;
using namespace std::chrono;

long long operations = 0;
long long testRuns = 200;
long long warmUpOperations = 200000000; // достаточно для прогрева
ofstream fout;

struct boolAlignedData {
	bool heading = 1;
	bool padding[63] = {0};
}firstBoolData;

struct intAlignedData {
	int heading = 1;
	bool padding[59] = {0};
}firstIntData;

struct doubleAlignedData {
	double heading = 1;
	bool padding[51] = {0};
}firstDoubleData;

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
	//cout << "warmUp in proccess ... " << endl;
	bool * pAddr1 = &firstBoolData.heading;
	boolAlignedData secondBoolData;
	for ( long long i = 0; i < warmUpOperations; ++i )
	{
		functionCASAsm (pAddr1,&firstBoolData.heading,&secondBoolData.heading,&secondBoolData.heading);
	}
}

static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long )hi) << 32 );
}

void testFunction()
{
	bool * pAddr1 = &firstBoolData.heading;
	boolAlignedData secondBoolData;
	int * pAddr2 = &firstIntData.heading;
	intAlignedData secondIntData;
	double * pAddr3 = &firstDoubleData.heading;
	doubleAlignedData secondDoubleData;
	unsigned long long int startRdtsc, endRdtsc;
	//high_resolution_clock::time_point start;
	//high_resolution_clock::time_point end;
	warmUp();
	for ( long long i = 0; i < testRuns; ++i )
	{
		operations += 100000;
//======================================================================	
		//start = high_resolution_clock::now();
		startRdtsc = rdtsc();
		for ( long long i = 0; i < operations; ++i )
		{
			functionCASAsm (pAddr1,&firstBoolData.heading,&secondBoolData.heading,&secondBoolData.heading);
		}
		endRdtsc = rdtsc();
		//end = high_resolution_clock::now();
		//auto elapsed = duration_cast<nanoseconds>(end-start).count();
		fout << "arrOperations = [arrOperations " << operations << "];" << endl;
		fout << "arrBoolCasAsmTime = [arrBoolCasAsmTime " << (endRdtsc-startRdtsc)/operations << "];" << endl;
//======================================================================
		//start = high_resolution_clock::now();
		startRdtsc = rdtsc();
		for ( long long i = 0; i < operations; ++i )
		{
			functionFAAAsm (&firstBoolData.heading,pAddr1);
		}
		endRdtsc = rdtsc();
		//end = high_resolution_clock::now();
		//elapsed = duration_cast<nanoseconds>(end-start).count();
		fout << "arrBoolFaaAsmTime = [arrBoolFaaAsmTime " << (endRdtsc-startRdtsc)/operations << "];" << endl;
//======================================================================
		//start = high_resolution_clock::now();
		startRdtsc = rdtsc();
		for ( long long i = 0; i < operations; ++i )
		{
			functionCASAsm (pAddr2,&firstIntData.heading,&secondIntData.heading,&secondIntData.heading);
		}
		endRdtsc = rdtsc();
		//end = high_resolution_clock::now();
		//elapsed = duration_cast<nanoseconds>(end-start).count();
		fout << "arrIntCasAsmTime = [arrIntCasAsmTime " << (endRdtsc-startRdtsc)/operations << "];" << endl;
//======================================================================
		//start = high_resolution_clock::now();
		startRdtsc = rdtsc();
		for ( long long i = 0; i < operations; ++i )
		{
			functionFAAAsm (&firstIntData.heading,pAddr2);
		}
		endRdtsc = rdtsc();
		//end = high_resolution_clock::now();
		//elapsed = duration_cast<nanoseconds>(end-start).count();
		fout << "arrIntFaaAsmTime = [arrIntFaaAsmTime " << (endRdtsc-startRdtsc)/operations << "];" << endl;
//======================================================================
		//start = high_resolution_clock::now();
		startRdtsc = rdtsc();
		for ( long long i = 0; i < operations; ++i )
		{
			functionCASAsm (pAddr3,&firstDoubleData.heading,&secondDoubleData.heading,&secondDoubleData.heading);
		}
		endRdtsc = rdtsc();
		//end = high_resolution_clock::now();
		//elapsed = duration_cast<nanoseconds>(end-start).count();
		fout << "arrDoubleCasAsmTime = [arrDoubleCasAsmTime " << (endRdtsc-startRdtsc)/operations << "];" << endl;
//======================================================================
		//start = high_resolution_clock::now();
		startRdtsc = rdtsc();
		for ( long long i = 0; i < operations; ++i )
		{
			functionFAAAsm (&firstDoubleData.heading,pAddr3);
		}
		endRdtsc = rdtsc();
		//end = high_resolution_clock::now();
		//elapsed = duration_cast<nanoseconds>(end-start).count();
		fout << "arrDoubleFaaAsmTime = [arrDoubleFaaAsmTime " << (endRdtsc-startRdtsc)/operations << "];" << endl;
//======================================================================
	//cout << "Iterations = " << operations << endl;
	}
}

int main ()
{
	fout.open("resultWithAlignedData.m");
	time_t myTime;
	myTime = time(NULL);
	cout << "Test start: " << ctime(&myTime) << endl;
	fout << "%Test start: " << ctime(&myTime) << endl;
	long mempagesize = sysconf(_SC_PAGESIZE);
	cout << "Memory pagesize on this machine : " << mempagesize << " bytes" << endl << endl;
	fout << "%Memory pagesize on this machine : " << mempagesize << " bytes" << endl << endl;
	fout << "arrOperations = [];" << endl;
	fout << "arrBoolCasAsmTime = [];" << endl;
	fout << "arrBoolFaaAsmTime = [];" << endl;
	fout << "arrIntCasAsmTime = [];" << endl;
	fout << "arrIntFaaAsmTime = [];" << endl;
	fout << "arrDoubleCasAsmTime = [];" << endl;
	fout << "arrDoubleFaaAsmTime = [];" << endl;
	cpu_set_t cpuset;
	int cpu = 1; // второе ядро
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);
	sched_setaffinity(0,sizeof(cpuset),&cpuset);
	thread threadFirst(testFunction);
	threadFirst.join();
	time_t myTime2;
	myTime2 = time(NULL);
	cout << endl << "Test finish: " << ctime(&myTime2) << endl;
	fout << "%Test finish: " << ctime(&myTime2) << endl;
	fout << "arrBoolCasAsmTime = smooth(arrBoolCasAsmTime);" << endl;
	fout << "arrBoolFaaAsmTime = smooth(arrBoolFaaAsmTime);" << endl;
	fout << "arrIntCasAsmTime = smooth(arrIntCasAsmTime);" << endl;
	fout << "arrIntFaaAsmTime = smooth(arrIntFaaAsmTime);" << endl;
	fout << "arrDoubleCasAsmTime = smooth(arrDoubleCasAsmTime);" << endl;
	fout << "arrDoubleFaaAsmTime = smooth(arrDoubleFaaAsmTime);" << endl;
	fout << "hold on" << endl;
	fout << "axis([-1000000 22000000 54 62])" << endl;
	fout << "plot(arrOperations,arrBoolCasAsmTime,'*-')" << endl;
	fout << "plot(arrOperations,arrIntCasAsmTime,'*-')" << endl;
	fout << "plot(arrOperations,arrDoubleCasAsmTime,'*-')" << endl;
	fout << "plot(arrOperations,arrBoolFaaAsmTime,'o-')" << endl;
	fout << "plot(arrOperations,arrIntFaaAsmTime,'o-')" << endl;
	fout << "plot(arrOperations,arrDoubleFaaAsmTime,'o-')" << endl;
	fout << "grid on" << endl;
	fout << "legend('Cas bool Asm','Cas int Asm','Cas double Asm','Faa bool Asm','Faa int Asm','Faa double Asm','North')" << endl;
    fout << "xlabel('cycles iterations with aligned 64 bytes data')" << endl;
    fout << "ylabel('time latency in nanosec')" << endl;
	fout.close();
	return 0;
}
