/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "platform.h"

#include "build/debug.h"

#include "common/utils.h"

#include "config/parameter_group.h"
#include "config/parameter_group_ids.h"

#include "sensors/gyro.h"

#include "fc/settings.h"

#include "blackbox/blackbox.h"

#include "cms/cms.h"

#include "fc/config.h"
#include "fc/controlrate_profile.h"
#include "fc/fc_core.h"
#include "fc/rc_adjustments.h"

#include "flight/altitude.h"
#include "flight/failsafe.h"
#include "flight/imu.h"
#include "flight/mixer.h"
#include "flight/navigation.h"
#include "flight/pid.h"
#include "flight/servos.h"

#include "io/gimbal.h"
#include "io/gps.h"
#include "io/ledstrip.h"
#include "io/osd.h"
#include "io/vtx_rtc6705.h"

#include "rx/rx.h"
#include "rx/spektrum.h"

#include "sensors/acceleration.h"
#include "sensors/barometer.h"
#include "sensors/battery.h"
#include "sensors/boardalignment.h"
#include "sensors/compass.h"

#include "telemetry/frsky.h"
#include "telemetry/telemetry.h"

// Sensor names (used in lookup tables for *_hardware settings and in status command output)
// sync with accelerationSensor_e
const char * const lookupTableAccHardware[] = {
    "AUTO", "NONE", "ADXL345", "MPU6050", "MMA8452", "BMA280", "LSM303DLHC",
    "MPU6000", "MPU6500", "MPU9250", "ICM20601", "ICM20602", "ICM20608", "ICM20689", "BMI160", "FAKE"
};
#if defined(USE_SENSOR_NAMES) || defined(BARO)
// sync with baroSensor_e
const char * const lookupTableBaroHardware[] = {
    "AUTO", "NONE", "BMP085", "MS5611", "BMP280"
};
#endif
#if defined(USE_SENSOR_NAMES) || defined(MAG)
// sync with magSensor_e
const char * const lookupTableMagHardware[] = {
    "AUTO", "NONE", "HMC5883", "AK8975", "AK8963"
};
#endif

static const char * const lookupTableOffOn[] = {
    "OFF", "ON"
};

static const char * const lookupTableCrashRecovery[] = {
    "OFF", "ON" ,"BEEP"
};

static const char * const lookupTableUnit[] = {
    "IMPERIAL", "METRIC"
};

static const char * const lookupTableAlignment[] = {
    "DEFAULT",
    "CW0",
    "CW90",
    "CW180",
    "CW270",
    "CW0FLIP",
    "CW90FLIP",
    "CW180FLIP",
    "CW270FLIP"
};

#ifdef GPS
static const char * const lookupTableGPSProvider[] = {
    "NMEA", "UBLOX"
};

static const char * const lookupTableGPSSBASMode[] = {
    "AUTO", "EGNOS", "WAAS", "MSAS", "GAGAN"
};
#endif

static const char * const lookupTableCurrentSensor[] = {
    "NONE", "ADC", "VIRTUAL", "ESC"
};

static const char * const lookupTableBatterySensor[] = {
    "NONE", "ADC", "ESC"
};

#ifdef USE_SERVOS
static const char * const lookupTableGimbalMode[] = {
    "NORMAL", "MIXTILT"
};
#endif

#ifdef BLACKBOX
static const char * const lookupTableBlackboxDevice[] = {
    "NONE", "SPIFLASH", "SDCARD", "SERIAL"
};
#endif

#ifdef SERIAL_RX
static const char * const lookupTableSerialRX[] = {
    "SPEK1024",
    "SPEK2048",
    "SBUS",
    "SUMD",
    "SUMH",
    "XB-B",
    "XB-B-RJ01",
    "IBUS",
    "JETIEXBUS",
    "CRSF",
    "SRXL"
};
#endif

#ifdef USE_RX_SPI
// sync with rx_spi_protocol_e
static const char * const lookupTableRxSpi[] = {
    "V202_250K",
    "V202_1M",
    "SYMA_X",
    "SYMA_X5C",
    "CX10",
    "CX10A",
    "H8_3D",
    "INAV"
};
#endif

static const char * const lookupTableGyroLpf[] = {
    "OFF",
    "188HZ",
    "98HZ",
    "42HZ",
    "20HZ",
    "10HZ",
    "5HZ",
    "EXPERIMENTAL"
};

static const char * const lookupTableDebug[DEBUG_COUNT] = {
    "NONE",
    "CYCLETIME",
    "BATTERY",
    "GYRO",
    "ACCELEROMETER",
    "MIXER",
    "AIRMODE",
    "PIDLOOP",
    "NOTCH",
    "RC_INTERPOLATION",
    "VELOCITY",
    "DFILTER",
    "ANGLERATE",
    "ESC_SENSOR",
    "SCHEDULER",
    "STACK",
    "ESC_SENSOR_RPM",
    "ESC_SENSOR_TMP",
    "ALTITUDE"
};

#ifdef OSD
static const char * const lookupTableOsdType[] = {
    "AUTO",
    "PAL",
    "NTSC"
};
#endif

static const char * const lookupTableSuperExpoYaw[] = {
    "OFF", "ON", "ALWAYS"
};

static const char * const lookupTablePwmProtocol[] = {
    "OFF", "ONESHOT125", "ONESHOT42", "MULTISHOT", "BRUSHED",
#ifdef USE_DSHOT
    "DSHOT150", "DSHOT300", "DSHOT600", "DSHOT1200"
#endif
};

static const char * const lookupTableRcInterpolation[] = {
    "OFF", "PRESET", "AUTO", "MANUAL"
};

static const char * const lookupTableRcInterpolationChannels[] = {
    "RP", "RPY", "RPYT"
};

static const char * const lookupTableLowpassType[] = {
    "PT1", "BIQUAD", "FIR"
};

static const char * const lookupTableFailsafe[] = {
    "AUTO-LAND", "DROP"
};

