#include "ads129x.h"
#include "config.h"

#include <fstream>
#include <iostream>
#include <cassert>
#include <cstring>
#include <queue>
#include <mutex>
#include <sched.h>
#include <pthread.h>

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wiringPi.h>
#include <unistd.h>
#include <queue>

#define APP_EVENT_NEWFILE    0x01
#define APP_EVENT_CLOSEFILE  0x02
#define APP_EVENT_EXIT       0x03
#define APP_EVENT_DRDY       0x04

using namespace std;

queue<char>  ads_drdy_queue;
queue<char>  ads_thrd_queue;
string       filename;
ofstream     logfile;
mutex        mtx;

void ads_handler(void)
{
    ads_drdy_queue.push(APP_EVENT_DRDY);
}

void *ads_thread(void *threadid)
{
    // int ret;
    // pthread_t this_thread;
    // struct sched_param params;

	long tid;
	bool logging_en = false;

	tid = (long)threadid;

    struct ads_setting ads_brd;
    unsigned char buffer0[27];
    unsigned char buffer3[27];
    int temp;
    unsigned int value;
    unsigned char prefix;
    double scale_to_vol;
    double ddata[8]; // 8 chs converted data in double

    mtx.lock();

    // this_thread = pthread_self();
    // // We'll set the priority to the maximum.
    // params.sched_priority = sched_get_priority_max(SCHED_FIFO);
    // ret = pthread_setschedparam(this_thread, SCHED_FIFO, &params);
    // if (ret != 0) {
    //     // Print the error
    //     printf("Error to set thread realtime priority\r\n");
    //     printf("Try to run with SUDO permission\r\n");
    //     pthread_exit(NULL);   
    // }

    wiringPiSetup();
    
    if(wiringPiISR(ADS_DRDY_PIN,INT_EDGE_FALLING,&ads_handler) < 0)
	{
        printf("Error to setup ADS ISR\r\n");
        pthread_exit(NULL);
	}

    switch (ADS_GAIN_SET)
    {
    case ADS_GAIN_01:
        scale_to_vol = 5.0/1.0/pow(2.0,24);
        break;
    case ADS_GAIN_02:
        scale_to_vol = 5.0/2.0/pow(2.0,24);
        break;
    case ADS_GAIN_04:
        scale_to_vol = 5.0/4.0/pow(2.0,24);
        break;
    case ADS_GAIN_06:
        scale_to_vol = 5.0/6.0/pow(2.0,24);
        break;
    case ADS_GAIN_08:
        scale_to_vol = 5.0/8.0/pow(2.0,24);
        break;
    case ADS_GAIN_12:
        scale_to_vol = 5.0/12.0/pow(2.0,24);
        break;
    case ADS_GAIN_24:
        scale_to_vol = 5.0/24.0/pow(2.0,24);
        break;
    default:
        scale_to_vol = 5.0/24.0/pow(2.0,24);
        break;
    }

    ads_brd.sampling_rate = ADS_DATA_RATE;
    ads_brd.outclk_en = 0;
    ads_brd.power_down = 0;
    ads_brd.gain = ADS_GAIN_SET;
    ads_brd.input_type = ADS_INPUT_TYPE;
    ads_brd.bias_set = ADS_BIAS_SET;
    ads_brd.srb2_set = ADS_SRB2_SET;
    ads_brd.srb1_set = ADS_SRB1_SET;

    ads_initialize(ADS_CS0_PIN, &ads_brd);// mother board

    ads_brd.outclk_en = 0;
    ads_initialize(ADS_CS3_PIN, &ads_brd);// mother board

    mtx.unlock(); 

	while(1)
	{
		if(!ads_thrd_queue.empty())
		{
            ads_cs_low(ADS_CS3_PIN);
            ads_stream_stop(ADS_CS0_PIN);
            ads_cs_high(ADS_CS3_PIN);

			switch(ads_thrd_queue.front())
			{
			case APP_EVENT_NEWFILE:
				logging_en = true;
                if(logfile.is_open())
					logfile.close();
				logfile.open(filename.c_str());
				assert(!logfile.fail());

                while(!ads_drdy_queue.empty())
                {
                    ads_drdy_queue.pop();
                }

                ads_cs_low(ADS_CS3_PIN);
                ads_stream_start(ADS_CS0_PIN);
                ads_cs_high(ADS_CS3_PIN);

				break;
			case APP_EVENT_CLOSEFILE:
				logging_en = false;
                if(logfile.is_open())
                    logfile.close();
				break;
			case APP_EVENT_EXIT:
				logging_en = false;
                if(logfile.is_open())
                    logfile.close();
				pthread_exit(NULL);
				break;
			default:
				break;
			}
			ads_thrd_queue.pop();
		}
		
		if(logging_en)
		{
			if(!ads_drdy_queue.empty())
            {
                while(!ads_drdy_queue.empty())
                	ads_drdy_queue.pop();
                memset(buffer0,0,27);
                memset(buffer3,0,27);
                ads_stream_fetch(ADS_CS0_PIN, buffer0);
                ads_stream_fetch(ADS_CS3_PIN, buffer3);
                
                //raw data convert to be in double
                for(int i=0; i<8; i++)
                {
                    if(buffer0[i*3+3] > 127)
                    {
                        prefix = 0xFF;
                    }
                    else
                    {
                        prefix = 0x00;
                    }
                    //temp = (prefix<<24) + (buffer0[i*3+3]<<16) + (buffer0[i*3+3+1]<<8) + (buffer0[i*3+3+2]);
					//temp = int((uint32_t(buffer0[i*3+3])<<24) | (uint32_t(buffer0[i*3+3+1])<<16) | (uint32_t(buffer0[i*3+3+2])<<8))>>8;
                    //Two's complement to int
                    value = ((uint32_t(buffer0[i*3+3])<<16) | (uint32_t(buffer0[i*3+3+1])<<8) | (uint32_t(buffer0[i*3+3+2]))) & 0x7FFFFF;
                    if(buffer0[i*3+3] & 0x80) // minus
                    {
                        value = (~(value-1)) & 0x7FFFFF;
                        temp = -int(value);
                    }
                    else //positive
                    {
                        temp = int(value);
                    }
                    
                    ddata[i] = temp * scale_to_vol;
                }
                if(logfile.is_open())
                {
                    logfile<<ddata[0]<<","<<ddata[1]<<","<<ddata[2]<<","<<ddata[3]<<",";
                    logfile<<ddata[4]<<","<<ddata[5]<<","<<ddata[6]<<","<<ddata[7]<<",";
                }

                //raw data convert to be in double
                for(int i=0; i<8; i++)
                {
                    if(buffer3[i*3+3] > 127)
                    {
                        prefix = 0xFF;
                    }
                    else
                    {
                        prefix = 0x00;
                    }
                    //Two's complement to int
                    value = ((uint32_t(buffer3[i*3+3])<<16) | (uint32_t(buffer3[i*3+3+1])<<8) | (uint32_t(buffer3[i*3+3+2]))) & 0x7FFFFF;
                    if(buffer3[i*3+3] & 0x80) // minus
                    {
                        value = (~(value-1)) & 0x7FFFFF;
                        temp = -int(value);
                    }
                    else //positive
                    {
                        temp = int(value);
                    }
                    
                    ddata[i] = temp * scale_to_vol;
                }
                if(logfile.is_open())
                {
                    logfile<<ddata[0]<<","<<ddata[1]<<","<<ddata[2]<<","<<ddata[3]<<",";
                    logfile<<ddata[4]<<","<<ddata[5]<<","<<ddata[6]<<","<<ddata[7]<<endl;
                }
            }
	    }
    }
}

