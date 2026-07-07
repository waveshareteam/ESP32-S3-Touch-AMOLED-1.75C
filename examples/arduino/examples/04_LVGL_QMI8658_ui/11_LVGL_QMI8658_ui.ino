#include <lvgl.h>
#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include "lv_conf.h"
#include <Arduino.h>
#include <Wire.h>
#include "SensorQMI8658.hpp"

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[LCD_WIDTH * LCD_HEIGHT / 10];

SensorQMI8658 qmi;

IMUdata acc;
IMUdata gyr;

lv_obj_t *label;                  // Global label object
lv_obj_t *chart;                  // Global chart object
lv_chart_series_t *acc_series_x;  // Acceleration X series
lv_chart_series_t *acc_series_y;  // Acceleration Y series
lv_chart_series_t *acc_series_z;  // Acceleration Z series

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS /* CS */, LCD_SCLK /* SCK */, LCD_SDIO0 /* SDIO0 */, LCD_SDIO1 /* SDIO1 */,
  LCD_SDIO2 /* SDIO2 */, LCD_SDIO3 /* SDIO3 */);

Arduino_CO5300 *gfx = new Arduino_CO5300(
  bus, LCD_RESET /* RST */, 0 /* rotation */, LCD_WIDTH /* width */, LCD_HEIGHT /* height */, 6, 0, 0, 0);

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char *buf) {
  Serial.printf(buf);
  Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

  lv_disp_flush_ready(disp);
}

void example_increase_lvgl_tick(void *arg) {
  /* Tell LVGL how many milliseconds has elapsed */
  lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static uint8_t count = 0;
void example_increase_reboot(void *arg) {
  count++;
  if (count == 30) {
    esp_restart();
  }
}

void setup() {
  Serial.begin(115200); /* prepare for possible serial debug */
  Wire.begin(IIC_SDA, IIC_SCL);

  gfx->begin();
  gfx->setBrightness(200);

  String LVGL_Arduino = "Hello Arduino! ";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

  Serial.println(LVGL_Arduino);
  Serial.println("I am LVGL_Arduino");

  lv_init();

#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, LCD_WIDTH * LCD_HEIGHT / 10);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = LCD_WIDTH;
  disp_drv.ver_res = LCD_HEIGHT;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  // indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "Hello Ardino and LVGL!");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

  const esp_timer_create_args_t lvgl_tick_timer_args = {
    .callback = &example_increase_lvgl_tick,
    .name = "lvgl_tick"
  };

  const esp_timer_create_args_t reboot_timer_args = {
    .callback = &example_increase_reboot,
    .name = "reboot"
  };

  esp_timer_handle_t lvgl_tick_timer = NULL;
  esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
  esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);

  label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "Initializing...");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

  /* Create chart */
  chart = lv_chart_create(lv_scr_act());
  lv_obj_set_size(chart, 240, 280);
  lv_obj_align(chart, LV_ALIGN_CENTER, 0, 0);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);              /* Set the type to line */
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -3, 3); /* Set the range of y axis */
  lv_chart_set_point_count(chart, 20);                       /* Set the number of data points */
  acc_series_x = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
  acc_series_y = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
  acc_series_z = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

  Serial.println("Setup done");

if (!qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    Serial.println("Failed to find QMI8658 - check your wiring!");
    while (1) {
      delay(1000);
    }
  }

  qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_1000Hz, SensorQMI8658::LPF_MODE_0);
  qmi.enableAccelerometer();

  Serial.println("Read data now...");
}

void loop() {
  lv_timer_handler(); /* let the GUI do its work */
  delay(5);

  if (qmi.getDataReady()) {
    if (qmi.getAccelerometer(acc.x, acc.y, acc.z)) {
      Serial.print("{ACCEL: ");
      Serial.print(acc.x);
      Serial.print(",");
      Serial.print(acc.y);
      Serial.print(",");
      Serial.print(acc.z);
      Serial.println("}");

      // Update chart with new accelerometer data
      lv_chart_set_next_value(chart, acc_series_x, acc.x);
      lv_chart_set_next_value(chart, acc_series_y, acc.y);
      lv_chart_set_next_value(chart, acc_series_z, acc.z);
    }

    if (qmi.getGyroscope(gyr.x, gyr.y, gyr.z)) {
      Serial.print("{GYRO: ");
      Serial.print(gyr.x);
      Serial.print(",");
      Serial.print(gyr.y);
      Serial.print(",");
      Serial.print(gyr.z);
      Serial.println("}");
    }
  }
  delay(20);  // Increase the frequency of data polling
}
