#ifndef _ADS129x_H
#define _ADS129x_H

#define ADS_ID_VAL   0x3E

#define LOFF_MAG_6NA        (0b00000000)
#define LOFF_MAG_24NA       (0b00000100)
#define LOFF_MAG_6UA        (0b00001000)
#define LOFF_MAG_24UA       (0b00001100)
#define LOFF_FREQ_DC        (0b00000000)
#define LOFF_FREQ_7p8HZ     (0b00000001)
#define LOFF_FREQ_31p2HZ    (0b00000010)
#define LOFF_FREQ_FS_4      (0b00000011)

enum ads129x_cmd{
    ADS_CMD_WAKEUP  = 0x02,
    ADS_CMD_STANDBY = 0x04,
    ADS_CMD_RESET   = 0x06,
    ADS_CMD_START   = 0x08,
    ADS_CMD_STOP    = 0x0A,
    ADS_CMD_RDATAC  = 0x10,
    ADS_CMD_SDATAC  = 0x11,
    ADS_CMD_RDATA   = 0x12,
    ADS_CMD_RREG    = 0x20,
    ADS_CMD_WREG    = 0x40
};

enum ads129x_reg{
    ADS_REG_ID      = 0x00,
    ADS_REG_CONFIG1 = 0x01,
    ADS_REG_CONFIG2 = 0x02,
    ADS_REG_CONFIG3 = 0x03,
    ADS_REG_LOFF    = 0x04,
    ADS_REG_CH1SET  = 0x05,
    ADS_REG_CH2SET  = 0x06,
    ADS_REG_CH3SET  = 0x07,
    ADS_REG_CH4SET  = 0x08,
    ADS_REG_CH5SET  = 0x09,
    ADS_REG_CH6SET  = 0x0A,
    ADS_REG_CH7SET  = 0x0B,
    ADS_REG_CH8SET  = 0x0C,
    ADS_REG_BIAS_SENSP = 0x0D,
    ADS_REG_BIAS_SENSN = 0x0E,
    ADS_REG_LOFF_SENSP = 0x0F,
    ADS_REG_LOFF_SENSN = 0x10,
    ADS_REG_LOFF_FLIP  = 0x11,
    ADS_REG_LOFF_STATP = 0x12,
    ADS_REG_LOFF_STATN = 0x13,
    ADS_REG_GPIO       = 0x14,
    ADS_REG_MISC1      = 0x15,
    ADS_REG_MISC2      = 0x16,
    ADS_REG_CONFIG4    = 0x17
};

enum ads129x_gain{
    ADS_GAIN_01  =  0x00,
    ADS_GAIN_02  =  0x10,
    ADS_GAIN_04  =  0x20,
    ADS_GAIN_06  =  0x30,
    ADS_GAIN_08  =  0x40,
    ADS_GAIN_12  =  0x50,
    ADS_GAIN_24  =  0x60
};

enum ads129x_input_type{
    ADS_INPUT_NORMAL     =  0x00,
    ADS_INPUT_SHORTED    =  0x00,
    ADS_INPUT_BIAS_MEAS  =  0x00,
    ADS_INPUT_MVDD       =  0x00,
    ADS_INPUT_TEMP       =  0x00,
    ADS_INPUT_TESTING    =  0x00,
    ADS_INPUT_BIAS_DRP   =  0x00,
    ADS_INPUT_BIAS_DRN   =  0x00
};

enum ads_data_rate {
    ADS_DATA_RATE_16000 = 0,
    ADS_DATA_RATE_8000,
    ADS_DATA_RATE_4000,
    ADS_DATA_RATE_2000,
    ADS_DATA_RATE_1000,
    ADS_DATA_RATE_500,
    ADS_DATA_RATE_250
};

struct ads_setting{
    unsigned char sampling_rate;
    unsigned char outclk_en;
    //channel settings
    unsigned char power_down;
    unsigned char gain;
    unsigned char input_type;
    unsigned char bias_set;
    unsigned char srb2_set;
    unsigned char srb1_set;
};
void ads_cs_low(int ss);
void ads_cs_high(int ss);
int ads_initialize(int ss, struct ads_setting *setting);
void ads_stream_start(int ss);
void ads_stream_stop(int ss);
void ads_stream_fetch(int ss, unsigned char* buffer);
void ads_output_clk(int ss, int enabled);
void ads_configure_leadoff_detection(int ss, unsigned char amplitudeCode, unsigned char freqCode);
void ads_change_leadoff_detect(int ss, unsigned char ch, int p_en, int n_en);
void ads_deactivate_ch(int ss, unsigned char ch);
void ads_wakeup(int ss);
void ads_standby(int ss);
void ads_reset(int ss);
void ads_start(int ss);
void ads_stop(int ss);
void ads_rdatac(int ss);
void ads_sdatac(int ss);
void ads_rdata(int ss, unsigned char *data);
unsigned char ads_rreg(int ss, unsigned char reg_addr);
void ads_wreg(int ss, unsigned char reg_addr, unsigned char reg_val);

#endif
