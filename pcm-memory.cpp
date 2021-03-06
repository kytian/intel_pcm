/*
   Copyright (c) 2009-2012, Intel Corporation
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of Intel Corporation nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// written by Patrick Lu


/*!     \file pcm-memory.cpp
  \brief Example of using CPU counters: implements a performance counter monitoring utility for memory controller channels
  */
#define HACK_TO_REMOVE_DUPLICATE_ERROR
#include <iostream>
#ifdef _MSC_VER
#pragma warning(disable : 4996) // for sprintf
#include <windows.h>
#include "../PCM_Win/windriver.h"
#else
#include <unistd.h>
#include <signal.h>
#endif
#include <math.h>
#include <iomanip>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <assert.h>
#include "cpucounters.h"
#include "utils.h"

//Programmable iMC counter
#define READ 0
#define WRITE 1
#define PARTIAL 2

using namespace std;

void print_help(char * prog_name)
{
#ifdef _MSC_VER
    cout << " Usage: " << prog_name << " <delay>|\"external_program parameters\"|--help|--uninstallDriver|--installDriver <other options>" << endl;
#else
    cout << " Usage: " << prog_name << " <delay>|[external_program parameters]" << endl;
#endif
    cout << " Example: " << prog_name << " \"sleep 1\"" << endl;
    cout << " Example: " << prog_name << " 1" << endl;
    cout << endl;
}

