#include "ads129x.h"
#include "config.h"

#include <sstream>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstring>
#include <queue>
#include <mutex>
#include <sched.h>
#include <pthread.h>
#include <netdb.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wiringPi.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#define APP_EVENT_NEWFILE    0x01
#define APP_EVENT_CLOSEFILE  0x02
#define APP_EVENT_EXIT       0x03
#define APP_EVENT_DRDY       0x04

using namespace std;

bool           is_connected = false;
bool           is_try_exit = false;
queue<char>    ads_drdy_queue;
queue<string*> forward_queue;
mutex          forward_mutex;
mutex          m_mutex;

void ads_handler(void)
{
    ads_drdy_queue.push(APP_EVENT_DRDY);
}

void *ads_thread(void *threadid)
{
    struct ads_setting ads_brd;
    double scale_to_vol;
    int temp;
    unsigned int value;
    unsigned char prefix;
    unsigned char buffer[27];
    double ddata[8]; // 8 chs converted data in double
    unsigned int frame_counter = 0;
    int     data_str_counter = SERVER_DATA_NUM;
    string* data_str = new string();
    struct timeval te;

    stringstream in;

    wiringPiSetup();
    
    if(wiringPiISR(ADS_DRDY_PIN,INT_EDGE_FALLING,&ads_handler) < 0)
	{
        printf("[FAILED] to setup ADS ISR\r\n");
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

    ads_stream_start(ADS_CS0_PIN);

    while(1)
	{
        if(!ads_drdy_queue.empty())
        {
            while(!ads_drdy_queue.empty())
                ads_drdy_queue.pop();
            memset(buffer,0,27);

            gettimeofday(&te, NULL); // get current time
            ads_stream_fetch(ADS_CS0_PIN, buffer);
            
            //raw data convert to be in double
            for(int i=0; i<8; i++)
            {
                if(buffer[i*3+3] > 127)
                {
                    prefix = 0xFF;
                }
                else
                {
                    prefix = 0x00;
                }
               //Two's complement to int
                value = ((uint32_t(buffer[i*3+3])<<16) | (uint32_t(buffer[i*3+3+1])<<8) | (uint32_t(buffer[i*3+3+2]))) & 0x7FFFFF;
                if(buffer[i*3+3] & 0x80) // minus
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

            if(data_str_counter-- > 0)
            {
                // in<<"{\"id\":"<<DEVICE_ID<<",";
                // in<<"\"ts\":"<<(unsigned long)time(NULL)<<",";
                // in<<"\"sec\":"<<te.tv_sec<<",";
                // in<<"\"usec\":"<<te.tv_usec<<",";
                // in<<"\"frame\":"<<frame_counter++<<",";
                // in<<"\"data\":\""<<std::setprecision(SERVER_DATA_PRECISION)<<ddata[0];
                int i = 0;
                frame_counter++;
                while(i < 7)
                {
                    in<<std::setprecision(SERVER_DATA_PRECISION)<<ddata[i++]<<",";
                }

                in<<std::setprecision(SERVER_DATA_PRECISION)<<ddata[i];
                // in<<"\"}";
                // in<<"\r\n";

                //below in hex format

                // data_str->append(in.str());
            }  

            if(data_str_counter <= 0)
            {

                in<<"\"}";
                in<<"\r\n";
                data_str->append(in.str());

                forward_mutex.lock();
                forward_queue.push(data_str);
                forward_mutex.unlock();

                data_str_counter = SERVER_DATA_NUM;
                data_str = new string();
                in.str("");
                in<<"{\"id\":"<<DEVICE_ID<<",";
                in<<"\"ts\":"<<(unsigned long)time(NULL)<<",";
                in<<"\"sec\":"<<te.tv_sec<<",";
                in<<"\"data\":\"";
            }  
            else
            {
                in<<",";
            }
            
        }

        m_mutex.lock();
        if(is_try_exit)
        {
            printf("[INFO] terminate ads thread\r\n");
            exit (1);
        }
        m_mutex.unlock();
    }
}

void *forward_thread(void *threadid)
{
    int sockfd;  
    struct sockaddr_in server_addr;
    string* data_str;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("[FAILED] to create socket descripitor\r\n");
        exit(1);
    }

    server_addr.sin_family = AF_INET;      /* host byte order */
    server_addr.sin_port = htons(SERVER_PORT);    /* short, network byte order */
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    //inet_aton(SERVER_IP, &server_addr.sin_addr);

    bzero(&(server_addr.sin_zero), 8);     /* zero the rest of the struct */

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        printf("[FAILED] to connect to server\r\n");
        exit(1);
    }

	while (1) {
        if(!forward_queue.empty())
        {
            forward_mutex.lock();
            data_str = forward_queue.front();
            forward_queue.pop();
            forward_mutex.unlock();
            if(send(sockfd, data_str->c_str(), data_str->size(), 0) == -1)
            {
                printf("[FAILED] to send data to server\r\n");
                exit (1);
            }

            delete data_str;
            data_str = NULL;
        }

        m_mutex.lock();
        if(is_try_exit)
        {
            printf("[INFO] terminate forward thread\r\n");
            exit (1);
        }
        m_mutex.unlock();

		usleep(100);
	}

    close(sockfd);
}

int main()
{
    pthread_t ads_tid;
    pthread_t forward_tid;
    char key = 0;

    pthread_create(&forward_tid,NULL,forward_thread,(void *)1234);
    pthread_create(&ads_tid,NULL,ads_thread,(void *)1234);

	while(1)
	{
		cout<<endl;
		cout<<"Press Q to Exit"<<endl;
		key = getchar();
		if(key == 'q' || key == 'Q')
		{	
            m_mutex.lock();
            is_try_exit = true;
            m_mutex.unlock();
			
            pthread_join(forward_tid, NULL);
            pthread_join(ads_tid, NULL);

            cout<<"Goodbye."<<endl;

			exit(0);
		}
	}

	return 0;
}
