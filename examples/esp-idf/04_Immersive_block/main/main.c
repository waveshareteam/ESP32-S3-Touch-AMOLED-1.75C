#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "driver/gpio.h"

/* qmi8658.h defines M_PI; IDF v6 picolibc already provides it. */
#ifdef M_PI
#undef M_PI
#endif
#include "qmi8658.h"
i2c_master_bus_handle_t bus_handle;
static const char *TAG = "gyro_shapes";

typedef enum {
    SHAPE_CIRCLE,
    SHAPE_SQUARE,
    SHAPE_TRIANGLE,
    SHAPE_HEXAGON,
    SHAPE_COUNT
} ShapeType;

typedef struct {
    lv_obj_t *obj;
    ShapeType type;
    int radius;
    int x_pos;
    int y_pos;
    lv_color_t color;
} Shape;

#define MAX_SHAPES 15
#define MIN_SHAPE_SIZE 15
#define MAX_SHAPE_SIZE 30
#define MAX_TOTAL_PLACEMENT_ATTEMPTS (MAX_SHAPES * 20)
#define ACCEL_SCALE_FACTOR 5
#define TASK_DELAY_MS 33
#define DISPLAY_LOCK_TIMEOUT_MS (-1)
#define CALIBRATION_DEADZONE 0.05f
#define CALIBRATION_SAMPLES 200
#define CALIBRATION_MAX_RETRIES 5

#define SCREEN_WIDTH_MM  33.09f
#define SCREEN_HEIGHT_MM 41.51f
#define CORNER_RADIUS_MM 9.2f

static float accel_bias_x = 0.0f;
static float accel_bias_y = 0.0f;
static bool calibration_done = false;
static volatile bool recalibration_requested = false;
static int display_width = BSP_LCD_H_RES;
static int display_height = BSP_LCD_V_RES;
static Shape shapes[MAX_SHAPES];
static int shape_count = 0;

lv_color_t get_random_color() {
    return lv_color_hsv_to_rgb(rand() % 360, 70, 90);
}

bool check_overlap(const Shape *a, const Shape *b) {
    int dx = a->x_pos - b->x_pos;
    int dy = a->y_pos - b->y_pos;
    int distance_squared = dx * dx + dy * dy;
    int min_distance = a->radius + b->radius;
    return distance_squared < (min_distance * min_distance);
}

lv_obj_t *create_shape_obj(ShapeType type, int size, lv_color_t color) {
    lv_obj_t *obj;
    
    switch(type) {
        case SHAPE_CIRCLE: {
            obj = lv_obj_create(lv_screen_active());
            lv_obj_set_size(obj, size * 2, size * 2);
            lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
            break;
        }
        case SHAPE_SQUARE: {
            obj = lv_obj_create(lv_screen_active());
            lv_obj_set_size(obj, size * 2, size * 2);
            lv_obj_set_style_radius(obj, 0, 0);
            break;
        }
        case SHAPE_TRIANGLE: {
            obj = lv_obj_create(lv_screen_active());
            lv_obj_set_size(obj, size * 2, size * 2);
            lv_obj_set_style_radius(obj, 0, 0);
            static lv_style_t style;
            lv_style_init(&style);
            lv_style_set_bg_color(&style, color);
            lv_obj_add_style(obj, &style, 0);
            break;
        }
        case SHAPE_HEXAGON: {
            obj = lv_obj_create(lv_screen_active());
            lv_obj_set_size(obj, size * 2, size * 2);
            lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
            break;
        }
        default:
            return NULL;
    }
    
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 3, 0);
    return obj;
}