int main()
{
	char key = 0;
	pthread_t ads_log;
	pthread_create(&ads_log,NULL,ads_thread,(void *)1234);

    usleep(100);
    mtx.lock();

	while(1)
	{
		cout<<endl;
		cout<<"1: New Trial"<<endl;
		cout<<"2: Stop the Trial"<<endl;
		cout<<"3: Exit"<<endl;
		cout<<"Choice:";
		key = getchar();
		if(key == '1')
		{	
			string str;
			cout<<"Filename:";
			//getline(cin,filename);
			cin>>str;
			filename.clear();
			//hwt_filename = "gx3_";
			filename.append(str);
			filename.append(".csv");

            cout<<"Start Data Logging to "<<filename<<endl;
			ads_thrd_queue.push(APP_EVENT_NEWFILE);
		}
		else if(key == '2')
		{
            cout<<"Data Logging Stopped! Check the Recorded Data in "<<filename<<endl;
			ads_thrd_queue.push(APP_EVENT_CLOSEFILE);
		}
		else if(key == '3')
		{
            cout<<"Goodbye."<<endl;
			ads_thrd_queue.push(APP_EVENT_EXIT);
			pthread_join(ads_log, NULL);
			exit(0);
		}
	}
 
    mtx.unlock();

	return 0;
}