const lookupTableEntry_t lookupTables[] = {
    { lookupTableOffOn, sizeof(lookupTableOffOn) / sizeof(char *) },
    { lookupTableUnit, sizeof(lookupTableUnit) / sizeof(char *) },
    { lookupTableAlignment, sizeof(lookupTableAlignment) / sizeof(char *) },
#ifdef GPS
    { lookupTableGPSProvider, sizeof(lookupTableGPSProvider) / sizeof(char *) },
    { lookupTableGPSSBASMode, sizeof(lookupTableGPSSBASMode) / sizeof(char *) },
#endif
#ifdef BLACKBOX
    { lookupTableBlackboxDevice, sizeof(lookupTableBlackboxDevice) / sizeof(char *) },
#endif
    { lookupTableCurrentSensor, sizeof(lookupTableCurrentSensor) / sizeof(char *) },
    { lookupTableBatterySensor, sizeof(lookupTableBatterySensor) / sizeof(char *) },
#ifdef USE_SERVOS
    { lookupTableGimbalMode, sizeof(lookupTableGimbalMode) / sizeof(char *) },
#endif
#ifdef SERIAL_RX
    { lookupTableSerialRX, sizeof(lookupTableSerialRX) / sizeof(char *) },
#endif
#ifdef USE_RX_SPI
    { lookupTableRxSpi, sizeof(lookupTableRxSpi) / sizeof(char *) },
#endif
    { lookupTableGyroLpf, sizeof(lookupTableGyroLpf) / sizeof(char *) },
    { lookupTableAccHardware, sizeof(lookupTableAccHardware) / sizeof(char *) },
#ifdef BARO
    { lookupTableBaroHardware, sizeof(lookupTableBaroHardware) / sizeof(char *) },
#endif
#ifdef MAG
    { lookupTableMagHardware, sizeof(lookupTableMagHardware) / sizeof(char *) },
#endif
    { lookupTableDebug, sizeof(lookupTableDebug) / sizeof(char *) },
    { lookupTableSuperExpoYaw, sizeof(lookupTableSuperExpoYaw) / sizeof(char *) },
    { lookupTablePwmProtocol, sizeof(lookupTablePwmProtocol) / sizeof(char *) },
    { lookupTableRcInterpolation, sizeof(lookupTableRcInterpolation) / sizeof(char *) },
    { lookupTableRcInterpolationChannels, sizeof(lookupTableRcInterpolationChannels) / sizeof(char *) },
    { lookupTableLowpassType, sizeof(lookupTableLowpassType) / sizeof(char *) },
    { lookupTableFailsafe, sizeof(lookupTableFailsafe) / sizeof(char *) },
    { lookupTableCrashRecovery, sizeof(lookupTableCrashRecovery) / sizeof(char *) },
#ifdef OSD
    { lookupTableOsdType, sizeof(lookupTableOsdType) / sizeof(char *) },
#endif
};

const clivalue_t valueTable[] = {
// PG_GYRO_CONFIG
    { "align_gyro",                 VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_ALIGNMENT }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_align) },
    { "gyro_lpf",                   VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_GYRO_LPF }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_lpf) },
    { "gyro_sync_denom",            VAR_UINT8  | MASTER_VALUE, .config.minmax = { 1, 32 }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_sync_denom) },
    { "gyro_lowpass_type",          VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_LOWPASS_TYPE }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_soft_lpf_type) },
    { "gyro_lowpass_hz",            VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0,  255 }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_soft_lpf_hz) },
    { "gyro_notch1_hz",             VAR_UINT16 | MASTER_VALUE, .config.minmax = { 0, 16000 }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_soft_notch_hz_1) },
    { "gyro_notch1_cutoff",         VAR_UINT16 | MASTER_VALUE, .config.minmax = { 1, 16000 }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_soft_notch_cutoff_1) },
    { "gyro_notch2_hz",             VAR_UINT16 | MASTER_VALUE, .config.minmax = { 0, 16000 }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_soft_notch_hz_2) },
    { "gyro_notch2_cutoff",         VAR_UINT16 | MASTER_VALUE, .config.minmax = { 1, 16000 }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_soft_notch_cutoff_2) },
    { "moron_threshold",            VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0,  200 }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyroMovementCalibrationThreshold) },
#if defined(GYRO_USES_SPI)
#if defined(USE_GYRO_SPI_MPU6500) || defined(USE_GYRO_SPI_MPU9250) || defined(USE_GYRO_SPI_ICM20689)
    { "gyro_use_32khz",             VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_use_32khz) },
#endif
#if defined(USE_MPU_DATA_READY_SIGNAL)
    { "gyro_isr_update",            VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_isr_update) },
#endif
#endif
#ifdef USE_DUAL_GYRO
    { "gyro_to_use",                VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 1 }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_to_use) },
#endif

// PG_ACCELEROMETER_CONFIG
    { "align_acc",                  VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_ALIGNMENT }, PG_ACCELEROMETER_CONFIG, offsetof(accelerometerConfig_t, acc_align) },
    { "acc_hardware",               VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_ACC_HARDWARE }, PG_ACCELEROMETER_CONFIG, offsetof(accelerometerConfig_t, acc_hardware) },
    { "acc_lpf_hz",                 VAR_UINT16 | MASTER_VALUE, .config.minmax = { 0, 400 }, PG_ACCELEROMETER_CONFIG, offsetof(accelerometerConfig_t, acc_lpf_hz) },
    { "acc_trim_pitch",             VAR_INT16  | MASTER_VALUE, .config.minmax = { -300, 300 }, PG_ACCELEROMETER_CONFIG, offsetof(accelerometerConfig_t, accelerometerTrims.values.pitch) },
    { "acc_trim_roll",              VAR_INT16  | MASTER_VALUE, .config.minmax = { -300, 300 }, PG_ACCELEROMETER_CONFIG, offsetof(accelerometerConfig_t, accelerometerTrims.values.roll) },

// PG_COMPASS_CONFIG
#ifdef MAG
    { "align_mag",                  VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_ALIGNMENT }, PG_COMPASS_CONFIG, offsetof(compassConfig_t, mag_align) },
    { "mag_hardware",               VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_MAG_HARDWARE }, PG_COMPASS_CONFIG, offsetof(compassConfig_t, mag_hardware) },
    { "mag_declination",            VAR_INT16  | MASTER_VALUE, .config.minmax = { -18000, 18000 }, PG_COMPASS_CONFIG, offsetof(compassConfig_t, mag_declination) },
    { "magzero_x",                  VAR_INT16  | MASTER_VALUE, .config.minmax = { INT16_MIN, INT16_MAX }, PG_COMPASS_CONFIG, offsetof(compassConfig_t, magZero.raw[X]) },
    { "magzero_y",                  VAR_INT16  | MASTER_VALUE, .config.minmax = { INT16_MIN, INT16_MAX }, PG_COMPASS_CONFIG, offsetof(compassConfig_t, magZero.raw[Y]) },
    { "magzero_z",                  VAR_INT16  | MASTER_VALUE, .config.minmax = { INT16_MIN, INT16_MAX }, PG_COMPASS_CONFIG, offsetof(compassConfig_t, magZero.raw[Z]) },