void display_bandwidth(float *iMC_Rd_socket_chan, float *iMC_Wr_socket_chan, float *iMC_Rd_socket, float *iMC_Wr_socket, uint32 numSockets, uint32 num_imc_channels, uint64 *partial_write)
{
    float sysRead = 0.0, sysWrite = 0.0;
    uint32 skt = 0;
    cout.setf(ios::fixed);
    cout.precision(2);

    while(skt < numSockets)
    {
        if(!(skt % 2) && ((skt+1) < numSockets)) //This is even socket, and it has at least one more socket which can be displayed together
        {
            cout << "\
                \r---------------------------------------||---------------------------------------\n\
                \r--             Socket "<<skt<<"              --||--             Socket "<<skt+1<<"              --\n\
                \r---------------------------------------||---------------------------------------\n\
                \r---------------------------------------||---------------------------------------\n\
                \r---------------------------------------||---------------------------------------\n\
                \r--   Memory Performance Monitoring   --||--   Memory Performance Monitoring   --\n\
                \r---------------------------------------||---------------------------------------\n\
                \r"; 
            for(uint64 channel = 0; channel < num_imc_channels; ++channel)
            {
                if(iMC_Rd_socket_chan[skt*num_imc_channels+channel] < 0.0 && iMC_Wr_socket_chan[skt*num_imc_channels+channel] < 0.0) //If the channel read neg. value, the channel is not working; skip it.
                    continue;
                std::cout << "\r--  Mem Ch "
                    <<channel
                    <<": Reads (MB/s):"
                    <<setw(8)
                    <<iMC_Rd_socket_chan[skt*num_imc_channels+channel];
                cout <<"  --||--  Mem Ch "
                    <<channel
                    <<": Reads (MB/s):"
                    <<setw(8)
                    <<iMC_Rd_socket_chan[(skt+1)*num_imc_channels+channel]
                    <<"  --"
                    <<endl;
                cout << "\r--            Writes(MB/s):"
                    <<setw(8)
                    <<iMC_Wr_socket_chan[skt*num_imc_channels+channel];
                cout <<"  --||--            Writes(MB/s):"
                    <<setw(8)
                    <<iMC_Wr_socket_chan[(skt+1)*num_imc_channels+channel]
                    <<"  --"
                    <<endl;
            }
            cout << "\
                \r-- NODE"<<skt<<" Mem Read (MB/s):  "<<setw(8)<<iMC_Rd_socket[skt]<<"  --||-- NODE"<<skt+1<<" Mem Read (MB/s):  "<<setw(8)<<iMC_Rd_socket[skt+1]<<"  --\n\
                \r-- NODE"<<skt<<" Mem Write (MB/s): "<<setw(8)<<iMC_Wr_socket[skt]<<"  --||-- NODE"<<skt+1<<" Mem Write (MB/s): "<<setw(8)<<iMC_Wr_socket[skt+1]<<"  --\n\
                \r-- NODE"<<skt<<" P. Write (T/s) :"<<dec<<setw(10)<<partial_write[skt]<<"  --||-- NODE"<<skt+1<<" P. Write (T/s): "<<dec<<setw(10)<<partial_write[skt+1]<<"  --\n\
                \r-- NODE"<<skt<<" Memory (MB/s): "<<setw(11)<<std::right<<iMC_Rd_socket[skt]+iMC_Wr_socket[skt]<<"  --||-- NODE"<<skt+1<<" Memory (MB/s): "<<setw(11)<<iMC_Rd_socket[skt+1]+iMC_Wr_socket[skt+1]<<"  --\n\
                \r";
           sysRead += iMC_Rd_socket[skt];
           sysRead += iMC_Rd_socket[skt+1];
           sysWrite += iMC_Wr_socket[skt];
           sysWrite += iMC_Wr_socket[skt+1];
           skt += 2;
        }
        else //Display one socket in this row
        {
            cout << "\
                \r---------------------------------------|\n\
                \r--             Socket "<<skt<<"              --|\n\
                \r---------------------------------------|\n\
                \r---------------------------------------|\n\
                \r---------------------------------------|\n\
                \r--   Memory Performance Monitoring   --|\n\
                \r---------------------------------------|\n\
                \r"; 
            for(uint64 channel = 0; channel < num_imc_channels; ++channel)
            {
                if(iMC_Rd_socket_chan[skt*num_imc_channels+channel] < 0.0 && iMC_Wr_socket_chan[skt*num_imc_channels+channel] < 0.0) //If the channel read neg. value, the channel is not working; skip it.
                    continue;
                cout << "--  Mem Ch "
                    <<channel
                    <<": Reads (MB/s):"
                    <<setw(8)
                    <<iMC_Rd_socket_chan[skt*num_imc_channels+channel]
                    <<"  --|\n--            Writes(MB/s):"
                    <<setw(8)
                    <<iMC_Wr_socket_chan[skt*num_imc_channels+channel]
                    <<"  --|\n";

            }
            cout << "\
                \r-- NODE"<<skt<<" Mem Read (MB/s):  "<<setw(8)<<iMC_Rd_socket[skt]<<"  --|\n\
                \r-- NODE"<<skt<<" Mem Write (MB/s) :"<<setw(8)<<iMC_Wr_socket[skt]<<"  --|\n\
                \r-- NODE"<<skt<<" P. Write (T/s) :"<<setw(10)<<dec<<partial_write[skt]<<"  --|\n\
                \r-- NODE"<<skt<<" Memory (MB/s): "<<setw(8)<<iMC_Rd_socket[skt]+iMC_Wr_socket[skt]<<"     --|\n\
                \r";

            sysRead += iMC_Rd_socket[skt];
            sysWrite += iMC_Wr_socket[skt];
            skt += 1;
        }
    }
    cout << "\
        \r---------------------------------------||---------------------------------------\n\
        \r--                   System Read Throughput(MB/s):"<<setw(10)<<sysRead<<"                  --\n\
        \r--                  System Write Throughput(MB/s):"<<setw(10)<<sysWrite<<"                  --\n\
        \r--                 System Memory Throughput(MB/s):"<<setw(10)<<sysRead+sysWrite<<"                  --\n\
        \r---------------------------------------||---------------------------------------" << endl;
}

const uint32 max_sockets = 4;
const uint32 max_imc_channels = 8;

