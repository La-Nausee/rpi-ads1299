#include "ads1299.h"
#include "config.h"

#include <stdio.h>
#include <errno.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <unistd.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>

#define SPI_CE0        0
#define ADS_CS_PIN     ADS_CS0_PIN

void main()
{
	int fd;
	int spiMode = 1;
	unsigned char buffer[3];

	wiringPiSetup();
	
	pinMode(ADS_PWRDN_PIN, OUTPUT);
	pinMode(ADS_CS_PIN, OUTPUT);
	pinMode(ADS_RESET_PIN, OUTPUT);
	
	digitalWrite(ADS_PWRDN_PIN, HIGH);
    digitalWrite(ADS_CS_PIN, HIGH);
	digitalWrite(ADS_RESET_PIN, HIGH);
        
	fd = wiringPiSPISetup(SPI_CE0, SPI_SPEED);
    ioctl(fd, SPI_IOC_WR_MODE, &spiMode);

	usleep(200000);
	digitalWrite(ADS_RESET_PIN, LOW);
	usleep(4);
    digitalWrite(ADS_RESET_PIN, HIGH);
    usleep(20);
        
	buffer[0] = ADS_CMD_RESET;
	digitalWrite(ADS_CS_PIN, LOW);
    wiringPiSPIDataRW(SPI_CE0,buffer,1);
    usleep(2);
    digitalWrite(ADS_CS_PIN, HIGH);
	usleep(20);

	buffer[0] = ADS_CMD_SDATAC;
	digitalWrite(ADS_CS_PIN, LOW);
    wiringPiSPIDataRW(SPI_CE0,buffer,1);
    usleep(2);
    digitalWrite(ADS_CS_PIN, HIGH);
	usleep(2);

	buffer[0] = ADS_REG_ID | 0x20;
    buffer[1] = 0x00;
    buffer[2] = 0x00;
	digitalWrite(ADS_CS_PIN, LOW);
	wiringPiSPIDataRW(SPI_CE0, buffer, 1);
	usleep(2);
	wiringPiSPIDataRW(SPI_CE0, buffer+1, 1);
	usleep(2);
    wiringPiSPIDataRW(SPI_CE0, buffer+2, 1);
    usleep(2);
    digitalWrite(ADS_CS_PIN, HIGH);
        
	printf("ADS129x ID: 0x%02X\r\n", buffer[2]);
}

