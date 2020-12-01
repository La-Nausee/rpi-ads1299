#ifndef _CONFIG_H
#define _CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ads129x.h"
 
#define DEVICE_ID               0
#define SERVER_IP               "163.143.158.127"
#define SERVER_PORT             6000
#define SERVER_DATA_NUM         100        //sent data packets within one communication, interval = SERVER_DATA_NUM/sampling_rate seconds
#define SERVER_DATA_PRECISION   6
#define SPI_SPEED               4000000  //0 ~ 20 MHz
#define ADS_DRDY_PIN            9
#define ADS_PWRDN_PIN           0
#define ADS_START_PIN           8
#define ADS_RESET_PIN           7

#define ADS_CS0_PIN      23
#define ADS_CS1_PIN      24
#define ADS_CS2_PIN      25
#define ADS_CS3_PIN      27
#define ADS_CS4_PIN      28
#define ADS_CS5_PIN      29

#define ENBALE_FIRST_EXPANDER    1
#define ENABLE_SECOND_EXPANDER   0 
#define ENABLE_THIRD_EXPANDER    0
#define ENABLE_FOURTH_EXPANDER   0
#define ENABLE_FIFTH_EXPANDER    0
#define ENABLE_SIXTH_EXPANDER    0

#define ADS_DATA_RATE   ADS_DATA_RATE_1000
#define ADS_INPUT_TYPE  ADS_INPUT_NORMAL      // input muxer setting
#define ADS_GAIN_SET    ADS_GAIN_24           // Gain setting
#define ADS_BIAS_SET    1                     // add this channel to bias generation
#define ADS_SRB2_SET    1                     // connect this P side to SRB2
#define ADS_SRB1_SET    0                     // don't use SRB1

#ifdef __cplusplus
}
#endif

#endif
