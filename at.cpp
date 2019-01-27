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

const auto nruns = 10'000'000;

std::atomic<int> atvar(0);
std::atomic<int> tempVar(0);
int exp = 0;
int des = 1;

std::atomic<bool> flagIState(false);
std::atomic<bool> flagSState(false);


static __inline__ unsigned long long rdtsc(void) // для замера времени (абстрактного)
{
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long )hi) << 32 );
}
//=======================================================================================
void meas_M()
{
    //auto get_time = std::chrono::steady_clock::now;
    unsigned long long int startRdtsc1 =0, endRdtsc1=0;
    //decltype(get_time()) start, end;
    double sumtime = 0;

    // Write var to set M (Modified) state
    //atvar.store(100);

    for (auto i = 0; i < nruns; i++) {
        //start = get_time();
        atvar.store(100);
        startRdtsc1 = rdtsc();
        atvar.compare_exchange_weak(exp, des);
        endRdtsc1 = rdtsc();
        //end = get_time();

       // auto elapsed = std::chrono::duration_cast
          //  <std::chrono::nanoseconds>(end - start).count();
        auto elapsed = (endRdtsc1 - startRdtsc1);  
        sumtime += elapsed;
    }

    std::cout << "Latency time (M) " << double(sumtime)/nruns << std::endl;
    //std::cout << "Bandwidth time (M) " << double(nruns)/sumtime << std::endl;
}
//=====================================================================================
// в Owned можно попасть только из Modified попаданием при чтении (probe read hit / Bus Read)
// если делать read hit, то будет петля на Modified
// probe write hit this is Bus Read X
void meas_O()
{
    //auto get_time = std::chrono::steady_clock::now;
    unsigned long long int startRdtsc1 =0, endRdtsc1=0;
    //decltype(get_time()) start, end;
    double sumtime = 0;

    // Write var to set M (Modified) state
    //atvar.store(100);
     // Read var to set O (Owned) state ???
    //tempVar.store(atvar);

    for (auto i = 0; i < nruns; i++) {
        //start = get_time();
        atvar.store(100); // may be need barriers ??? (i mean for example StoreStore)?
        tempVar.store(atvar);
        startRdtsc1 = rdtsc();
        atvar.compare_exchange_weak(exp, des);
        endRdtsc1 = rdtsc();
        //end = get_time();

       // auto elapsed = std::chrono::duration_cast
          //  <std::chrono::nanoseconds>(end - start).count();
        auto elapsed = (endRdtsc1 - startRdtsc1);  
        sumtime += elapsed;
    }

    std::cout << "Latency time (O) " << double(sumtime)/nruns << std::endl;
    //std::cout << "Bandwidth time (O) " << double(nruns)/sumtime << std::endl;
}
//=================================================================================
void meas_E()
{
    unsigned long long int startRdtsc1 =0, endRdtsc1=0;
    double sumtime = 0;

    // Write atvar to set E (Exclusive) state
    //tempVar.store(atvar);
    
    for (auto i = 0; i < nruns; i++) {
		tempVar.store(atvar);
        startRdtsc1 = rdtsc();
        atvar.compare_exchange_weak(exp, des);
        endRdtsc1 = rdtsc();
        auto elapsed = (endRdtsc1 - startRdtsc1);  
        sumtime += elapsed;
    }
    std::cout << "Latency time (E) " << double(sumtime)/nruns << std::endl;
    //std::cout << "Bandwidth time (E) " << double(nruns)/sumtime << std::endl;
}
//===================================================================================


void meas_I()
{
    //auto get_time = std::chrono::steady_clock::now;
    unsigned long long int startRdtsc1 =0, endRdtsc1=0;
    //decltype(get_time()) start, end;
    double sumtime = 0;

    for (auto i = 0; i < nruns; i++) {
        while (flagIState.load() == false) {}

        //start = get_time();
        startRdtsc1 = rdtsc();
        atvar.compare_exchange_weak(exp, des);
        endRdtsc1 = rdtsc();
        //end = get_time();

       // auto elapsed = std::chrono::duration_cast
          //  <std::chrono::nanoseconds>(end - start).count();
        auto elapsed = (endRdtsc1 - startRdtsc1);
        sumtime += elapsed;

        flagIState.store(true);
    }

    std::cout << "Latency time (I) " << double(sumtime)/nruns << std::endl;
    //std::cout << "Bandwidth time (I) " << double(nruns)/sumtime << std::endl;
}
//==================================================================================
void meas_S()
{
    //auto get_time = std::chrono::steady_clock::now;
    unsigned long long int startRdtsc1 =0, endRdtsc1=0;
    //decltype(get_time()) start, end;
    double sumtime = 0;

    for (auto i = 0; i < nruns; i++) {
        while (flagSState.load() == false) {}
		tempVar.store(atvar);
        //start = get_time();
        startRdtsc1 = rdtsc();
        atvar.compare_exchange_weak(exp, des);
        endRdtsc1 = rdtsc();
        //end = get_time();

       // auto elapsed = std::chrono::duration_cast
          //  <std::chrono::nanoseconds>(end - start).count();
        auto elapsed = (endRdtsc1 - startRdtsc1);
        sumtime += elapsed;
        flagSState.store(true);
    }

    std::cout << "Latency time (S) " << double(sumtime)/nruns << std::endl;
    //std::cout << "Bandwidth time (S) " << double(nruns)/sumtime << std::endl;
}
//==================================================================================
void prep_S()
{
    for (auto i = 0; i < nruns; i++) {
        tempVar.store(atvar);
        flagSState.store(true);
        while (flagSState.load() == false) {}
    }
}
//==================================================================================
void prep_I()
{
    for (auto i = 0; i < nruns; i++) {
        atvar.store(100);
        flagIState.store(true);
        while (flagIState.load() == false) {}
    }
}
//===================================================================================
int main(int argc, const char *argv[])
{
	
	meas_E(); 
    meas_M();
    meas_O(); // To Owned only from Modified by probe read hit
    
    // measure for I state
    std::thread meas_I_thr(meas_I), prep_I_thr(prep_I);
    meas_I_thr.join();
    prep_I_thr.join();
    
    // measure for S state
    std::thread meas_S_thr(meas_S), prep_S_thr(prep_S);
    meas_S_thr.join();
    prep_S_thr.join();

    return 0;
}
