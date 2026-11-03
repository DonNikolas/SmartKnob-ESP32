#pragma once
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
using esp_err_t=int;
using i2s_port_t=int;
using i2s_mode_t=int;
constexpr int ESP_OK=0,I2S_NUM_0=0,I2S_MODE_MASTER=1,I2S_MODE_RX=2,I2S_MODE_PDM=4;
constexpr int I2S_BITS_PER_SAMPLE_16BIT=16,I2S_CHANNEL_FMT_RIGHT_LEFT=0,I2S_COMM_FORMAT_STAND_I2S=0;
constexpr int I2S_PIN_NO_CHANGE=-1,I2S_PDM_DSR_16S=16;
enum { I2S_EVENT_DMA_ERROR, I2S_EVENT_RX_DONE, I2S_EVENT_RX_Q_OVF };
struct i2s_event_t { int type; size_t size; };
struct i2s_config_t {
    i2s_mode_t mode;
    int sample_rate,bits_per_sample,channel_format,communication_format,dma_buf_count,dma_buf_len;
    bool use_apll;
};
struct i2s_pin_config_t {int mck_io_num,bck_io_num,ws_io_num,data_out_num,data_in_num;};
esp_err_t i2s_driver_install(i2s_port_t,const i2s_config_t*,int,void*);
esp_err_t i2s_set_pin(i2s_port_t,const i2s_pin_config_t*);
esp_err_t i2s_set_pdm_rx_down_sample(i2s_port_t,int);
esp_err_t i2s_driver_uninstall(i2s_port_t);
esp_err_t i2s_read(i2s_port_t,void*,size_t,size_t*,TickType_t);
