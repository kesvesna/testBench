#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <time.h>
#include <unistd.h>

using namespace std;
using namespace std::chrono;

long long operations = 0;
long long testRuns = 150;
long long warmUpOperations = 2000000000;
ofstream fout;

inline void functionCASAsm (bool * pAddr1, bool * oldValue1, bool * newValue1, bool * returnValue1) 
{
	asm volatile("lock\n\tcmpxchgq %1, %2" // cas assembler
				: "=a" (returnValue1) : "r"  (newValue1), "m"(pAddr1), "0"(oldValue1) : "memory"
				);
}

inline void functionFAAAsm(bool * oldValue1, bool *pAddr1)
{
	asm volatile ("lock\n\tcmpxchgq %0, %1" // faa assembler
				: "+r" (oldValue1), "+m" (pAddr1) : : "memory");	
}

inline void functionCASAsm (int * pAddr2, int * oldValue2, int * newValue2, int * returnValue2) 
{
	asm volatile("lock\n\tcmpxchgq %1, %2" // cas assembler
				: "=a" (returnValue2) : "r"  (newValue2), "m"(pAddr2), "0"(oldValue2) : "memory"
				);
}

inline void functionFAAAsm(int * oldValue2, int *pAddr2)
{
	asm volatile ("lock\n\tcmpxchgq %0, %1" // faa assembler
				: "+r" (oldValue2), "+m" (pAddr2) : : "memory");	
}

inline void functionCASAsm (double * pAddr3, double * oldValue3, double * newValue3, double * returnValue3) 
{
	asm volatile("lock\n\tcmpxchgq %1, %2" // cas assembler
				: "=a" (returnValue3) : "r"  (newValue3), "m"(pAddr3), "0"(oldValue3) : "memory"
				);
}

inline void functionFAAAsm(double * oldValue3, double *pAddr3)
{
	asm volatile ("lock\n\tcmpxchgq %0, %1" // faa assembler
				: "+r" (oldValue3), "+m" (pAddr3) : : "memory");	
}

void warmUp()
{
	bool oldValue1 = 1, newValue1 = 0, returnValue1;
	bool * pAddr1 = &oldValue1;
	for ( int i = 0; i < warmUpOperations; ++i )
	{
	functionCASAsm (pAddr1,&oldValue1,&newValue1,&returnValue1);
	}
}

void testFunction()
{
	bool oldValue1 = 1, newValue1 = 0, returnValue1;
	bool * pAddr1 = &oldValue1;
	int oldValue2 = 22, newValue2, returnValue2;
	int * pAddr2 = &oldValue2;
	double oldValue3 = 22, newValue3, returnValue3;
	double * pAddr3 = &oldValue3;
	high_resolution_clock::time_point start;
	high_resolution_clock::time_point end;
	warmUp();
	for ( int i = 0; i < testRuns; ++i )
	{
	operations += 100000;
//======================================================================	
	start = high_resolution_clock::now();
	for ( int i = 0; i < operations; ++i )
	{
	functionCASAsm (pAddr1,&oldValue1,&newValue1,&returnValue1);
	}
	end = high_resolution_clock::now();
	auto elapsed = duration_cast<nanoseconds>(end-start).count();
	fout << "arrOperations = [arrOperations " << operations << "];" << endl;
	fout << "arrBoolCasAsmTime = [arrBoolCasAsmTime " << elapsed/operations << "];" << endl;
//======================================================================
	start = high_resolution_clock::now();
	for ( int i = 0; i < operations; ++i )
	{
	functionFAAAsm (&oldValue1,pAddr1);
	}
	end = high_resolution_clock::now();
	elapsed = duration_cast<nanoseconds>(end-start).count();
	fout << "arrBoolFaaAsmTime = [arrBoolFaaAsmTime " << elapsed/operations << "];" << endl;
//======================================================================
	start = high_resolution_clock::now();
	for ( int i = 0; i < operations; ++i )
	{
	functionCASAsm (pAddr2,&oldValue2,&newValue2,&returnValue2);
	}
	end = high_resolution_clock::now();
	elapsed = duration_cast<nanoseconds>(end-start).count();
	fout << "arrIntCasAsmTime = [arrIntCasAsmTime " << elapsed/operations << "];" << endl;
//======================================================================
	start = high_resolution_clock::now();
	for ( int i = 0; i < operations; ++i )
	{
	functionFAAAsm (&oldValue2,pAddr2);
	}
	end = high_resolution_clock::now();
	elapsed = duration_cast<nanoseconds>(end-start).count();
	fout << "arrIntFaaAsmTime = [arrIntFaaAsmTime " << elapsed/operations << "];" << endl;
//======================================================================
	start = high_resolution_clock::now();
	for ( int i = 0; i < operations; ++i )
	{
	functionCASAsm (pAddr3,&oldValue3,&newValue3,&returnValue3);
	}
	end = high_resolution_clock::now();
	elapsed = duration_cast<nanoseconds>(end-start).count();
	fout << "arrDoubleCasAsmTime = [arrDoubleCasAsmTime " << elapsed/operations << "];" << endl;
//======================================================================
	start = high_resolution_clock::now();
	for ( int i = 0; i < operations; ++i )
	{
	functionFAAAsm (&oldValue3,pAddr3);
	}
	end = high_resolution_clock::now();
	elapsed = duration_cast<nanoseconds>(end-start).count();
	fout << "arrDoubleFaaAsmTime = [arrDoubleFaaAsmTime " << elapsed/operations << "];" << endl;
//======================================================================
	//cout << "Iterations = " << operations << endl;
	}
	
	
}

int main ()
{
	fout.open("resultWithNotAlignedData.m");
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
	fout << "axis([-1000000 16000000 23.5 27.5])" << endl;
	fout << "plot(arrOperations,arrBoolCasAsmTime,'*-')" << endl;
	fout << "plot(arrOperations,arrIntCasAsmTime,'*-')" << endl;
	fout << "plot(arrOperations,arrDoubleCasAsmTime,'*-')" << endl;
	fout << "plot(arrOperations,arrBoolFaaAsmTime,'o-')" << endl;
	fout << "plot(arrOperations,arrIntFaaAsmTime,'o-')" << endl;
	fout << "plot(arrOperations,arrDoubleFaaAsmTime,'o-')" << endl;
	fout << "grid on" << endl;
	fout << "legend('Cas bool Asm','Cas int Asm','Cas double Asm','Faa bool Asm','Faa int Asm','Faa double Asm','North')" << endl;
    fout << "xlabel('cycles iterations with not aligned data')" << endl;
    fout << "ylabel('time latency in nanosec')" << endl;
	fout.close();
	return 0;
}
