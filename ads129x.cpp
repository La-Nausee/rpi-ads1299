#include "ads129x.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <unistd.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>

#define SPI_CE0    0
#define SPI_CE1    1

static unsigned char ads_common_initialized = 0;

void ads_delay(useconds_t usec)
{
    usleep(usec);
}

void ads_cs_low(int ss)
{
    if(ss < 0)
        return;
    digitalWrite(ss, LOW);
}

void ads_cs_high(int ss)
{
    if(ss < 0)
        return;
    digitalWrite(ss, HIGH);
}

void ads_xfer(unsigned char *data, int len)
{
    wiringPiSPIDataRW(SPI_CE0,data,len);
}

void ads_common_initialize(int ss)
{
    int fd;
	int spiMode = 1;

    wiringPiSetup();

    pinMode(ADS_PWRDN_PIN, OUTPUT);
	pinMode(ADS_RESET_PIN, OUTPUT);
    pinMode(ADS_START_PIN, OUTPUT);

    digitalWrite(ADS_PWRDN_PIN, HIGH);
	digitalWrite(ADS_RESET_PIN, HIGH);
    digitalWrite(ADS_START_PIN, HIGH);

    fd = wiringPiSPISetup(SPI_CE0, SPI_SPEED);
    ioctl(fd, SPI_IOC_WR_MODE, &spiMode);

    usleep(200000);
	digitalWrite(ADS_RESET_PIN, LOW);
	usleep(4);
    digitalWrite(ADS_RESET_PIN, HIGH);
    usleep(20);
}

int ads_initialize(int ss, struct ads_setting *setting)
{
    unsigned char reg_val;
	pinMode(ss, OUTPUT);
    digitalWrite(ss, HIGH);

    if(!ads_common_initialized)
    {
        ads_common_initialized = 1;

        ads_common_initialize(ss);

        printf("Initialize ADS expanding boards commonly\r\n");
    }

    ads_reset(ss);

    ads_stop(ss);

    ads_sdatac(ss);

    if(ads_rreg(ss, ADS_REG_ID) != ADS_ID_VAL)
    {
        printf("ADS device with CS Pin: %d is not detected.\r\n", ss);
        return -1;
    }

    printf("ADS device with CS Pin: %d is detected.\r\n", ss);

    // turn off all channels
    for(int i=1; i<=8; i++)
    {
        ads_deactivate_ch(ss, i);
    }

    reg_val = 0b10010000;
    if(setting->outclk_en)
    {
        reg_val |= (1<<5);
    }
    reg_val |= setting->sampling_rate;
    ads_wreg(ss, ADS_REG_CONFIG1, reg_val);
    
     
    //write channel settings
    reg_val = 0x00;
    reg_val |= setting->gain;
    reg_val |= setting->input_type;
    if(setting->power_down)
    {
        reg_val |= 0x80;
    }
    if(setting->srb2_set)
    {
        reg_val |= 0x08;
    }
    for(int i=0; i<=7; i++)
    {
        ads_wreg(ss, ADS_REG_CH1SET + i, reg_val);
    }
    if(setting->bias_set)
    {
        ads_wreg(ss, ADS_REG_BIAS_SENSP, 0xFF);
        ads_wreg(ss, ADS_REG_BIAS_SENSN, 0xFF);
    }
    else 
    {
        ads_wreg(ss, ADS_REG_BIAS_SENSP, 0x00);
        ads_wreg(ss, ADS_REG_BIAS_SENSN, 0x00);
    }
    if(setting->srb1_set)
    {
       ads_wreg(ss, ADS_REG_MISC1, 0x20);
    }
    else
    {
        ads_wreg(ss, ADS_REG_MISC1, 0x00);
    }
    
    ads_wreg(ss, ADS_REG_CONFIG3, 0b11101100);

    ads_delay(500000);

    ads_configure_leadoff_detection(ss, LOFF_MAG_6NA, LOFF_FREQ_31p2HZ);

    return 0;
}

void ads_stream_start(int ss)
{
    ads_rdatac(ss);

    ads_start(ss);
}

void ads_stream_stop(int ss)
{
    ads_stop(ss);

    ads_sdatac(ss);
}

// buffer: 3 bytes status data + 8*3 bytes channels' data
void ads_stream_fetch(int ss, unsigned char* buffer)
{
    ads_cs_low(ss);
    ads_xfer(buffer, 27);
    ads_cs_high(ss);
}

void ads_output_clk(int ss, int enabled)
{
    unsigned char reg_val = ads_rreg(ss, ADS_REG_CONFIG1);
    if(enabled)
    {
        reg_val |= (1<<5);
    }
    else
    {
        reg_val &= (~(1<<5));
    }
    ads_wreg(ss, ADS_REG_CONFIG1, reg_val);
}

void ads_configure_leadoff_detection(int ss, unsigned char amplitudeCode, unsigned char freqCode)
{
  unsigned char reg_val = 0;

  amplitudeCode &= 0b00001100; //only these two bits should be used
  freqCode &= 0b00000011;      //only these two bits should be used

  reg_val = ads_rreg(ss, ADS_REG_LOFF);  
  reg_val &= 0b11110000;    //clear out the last four bits
  reg_val |= amplitudeCode; //set the amplitude
  reg_val |= freqCode;      //set the frequency
  ads_wreg(ss, ADS_REG_LOFF, reg_val);
}

