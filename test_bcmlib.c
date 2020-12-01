/*
 * ads.c
 *
 * Code to perform communication with ADS1298
 * this time, /CS pin control done "by hand" to rule the delay between end of SCLK and
 * /CS rise (end of frame)
 *
 */
 
#define RESETPIN RPI_BPLUS_GPIO_J8_07 //RESET pin
#define CS_PIN  RPI_BPLUS_GPIO_J8_24 //SPI CS pin
#define CS_DELAY 3  //how many microseconds of delay
 
#include <stdio.h>
#include <bcm2835.h>
 
enum spi_command {
  //system commands
  WAKEUP = 0x02,
  STANDBY = 0x04,
  RESET = 0x06,
  START = 0x08,
  STOP = 0x0a,
  //read commands
  RDATAC = 0x10,
  SDATAC = 0x11,
  RDATA = 0x12,
  //register commands
  RREG = 0x20,
  WREG = 0x40
};
 
 
 
enum reg {
  // device settings
  ID = 0x00,
 
  // global settings
  CONFIG1 = 0x01,
  CONFIG2 = 0x02,
  CONFIG3 = 0x03,
  LOFF = 0x04,
 
  // channel specific settings
  CHnSET = 0x04,
  CH1SET = CHnSET + 1,
  CH2SET = CHnSET + 2,
  CH3SET = CHnSET + 3,
  CH4SET = CHnSET + 4,
  CH5SET = CHnSET + 5,
  CH6SET = CHnSET + 6,
  CH7SET = CHnSET + 7,
  CH8SET = CHnSET + 8,
  RLD_SENSP = 0x0d,
  RLD_SENSN = 0x0e,
  LOFF_SENSP = 0x0f,
  LOFF_SENSN = 0x10,
  LOFF_FLIP = 0x11,
 
  // lead off status
  LOFF_STATP = 0x12,
  LOFF_STATN = 0x13,
 
  // other
  GPIO = 0x14,
  PACE = 0x15,
  RESP = 0x16,
  CONFIG4 = 0x17,
  WCT1 = 0x18,
  WCT2 = 0x19
};
 
void cs_select(void){  //CS low (remember about negative logic here)
 bcm2835_gpio_clr(CS_PIN);
}
 
 
void cs_deselect(void){  //CS high (negative logic here)
 bcm2835_gpio_set(CS_PIN);
} 
 
 
 
 
int main(int argc, char **argv){
 
 int idVal = 0;
 int maxChan = 0;
 char buf[3] = {0,0,0};
 
 
 if(!bcm2835_init()){
  printf("Oops, bcm2835 not initialised correctly");
  return 1;
 }
 
 if(!bcm2835_spi_begin()){ //_spi_begin returns 1 if spi_begin does its job with success
  printf("SPI not initialised correctly (Maybe you are not running as root?)");
  return 1;
 }
 
 bcm2835_gpio_fsel(RESETPIN, BCM2835_GPIO_FSEL_OUTP); //RESETPIN as output
 bcm2835_gpio_write(RESETPIN, HIGH); //reset high
 
 bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST); //bit order (non-active in bcm2835 ver1.55 - always MSBFIRST)
 
 //Set CS pin polarity to low (LOW when CS active)
 //due to incosistency in time between end of SCLK and raising edge of /CS
 //this /CS pin will be handled manually now
 //see https://groups.google.com/forum/#!topic/bcm2835/wsMqsNqiquo (google group of bcm2835, thread titled: SPI co-operation with ADS1298. Incosistent reading.)
 //bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, 0); //original line
 
 //Set which CS pin to use for next transfers
 //bcm2835_spi_chipSelect(BCM2835_SPI_CS0); //commented out because of: see previous lines
 bcm2835_spi_chipSelect(BCM2835_SPI_CS_NONE); //do NOT set any /CS automatically
 
 bcm2835_gpio_fsel(CS_PIN, BCM2835_GPIO_FSEL_OUTP); //CS is an output
 cs_select();
 
 //Set SPI clock speed
 // BCM2835_SPI_CLOCK_DIVIDER_128   = 128,     ///< 128 = 512ns = = 1.953125MHz
 // BCM2835_SPI_CLOCK_DIVIDER_64    = 64,      ///< 64 = 256ns = 3.90625MHz
 // BCM2835_SPI_CLOCK_DIVIDER_32    = 32,      ///< 32 = 128ns = 7.8125MHz
 // BCM2835_SPI_CLOCK_DIVIDER_16    = 16,      ///< 16 = 64ns = 15.625MHz
 bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64); //< 64 = 256ns = 3.90625MHz  8 SCLK = 2.048us
 
 //Set SPI data mode
 // BCM2835_SPI_MODE1 = 1,  // CPOL = 0, CPHA = 1, Clock idle low, data is clocked in on falling edge, output data (change) on rising edge
 bcm2835_spi_setDataMode(BCM2835_SPI_MODE1); //(SPI_MODE_# | SPI_CS_HIGH)=Chip Select active high, (SPI_MODE_# | SPI_NO_CS)=1 device per bus no Chip Select
 
 //reset the device
 bcm2835_gpio_write(RESETPIN, LOW);
 delay(1);
 bcm2835_gpio_write(RESETPIN, HIGH);
 delay(150);//wait x ms (0.128s required)   
 
 buf[0]=(RREG | ID);
 buf[1]=0x00;
 buf[2]=0x00;
 
 //determine model number and number of channels available
 //register read command: send two bytes to ADS1298: 001r rrrr and 000n nnnn,
 //where r rrrr - starting register, n nnnn - (number of register to read - 1) ALL IN ONE BURST (no CS change)
 
 printf("Before spi_transfern:\n");
 printf("Hex value of buf[0] = %x \n", buf[0]);
 printf("Hex value of buf[1] = %x \n", buf[1]);
 printf("Hex value of buf[2] = %x \n", buf[2]); 
 
 cs_select();
 bcm2835_spi_transfer(SDATAC);
 bcm2835_delayMicroseconds(CS_DELAY);
 cs_deselect();
 bcm2835_delayMicroseconds(5); //minimum of 2 SCLK (0.5us for 3.9MHz)
 cs_select();
 bcm2835_spi_transfern(buf, 3);
 bcm2835_delayMicroseconds(CS_DELAY);
 cs_deselect();
 
 printf("After spi_transfern:\n");
 printf("Hex value of buf[0] = %x \n", buf[0]);
 printf("Hex value of buf[1] = %x \n", buf[1]);
 printf("Hex value of buf[2] = %x \n", buf[2]);
 
 switch (buf[2] & 0b00011111){ //least significant bits say about channels
  case 0b10000: //16
   maxChan = 4; //asd1294
   break;
  case 0b10001: //17
   maxChan = 6; //ads1296
   break;
  case 0b10010: //18
   maxChan = 8; //ads1298
   break;
  case 0b11110: //30
   maxChan = 8; //ads1299
   break;
  default:
   maxChan = 0;
  }  
 
 idVal=buf[2];
 printf("idVal: %x \n", idVal); //hex92 (or dec146) expected from ads1298
 printf("Device Type (ID Control Register): ");
 printf("%d\n", idVal);
 printf("Channels: ");
 printf("%d\n\n", maxChan);
 
 delay(20);
 
 cs_select();
 bcm2835_spi_transfer(SDATAC);
 bcm2835_delayMicroseconds(CS_DELAY);
 cs_deselect();
 //Return SPI pins to default inputs state
 bcm2835_spi_end(); 
 return 0;
}