void calculate_bandwidth(PCM *m, const ServerUncorePowerState uncState1[], const ServerUncorePowerState uncState2[], uint64 elapsedTime)
{
    //const uint32 num_imc_channels = m->getMCChannelsPerSocket();
    float iMC_Rd_socket_chan[max_sockets][max_imc_channels];
    float iMC_Wr_socket_chan[max_sockets][max_imc_channels];
    float iMC_Rd_socket[max_sockets];
    float iMC_Wr_socket[max_sockets];
    uint64 partial_write[max_sockets];

    for(uint32 skt = 0; skt < m->getNumSockets(); ++skt)
    {
        iMC_Rd_socket[skt] = 0.0;
        iMC_Wr_socket[skt] = 0.0;
        partial_write[skt] = 0;

        for(uint32 channel = 0; channel < max_imc_channels; ++channel)
        {
            if(getMCCounter(channel,READ,uncState1[skt],uncState2[skt]) == 0.0 && getMCCounter(channel,WRITE,uncState1[skt],uncState2[skt]) == 0.0) //In case of JKT-EN, there are only three channels. Skip one and continue.
            {
                iMC_Rd_socket_chan[skt][channel] = -1.0;
                iMC_Wr_socket_chan[skt][channel] = -1.0;
                continue;
            }

            iMC_Rd_socket_chan[skt][channel] = (float) (getMCCounter(channel,READ,uncState1[skt],uncState2[skt]) * 64 / 1000000.0 / (elapsedTime/1000.0));
            iMC_Wr_socket_chan[skt][channel] = (float) (getMCCounter(channel,WRITE,uncState1[skt],uncState2[skt]) * 64 / 1000000.0 / (elapsedTime/1000.0));

            iMC_Rd_socket[skt] += iMC_Rd_socket_chan[skt][channel];
            iMC_Wr_socket[skt] += iMC_Wr_socket_chan[skt][channel];

            partial_write[skt] += (uint64) (getMCCounter(channel,PARTIAL,uncState1[skt],uncState2[skt]) / (elapsedTime/1000.0));
        }
    }

    display_bandwidth(iMC_Rd_socket_chan[0], iMC_Wr_socket_chan[0], iMC_Rd_socket, iMC_Wr_socket, m->getNumSockets(), max_imc_channels, partial_write);
}