#endif

// PG_BAROMETER_CONFIG
#ifdef BARO
    { "baro_hardware",              VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_BARO_HARDWARE }, PG_BAROMETER_CONFIG, offsetof(barometerConfig_t, baro_hardware) },
    { "baro_tab_size",              VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, BARO_SAMPLE_COUNT_MAX }, PG_BAROMETER_CONFIG, offsetof(barometerConfig_t, baro_sample_count) },
    { "baro_noise_lpf",             VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, 1000 }, PG_BAROMETER_CONFIG, offsetof(barometerConfig_t, baro_noise_lpf) },
    { "baro_cf_vel",                VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, 1000 }, PG_BAROMETER_CONFIG, offsetof(barometerConfig_t, baro_cf_vel) },
    { "baro_cf_alt",                VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, 1000 }, PG_BAROMETER_CONFIG, offsetof(barometerConfig_t, baro_cf_alt) },
#endif

// PG_RX_CONFIG
    { "mid_rc",                     VAR_UINT16 | MASTER_VALUE, .config.minmax = { 1200, 1700 }, PG_RX_CONFIG, offsetof(rxConfig_t, midrc) },
    { "min_check",                  VAR_UINT16 | MASTER_VALUE, .config.minmax = { PWM_RANGE_ZERO, PWM_RANGE_MAX }, PG_RX_CONFIG, offsetof(rxConfig_t, mincheck) },
    { "max_check",                  VAR_UINT16 | MASTER_VALUE, .config.minmax = { PWM_RANGE_ZERO, PWM_RANGE_MAX }, PG_RX_CONFIG, offsetof(rxConfig_t, maxcheck) },
    { "rssi_channel",               VAR_INT8   | MASTER_VALUE, .config.minmax = { 0, MAX_SUPPORTED_RC_CHANNEL_COUNT }, PG_RX_CONFIG, offsetof(rxConfig_t, rssi_channel) },
    { "rssi_scale",                 VAR_UINT8  | MASTER_VALUE, .config.minmax = { RSSI_SCALE_MIN, RSSI_SCALE_MAX }, PG_RX_CONFIG, offsetof(rxConfig_t, rssi_scale) },
    { "rssi_invert",                VAR_INT8   | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_RX_CONFIG, offsetof(rxConfig_t, rssi_invert) },
    { "rc_interp",                  VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_RC_INTERPOLATION }, PG_RX_CONFIG, offsetof(rxConfig_t, rcInterpolation) },
    { "rc_interp_ch",               VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_RC_INTERPOLATION_CHANNELS }, PG_RX_CONFIG, offsetof(rxConfig_t, rcInterpolationChannels) },
    { "rc_interp_int",              VAR_UINT8  | MASTER_VALUE, .config.minmax = { 1, 50 }, PG_RX_CONFIG, offsetof(rxConfig_t, rcInterpolationInterval) },
    { "fpv_mix_degrees",            VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 50 }, PG_RX_CONFIG, offsetof(rxConfig_t, fpvCamAngleDegrees) },
    { "max_aux_channels",           VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, MAX_AUX_CHANNEL_COUNT }, PG_RX_CONFIG, offsetof(rxConfig_t, max_aux_channel) },
#ifdef SERIAL_RX
    { "serialrx_provider",          VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_SERIAL_RX }, PG_RX_CONFIG, offsetof(rxConfig_t, serialrx_provider) },
    { "sbus_inversion",             VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_RX_CONFIG, offsetof(rxConfig_t, sbus_inversion) },
#endif
#ifdef SPEKTRUM_BIND_PIN
    { "spektrum_sat_bind",          VAR_UINT8  | MASTER_VALUE, .config.minmax = { SPEKTRUM_SAT_BIND_DISABLED, SPEKTRUM_SAT_BIND_MAX}, PG_RX_CONFIG, offsetof(rxConfig_t, spektrum_sat_bind) },
    { "spektrum_sat_bind_autoreset",VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 1 }, PG_RX_CONFIG, offsetof(rxConfig_t, spektrum_sat_bind_autoreset) },
#endif
    { "airmode_start_throttle",     VAR_UINT16 | MASTER_VALUE, .config.minmax = { 1000, 2000 }, PG_RX_CONFIG, offsetof(rxConfig_t, airModeActivateThreshold) },
    { "rx_min_usec",                VAR_UINT16 | MASTER_VALUE, .config.minmax = { PWM_PULSE_MIN, PWM_PULSE_MAX }, PG_RX_CONFIG, offsetof(rxConfig_t, rx_min_usec) },
    { "rx_max_usec",                VAR_UINT16 | MASTER_VALUE, .config.minmax = { PWM_PULSE_MIN, PWM_PULSE_MAX }, PG_RX_CONFIG, offsetof(rxConfig_t, rx_max_usec) },
#ifdef STM32F4
    { "serialrx_halfduplex",        VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_RX_CONFIG, offsetof(rxConfig_t, halfDuplex) },
#endif

// PG_PWM_CONFIG
#if defined(USE_PWM)
    { "input_filtering_mode",       VAR_INT8   | MASTER_VALUE | MODE_LOOKUP,  .config.lookup = { TABLE_OFF_ON }, PG_PWM_CONFIG, offsetof(pwmConfig_t, inputFilteringMode) },
#endif

// PG_BLACKBOX_CONFIG
#ifdef BLACKBOX
    { "blackbox_rate_num",          VAR_UINT8  | MASTER_VALUE, .config.minmax = { 1, 32 }, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, rate_num) },
    { "blackbox_rate_denom",        VAR_UINT8  | MASTER_VALUE, .config.minmax = { 1, 32 }, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, rate_denom) },
    { "blackbox_device",            VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_BLACKBOX_DEVICE }, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, device) },
    { "blackbox_on_motor_test",     VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, on_motor_test) },
    { "blackbox_record_acc",        VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, record_acc) },
#endif

