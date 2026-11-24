/**
 * @file adaps_rx_temprature.c
 * @author rongchao (rongchao.xu@adaps-ph.com)
 * @brief 
 * @version 0.1
 * @date 2023-10-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */


#include "bsp_adc.h"

//NCU15XH103F6SRC 温度-电阻对应表(R-center)
//来源：https://www.murata.com/products/productdata/8798326980638/NTHG08.txt?1467257423000
static const float s_ntc_factor_tab[] = {
195.6520,
184.9171,
174.8452,
165.3910,
156.5125,
148.1710,
140.3304,
132.9576,
126.0215,
119.4936,
113.3471,
107.5649,
102.1155,
 96.9776,
 92.1315,
 87.5588,
 83.2424,
 79.1663,
 75.3157,
 71.6768,
 68.2367,
 64.9907,
 61.9190,
 59.0113,
 56.2579,
 53.6496,
 51.1779,
 48.8349,
 46.6132,
 44.5058,
 42.5062,
 40.5997,
 38.7905,
 37.0729,
 35.4417,
 33.8922,
 32.4197,
 31.0200,
 29.6890,
 28.4231,
 27.2186,
 26.0760,
 24.9877,
 23.9509,
 22.9629,
 22.0211,
 21.1230,
 20.2666,
 19.4495,
 18.6698,
 17.9255,
 17.2139,
 16.5344,
 15.8856,
 15.2658,
 14.6735,
 14.1075,
 13.5664,
 13.0489,
 12.5540,
 12.0805,
 11.6281,
 11.1947,
 10.7795,
 10.3815,
 10.0000,
 9.6342,
 9.2835,
 8.9470,
 8.6242,
 8.3145,
 8.0181,
 7.7337,
 7.4609,
 7.1991,
 6.9479,
 6.7067,
 6.4751,
 6.2526,
 6.0390,
 5.8336,
 5.6357,
 5.4454,
 5.2623,
 5.0863,
 4.9169,
 4.7539,
 4.5971,
 4.4461,
 4.3008,
 4.1609,
 4.0262,
 3.8964,
 3.7714,
 3.6510,
 3.5350,
 3.4231,
 3.3152,
 3.2113,
 3.1110,
 3.0143,
 2.9224,
 2.8337,
 2.7482,
 2.6657,
 2.5861,
 2.5093,
 2.4351,
 2.3635,
 2.2943,
 2.2275,
 2.1627,
 2.1001,
 2.0396,
 1.9811,
 1.9245,
 1.8698,
 1.8170,
 1.7658,
 1.7164,
 1.6685,
 1.6224,
 1.5777,
 1.5345,
 1.4927,
 1.4521,
 1.4129,
 1.3749,
 1.3381,
 1.3025,
 1.2680,
 1.2343,
 1.2016,
 1.1700,
 1.1393,
 1.1096,
 1.0807,
 1.0528,
 1.0256,
 0.9993,
 0.9738,
 0.9492,
 0.9254,
 0.9022,
 0.8798,
 0.8580,
 0.8368,
 0.8162,
 0.7963,
 0.7769,
 0.7580,
 0.7397,
 0.7219,
 0.7046,
 0.6878,
 0.6715,
 0.6556,
 0.6402,
 0.6252,
 0.6106,
 0.5964,
 0.5826,
 0.5692,
 0.5562,
 0.5435,
 0.5311,
 0.5191,
 0.5074,
 0.4961,
 0.4850,
 0.4742,
 0.4637,
 0.4535,
 0.4435,
 0.4338,
 0.4243,
 0.4151,
 0.4062,
 0.3974,
 0.3889,
 0.3807,
 0.3726,
 0.3647,
 0.3571,
 0.3496,
 0.3423,
 0.3352,
 0.3283,
 0.3215,
 0.3150,
 0.3085
};
#define NTC_FACTOR_CNT  sizeof(s_ntc_factor_tab) / sizeof(s_ntc_factor_tab[0])
#define NTC_TEMP_MIN  (-40.0f)

static float ntc_temp_cal(uint32_t adc_result)
{
    #define NTC_ADC_VREF   (2.5f) //2.5V
    #define NTC_ADC_R1     (10.0f) //10k ohm
    #define NTC_ADC_MAX     4096
    int i;
    float r;
    float temprature = NTC_TEMP_MIN;
    float last_val;
    float cur_val;

    if(adc_result >= NTC_ADC_MAX){
        return NTC_TEMP_MIN;
    }

    //[r / (NTC_ADC_R1 + r)] = [adc_result / NTC_ADC_MAX]
    //==> r = (NTC_ADC_R1 * adc_result) / (NTC_ADC_MAX - adc_result)
    r = (NTC_ADC_R1 * adc_result) / (NTC_ADC_MAX - adc_result);

    last_val = s_ntc_factor_tab[NTC_FACTOR_CNT - 1];
    if(last_val > r){
        temprature += (NTC_FACTOR_CNT - 1);
    }else if(s_ntc_factor_tab[0] > r){
        i = NTC_FACTOR_CNT - 2;
        for(i = NTC_FACTOR_CNT - 2; i >= 0; i--){
            cur_val = s_ntc_factor_tab[i];
            if(cur_val > r){
                temprature += (i + (cur_val - r) / (cur_val - last_val));
                break;
            }
            last_val = cur_val;
        }
    }
    return temprature;
}

float rx_temprature_get(void)
{
    uint32_t adc_result;
    adc_result = adc_get_result_average(ADC_CHANNEL_RX_TEMPERATURE, 10);
    return ntc_temp_cal(adc_result);
}