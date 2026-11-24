/**
 * @file adaps_imu.c
 * @author
 * @brief 
 * @version 0.1
 * @date 2026-06-09
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "common.h"
#include "bsp_spi.h"
#include "bsp_basic.h"
#include "bsp_gpio.h"
#include "board_config.h"
#include "adaps_imu.h"
#include "bmi270.h"
#include "bmi270_context.h"
#include "adaps_soc_protocol.h"

#define BMI270_SPI_CHN  IMU_SPI_CHAN

/*! Earth's gravity in m/s^2 */
#define GRAVITY_EARTH  (9.80665f)

/*! Macros to select the sensors                   */
#define ACCEL          UINT8_C(0x00)
#define GYRO           UINT8_C(0x01)

enum imu_state_e{
    IMU_STATE_NOT_INIT = 0,
    IMU_STATE_SAMPLE_STOP,
    IMU_STATE_SAMPLE_START,
    IMU_STATE_INIT_FAIL
};

static uint32_t s_bmi270_state = IMU_STATE_NOT_INIT;
static struct bmi2_dev s_bmi_dev;
static struct bmi2_sens_config s_bmi2_sens_config;
/* Structure to define type of sensor and their respective data. */
static struct bmi2_sens_data s_bmi2_sensor_data;

//实现Middlewares\BMI270\bmi270_examples\common\common.h
/******************************************************************************/
/*!                       Function Definitions                                */

/*!
 *  @brief Function for reading the sensor's registers through I2C bus.
 *
 *  @param[in] reg_addr     : Register address.
 *  @param[out] reg_data    : Pointer to the data buffer to store the read data.
 *  @param[in] length       : No of bytes to read.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return Status of execution
 *  @retval = BMI2_INTF_RET_SUCCESS -> Success
 *  @retval != BMI2_INTF_RET_SUCCESS  -> Failure Info
 *
 */
BMI2_INTF_RETURN_TYPE bmi2_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    return BMI2_E_COM_FAIL;
}

/*!
 *  @brief Function for writing the sensor's registers through I2C bus.
 *
 *  @param[in] reg_addr     : Register address.
 *  @param[in] reg_data     : Pointer to the data buffer whose value is to be written.
 *  @param[in] length       : No of bytes to write.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return Status of execution
 *  @retval = BMI2_INTF_RET_SUCCESS -> Success
 *  @retval != BMI2_INTF_RET_SUCCESS  -> Failure Info
 *
 */
BMI2_INTF_RETURN_TYPE bmi2_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    return BMI2_E_COM_FAIL;
}

/*!
 *  @brief Function for reading the sensor's registers through SPI bus.
 *
 *  @param[in] reg_addr     : Register address.
 *  @param[out] reg_data    : Pointer to the data buffer to store the read data.
 *  @param[in] length       : No of bytes to read.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return Status of execution
 *  @retval = BMI2_INTF_RET_SUCCESS -> Success
 *  @retval != BMI2_INTF_RET_SUCCESS  -> Failure Info
 *
 */
BMI2_INTF_RETURN_TYPE bmi2_spi_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    int ret;
    ret = bsp_spi_lock(BMI270_SPI_CHN, 1);
    ret |= bsp_spi_write_data(BMI270_SPI_CHN, &reg_addr, sizeof(reg_addr));
    ret |= bsp_spi_read_data(BMI270_SPI_CHN, reg_data, len);
    ret |= bsp_spi_unlock(BMI270_SPI_CHN);

    if(ret != 0){
        return BMI2_E_COM_FAIL;
    }

    return BMI2_INTF_RET_SUCCESS;
}

/*!
 *  @brief Function for writing the sensor's registers through SPI bus.
 *
 *  @param[in] reg_addr     : Register address.
 *  @param[in] reg_data     : Pointer to the data buffer whose data has to be written.
 *  @param[in] length       : No of bytes to write.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return Status of execution
 *  @retval = BMI2_INTF_RET_SUCCESS -> Success
 *  @retval != BMI2_INTF_RET_SUCCESS  -> Failure Info
 *
 */