// PG_MOTOR_CONFIG
    { "min_throttle",               VAR_UINT16 | MASTER_VALUE, .config.minmax = { PWM_RANGE_ZERO, PWM_RANGE_MAX }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, minthrottle) },
    { "max_throttle",               VAR_UINT16 | MASTER_VALUE, .config.minmax = { PWM_RANGE_ZERO, PWM_RANGE_MAX }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, maxthrottle) },
    { "min_command",                VAR_UINT16 | MASTER_VALUE, .config.minmax = { PWM_RANGE_ZERO, PWM_RANGE_MAX }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, mincommand) },
#ifdef USE_DSHOT
    { "dshot_idle_value",           VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, 2000 }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, digitalIdleOffsetValue) },
#endif
    { "use_unsynced_pwm",           VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, dev.useUnsyncedPwm) },
    { "motor_pwm_protocol",         VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_MOTOR_PWM_PROTOCOL }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, dev.motorPwmProtocol) },
    { "motor_pwm_rate",             VAR_UINT16 | MASTER_VALUE, .config.minmax = { 200, 32000 }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, dev.motorPwmRate) },
    { "motor_pwm_inversion",        VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, dev.motorPwmInversion) },

// PG_THROTTLE_CORRECTION_CONFIG
    { "thr_corr_value",             VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0,  150 }, PG_THROTTLE_CORRECTION_CONFIG, offsetof(throttleCorrectionConfig_t, throttle_correction_value) },
    { "thr_corr_angle",             VAR_UINT16 | MASTER_VALUE, .config.minmax = { 1,  900 }, PG_THROTTLE_CORRECTION_CONFIG, offsetof(throttleCorrectionConfig_t, throttle_correction_angle) },

// PG_FAILSAFE_CONFIG
    { "failsafe_delay",             VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 200 }, PG_FAILSAFE_CONFIG, offsetof(failsafeConfig_t, failsafe_delay) },
    { "failsafe_off_delay",         VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 200 }, PG_FAILSAFE_CONFIG, offsetof(failsafeConfig_t, failsafe_off_delay) },
    { "failsafe_throttle",          VAR_UINT16 | MASTER_VALUE, .config.minmax = { PWM_RANGE_MIN, PWM_RANGE_MAX }, PG_FAILSAFE_CONFIG, offsetof(failsafeConfig_t, failsafe_throttle) },
    { "failsafe_kill_switch",       VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_FAILSAFE_CONFIG, offsetof(failsafeConfig_t, failsafe_kill_switch) },
    { "failsafe_throttle_low_delay",VAR_UINT16 | MASTER_VALUE, .config.minmax = { 0, 300 }, PG_FAILSAFE_CONFIG, offsetof(failsafeConfig_t, failsafe_throttle_low_delay) },
    { "failsafe_procedure",         VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_FAILSAFE }, PG_FAILSAFE_CONFIG, offsetof(failsafeConfig_t, failsafe_procedure) },

// PG_BOARDALIGNMENT_CONFIG
    { "align_board_roll",           VAR_INT16  | MASTER_VALUE, .config.minmax = { -180, 360 }, PG_BOARD_ALIGNMENT, offsetof(boardAlignment_t, rollDegrees) },
    { "align_board_pitch",          VAR_INT16  | MASTER_VALUE, .config.minmax = { -180, 360 }, PG_BOARD_ALIGNMENT, offsetof(boardAlignment_t, pitchDegrees) },
    { "align_board_yaw",            VAR_INT16  | MASTER_VALUE, .config.minmax = { -180, 360 }, PG_BOARD_ALIGNMENT, offsetof(boardAlignment_t, yawDegrees) },

// PG_GIMBAL_CONFIG
#ifdef USE_SERVOS
    { "gimbal_mode",                VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_GIMBAL_MODE }, PG_GIMBAL_CONFIG, offsetof(gimbalConfig_t, mode) },
#endif

// PG_BATTERY_CONFIG
    { "bat_capacity",               VAR_UINT16 | MASTER_VALUE, .config.minmax = { 0, 20000 }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, batteryCapacity) },
    { "vbat_max_cell_voltage",      VAR_UINT8  | MASTER_VALUE, .config.minmax = { 10, 50 }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, vbatmaxcellvoltage) },
    { "vbat_min_cell_voltage",      VAR_UINT8  | MASTER_VALUE, .config.minmax = { 10, 50 }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, vbatmincellvoltage) },
    { "vbat_warning_cell_voltage",  VAR_UINT8  | MASTER_VALUE, .config.minmax = { 10, 50 }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, vbatwarningcellvoltage) },
    { "vbat_hysteresis",            VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 250 }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, vbathysteresis) },
    { "current_meter",              VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_CURRENT_METER }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, currentMeterSource) },
    { "battery_meter",              VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_VOLTAGE_METER }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, voltageMeterSource) },
    { "vbat_detect_cell_voltage",   VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 200 }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, vbatnotpresentcellvoltage) },
    { "use_vbat_alerts",            VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, useVBatAlerts) },
    { "use_cbat_alerts",            VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, useConsumptionAlerts) },
    { "cbat_alert_percent",         VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 100 }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, consumptionWarningPercentage) },

//  PG_VOLTAGE_SENSOR_ADC_CONFIG
    { "vbat_scale",                 VAR_UINT8  | MASTER_VALUE, .config.minmax = { VBAT_SCALE_MIN, VBAT_SCALE_MAX }, PG_VOLTAGE_SENSOR_ADC_CONFIG, offsetof(voltageSensorADCConfig_t, vbatscale) },

// PG_CURRENT_SENSOR_ADC_CONFIG
    { "ibata_scale",                VAR_INT16  | MASTER_VALUE, .config.minmax = { -16000, 16000 }, PG_CURRENT_SENSOR_ADC_CONFIG, offsetof(currentSensorADCConfig_t, scale) },
    { "ibata_offset",               VAR_INT16  | MASTER_VALUE, .config.minmax = { -16000, 16000 }, PG_CURRENT_SENSOR_ADC_CONFIG, offsetof(currentSensorADCConfig_t, offset) },