void generate_random_shapes() {
    srand(time(NULL));
    shape_count = 0;
    int placement_attempts = 0;

    if (display_width <= (2 * MAX_SHAPE_SIZE) || display_height <= (2 * MAX_SHAPE_SIZE)) {
        ESP_LOGE(TAG, "Display size %dx%d is too small for shape generation",
                 display_width, display_height);
        return;
    }
    
    while (shape_count < MAX_SHAPES && placement_attempts < MAX_TOTAL_PLACEMENT_ATTEMPTS) {
        Shape new_shape = {0};
        new_shape.type = rand() % SHAPE_COUNT;
        new_shape.radius = MIN_SHAPE_SIZE + rand() % (MAX_SHAPE_SIZE - MIN_SHAPE_SIZE + 1);
        
        bool valid_position = false;
        for (int attempts = 0;
             attempts < 100 && placement_attempts < MAX_TOTAL_PLACEMENT_ATTEMPTS;
             attempts++) {
            placement_attempts++;
            int x_range = display_width - 2 * new_shape.radius;
            int y_range = display_height - 2 * new_shape.radius;
            if (x_range <= 0 || y_range <= 0) {
                break;
            }

            new_shape.x_pos = new_shape.radius + rand() % x_range;
            new_shape.y_pos = new_shape.radius + rand() % y_range;

            valid_position = true;
            for (int i = 0; i < shape_count; i++) {
                if (check_overlap(&new_shape, &shapes[i])) {
                    valid_position = false;
                    break;
                }
            }

            if (valid_position) {
                break;
            }
        }
        
        if (!valid_position) {
            continue;
        }

        new_shape.color = get_random_color();
        new_shape.obj = create_shape_obj(new_shape.type, new_shape.radius, new_shape.color);
        if (!new_shape.obj) {
            ESP_LOGW(TAG, "Failed to create shape object");
            continue;
        }

        lv_obj_set_pos(new_shape.obj, new_shape.x_pos - new_shape.radius,
                       new_shape.y_pos - new_shape.radius);
        shapes[shape_count] = new_shape;
        shape_count++;
    }
    if (shape_count < MAX_SHAPES) {
        ESP_LOGW(TAG, "Placed only %d/%d shapes after %d attempts", shape_count,
                 MAX_SHAPES, placement_attempts);
    }
}