BMI2_INTF_RETURN_TYPE bmi2_spi_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    int ret;
    ret = bsp_spi_lock(BMI270_SPI_CHN, 1);
    ret |= bsp_spi_write_data(BMI270_SPI_CHN, &reg_addr, sizeof(reg_addr));
    ret |= bsp_spi_write_data(BMI270_SPI_CHN, (uint8_t *)reg_data, len);
    ret |= bsp_spi_unlock(BMI270_SPI_CHN);
    if(ret != 0){
        return BMI2_E_COM_FAIL;
    }
    return BMI2_INTF_RET_SUCCESS;
}

/*!
 * @brief This function provides the delay for required time (Microsecond) as per the input provided in some of the
 * APIs.
 *
 *  @param[in] period       : The required wait time in microsecond.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return void.
 *
 */
void bmi2_delay_us(uint32_t period, void *intf_ptr)
{
    bsp_delay_us(period);
}

/*!
 *  @brief Function to select the interface between SPI and I2C.
 *
 *  @param[in] bmi    : Structure instance of bmi2_dev
 *  @param[in] intf   : Interface selection parameter
 *
 *  @return Status of execution
 *  @retval 0 -> Success
 *  @retval < 0 -> Failure Info
 */
int8_t bmi2_interface_init(struct bmi2_dev *bmi, uint8_t intf)
{
    if(NULL == bmi){
        return BMI2_E_NULL_PTR;
    }
    if(intf != BMI2_SPI_INTF){
        return BMI2_E_COM_FAIL;
    }

    bmi->intf = BMI2_SPI_INTF;
    bmi->read = bmi2_spi_read;
    bmi->write = bmi2_spi_write;
    bmi->intf_ptr = NULL;
    bmi->delay_us = bmi2_delay_us;
    bmi->read_write_len = 46;
    bmi->config_file_ptr = NULL;

    return BMI2_OK;
}

/*!
 *  @brief Prints the execution status of the APIs.
 *
 *  @param[in] rslt     : Error code returned by the API whose execution status has to be printed.
 *
 *  @return void.
 */