// PG_CURRENT_SENSOR_ADC_CONFIG
#ifdef USE_VIRTUAL_CURRENT_METER
    { "ibatv_scale",                VAR_INT16  | MASTER_VALUE, .config.minmax = { -16000, 16000 }, PG_CURRENT_SENSOR_VIRTUAL_CONFIG, offsetof(currentSensorVirtualConfig_t, scale) },
    { "ibatv_offset",               VAR_INT16  | MASTER_VALUE, .config.minmax = { -16000, 16000 }, PG_CURRENT_SENSOR_VIRTUAL_CONFIG, offsetof(currentSensorVirtualConfig_t, offset) },
#endif

// PG_BEEPER_DEV_CONFIG
#ifdef BEEPER
    { "beeper_inversion",           VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_BEEPER_DEV_CONFIG, offsetof(beeperDevConfig_t, isInverted) },
    { "beeper_od",                  VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_BEEPER_DEV_CONFIG, offsetof(beeperDevConfig_t, isOpenDrain) },
    { "beeper_frequency",           VAR_INT16  | MASTER_VALUE, .config.minmax = { 0, 16000 }, PG_BEEPER_DEV_CONFIG, offsetof(beeperDevConfig_t, frequency) },
#endif

// PG_MIXER_CONFIG
    { "yaw_motors_reversed",        VAR_INT8   | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_MIXER_CONFIG, offsetof(mixerConfig_t, yaw_motors_reversed) },

// PG_MOTOR_3D_CONFIG
    { "3d_deadband_low",            VAR_UINT16 | MASTER_VALUE, .config.minmax = { PWM_RANGE_ZERO, PWM_RANGE_MAX }, PG_MOTOR_3D_CONFIG, offsetof(flight3DConfig_t, deadband3d_low) }, // FIXME upper limit should match code in the mixer, 1500 currently
    { "3d_deadband_high",           VAR_UINT16 | MASTER_VALUE, .config.minmax = { PWM_RANGE_ZERO, PWM_RANGE_MAX }, PG_MOTOR_3D_CONFIG, offsetof(flight3DConfig_t, deadband3d_high) }, // FIXME lower limit should match code in the mixer, 1500 currently,
    { "3d_neutral",                 VAR_UINT16 | MASTER_VALUE, .config.minmax = { PWM_RANGE_ZERO, PWM_RANGE_MAX }, PG_MOTOR_3D_CONFIG, offsetof(flight3DConfig_t, neutral3d) },
    { "3d_deadband_throttle",       VAR_UINT16 | MASTER_VALUE, .config.minmax = { PWM_RANGE_ZERO, PWM_RANGE_MAX }, PG_MOTOR_3D_CONFIG, offsetof(flight3DConfig_t, deadband3d_throttle) },

// PG_SERVO_CONFIG
#ifdef USE_SERVOS
    { "servo_center_pulse",         VAR_UINT16 | MASTER_VALUE, .config.minmax = { PWM_RANGE_ZERO, PWM_RANGE_MAX }, PG_SERVO_CONFIG, offsetof(servoConfig_t, dev.servoCenterPulse) },
    { "servo_pwm_rate",             VAR_UINT16 | MASTER_VALUE, .config.minmax = { 50, 498 }, PG_SERVO_CONFIG, offsetof(servoConfig_t, dev.servoPwmRate) },
    { "servo_lowpass_hz",           VAR_UINT16 | MASTER_VALUE, .config.minmax = { 0, 400}, PG_SERVO_CONFIG, offsetof(servoConfig_t, servo_lowpass_freq) },
    { "tri_unarmed_servo",          VAR_INT8   | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_SERVO_CONFIG, offsetof(servoConfig_t, tri_unarmed_servo) },
    { "channel_forwarding_start",   VAR_UINT8  | MASTER_VALUE, .config.minmax = { AUX1, MAX_SUPPORTED_RC_CHANNEL_COUNT }, PG_SERVO_CONFIG, offsetof(servoConfig_t, channelForwardingStartChannel) },
#endif

// PG_CONTROLRATE_PROFILES
    { "rc_rate",                    VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmax = { 0, 255 }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rcRate8) },
    { "rc_rate_yaw",                VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmax = { 0, 255 }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rcYawRate8) },
    { "rc_expo",                    VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmax = { 0, 100 }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rcExpo8) },
    { "rc_expo_yaw",                VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmax = { 0, 100 }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rcYawExpo8) },
    { "thr_mid",                    VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmax = { 0, 100 }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, thrMid8) },
    { "thr_expo",                   VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmax = { 0, 100 }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, thrExpo8) },
    { "roll_srate",                 VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmax = { 0, CONTROL_RATE_CONFIG_ROLL_PITCH_RATE_MAX }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rates[FD_ROLL]) },
    { "pitch_srate",                VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmax = { 0, CONTROL_RATE_CONFIG_ROLL_PITCH_RATE_MAX }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rates[FD_PITCH]) },
    { "yaw_srate",                  VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmax = { 0, CONTROL_RATE_CONFIG_YAW_RATE_MAX }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rates[FD_YAW]) },
    { "tpa_rate",                   VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmax = { 0, CONTROL_RATE_CONFIG_TPA_MAX}, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, dynThrPID) },
    { "tpa_breakpoint",             VAR_UINT16 | PROFILE_RATE_VALUE, .config.minmax = { PWM_RANGE_MIN,  PWM_RANGE_MAX}, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, tpa_breakpoint) },

// PG_SERIAL_CONFIG
    { "reboot_character",           VAR_UINT8  | MASTER_VALUE, .config.minmax = { 48, 126 }, PG_SERIAL_CONFIG, offsetof(serialConfig_t, reboot_character) },
    { "serial_update_rate_hz",      VAR_UINT16 | MASTER_VALUE, .config.minmax = { 100, 2000 }, PG_SERIAL_CONFIG, offsetof(serialConfig_t, serial_update_rate_hz) },

// PG_IMU_CONFIG
    { "accxy_deadband",             VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 100 }, PG_IMU_CONFIG, offsetof(imuConfig_t, accDeadband.xy) },
    { "accz_deadband",              VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 100 }, PG_IMU_CONFIG, offsetof(imuConfig_t, accDeadband.z) },
    { "acc_unarmedcal",             VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_IMU_CONFIG, offsetof(imuConfig_t, acc_unarmedcal) },
    { "imu_dcm_kp",                 VAR_UINT16 | MASTER_VALUE, .config.minmax = { 0, 32000 }, PG_IMU_CONFIG, offsetof(imuConfig_t, dcm_kp) },
    { "imu_dcm_ki",                 VAR_UINT16 | MASTER_VALUE, .config.minmax = { 0, 32000 }, PG_IMU_CONFIG, offsetof(imuConfig_t, dcm_ki) },
    { "small_angle",                VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 180 }, PG_IMU_CONFIG, offsetof(imuConfig_t, small_angle) },