static esp_err_t perform_level_calibration(qmi8658_dev_t *dev) {
    qmi8658_data_t data;

    ESP_LOGI(TAG, "Starting level calibration...");
    ESP_LOGI(TAG, "Please place device on a level surface");

    bool have_calibration = false;

    for (int attempt = 1; attempt <= CALIBRATION_MAX_RETRIES; attempt++) {
        float sum_x = 0.0f, sum_y = 0.0f;
        float max_x = -10.0f, min_x = 10.0f;
        float max_y = -10.0f, min_y = 10.0f;
        int valid_samples = 0;

        for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
            if (qmi8658_read_sensor_data(dev, &data) == ESP_OK) {
                sum_x += data.accelX;
                sum_y += data.accelY;
                valid_samples++;

                if (data.accelX > max_x) max_x = data.accelX;
                if (data.accelX < min_x) min_x = data.accelX;
                if (data.accelY > max_y) max_y = data.accelY;
                if (data.accelY < min_y) min_y = data.accelY;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (valid_samples == 0) {
            ESP_LOGW(TAG, "Calibration attempt %d/%d read no samples", attempt, CALIBRATION_MAX_RETRIES);
            continue;
        }

        float range_x = max_x - min_x;
        float range_y = max_y - min_y;
        accel_bias_x = sum_x / valid_samples;
        accel_bias_y = sum_y / valid_samples;
        have_calibration = true;

        if (range_x <= 0.1f && range_y <= 0.1f) {
            calibration_done = true;
            ESP_LOGI(TAG, "Calibration complete. Bias X: %.4f m/s^2, Bias Y: %.4f m/s^2",
                     accel_bias_x, accel_bias_y);
            ESP_LOGI(TAG, "Device is now level. Shapes should be stationary.");
            return ESP_OK;
        }

        ESP_LOGW(TAG, "Calibration unstable on attempt %d/%d (X range: %.4f, Y range: %.4f)",
                 attempt, CALIBRATION_MAX_RETRIES, range_x, range_y);
    }

    if (!have_calibration) {
        calibration_done = false;
        return ESP_FAIL;
    }

    calibration_done = true;
    ESP_LOGW(TAG, "Using the last calibration sample set after unstable readings");
    return ESP_OK;
}

void apply_calibration_and_deadzone(qmi8658_data_t *data) {
    if (calibration_done) {
        data->accelX -= accel_bias_x;
        data->accelY -= accel_bias_y;
        
        if (fabsf(data->accelX) < CALIBRATION_DEADZONE) data->accelX = 0.0f;
        if (fabsf(data->accelY) < CALIBRATION_DEADZONE) data->accelY = 0.0f;
    }
}

static void constrain_to_rounded_rect(int *x, int *y, int width, int height, 
                                     float corner_radius_mm, int shape_radius) {
    float px_per_mm_x = (float)width / SCREEN_WIDTH_MM;
    float px_per_mm_y = (float)height / SCREEN_HEIGHT_MM;
    
    float px_per_mm = (px_per_mm_x < px_per_mm_y) ? px_per_mm_x : px_per_mm_y;
    
    int corner_radius_px = (int)(corner_radius_mm * px_per_mm);
    
    if (corner_radius_px < shape_radius) {
        corner_radius_px = shape_radius + 5;
    }

    int safe_left = corner_radius_px;
    int safe_right = width - corner_radius_px;
    int safe_top = corner_radius_px;
    int safe_bottom = height - corner_radius_px;

    if (*x < shape_radius) *x = shape_radius;
    if (*x > width - shape_radius) *x = width - shape_radius;
    if (*y < shape_radius) *y = shape_radius;
    if (*y > height - shape_radius) *y = height - shape_radius;

    int corner_radius_effective = corner_radius_px - shape_radius;
    
    if (*x < safe_left && *y < safe_top) {
        float dx = safe_left - *x;
        float dy = safe_top - *y;
        float dist = sqrtf(dx*dx + dy*dy);
        
        if (dist > corner_radius_effective) {
            float ratio = corner_radius_effective / dist;
            *x = safe_left - (int)(dx * ratio);
            *y = safe_top - (int)(dy * ratio);
        }
    }
    else if (*x > safe_right && *y < safe_top) {
        float dx = *x - safe_right;
        float dy = safe_top - *y;
        float dist = sqrtf(dx*dx + dy*dy);
        
        if (dist > corner_radius_effective) {
            float ratio = corner_radius_effective / dist;
            *x = safe_right + (int)(dx * ratio);
            *y = safe_top - (int)(dy * ratio);
        }
    }
    else if (*x < safe_left && *y > safe_bottom) {
        float dx = safe_left - *x;
        float dy = *y - safe_bottom;
        float dist = sqrtf(dx*dx + dy*dy);
        
        if (dist > corner_radius_effective) {
            float ratio = corner_radius_effective / dist;
            *x = safe_left - (int)(dx * ratio);
            *y = safe_bottom + (int)(dy * ratio);
        }
    }
    else if (*x > safe_right && *y > safe_bottom) {
        float dx = *x - safe_right;
        float dy = *y - safe_bottom;
        float dist = sqrtf(dx*dx + dy*dy);
        
        if (dist > corner_radius_effective) {
            float ratio = corner_radius_effective / dist;
            *x = safe_right + (int)(dx * ratio);
            *y = safe_bottom + (int)(dy * ratio);
        }
    }
}

bool check_collision_impending(const Shape *a, const Shape *b, int a_new_x, int a_new_y) {
    int dx = a_new_x - b->x_pos;
    int dy = a_new_y - b->y_pos;
    int distance_squared = dx * dx + dy * dy;
    int min_distance = a->radius + b->radius;
    return distance_squared < (min_distance * min_distance);
}

void handle_shape_collisions(int index) {
    for (int i = 0; i < shape_count; i++) {
        if (i == index) continue;
        
        if (check_overlap(&shapes[index], &shapes[i])) {
            int dx = shapes[index].x_pos - shapes[i].x_pos;
            int dy = shapes[index].y_pos - shapes[i].y_pos;
            float distance = sqrtf(dx*dx + dy*dy);
            float overlap = (shapes[index].radius + shapes[i].radius) - distance;
            
            if (distance > 0) {
                float ratio = overlap / (2 * distance);
                shapes[index].x_pos += (int)(dx * ratio);
                shapes[index].y_pos += (int)(dy * ratio);
                shapes[i].x_pos -= (int)(dx * ratio);
                shapes[i].y_pos -= (int)(dy * ratio);
            } else {
                shapes[index].x_pos += (rand() % 11) - 5;
                shapes[index].y_pos += (rand() % 11) - 5;
            }
        }
    }
}

static void shapes_update_task(void *arg) {
    qmi8658_dev_t *dev = (qmi8658_dev_t *)arg;
    qmi8658_data_t data;
    if (display_width <= 0 || display_height <= 0) {
        display_width = BSP_LCD_H_RES;
        display_height = BSP_LCD_V_RES;
    }

    while (shape_count == 0) {
        if (bsp_display_lock(DISPLAY_LOCK_TIMEOUT_MS) == ESP_OK) {
            generate_random_shapes();
            bsp_display_unlock();
        } else {
            ESP_LOGW(TAG, "Waiting for display lock before initial shape generation");
        }
        if (shape_count == 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    while (1) {
        if (recalibration_requested) {
            recalibration_requested = false;
            esp_err_t cal_ret = perform_level_calibration(dev);
            if (cal_ret != ESP_OK) {
                ESP_LOGE(TAG, "Recalibration failed: %s", esp_err_to_name(cal_ret));
            }
        }
        
        bool ready;
        esp_err_t ret = qmi8658_is_data_ready(dev, &ready);
        if (ret != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));
            continue;
        }

        if (ready) {
            ret = qmi8658_read_sensor_data(dev, &data);
            if (ret == ESP_OK) {
                apply_calibration_and_deadzone(&data);
                
                int move_x = (int)(data.accelY * ACCEL_SCALE_FACTOR);
                int move_y = (int)(data.accelX * ACCEL_SCALE_FACTOR);

                if (move_x == 0 && move_y == 0) {
                    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));
                    continue;
                }
                
                if (bsp_display_lock(DISPLAY_LOCK_TIMEOUT_MS) != ESP_OK) {
                    ESP_LOGW(TAG, "Skipping shape update because display lock failed");
                    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));
                    continue;
                }
                
                for (int i = 0; i < shape_count; i++) {
                    int old_x = shapes[i].x_pos;
                    int old_y = shapes[i].y_pos;
                    int new_x = shapes[i].x_pos + move_x;
                    int new_y = shapes[i].y_pos + move_y;
                    
                    bool collision = false;
                    for (int j = 0; j < shape_count; j++) {
                        if (i == j) continue;
                        if (check_collision_impending(&shapes[i], &shapes[j], new_x, new_y)) {
                            collision = true;
                            break;
                        }
                    }
                    
                    if (!collision) {
                        shapes[i].x_pos = new_x;
                        shapes[i].y_pos = new_y;
                    }
                    
                    constrain_to_rounded_rect(&shapes[i].x_pos, &shapes[i].y_pos, 
                                             display_width, display_height, 
                                             CORNER_RADIUS_MM, shapes[i].radius);
                    
                    handle_shape_collisions(i);

                    if (shapes[i].x_pos != old_x || shapes[i].y_pos != old_y) {
                        lv_obj_set_pos(shapes[i].obj,
                                       shapes[i].x_pos - shapes[i].radius,
                                       shapes[i].y_pos - shapes[i].radius);
                    }
                }
                
                bsp_display_unlock();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));
    }
}