void bmi2_error_codes_print_result(int8_t rslt)
{
    switch (rslt)
    {
        case BMI2_OK:

            /* Do nothing */
            break;

        case BMI2_W_FIFO_EMPTY:
            printf("Warning [%d] : FIFO empty\r\n", rslt);
            break;
        case BMI2_W_PARTIAL_READ:
            printf("Warning [%d] : FIFO partial read\r\n", rslt);
            break;
        case BMI2_E_NULL_PTR:
            printf(
                "Error [%d] : Null pointer error. It occurs when the user tries to assign value (not address) to a pointer," " which has been initialized to NULL.\r\n",
                rslt);
            break;

        case BMI2_E_COM_FAIL:
            printf(
                "Error [%d] : Communication failure error. It occurs due to read/write operation failure and also due " "to power failure during communication\r\n",
                rslt);
            break;

        case BMI2_E_DEV_NOT_FOUND:
            printf("Error [%d] : Device not found error. It occurs when the device chip id is incorrectly read\r\n",
                   rslt);
            break;

        case BMI2_E_INVALID_SENSOR:
            printf(
                "Error [%d] : Invalid sensor error. It occurs when there is a mismatch in the requested feature with the " "available one\r\n",
                rslt);
            break;

        case BMI2_E_SELF_TEST_FAIL:
            printf(
                "Error [%d] : Self-test failed error. It occurs when the validation of accel self-test data is " "not satisfied\r\n",
                rslt);
            break;

        case BMI2_E_INVALID_INT_PIN:
            printf(
                "Error [%d] : Invalid interrupt pin error. It occurs when the user tries to configure interrupt pins " "apart from INT1 and INT2\r\n",
                rslt);
            break;

        case BMI2_E_OUT_OF_RANGE:
            printf(
                "Error [%d] : Out of range error. It occurs when the data exceeds from filtered or unfiltered data from " "fifo and also when the range exceeds the maximum range for accel and gyro while performing FOC\r\n",
                rslt);
            break;

        case BMI2_E_ACC_INVALID_CFG:
            printf(
                "Error [%d] : Invalid Accel configuration error. It occurs when there is an error in accel configuration" " register which could be one among range, BW or filter performance in reg address 0x40\r\n",
                rslt);
            break;

        case BMI2_E_GYRO_INVALID_CFG:
            printf(
                "Error [%d] : Invalid Gyro configuration error. It occurs when there is a error in gyro configuration" "register which could be one among range, BW or filter performance in reg address 0x42\r\n",
                rslt);
            break;

        case BMI2_E_ACC_GYR_INVALID_CFG:
            printf(
                "Error [%d] : Invalid Accel-Gyro configuration error. It occurs when there is a error in accel and gyro" " configuration registers which could be one among range, BW or filter performance in reg address 0x40 " "and 0x42\r\n",
                rslt);
            break;

        case BMI2_E_CONFIG_LOAD:
            printf(
                "Error [%d] : Configuration load error. It occurs when failure observed while loading the configuration " "into the sensor\r\n",
                rslt);
            break;

        case BMI2_E_INVALID_PAGE:
            printf(
                "Error [%d] : Invalid page error. It occurs due to failure in writing the correct feature configuration " "from selected page\r\n",
                rslt);
            break;

        case BMI2_E_SET_APS_FAIL:
            printf(
                "Error [%d] : APS failure error. It occurs due to failure in write of advance power mode configuration " "register\r\n",
                rslt);
            break;

        case BMI2_E_AUX_INVALID_CFG:
            printf(
                "Error [%d] : Invalid AUX configuration error. It occurs when the auxiliary interface settings are not " "enabled properly\r\n",
                rslt);
            break;

        case BMI2_E_AUX_BUSY:
            printf(
                "Error [%d] : AUX busy error. It occurs when the auxiliary interface buses are engaged while configuring" " the AUX\r\n",
                rslt);
            break;

        case BMI2_E_REMAP_ERROR:
            printf(
                "Error [%d] : Remap error. It occurs due to failure in assigning the remap axes data for all the axes " "after change in axis position\r\n",
                rslt);
            break;

        case BMI2_E_GYR_USER_GAIN_UPD_FAIL:
            printf(
                "Error [%d] : Gyro user gain update fail error. It occurs when the reading of user gain update status " "fails\r\n",
                rslt);
            break;

        case BMI2_E_SELF_TEST_NOT_DONE:
            printf(
                "Error [%d] : Self-test not done error. It occurs when the self-test process is ongoing or not " "completed\r\n",
                rslt);
            break;

        case BMI2_E_INVALID_INPUT:
            printf("Error [%d] : Invalid input error. It occurs when the sensor input validity fails\r\n", rslt);
            break;

        case BMI2_E_INVALID_STATUS:
            printf("Error [%d] : Invalid status error. It occurs when the feature/sensor validity fails\r\n", rslt);
            break;

        case BMI2_E_CRT_ERROR:
            printf("Error [%d] : CRT error. It occurs when the CRT test has failed\r\n", rslt);
            break;

        case BMI2_E_ST_ALREADY_RUNNING:
            printf(
                "Error [%d] : Self-test already running error. It occurs when the self-test is already running and " "another has been initiated\r\n",
                rslt);
            break;

        case BMI2_E_CRT_READY_FOR_DL_FAIL_ABORT:
            printf(
                "Error [%d] : CRT ready for download fail abort error. It occurs when download in CRT fails due to wrong " "address location\r\n",
                rslt);
            break;

        case BMI2_E_DL_ERROR:
            printf(
                "Error [%d] : Download error. It occurs when write length exceeds that of the maximum burst length\r\n",
                rslt);
            break;

        case BMI2_E_PRECON_ERROR:
            printf(
                "Error [%d] : Pre-conditional error. It occurs when precondition to start the feature was not " "completed\r\n",
                rslt);
            break;

        case BMI2_E_ABORT_ERROR:
            printf("Error [%d] : Abort error. It occurs when the device was shaken during CRT test\r\n", rslt);
            break;

        case BMI2_E_WRITE_CYCLE_ONGOING:
            printf(
                "Error [%d] : Write cycle ongoing error. It occurs when the write cycle is already running and another " "has been initiated\r\n",
                rslt);
            break;

        case BMI2_E_ST_NOT_RUNING:
            printf(
                "Error [%d] : Self-test is not running error. It occurs when self-test running is disabled while it's " "running\r\n",
                rslt);
            break;

        case BMI2_E_DATA_RDY_INT_FAILED:
            printf(
                "Error [%d] : Data ready interrupt error. It occurs when the sample count exceeds the FOC sample limit " "and data ready status is not updated\r\n",
                rslt);
            break;

        case BMI2_E_INVALID_FOC_POSITION:
            printf(
                "Error [%d] : Invalid FOC position error. It occurs when average FOC data is obtained for the wrong" " axes\r\n",
                rslt);
            break;

        default:
            printf("Error [%d] : Unknown error code\r\n", rslt);
            break;
    }
}