// PG_ARMING_CONFIG
    { "auto_disarm_delay",          VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 60 }, PG_ARMING_CONFIG, offsetof(armingConfig_t, auto_disarm_delay) },
    { "disarm_kill_switch",         VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_ARMING_CONFIG, offsetof(armingConfig_t, disarm_kill_switch) },
    { "gyro_cal_on_first_arm",      VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_ARMING_CONFIG, offsetof(armingConfig_t, gyro_cal_on_first_arm) },


// PG_GPS_CONFIG
#ifdef GPS
    { "gps_provider",               VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_GPS_PROVIDER }, PG_GPS_CONFIG, offsetof(gpsConfig_t, provider) },
    { "gps_sbas_mode",              VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_GPS_SBAS_MODE }, PG_GPS_CONFIG, offsetof(gpsConfig_t, sbasMode) },
    { "gps_auto_config",            VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_GPS_CONFIG, offsetof(gpsConfig_t, autoConfig) },
    { "gps_auto_baud",              VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_GPS_CONFIG, offsetof(gpsConfig_t, autoBaud) },
#endif

// PG_NAVIGATION_CONFIG
#ifdef GPS
    { "gps_wp_radius",              VAR_UINT16 | MASTER_VALUE, .config.minmax = { 0, 2000 }, PG_NAVIGATION_CONFIG, offsetof(navigationConfig_t, gps_wp_radius) },
    { "nav_controls_heading",       VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_NAVIGATION_CONFIG, offsetof(navigationConfig_t, nav_controls_heading) },
    { "nav_speed_min",              VAR_UINT16 | MASTER_VALUE, .config.minmax = { 10, 2000 }, PG_NAVIGATION_CONFIG, offsetof(navigationConfig_t, nav_speed_min) },
    { "nav_speed_max",              VAR_UINT16 | MASTER_VALUE, .config.minmax = { 10, 2000 }, PG_NAVIGATION_CONFIG, offsetof(navigationConfig_t, nav_speed_max) },
    { "nav_slew_rate",              VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 100 }, PG_NAVIGATION_CONFIG, offsetof(navigationConfig_t, nav_slew_rate) },
#endif

// PG_AIRPLANE_CONFIG
#if defined(BARO) || defined(SONAR)
    { "fixedwing_althold_reversed", VAR_INT8   | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_AIRPLANE_CONFIG, offsetof(airplaneConfig_t, fixedwing_althold_reversed) },
#endif

// PG_RC_CONTROLS_CONFIG
    { "alt_hold_deadband",          VAR_UINT8  | MASTER_VALUE, .config.minmax = { 1, 250 }, PG_RC_CONTROLS_CONFIG, offsetof(rcControlsConfig_t, alt_hold_deadband) },
    { "alt_hold_fast_change",       VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_RC_CONTROLS_CONFIG, offsetof(rcControlsConfig_t, alt_hold_fast_change) },
    { "deadband",                   VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 32 }, PG_RC_CONTROLS_CONFIG, offsetof(rcControlsConfig_t, deadband) },
    { "yaw_deadband",               VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 100 }, PG_RC_CONTROLS_CONFIG, offsetof(rcControlsConfig_t, yaw_deadband) },
    { "yaw_control_reversed",       VAR_INT8   | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_RC_CONTROLS_CONFIG, offsetof(rcControlsConfig_t, yaw_control_reversed) },

// PG_PID_CONFIG
    { "pid_process_denom",          VAR_UINT8  | MASTER_VALUE,  .config.minmax = { 1, MAX_PID_PROCESS_DENOM }, PG_PID_CONFIG, offsetof(pidConfig_t, pid_process_denom) },

