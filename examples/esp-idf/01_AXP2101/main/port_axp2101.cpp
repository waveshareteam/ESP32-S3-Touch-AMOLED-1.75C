#include <stdint.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "axp2101_registers.h"

static const char *TAG = "AXP2101";

extern int pmu_register_read(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len);
extern int pmu_register_write_byte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len);

static uint8_t status_registers[3];

static esp_err_t axp2101_read_reg(uint8_t reg, uint8_t *value)
{
    return pmu_register_read(AXP2101_I2C_ADDRESS, reg, value, 1) == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t axp2101_write_reg(uint8_t reg, uint8_t value)
{
    return pmu_register_write_byte(AXP2101_I2C_ADDRESS, reg, &value, 1) == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t axp2101_update_reg(uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t current = 0;
    esp_err_t ret = axp2101_read_reg(reg, &current);
    if (ret != ESP_OK) {
        return ret;
    }
    current = (current & ~mask) | (value & mask);
    return axp2101_write_reg(reg, current);
}

static bool axp2101_get_bit(uint8_t reg, uint8_t bit)
{
    uint8_t value = 0;
    return axp2101_read_reg(reg, &value) == ESP_OK && ((value >> bit) & 0x01);
}

static esp_err_t axp2101_set_bit(uint8_t reg, uint8_t bit, bool enable)
{
    const uint8_t mask = 1U << bit;
    return axp2101_update_reg(reg, mask, enable ? mask : 0);
}

static uint16_t axp2101_read_h6l8(uint8_t high_reg, uint8_t low_reg)
{
    uint8_t high = 0;
    uint8_t low = 0;
    if (axp2101_read_reg(high_reg, &high) != ESP_OK || axp2101_read_reg(low_reg, &low) != ESP_OK) {
        return 0;
    }
    return ((high & 0x3F) << 8) | low;
}

static uint16_t axp2101_read_h5l8(uint8_t high_reg, uint8_t low_reg)
{
    uint8_t high = 0;
    uint8_t low = 0;
    if (axp2101_read_reg(high_reg, &high) != ESP_OK || axp2101_read_reg(low_reg, &low) != ESP_OK) {
        return 0;
    }
    return ((high & 0x1F) << 8) | low;
}

static esp_err_t axp2101_set_linear_voltage(uint8_t reg, uint16_t min_mv, uint16_t max_mv,
                                            uint16_t step_mv, uint16_t millivolt)
{
    if (millivolt < min_mv || millivolt > max_mv || (millivolt % step_mv) != 0) {
        ESP_LOGE(TAG, "Invalid voltage %u mV for register 0x%02x", millivolt, reg);
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t current = 0;
    esp_err_t ret = axp2101_read_reg(reg, &current);
    if (ret != ESP_OK) {
        return ret;
    }
    const uint8_t code = (millivolt - min_mv) / step_mv;
    return axp2101_write_reg(reg, (current & 0xE0) | code);
}

static uint16_t axp2101_get_linear_voltage(uint8_t reg, uint16_t min_mv, uint16_t step_mv)
{
    uint8_t value = 0;
    if (axp2101_read_reg(reg, &value) != ESP_OK) {
        return 0;
    }
    return (value & 0x1F) * step_mv + min_mv;
}

static uint16_t axp2101_get_dc2_voltage(uint8_t reg)
{
    uint8_t value = 0;
    if (axp2101_read_reg(reg, &value) != ESP_OK) {
        return 0;
    }
    const uint8_t code = value & 0x7F;
    if (code < AXP2101_DCDC2_4_RANGE2_BASE) {
        return code * AXP2101_DCDC2_4_RANGE1_STEP_MV + AXP2101_DCDC2_4_RANGE1_MIN_MV;
    }
    return code * AXP2101_DCDC2_4_RANGE2_STEP_MV - 200;
}

static uint16_t axp2101_get_dc3_voltage(void)
{
    uint8_t value = 0;
    if (axp2101_read_reg(AXP2101_REG_DC_VOL2_CTRL, &value) != ESP_OK) {
        return 0;
    }
    const uint8_t code = value & 0x7F;
    if (code < AXP2101_DCDC2_4_RANGE2_BASE) {
        return code * AXP2101_DCDC2_4_RANGE1_STEP_MV + AXP2101_DCDC2_4_RANGE1_MIN_MV;
    }
    if (code < AXP2101_DCDC3_RANGE3_BASE) {
        return code * AXP2101_DCDC2_4_RANGE2_STEP_MV - 200;
    }
    return code * AXP2101_DCDC3_RANGE3_STEP_MV - 7200;
}

static uint16_t axp2101_get_dc5_voltage(void)
{
    uint8_t value = 0;
    if (axp2101_read_reg(AXP2101_REG_DC_VOL4_CTRL, &value) != ESP_OK) {
        return 0;
    }
    const uint8_t code = value & 0x1F;
    if (code == AXP2101_DCDC5_1200MV_CODE) {
        return 1200;
    }
    return code * AXP2101_DCDC5_STEP_MV + AXP2101_DCDC5_MIN_MV;
}

static bool axp2101_is_vbus_good(void)
{
    return axp2101_get_bit(AXP2101_REG_STATUS1, 5);
}

static bool axp2101_is_battery_connected(void)
{
    return axp2101_get_bit(AXP2101_REG_STATUS1, 3);
}

static bool axp2101_is_charging(void)
{
    uint8_t value = 0;
    return axp2101_read_reg(AXP2101_REG_STATUS2, &value) == ESP_OK && ((value >> 5) == 0x01);
}

static bool axp2101_is_discharge(void)
{
    uint8_t value = 0;
    return axp2101_read_reg(AXP2101_REG_STATUS2, &value) == ESP_OK && ((value >> 5) == 0x02);
}

static bool axp2101_is_standby(void)
{
    uint8_t value = 0;
    return axp2101_read_reg(AXP2101_REG_STATUS2, &value) == ESP_OK && ((value >> 5) == 0x00);
}

static bool axp2101_is_vbus_in(void)
{
    return !axp2101_get_bit(AXP2101_REG_STATUS2, 3) && axp2101_is_vbus_good();
}

static axp2101_charge_state_t axp2101_get_charger_status(void)
{
    uint8_t value = 0;
    if (axp2101_read_reg(AXP2101_REG_STATUS2, &value) != ESP_OK) {
        return AXP2101_CHG_STOP_STATE;
    }
    return (axp2101_charge_state_t)(value & 0x07);
}

static const char *axp2101_charge_status_name(axp2101_charge_state_t status)
{
    switch (status) {
    case AXP2101_CHG_TRI_STATE:
        return "tri_charge";
    case AXP2101_CHG_PRE_STATE:
        return "pre_charge";
    case AXP2101_CHG_CC_STATE:
        return "constant charge";
    case AXP2101_CHG_CV_STATE:
        return "constant voltage";
    case AXP2101_CHG_DONE_STATE:
        return "charge done";
    case AXP2101_CHG_STOP_STATE:
    default:
        return "not charge";
    }
}

static esp_err_t axp2101_clear_irq_status(void)
{
    esp_err_t ret = ESP_OK;
    for (uint8_t i = 0; i < 3; i++) {
        ret |= axp2101_write_reg(AXP2101_REG_INTSTS1 + i, 0xFF);
        status_registers[i] = 0;
    }
    return ret;
}

static esp_err_t axp2101_get_irq_status(void)
{
    esp_err_t ret = ESP_OK;
    ret |= axp2101_read_reg(AXP2101_REG_INTSTS1, &status_registers[0]);
    ret |= axp2101_read_reg(AXP2101_REG_INTSTS2, &status_registers[1]);
    ret |= axp2101_read_reg(AXP2101_REG_INTSTS3, &status_registers[2]);
    return ret;
}

static esp_err_t axp2101_set_irq_mask(uint32_t mask)
{
    esp_err_t ret = ESP_OK;
    ret |= axp2101_write_reg(AXP2101_REG_INTEN1, mask & 0xFF);
    ret |= axp2101_write_reg(AXP2101_REG_INTEN2, (mask >> 8) & 0xFF);
    ret |= axp2101_write_reg(AXP2101_REG_INTEN3, (mask >> 16) & 0xFF);
    return ret;
}

static uint16_t axp2101_get_battery_voltage(void)
{
    if (!axp2101_is_battery_connected()) {
        return 0;
    }
    return axp2101_read_h5l8(AXP2101_REG_ADC_DATA_RESULT0, AXP2101_REG_ADC_DATA_RESULT1);
}

static uint16_t axp2101_get_vbus_voltage(void)
{
    if (!axp2101_is_vbus_in()) {
        return 0;
    }
    return axp2101_read_h6l8(AXP2101_REG_ADC_DATA_RESULT4, AXP2101_REG_ADC_DATA_RESULT5);
}

static uint16_t axp2101_get_system_voltage(void)
{
    return axp2101_read_h6l8(AXP2101_REG_ADC_DATA_RESULT6, AXP2101_REG_ADC_DATA_RESULT7);
}

static float axp2101_get_temperature(void)
{
    const uint16_t raw = axp2101_read_h6l8(AXP2101_REG_ADC_DATA_RESULT8, AXP2101_REG_ADC_DATA_RESULT9);
    return 22.0f + (7274.0f - raw) / 20.0f;
}

static int axp2101_get_battery_percent(void)
{
    if (!axp2101_is_battery_connected()) {
        return -1;
    }
    uint8_t value = 0;
    return axp2101_read_reg(AXP2101_REG_BAT_PERCENT_DATA, &value) == ESP_OK ? value : -1;
}

static esp_err_t axp2101_configure_outputs(void)
{
    esp_err_t ret = ESP_OK;

    ret |= axp2101_set_bit(AXP2101_REG_DC_ONOFF_DVM_CTRL, 1, false);
    ret |= axp2101_set_bit(AXP2101_REG_DC_ONOFF_DVM_CTRL, 2, false);
    ret |= axp2101_set_bit(AXP2101_REG_DC_ONOFF_DVM_CTRL, 3, false);
    ret |= axp2101_set_bit(AXP2101_REG_DC_ONOFF_DVM_CTRL, 4, false);
    ret |= axp2101_update_reg(AXP2101_REG_LDO_ONOFF_CTRL0, 0xFF, 0);
    ret |= axp2101_set_bit(AXP2101_REG_LDO_ONOFF_CTRL1, 0, false);

    ret |= axp2101_set_linear_voltage(AXP2101_REG_DC_VOL0_CTRL, AXP2101_DCDC1_MIN_MV,
                                      AXP2101_DCDC1_MAX_MV, AXP2101_DCDC1_STEP_MV, 3300);
    ret |= axp2101_set_bit(AXP2101_REG_DC_ONOFF_DVM_CTRL, 0, true);
    ret |= axp2101_set_linear_voltage(AXP2101_REG_LDO_VOL0_CTRL, AXP2101_LDO_MIN_MV,
                                      3500, AXP2101_LDO_STEP_MV, 3300);
    ret |= axp2101_set_bit(AXP2101_REG_LDO_ONOFF_CTRL0, 0, true);

    return ret;
}

static void axp2101_log_outputs(void)
{
    ESP_LOGI(TAG, "DCDC=======================================================================");
    ESP_LOGI(TAG, "DC1  : %s   Voltage:%u mV", axp2101_get_bit(AXP2101_REG_DC_ONOFF_DVM_CTRL, 0) ? "+" : "-",
             axp2101_get_linear_voltage(AXP2101_REG_DC_VOL0_CTRL, AXP2101_DCDC1_MIN_MV, AXP2101_DCDC1_STEP_MV));
    ESP_LOGI(TAG, "DC2  : %s   Voltage:%u mV", axp2101_get_bit(AXP2101_REG_DC_ONOFF_DVM_CTRL, 1) ? "+" : "-",
             axp2101_get_dc2_voltage(AXP2101_REG_DC_VOL1_CTRL));
    ESP_LOGI(TAG, "DC3  : %s   Voltage:%u mV", axp2101_get_bit(AXP2101_REG_DC_ONOFF_DVM_CTRL, 2) ? "+" : "-",
             axp2101_get_dc3_voltage());
    ESP_LOGI(TAG, "DC4  : %s   Voltage:%u mV", axp2101_get_bit(AXP2101_REG_DC_ONOFF_DVM_CTRL, 3) ? "+" : "-",
             axp2101_get_dc2_voltage(AXP2101_REG_DC_VOL3_CTRL));
    ESP_LOGI(TAG, "DC5  : %s   Voltage:%u mV", axp2101_get_bit(AXP2101_REG_DC_ONOFF_DVM_CTRL, 4) ? "+" : "-",
             axp2101_get_dc5_voltage());
    ESP_LOGI(TAG, "ALDO=======================================================================");
    ESP_LOGI(TAG, "ALDO1: %s   Voltage:%u mV", axp2101_get_bit(AXP2101_REG_LDO_ONOFF_CTRL0, 0) ? "+" : "-",
             axp2101_get_linear_voltage(AXP2101_REG_LDO_VOL0_CTRL, AXP2101_LDO_MIN_MV, AXP2101_LDO_STEP_MV));
    ESP_LOGI(TAG, "ALDO2: %s   Voltage:%u mV", axp2101_get_bit(AXP2101_REG_LDO_ONOFF_CTRL0, 1) ? "+" : "-",
             axp2101_get_linear_voltage(AXP2101_REG_LDO_VOL1_CTRL, AXP2101_LDO_MIN_MV, AXP2101_LDO_STEP_MV));
    ESP_LOGI(TAG, "ALDO3: %s   Voltage:%u mV", axp2101_get_bit(AXP2101_REG_LDO_ONOFF_CTRL0, 2) ? "+" : "-",
             axp2101_get_linear_voltage(AXP2101_REG_LDO_VOL2_CTRL, AXP2101_LDO_MIN_MV, AXP2101_LDO_STEP_MV));
    ESP_LOGI(TAG, "ALDO4: %s   Voltage:%u mV", axp2101_get_bit(AXP2101_REG_LDO_ONOFF_CTRL0, 3) ? "+" : "-",
             axp2101_get_linear_voltage(AXP2101_REG_LDO_VOL3_CTRL, AXP2101_LDO_MIN_MV, AXP2101_LDO_STEP_MV));
    ESP_LOGI(TAG, "BLDO=======================================================================");
    ESP_LOGI(TAG, "BLDO1: %s   Voltage:%u mV", axp2101_get_bit(AXP2101_REG_LDO_ONOFF_CTRL0, 4) ? "+" : "-",
             axp2101_get_linear_voltage(AXP2101_REG_LDO_VOL4_CTRL, AXP2101_LDO_MIN_MV, AXP2101_LDO_STEP_MV));
    ESP_LOGI(TAG, "BLDO2: %s   Voltage:%u mV", axp2101_get_bit(AXP2101_REG_LDO_ONOFF_CTRL0, 5) ? "+" : "-",
             axp2101_get_linear_voltage(AXP2101_REG_LDO_VOL5_CTRL, AXP2101_LDO_MIN_MV, AXP2101_LDO_STEP_MV));
    ESP_LOGI(TAG, "CPUSLDO====================================================================");
    ESP_LOGI(TAG, "CPUSLDO: %s Voltage:%u mV", axp2101_get_bit(AXP2101_REG_LDO_ONOFF_CTRL0, 6) ? "+" : "-",
             axp2101_get_linear_voltage(AXP2101_REG_LDO_VOL6_CTRL, AXP2101_LDO_MIN_MV, AXP2101_CPUSLDO_STEP_MV));
    ESP_LOGI(TAG, "DLDO=======================================================================");
    ESP_LOGI(TAG, "DLDO1: %s   Voltage:%u mV", axp2101_get_bit(AXP2101_REG_LDO_ONOFF_CTRL0, 7) ? "+" : "-",
             axp2101_get_linear_voltage(AXP2101_REG_LDO_VOL7_CTRL, AXP2101_LDO_MIN_MV, AXP2101_LDO_STEP_MV));
    ESP_LOGI(TAG, "DLDO2: %s   Voltage:%u mV", axp2101_get_bit(AXP2101_REG_LDO_ONOFF_CTRL1, 0) ? "+" : "-",
             axp2101_get_linear_voltage(AXP2101_REG_LDO_VOL8_CTRL, AXP2101_LDO_MIN_MV, AXP2101_LDO_STEP_MV));
    ESP_LOGI(TAG, "===========================================================================");
}

esp_err_t pmu_init()
{
    uint8_t chip_id = 0;
    esp_err_t ret = axp2101_read_reg(AXP2101_REG_IC_TYPE, &chip_id);
    if (ret != ESP_OK || chip_id != AXP2101_CHIP_ID) {
        ESP_LOGE(TAG, "Init PMU FAILED, chip id: 0x%02x", chip_id);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Init PMU SUCCESS!");

    ret |= axp2101_set_bit(AXP2101_REG_ADC_CHANNEL_CTRL, 1, false);
    ret |= axp2101_configure_outputs();
    if (ret != ESP_OK) {
        return ret;
    }
    axp2101_log_outputs();

    ret |= axp2101_clear_irq_status();
    ret |= axp2101_set_bit(AXP2101_REG_ADC_CHANNEL_CTRL, 4, true);
    ret |= axp2101_set_bit(AXP2101_REG_ADC_CHANNEL_CTRL, 3, true);
    ret |= axp2101_set_bit(AXP2101_REG_ADC_CHANNEL_CTRL, 2, true);
    ret |= axp2101_set_bit(AXP2101_REG_ADC_CHANNEL_CTRL, 0, true);
    ret |= axp2101_set_bit(AXP2101_REG_ADC_CHANNEL_CTRL, 1, false);
    ret |= axp2101_set_irq_mask(0);
    ret |= axp2101_clear_irq_status();
    ret |= axp2101_set_irq_mask(AXP2101_IRQ_BAT_INSERT | AXP2101_IRQ_BAT_REMOVE |
                                AXP2101_IRQ_VBUS_INSERT | AXP2101_IRQ_VBUS_REMOVE |
                                AXP2101_IRQ_PKEY_SHORT | AXP2101_IRQ_PKEY_LONG |
                                AXP2101_IRQ_BAT_CHG_DONE | AXP2101_IRQ_BAT_CHG_START);
    ret |= axp2101_update_reg(AXP2101_REG_IPRECHG_SET, 0x03, AXP2101_PRECHARGE_50MA);
    ret |= axp2101_update_reg(AXP2101_REG_ICC_CHG_SET, 0x1F, AXP2101_CHARGE_CURRENT_400MA);
    ret |= axp2101_update_reg(AXP2101_REG_ITERM_CHG_SET_CTRL, 0x0F, AXP2101_TERMINATION_CURRENT_25MA);
    ret |= axp2101_update_reg(AXP2101_REG_CV_CHG_VOL_SET, 0x03, AXP2101_CHARGE_VOLTAGE_4V2);

    ESP_LOGI(TAG, "battery percentage:%d %%", axp2101_get_battery_percent());
    return ret;
}

void pmu_isr_handler()
{
    (void)axp2101_get_irq_status();

    ESP_LOGI(TAG, "Power Temperature: %.2f degC", axp2101_get_temperature());
    ESP_LOGI(TAG, "isCharging: %s", axp2101_is_charging() ? "YES" : "NO");
    ESP_LOGI(TAG, "isDischarge: %s", axp2101_is_discharge() ? "YES" : "NO");
    ESP_LOGI(TAG, "isStandby: %s", axp2101_is_standby() ? "YES" : "NO");
    ESP_LOGI(TAG, "isVbusIn: %s", axp2101_is_vbus_in() ? "YES" : "NO");
    ESP_LOGI(TAG, "isVbusGood: %s", axp2101_is_vbus_good() ? "YES" : "NO");
    ESP_LOGI(TAG, "Charger Status: %s", axp2101_charge_status_name(axp2101_get_charger_status()));
    ESP_LOGI(TAG, "getBattVoltage: %d mV", axp2101_get_battery_voltage());
    ESP_LOGI(TAG, "getVbusVoltage: %d mV", axp2101_get_vbus_voltage());
    ESP_LOGI(TAG, "getSystemVoltage: %d mV", axp2101_get_system_voltage());

    if (axp2101_is_battery_connected()) {
        ESP_LOGI(TAG, "getBatteryPercent: %d %%", axp2101_get_battery_percent());
    }
    (void)axp2101_clear_irq_status();
}