/*!
 * @brief This internal API is used to set configurations for accel and gyro.
 */
static int8_t set_accel_gyro_config(struct bmi2_dev *bmi)
{
    /* Status of api are returned to this variable. */
    int8_t rslt;

    /* Structure to define accelerometer and gyro configuration. */
    struct bmi2_sens_config config[2];

    /* Configure the type of feature. */
    config[ACCEL].type = BMI2_ACCEL;
    config[GYRO].type = BMI2_GYRO;

    /* Get default configurations for the type of feature selected. */
    rslt = bmi2_get_sensor_config(config, 2, bmi);
    bmi2_error_codes_print_result(rslt);

    /* Map data ready interrupt to interrupt pin. */
    rslt = bmi2_map_data_int(BMI2_DRDY_INT, BMI2_INT1, bmi);
    bmi2_error_codes_print_result(rslt);

    if (rslt == BMI2_OK)
    {
        /* NOTE: The user can change the following configuration parameters according to their requirement. */
        /* Set Output Data Rate */
        config[ACCEL].cfg.acc.odr = BMI2_ACC_ODR_200HZ;

        /* Gravity range of the sensor (+/- 2G, 4G, 8G, 16G). */
        config[ACCEL].cfg.acc.range = BMI2_ACC_RANGE_2G;

        /* The bandwidth parameter is used to configure the number of sensor samples that are averaged
         * if it is set to 2, then 2^(bandwidth parameter) samples
         * are averaged, resulting in 4 averaged samples.
         * Note1 : For more information, refer the datasheet.
         * Note2 : A higher number of averaged samples will result in a lower noise level of the signal, but
         * this has an adverse effect on the power consumed.
         */
        config[ACCEL].cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;

        /* Enable the filter performance mode where averaging of samples
         * will be done based on above set bandwidth and ODR.
         * There are two modes
         *  0 -> Ultra low power mode
         *  1 -> High performance mode(Default)
         * For more info refer datasheet.
         */
        config[ACCEL].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;

        /* The user can change the following configuration parameters according to their requirement. */
        /* Set Output Data Rate */
        config[GYRO].cfg.gyr.odr = BMI2_GYR_ODR_200HZ;

        /* Gyroscope Angular Rate Measurement Range.By default the range is 2000dps. */
        config[GYRO].cfg.gyr.range = BMI2_GYR_RANGE_2000;

        /* Gyroscope bandwidth parameters. By default the gyro bandwidth is in normal mode. */
        config[GYRO].cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;

        /* Enable/Disable the noise performance mode for precision yaw rate sensing
         * There are two modes
         *  0 -> Ultra low power mode(Default)
         *  1 -> High performance mode
         */
        config[GYRO].cfg.gyr.noise_perf = BMI2_POWER_OPT_MODE;

        /* Enable/Disable the filter performance mode where averaging of samples
         * will be done based on above set bandwidth and ODR.
         * There are two modes
         *  0 -> Ultra low power mode
         *  1 -> High performance mode(Default)
         */
        config[GYRO].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;

        /* Set the accel and gyro configurations. */
        rslt = bmi2_set_sensor_config(config, 2, bmi);
        bmi2_error_codes_print_result(rslt);
    }

    return rslt;
}

/*!
 * @brief This function converts lsb to meter per second squared for 16 bit accelerometer at
 * range 2G, 4G, 8G or 16G.
 */
static float lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width)
{
    double power = 2;

    float half_scale = (float)((pow((double)power, (double)bit_width) / 2.0f));

    return (GRAVITY_EARTH * val * g_range) / half_scale;
}

/*!
 * @brief This function converts lsb to degree per second for 16 bit gyro at
 * range 125, 250, 500, 1000 or 2000dps.
 */
static float lsb_to_dps(int16_t val, float dps, uint8_t bit_width)
{
    double power = 2;

    float half_scale = (float)((pow((double)power, (double)bit_width) / 2.0f));

    return (dps / (half_scale)) * (val);
}

static void bmi_int1_evt_cb(void)
{
    printf("bmi_int1_evt_cb...\n");
}