// PG_PID_PROFILE
    { "dterm_lowpass_type",         VAR_UINT8  | PROFILE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_LOWPASS_TYPE }, PG_PID_PROFILE, offsetof(pidProfile_t, dterm_filter_type) },
    { "dterm_lowpass",              VAR_INT16  | PROFILE_VALUE, .config.minmax = { 0, 16000 }, PG_PID_PROFILE, offsetof(pidProfile_t, dterm_lpf_hz) },
    { "dterm_notch_hz",             VAR_UINT16 | PROFILE_VALUE, .config.minmax = { 0, 16000 }, PG_PID_PROFILE, offsetof(pidProfile_t, dterm_notch_hz) },
    { "dterm_notch_cutoff",         VAR_UINT16 | PROFILE_VALUE, .config.minmax = { 1, 16000 }, PG_PID_PROFILE, offsetof(pidProfile_t, dterm_notch_cutoff) },
    { "vbat_pid_gain",              VAR_UINT8  | PROFILE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_PID_PROFILE, offsetof(pidProfile_t, vbatPidCompensation) },
    { "pid_at_min_throttle",        VAR_UINT8  | PROFILE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_PID_PROFILE, offsetof(pidProfile_t, pidAtMinThrottle) },
    { "anti_gravity_threshold",     VAR_UINT16 | PROFILE_VALUE, .config.minmax = { 20, 1000 }, PG_PID_PROFILE, offsetof(pidProfile_t, itermThrottleThreshold) },
    { "anti_gravity_gain",          VAR_UINT16 | PROFILE_VALUE, .config.minmax = { 1, 30000 }, PG_PID_PROFILE, offsetof(pidProfile_t, itermAcceleratorGain) },
    { "setpoint_relax_ratio",       VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 100 }, PG_PID_PROFILE, offsetof(pidProfile_t, setpointRelaxRatio) },
    { "dterm_setpoint_weight",      VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 254 }, PG_PID_PROFILE, offsetof(pidProfile_t, dtermSetpointWeight) },
    { "acc_limit_yaw",              VAR_UINT16 | PROFILE_VALUE, .config.minmax = { 1, 500 }, PG_PID_PROFILE, offsetof(pidProfile_t, yawRateAccelLimit) },
    { "acc_limit",                  VAR_UINT16 | PROFILE_VALUE, .config.minmax = { 1, 500 }, PG_PID_PROFILE, offsetof(pidProfile_t, rateAccelLimit) },
    { "crash_dthreshold",           VAR_UINT16 | PROFILE_VALUE, .config.minmax = { 0, 2000 }, PG_PID_PROFILE, offsetof(pidProfile_t, crash_dthreshold) },
    { "crash_gthreshold",           VAR_UINT16 | PROFILE_VALUE, .config.minmax = { 0, 2000 }, PG_PID_PROFILE, offsetof(pidProfile_t, crash_gthreshold) },
    { "crash_time",                 VAR_UINT16 | PROFILE_VALUE, .config.minmax = { 0, 5000 }, PG_PID_PROFILE, offsetof(pidProfile_t, crash_time) },
    { "crash_recovery_angle",       VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 30 }, PG_PID_PROFILE, offsetof(pidProfile_t, crash_recovery_angle) },
    { "crash_recovery_rate",        VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 255 }, PG_PID_PROFILE, offsetof(pidProfile_t, crash_recovery_rate) },
    { "crash_recovery",             VAR_UINT8  | PROFILE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_CRASH_RECOVERY }, PG_PID_PROFILE, offsetof(pidProfile_t, crash_recovery) },

    { "iterm_windup",               VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 30, 100 }, PG_PID_PROFILE, offsetof(pidProfile_t, itermWindupPointPercent) },
    { "yaw_lowpass",                VAR_UINT16 | PROFILE_VALUE, .config.minmax = { 0, 500 }, PG_PID_PROFILE, offsetof(pidProfile_t, yaw_lpf_hz) },
    { "pidsum_limit",               VAR_UINT16 | PROFILE_VALUE, .config.minmax = { 100, 1000 }, PG_PID_PROFILE, offsetof(pidProfile_t, pidSumLimit) },
    { "pidsum_limit_yaw",           VAR_UINT16 | PROFILE_VALUE, .config.minmax = { 100, 1000 }, PG_PID_PROFILE, offsetof(pidProfile_t, pidSumLimitYaw) },

    { "p_pitch",                    VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_PITCH].P) },
    { "i_pitch",                    VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_PITCH].I) },
    { "d_pitch",                    VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_PITCH].D) },
    { "p_roll",                     VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_ROLL].P) },
    { "i_roll",                     VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_ROLL].I) },
    { "d_roll",                     VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_ROLL].D) },
    { "p_yaw",                      VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_YAW].P) },
    { "i_yaw",                      VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_YAW].I) },
    { "d_yaw",                      VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_YAW].D) },

    { "p_alt",                      VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_ALT].P) },
    { "i_alt",                      VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_ALT].I) },
    { "d_alt",                      VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_ALT].D) },

    { "p_level",                    VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_LEVEL].P) },
    { "i_level",                    VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_LEVEL].I) },
    { "d_level",                    VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_LEVEL].D) },

    { "p_vel",                      VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_VEL].P) },
    { "i_vel",                      VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_VEL].I) },
    { "d_vel",                      VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_VEL].D) },

    { "level_sensitivity",          VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 10, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, levelSensitivity) },
    { "level_limit",                VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 10, 120 }, PG_PID_PROFILE, offsetof(pidProfile_t, levelAngleLimit) },

    { "horizon_tilt_effect",        VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0,  250 }, PG_PID_PROFILE, offsetof(pidProfile_t, horizon_tilt_effect) },
    { "horizon_tilt_expert_mode",   VAR_UINT8  | PROFILE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_PID_PROFILE, offsetof(pidProfile_t, horizon_tilt_expert_mode) },
#ifdef GPS
    { "gps_pos_p",                  VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_POS].P) },
    { "gps_pos_i",                  VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_POS].I) },
    { "gps_pos_d",                  VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_POS].D) },
    { "gps_posr_p",                 VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_POSR].P) },
    { "gps_posr_i",                 VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_POSR].I) },
    { "gps_posr_d",                 VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_POSR].D) },
    { "gps_nav_p",                  VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_NAVR].P) },
    { "gps_nav_i",                  VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_NAVR].I) },
    { "gps_nav_d",                  VAR_UINT8  | PROFILE_VALUE, .config.minmax = { 0, 200 }, PG_PID_PROFILE, offsetof(pidProfile_t, pid[PID_NAVR].D) },
#endif

// PG_TELEMETRY_CONFIG
#ifdef TELEMETRY
    { "tlm_switch",                 VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, telemetry_switch) },
    { "tlm_inversion",              VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, telemetry_inversion) },
    { "tlm_halfduplex",             VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, halfDuplex) },
    { "frsky_default_lat",          VAR_INT16  | MASTER_VALUE, .config.minmax = { -9000, 9000 }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, gpsNoFixLatitude) },
    { "frsky_default_long",         VAR_INT16  | MASTER_VALUE, .config.minmax = { -18000, 18000 }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, gpsNoFixLongitude) },
    { "frsky_gps_format",           VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, FRSKY_FORMAT_NMEA }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, frsky_coordinate_format) },
    { "frsky_unit",                 VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_UNIT }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, frsky_unit) },
    { "frsky_vfas_precision",       VAR_UINT8  | MASTER_VALUE, .config.minmax = { FRSKY_VFAS_PRECISION_LOW,  FRSKY_VFAS_PRECISION_HIGH }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, frsky_vfas_precision) },
    { "frsky_vfas_cell_voltage",    VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, frsky_vfas_cell_voltage) },
    { "hott_alarm_int",             VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 120 }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, hottAlarmSoundInterval) },
    { "pid_in_tlm",                 VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = {TABLE_OFF_ON }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, pidValuesAsTelemetry) },
#if defined(TELEMETRY_IBUS)
    { "ibus_report_cell_voltage",   VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, report_cell_voltage) },
#endif
#endif

// PG_LED_STRIP_CONFIG
#ifdef LED_STRIP
    { "ledstrip_visual_beeper",     VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_LED_STRIP_CONFIG, offsetof(ledStripConfig_t, ledstrip_visual_beeper) },
#endif

// PG_SDCARD_CONFIG
#ifdef USE_SDCARD
    { "sdcard_dma",                 VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_SDCARD_CONFIG, offsetof(sdcardConfig_t, useDma) },
#endif