static void IRAM_ATTR button_isr_handler(void* arg) {
    recalibration_requested = true;
}

void init_calibration_button() {
    const int CALIB_BUTTON_GPIO = GPIO_NUM_0;
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CALIB_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&io_conf);
    
    gpio_install_isr_service(0);
    gpio_isr_handler_add(CALIB_BUTTON_GPIO, button_isr_handler, NULL);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (bsp_display_start() == NULL) {
        ESP_LOGE(TAG, "Failed to start display");
        return;
    }
    bsp_display_backlight_on();
    display_width = BSP_LCD_H_RES;
    display_height = BSP_LCD_V_RES;

    if (bsp_display_lock(DISPLAY_LOCK_TIMEOUT_MS) == ESP_OK) {
        lv_obj_t *calib_label = lv_label_create(lv_screen_active());
        lv_label_set_text(calib_label, "Press BOOT to recalibrate");
        lv_obj_align(calib_label, LV_ALIGN_BOTTOM_MID, 0, -10);
        bsp_display_unlock();
    } else {
        ESP_LOGW(TAG, "Unable to create calibration label because display lock failed");
    }

    bus_handle = bsp_i2c_get_handle();
    qmi8658_dev_t *dev = calloc(1, sizeof(qmi8658_dev_t));
    ESP_ERROR_CHECK(dev ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(qmi8658_init(dev, bus_handle, QMI8658_ADDRESS_HIGH));

    qmi8658_set_accel_range(dev, QMI8658_ACCEL_RANGE_8G);
    qmi8658_set_accel_odr(dev, QMI8658_ACCEL_ODR_500HZ);
    qmi8658_set_accel_unit_mps2(dev, true);
    
    qmi8658_write_register(dev, QMI8658_CTRL5, 0x03);

    init_calibration_button();

    ret = perform_level_calibration(dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Initial calibration failed: %s", esp_err_to_name(ret));
    }

    BaseType_t task_ret = xTaskCreate(
        shapes_update_task,
        "shapes_update",
        8192,
        dev,
        2,
        NULL
    );
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create shapes update task");
        free(dev);
    }
}