static void bmi_int2_evt_cb(void)
{
    printf("bmi_int2_evt_cb...\n");
}

//=======================================================

int imu_init(void)
{
    int8_t rslt;
    /* Assign accel and gyro sensor to variable. */
    uint8_t sensor_list[2] = { BMI2_ACCEL, BMI2_GYRO };

    bsp_gpio_write(BMI_EN_PORT, BMI_EN_PIN, 1);

    gpio_interrupt_set_cb(BMI_INT1_PORT, BMI_INT1_PIN, bmi_int1_evt_cb);
    gpio_interrupt_set_cb(BMI_INT2_PORT, BMI_INT2_PIN, bmi_int2_evt_cb);

    /* Interface reference is given as a parameter
     * For I2C : BMI2_I2C_INTF
     * For SPI : BMI2_SPI_INTF
     */
    rslt = bmi2_interface_init(&s_bmi_dev, BMI2_SPI_INTF);
    bmi2_error_codes_print_result(rslt);
    if(rslt != BMI2_OK){
        printf("bmi2_interface_init fail\n");
        return -1;
    }

    /* Initialize bmi270. */
    rslt = bmi270_init(&s_bmi_dev);
    bmi2_error_codes_print_result(rslt);
    if(rslt != BMI2_OK){
        printf("bmi270_init fail\n");
        return -1;
    }

    /* Accel and gyro configuration settings. */
    rslt = set_accel_gyro_config(&s_bmi_dev);
    bmi2_error_codes_print_result(rslt);
    if(rslt != BMI2_OK){
        printf("set_accel_gyro_config fail\n");
        return -1;
    }

    /* NOTE:
        * Accel and Gyro enable must be done after setting configurations
        */
    rslt = bmi2_sensor_enable(sensor_list, 2, &s_bmi_dev);
    bmi2_error_codes_print_result(rslt);
    if(rslt != BMI2_OK){
        printf("bmi2_sensor_enable fail\n");
        return -1;
    }

    s_bmi2_sens_config.type = BMI2_ACCEL;

    /* Get the accel configurations. */
    rslt = bmi2_get_sensor_config(&s_bmi2_sens_config, 1, &s_bmi_dev);
    bmi2_error_codes_print_result(rslt);
    if(rslt != BMI2_OK){
        printf("bmi2_get_sensor_config fail\n");
        return -1;
    }

    s_bmi270_state = IMU_STATE_SAMPLE_STOP;
    printf("bmi270 init ok\n");

    return 0;
}

void imu_proc(void)
{
    static uint32_t last_sample_time = 0;
    static uint8_t send_buf[128];
    uint32_t u32val;
    int8_t rslt;
    float acc_x;
    float acc_y;
    float acc_z;
    float gyr_x;
    float gyr_y;
    float gyr_z;

    if(reg_get_flag(OFFSET_IMU_WORK_CTRL)){
        reg_reset_flag(OFFSET_IMU_WORK_CTRL, REG_FLAG_ALL);
        if(reg_val_get(OFFSET_IMU_WORK_CTRL)){
            printf("imu work ctrl 'start', cur state %d\n", s_bmi270_state);
            if(s_bmi270_state == IMU_STATE_SAMPLE_STOP){
                s_bmi270_state = IMU_STATE_SAMPLE_START;
            }
        }else{
            printf("imu work ctrl 'stop', cur state %d\n", s_bmi270_state);
            if(s_bmi270_state == IMU_STATE_SAMPLE_START){
                s_bmi270_state = IMU_STATE_SAMPLE_STOP;
            }            
        }
    }

    if(s_bmi270_state == IMU_STATE_SAMPLE_START){
        u32val = reg_val_get(OFFSET_IMU_SAMPLE_PERIOD);
        if(bsp_tick_get() - last_sample_time > u32val){
            do{
                rslt = bmi2_get_sensor_data(&s_bmi2_sensor_data, &s_bmi_dev);
                if(rslt != BMI2_OK){
                    break;
                }
                if((!(s_bmi2_sensor_data.status & BMI2_DRDY_ACC))
                || (!(s_bmi2_sensor_data.status & BMI2_DRDY_GYR))){
                    break;
                }
                last_sample_time = bsp_tick_get();
                /* Converting lsb to meter per second squared for 16 bit accelerometer at 2G range. */
                acc_x = lsb_to_mps2(s_bmi2_sensor_data.acc.x, (float)2, s_bmi_dev.resolution);
                acc_y = lsb_to_mps2(s_bmi2_sensor_data.acc.y, (float)2, s_bmi_dev.resolution);
                acc_z = lsb_to_mps2(s_bmi2_sensor_data.acc.z, (float)2, s_bmi_dev.resolution);
                /* Converting lsb to degree per second for 16 bit gyro at 2000dps range. */
                gyr_x = lsb_to_dps(s_bmi2_sensor_data.gyr.x, (float)2000, s_bmi_dev.resolution);
                gyr_y = lsb_to_dps(s_bmi2_sensor_data.gyr.y, (float)2000, s_bmi_dev.resolution);
                gyr_z = lsb_to_dps(s_bmi2_sensor_data.gyr.z, (float)2000, s_bmi_dev.resolution);
                // format: "$IMU,timestamp,ax,ay,az,gx,gy,gz,checksum\r\n"
                snprintf(send_buf, sizeof(send_buf),
                         "$IMU,%u,%4.2f,%4.2f,%4.2f,%4.2f,%4.2f,%4.2f",
                         last_sample_time, acc_x, acc_y, acc_z, gyr_x, gyr_y,
                         gyr_z);
                u32val = calc_str_xor(send_buf);
                printf("%s,%02X\r\n", send_buf, u32val);
            }while(0);
        }
    }
}