int main(int argc, char * argv[])
{
#ifdef PCM_FORCE_SILENT
    null_stream nullStream1, nullStream2;
    std::cout.rdbuf(&nullStream1);
    std::cerr.rdbuf(&nullStream2);
#endif

    cout << endl;
    cout << " Intel(r) Performance Counter Monitor: Memory Bandwidth Monitoring Utility " << INTEL_PCM_VERSION << endl;
    cout << endl;
    cout << " Copyright (c) 2009-2013 Intel Corporation" << endl;
    cout << " This utility measures memory bandwidth per channel in real-time" << endl;
    cout << endl;
#ifdef _MSC_VER
    // Increase the priority a bit to improve context switching delays on Windows
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    TCHAR driverPath[1032];
    GetCurrentDirectory(1024, driverPath);
    wcscat_s(driverPath, 1032, L"\\msr.sys");

    SetConsoleCtrlHandler((PHANDLER_ROUTINE)cleanup, TRUE);
#else
    signal(SIGPIPE, cleanup);
    signal(SIGINT, cleanup);
    signal(SIGKILL, cleanup);
    signal(SIGTERM, cleanup);
#endif

    int delay = 1;
    char * sysCmd = NULL;

    if (argc >= 2)
    {
        if (strcmp(argv[1], "--help") == 0 ||
                strcmp(argv[1], "-h") == 0 ||
                strcmp(argv[1], "/h") == 0)
        {
            print_help(argv[0]);
            return -1;
        }

#ifdef _MSC_VER
        if (strcmp(argv[1], "--uninstallDriver") == 0)
        {
            Driver tmpDrvObject;
            tmpDrvObject.uninstall();
            cout << "msr.sys driver has been uninstalled. You might need to reboot the system to make this effective." << endl;
            return 0;
        }
        if (strcmp(argv[1], "--installDriver") == 0)
        {
            Driver tmpDrvObject;
            if (!tmpDrvObject.start(driverPath))
            {
                cout << "Can not access CPU counters" << endl;
                cout << "You must have signed msr.sys driver in your current directory and have administrator rights to run this program" << endl;
                return -1;
            }
            return 0;
        }
#endif

        delay = atoi(argv[1]);
        if (delay <= 0)
        {
            sysCmd = argv[1];
        }
    }

#ifdef _MSC_VER
    // WARNING: This driver code (msr.sys) is only for testing purposes, not for production use
    Driver drv;
    // drv.stop();     // restart driver (usually not needed)
    if (!drv.start(driverPath))
    {
        cout << "Cannot access CPU counters" << endl;
        cout << "You must have signed msr.sys driver in your current directory and have administrator rights to run this program" << endl;
    }
#endif

    PCM * m = PCM::getInstance();
    m->disableJKTWorkaround();
    PCM::ErrorCode status = m->program();
    switch (status)
    {
        case PCM::Success:
            break;
        case PCM::MSRAccessDenied:
            cout << "Access to Intel(r) Performance Counter Monitor has denied (no MSR or PCI CFG space access)." << endl;
            return -1;
        case PCM::PMUBusy:
            cout << "Access to Intel(r) Performance Counter Monitor has denied (Performance Monitoring Unit is occupied by other application). Try to stop the application that uses PMU." << endl;
            cout << "Alternatively you can try to reset PMU configuration at your own risk. Try to reset? (y/n)" << endl;
            char yn;
            std::cin >> yn;
            if ('y' == yn)
            {
                m->resetPMU();
                cout << "PMU configuration has been reset. Try to rerun the program again." << endl;
            }
            return -1;
        default:
            cout << "Access to Intel(r) Performance Counter Monitor has denied (Unknown error)." << endl;
            return -1;
    }
    
    cout << "\nDetected "<< m->getCPUBrandString() << " \"Intel(r) microarchitecture codename "<<m->getUArchCodename()<<"\""<<endl;
    if(!m->hasPCICFGUncore())
    {
        cout << "Jaketown, Ivytown or Haswell Server CPU is required for this tool!" << endl;
        if(m->memoryTrafficMetricsAvailable())
            cout << "For processor-level memory bandwidth statistics please use pcm.x" << endl;
        m->cleanup();
        return -1;
    }

    if(m->getNumSockets() > max_sockets)
    {
        cout << "Only systems with up to "<<max_sockets<<" sockets are supported! Program aborted" << endl;
        m->cleanup();
        return -1;
    }

    ServerUncorePowerState * BeforeState = new ServerUncorePowerState[m->getNumSockets()];
    ServerUncorePowerState * AfterState = new ServerUncorePowerState[m->getNumSockets()];
    uint64 BeforeTime = 0, AfterTime = 0;

    cout << "Update every "<<delay<<" seconds"<< endl;

    BeforeTime = m->getTickCount();
    for(uint32 i=0; i<m->getNumSockets(); ++i)
        BeforeState[i] = m->getServerUncorePowerState(i); 

    while(1)
    {
#ifdef _MSC_VER
        int delay_ms = delay * 1000;
        // compensate slow Windows console output
        if(AfterTime) delay_ms -= (int)(m->getTickCount() - BeforeTime);
        if(delay_ms < 0) delay_ms = 0;
#else
        int delay_ms = delay * 1000;
#endif

        if(sysCmd)
            MySystem(sysCmd);
        else
            MySleepMs(delay_ms);

        AfterTime = m->getTickCount();
        for(uint32 i=0; i<m->getNumSockets(); ++i)
            AfterState[i] = m->getServerUncorePowerState(i);

        cout << "Time elapsed: "<<dec<<fixed<<AfterTime-BeforeTime<<" ms\n";
        cout << "Called sleep function for "<<dec<<fixed<<delay_ms<<" ms\n";

        calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime);

        swap(BeforeTime, AfterTime);
        swap(BeforeState, AfterState);

        if(sysCmd) break;

    }

    delete[] BeforeState;
    delete[] AfterState;

    m->cleanup();

    return 0;
}