//ch 1 ~ 8
void ads_change_leadoff_detect(int ss, unsigned char ch, int p_en, int n_en)
{
    unsigned char P_setting, N_setting;
    P_setting = ads_rreg(ss, ADS_REG_LOFF_SENSP);
    N_setting = ads_rreg(ss, ADS_REG_LOFF_SENSN);

    if(p_en)
    {
        P_setting |= (1<<(ch-1));
    }
    else
    {
        P_setting &= (~(1<<(ch-1)));
    }

    if(n_en)
    {
        N_setting |= (1<<(ch-1));
    }
    else
    {
        N_setting &= (~(1<<(ch-1)));
    }

    ads_wreg(ss, ADS_REG_LOFF_SENSP, P_setting);
    ads_wreg(ss, ADS_REG_LOFF_SENSN, N_setting);
}

//ch 1 ~ 8
void ads_deactivate_ch(int ss, unsigned char ch)
{   
    unsigned char setting = ads_rreg(ss, ADS_REG_CH1SET + ch - 1);
    setting |= (1<<7);
    setting &= (~(1<<3));
    ads_wreg(ss, ADS_REG_CH1SET + ch - 1, setting);

    setting = ads_rreg(ss, ADS_REG_BIAS_SENSP);
    setting &= (~(1<<(ch-1)));
    ads_wreg(ss, ADS_REG_BIAS_SENSP, setting);

    setting = ads_rreg(ss, ADS_REG_BIAS_SENSN);
    setting &= (~(1<<(ch-1)));
    ads_wreg(ss, ADS_REG_BIAS_SENSN, setting);

    ads_change_leadoff_detect(ss, ch, 0, 0);
}

//This is required when exiting standby mode
void ads_wakeup(int ss)
{
    unsigned char cmd = ADS_CMD_WAKEUP;
    ads_cs_low(ss);
    ads_xfer(&cmd,1);
    ads_delay(2); // >= 4tclk ~= 2us
    ads_cs_high(ss);
    ads_delay(2); // >= 4tclk
}

void ads_standby(int ss)
{
    unsigned char cmd = ADS_CMD_STANDBY;
    ads_cs_low(ss);
    ads_xfer(&cmd,1);
    ads_delay(2); // >= 4tclk ~= 2us
    ads_cs_high(ss);
    ads_delay(2); // >= 4tclk
}

void ads_reset(int ss)
{
    unsigned char cmd = ADS_CMD_RESET;
    ads_cs_low(ss);
    ads_xfer(&cmd,1);
    ads_delay(2); // >= 4tclk ~= 2us
    ads_cs_high(ss);
    ads_delay(10); // >= 18tclk
}

void ads_start(int ss)
{
    unsigned char cmd = ADS_CMD_START;
    ads_cs_low(ss);
    ads_xfer(&cmd,1);
    ads_delay(2); // >= 4tclk ~= 2us
    ads_cs_high(ss);
    ads_delay(2); // >= 4tclk
}

void ads_stop(int ss)
{
    unsigned char cmd = ADS_CMD_STOP;
    ads_cs_low(ss);
    ads_xfer(&cmd,1);
    ads_delay(2); // >= 4tclk ~= 2us
    ads_cs_high(ss);
    ads_delay(2); // >= 4tclk
}

void ads_rdatac(int ss)
{
    unsigned char cmd = ADS_CMD_RDATAC;
    ads_cs_low(ss);
    ads_xfer(&cmd,1);
    ads_delay(2); // >= 4tclk ~= 2us
    ads_cs_high(ss);
    ads_delay(2); // >= 4tclk
}


void ads_sdatac(int ss)
{
    unsigned char cmd = ADS_CMD_SDATAC;
    ads_cs_low(ss);
    ads_xfer(&cmd,1);
    ads_delay(2); // >= 4tclk ~= 2us
    ads_cs_high(ss);
    ads_delay(2); // >= 4tclk
}

// data length = 27 bytes(216 bits), 3 status bytes + 8 * Ch data 
void ads_rdata(int ss, unsigned char *data)
{
    unsigned char * buffer = (unsigned char*) malloc(28);
    buffer[0] = ADS_CMD_RDATA;
    ads_cs_low(ss);
    ads_xfer(buffer,28);
    ads_delay(2); // >= 4tclk ~= 2us
    ads_cs_high(ss);
    memcpy(data, buffer+1, 27);
    free(buffer);
}

// must be in 4MHz to interface
unsigned char ads_rreg(int ss, unsigned char reg_addr)
{
    unsigned char buffer[3];
    buffer[0] = ADS_CMD_RREG | reg_addr;
    buffer[1] = 0x00;
    buffer[2] = 0x00;
    ads_cs_low(ss);
    ads_xfer(buffer,1);
    ads_delay(2); // >= 4tclk ~= 2us
    ads_xfer(buffer+1,1);
    ads_delay(2); // >= 4tclk ~= 2us
    ads_xfer(buffer+2,1);
    ads_delay(2); // >= 4tclk ~= 2us
    ads_cs_high(ss);
    
    return buffer[2];
}

// must be in 4MHz to interface
void ads_wreg(int ss, unsigned char reg_addr, unsigned char reg_val)
{
    unsigned char buffer[3];
    buffer[0] = ADS_CMD_WREG | reg_addr;
    buffer[1] = 0x00;
    buffer[2] = reg_val;
    ads_cs_low(ss);
    ads_xfer(buffer,1);
    ads_delay(2); // >= 4tclk ~= 2us
    ads_xfer(buffer+1,1);
    ads_delay(2); // >= 4tclk ~= 2us
    ads_xfer(buffer+2,1);
    ads_delay(2); // >= 4tclk ~= 2us
    ads_cs_high(ss);
}