#ifdef HW_HOST_BMI_CHIP
void imu_test(void)
{
    int8_t rslt;
    float acc_x = 0, acc_y = 0, acc_z = 0;
    float gyr_x = 0, gyr_y = 0, gyr_z = 0;
    printf("\n=====================================\n");
    if(s_bmi270_state == IMU_STATE_NOT_INIT){
        printf("bmi270 is not inited\n");
        return;
    }else if(s_bmi270_state == IMU_STATE_INIT_FAIL){
        printf("bmi270 was fail to init\n");
        return;
    }
    rslt = bmi2_get_sensor_data(&s_bmi2_sensor_data, &s_bmi_dev);
    bmi2_error_codes_print_result(rslt);
    if(rslt != BMI2_OK){
        printf("bmi2_get_sensor_data fail\n");
        return;
    }

    if((s_bmi2_sensor_data.status & BMI2_DRDY_ACC) 
    && (s_bmi2_sensor_data.status & BMI2_DRDY_GYR)){
        /* Converting lsb to meter per second squared for 16 bit accelerometer at 2G range. */
        acc_x = lsb_to_mps2(s_bmi2_sensor_data.acc.x, (float)2, s_bmi_dev.resolution);
        acc_y = lsb_to_mps2(s_bmi2_sensor_data.acc.y, (float)2, s_bmi_dev.resolution);
        acc_z = lsb_to_mps2(s_bmi2_sensor_data.acc.z, (float)2, s_bmi_dev.resolution);

        /* Converting lsb to degree per second for 16 bit gyro at 2000dps range. */
        gyr_x = lsb_to_dps(s_bmi2_sensor_data.gyr.x, (float)2000, s_bmi_dev.resolution);
        gyr_y = lsb_to_dps(s_bmi2_sensor_data.gyr.y, (float)2000, s_bmi_dev.resolution);
        gyr_z = lsb_to_dps(s_bmi2_sensor_data.gyr.z, (float)2000, s_bmi_dev.resolution);

        printf("%d, %d, %d, %d, %4.2f, %4.2f, %4.2f, %d, %d, %d, %4.2f, %4.2f, %4.2f\n",
                s_bmi2_sens_config.cfg.acc.range,
                s_bmi2_sensor_data.acc.x,
                s_bmi2_sensor_data.acc.y,
                s_bmi2_sensor_data.acc.z,
                acc_x,
                acc_y,
                acc_z,
                s_bmi2_sensor_data.gyr.x,
                s_bmi2_sensor_data.gyr.y,
                s_bmi2_sensor_data.gyr.z,
                gyr_x,
                gyr_y,
                gyr_z);
    }else{
        printf("bmi2 sensor data status(0x%x) is not ready!\n", s_bmi2_sensor_data.status);
    }
}
#else
void imu_test(void)
{
    printf("\nUnsupport imu...\n");
}
#endif