// PG_OSD_CONFIG
#ifdef OSD
    { "osd_units",                  VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_UNIT }, PG_OSD_CONFIG, offsetof(osdConfig_t, units) },

    { "osd_rssi_alarm",             VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 100 }, PG_OSD_CONFIG, offsetof(osdConfig_t, rssi_alarm) },
    { "osd_cap_alarm",              VAR_UINT16 | MASTER_VALUE, .config.minmax = { 0, 20000 }, PG_OSD_CONFIG, offsetof(osdConfig_t, cap_alarm) },
    { "osd_time_alarm",             VAR_UINT16 | MASTER_VALUE, .config.minmax = { 0, 60 }, PG_OSD_CONFIG, offsetof(osdConfig_t, time_alarm) },
    { "osd_alt_alarm",              VAR_UINT16 | MASTER_VALUE, .config.minmax = { 0, 10000 }, PG_OSD_CONFIG, offsetof(osdConfig_t, alt_alarm) },

    { "osd_vbat_pos",               VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_MAIN_BATT_VOLTAGE]) },
    { "osd_rssi_pos",               VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_RSSI_VALUE]) },
    { "osd_flytimer_pos",           VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_FLYTIME]) },
    { "osd_ontimer_pos",            VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_ONTIME]) },
    { "osd_flymode_pos",            VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_FLYMODE]) },
    { "osd_throttle_pos",           VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_THROTTLE_POS]) },
    { "osd_vtx_channel_pos",        VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_VTX_CHANNEL]) },
    { "osd_crosshairs",             VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_CROSSHAIRS]) },
    { "osd_horizon_pos",            VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_ARTIFICIAL_HORIZON]) },
    { "osd_current_pos",            VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_CURRENT_DRAW]) },
    { "osd_mah_drawn_pos",          VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_MAH_DRAWN]) },
    { "osd_craft_name_pos",         VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_CRAFT_NAME]) },
    { "osd_gps_speed_pos",          VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_GPS_SPEED]) },
    { "osd_gps_lon",                VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_GPS_LON]) },
    { "osd_gps_lat",                VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_GPS_LAT]) },
    { "osd_gps_sats_pos",           VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_GPS_SATS]) },
    { "osd_altitude_pos",           VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_ALTITUDE]) },
    { "osd_pid_roll_pos",           VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_ROLL_PIDS]) },
    { "osd_pid_pitch_pos",          VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_PITCH_PIDS]) },
    { "osd_pid_yaw_pos",            VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_YAW_PIDS]) },
    { "osd_debug_pos",              VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_DEBUG]) },
    { "osd_power_pos",              VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_POWER]) },
    { "osd_pidrate_profile_pos",    VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_PIDRATE_PROFILE]) },
    { "osd_battery_warning_pos",    VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_MAIN_BATT_WARNING]) },
    { "osd_avg_cell_voltage_pos",   VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_AVG_CELL_VOLTAGE]) },
    { "osd_pit_ang_pos",            VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_PITCH_ANGLE]) },
    { "osd_rol_ang_pos",            VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_ROLL_ANGLE]) },
    { "osd_battery_usage_pos",      VAR_UINT16  | MASTER_VALUE, .config.minmax = { 0, OSD_POSCFG_MAX }, PG_OSD_CONFIG, offsetof(osdConfig_t, item_pos[OSD_MAIN_BATT_USAGE]) },

#endif

// PG_SYSTEM_CONFIG
#ifndef SKIP_TASK_STATISTICS
    { "task_statistics",            VAR_INT8   | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_SYSTEM_CONFIG, offsetof(systemConfig_t, task_statistics) },
#endif
    { "debug_mode",                 VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_DEBUG }, PG_SYSTEM_CONFIG, offsetof(systemConfig_t, debug_mode) },

// PG_VTX_CONFIG
#ifdef VTX_RTC6705
    { "vtx_band",                   VAR_UINT8  | MASTER_VALUE, .config.minmax = { 1, 5 }, PG_VTX_RTC6705_CONFIG, offsetof(vtxRTC6705Config_t, band) },
    { "vtx_channel",                VAR_UINT8  | MASTER_VALUE, .config.minmax = { 1, 8 }, PG_VTX_RTC6705_CONFIG, offsetof(vtxRTC6705Config_t, channel) },
    { "vtx_power",                  VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, RTC6705_POWER_COUNT - 1 }, PG_VTX_RTC6705_CONFIG, offsetof(vtxRTC6705Config_t, power) },
#endif

// PG_VCD_CONFIG
#ifdef USE_MAX7456
    { "vcd_video_system",           VAR_UINT8   | MASTER_VALUE, .config.minmax = { 0, 2 }, PG_VCD_CONFIG, offsetof(vcdProfile_t, video_system) },
    { "vcd_h_offset",               VAR_INT8    | MASTER_VALUE, .config.minmax = { -32, 31 }, PG_VCD_CONFIG, offsetof(vcdProfile_t, h_offset) },
    { "vcd_v_offset",               VAR_INT8    | MASTER_VALUE, .config.minmax = { -15, 16 }, PG_VCD_CONFIG, offsetof(vcdProfile_t, v_offset) },
#endif

// PG_DISPLAY_PORT_MSP_CONFIG
#ifdef USE_MSP_DISPLAYPORT
    { "displayport_msp_col_adjust", VAR_INT8    | MASTER_VALUE, .config.minmax = { -6, 0 }, PG_DISPLAY_PORT_MSP_CONFIG, offsetof(displayPortProfile_t, colAdjust) },
    { "displayport_msp_row_adjust", VAR_INT8    | MASTER_VALUE, .config.minmax = { -3, 0 }, PG_DISPLAY_PORT_MSP_CONFIG, offsetof(displayPortProfile_t, rowAdjust) },
#endif

// PG_DISPLAY_PORT_MSP_CONFIG
#ifdef USE_MAX7456
    { "displayport_max7456_col_adjust", VAR_INT8| MASTER_VALUE, .config.minmax = { -6, 0 }, PG_DISPLAY_PORT_MSP_CONFIG, offsetof(displayPortProfile_t, colAdjust) },
    { "displayport_max7456_row_adjust", VAR_INT8| MASTER_VALUE, .config.minmax = { -3, 0 }, PG_DISPLAY_PORT_MAX7456_CONFIG, offsetof(displayPortProfile_t, rowAdjust) },
#endif
};

const uint16_t valueTableEntryCount = ARRAYLEN(valueTable);

void settingsBuildCheck() {
    BUILD_BUG_ON(LOOKUP_TABLE_COUNT != ARRAYLEN(lookupTables));
}
