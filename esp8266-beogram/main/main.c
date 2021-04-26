#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/hw_timer.h"
#include "driver/pwm.h"
#include "driver/uart.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "pid.h"
#include "server.h"

// Pins
#define INPUT_MOTOR_TACHO_PIN 14 // D5
#define INPUT_SWITCH_PIN 2 // D4
#define OUTPUT_MOTOR_PIN 4 // D2
#define OUTPUT_STATUS_PIN0 12 // D6
#define OUTPUT_STATUS_PIN1 13 // D7
#define OUTPUT_STATUS_PIN2 15 // D8
#define OUTPUT_RELAY_PIN 5 // D1

// General constants
#define UART_BUF_SIZE 1024

// TT-related Constants
#define RELAY_TIME 500000
#define FREQ_COUNTER_DT_OFF 10000
#define FREQ_COUNTER_PERIOD_OFF 10000
#define FREQ_COUNTER_SAMPLE_SIZE 24
#define DRIVER_PWM_PERIOD 1000
#define ACCURACY 0.01

/* #define DEBUG_DT */

/*
 * Globals
 */
static struct {
    float current;
    uint32_t current_pwm;
} driver;

static struct {
    xQueueHandle evt_queue;
    int64_t last;
    bool off_detected;
} freq_counter;

static struct {
    struct pid pid;
    uint32_t setpoint;
    float Kp;
    float Ti;
    float Td;

    uint32_t current;
    uint32_t current_avg;

    int relay_state;
    int switch_state;
} state;

/* Output */
static void driver_update(float speed){
    driver.current = speed;
    driver.current_pwm = (1. - speed)*DRIVER_PWM_PERIOD;
    pwm_set_duty(0, driver.current_pwm);
    pwm_start();
}

/*
 * Input
 */
static void handle_isr(void *arg){
    int64_t time = esp_timer_get_time();
    if(freq_counter.last){
        uint32_t dt = (uint32_t)(time - freq_counter.last);
        if(dt > FREQ_COUNTER_DT_OFF){
            dt = FREQ_COUNTER_DT_OFF;
            freq_counter.off_detected = true;
        }else{
            freq_counter.off_detected = false;
        }
        xQueueSendFromISR(freq_counter.evt_queue, &dt, NULL);
    }
    freq_counter.last = time;
}

static void handle_timer(void *arg){
    int64_t dt = (uint32_t)(esp_timer_get_time() - freq_counter.last);
    if(dt > FREQ_COUNTER_DT_OFF){
        dt = FREQ_COUNTER_DT_OFF;
        freq_counter.off_detected = true;
        xQueueSendFromISR(freq_counter.evt_queue, &dt, NULL);
    }

    if(state.relay_state == 0){
        gpio_set_level(OUTPUT_RELAY_PIN, false);

    }else if(state.relay_state < RELAY_TIME / FREQ_COUNTER_PERIOD_OFF){
        gpio_set_level(OUTPUT_RELAY_PIN, true);
        state.relay_state++;
    }else{
        state.relay_state = 0;
    }

    state.switch_state = !gpio_get_level(INPUT_SWITCH_PIN);
}


static uint8_t *uart_buffer;

static void update_pid_weights(){
    state.pid.K.p = state.Kp;
    state.pid.K.i = state.Ti > 0. ? state.Kp / state.Ti : 0.;
    state.pid.K.d = state.Kp * state.Td;
}

/*
 * Main
 */
static void main_task(void* arg){

    driver_update(0.5);

    state.current_avg = FREQ_COUNTER_PERIOD_OFF;
    int avg_c = 0;
    int avg_agg = 0;

    for(;;){

#ifdef CONFIG_BG_ENABLE_UART
        if(freq_counter.off_detected){
            int len = uart_read_bytes(
                    UART_NUM_0, uart_buffer, UART_BUF_SIZE, 20 / portTICK_RATE_MS);
            int l;
            for(l=0; l<len && uart_buffer[l]!='\n'; l++);
            if(l < len){
                uart_buffer[l] = 0;
                if(l > 3){
                    if(uart_buffer[0] == 'K' &&
                            uart_buffer[1] == 'p' &&
                            uart_buffer[2] == '='){
                        state.Kp = atof((const char*)(uart_buffer + 3));
                        update_pid_weights();

                        uart_buffer[0] = 'k';
                        uart_buffer[1] = '\n';
                        uart_write_bytes(UART_NUM_0, (const char *) uart_buffer, 2);
                    }else if(uart_buffer[0] == 'T' &&
                            uart_buffer[1] == 'i' &&
                            uart_buffer[2] == '='){
                        state.Ti = atof((const char*)(uart_buffer + 3));
                        update_pid_weights();

                        uart_buffer[0] = 'k';
                        uart_buffer[1] = '\n';
                        uart_write_bytes(UART_NUM_0, (const char *) uart_buffer, 2);
                    }else if(uart_buffer[0] == 'T' &&
                            uart_buffer[1] == 'd' &&
                            uart_buffer[2] == '='){
                        state.Td = atof((const char*)(uart_buffer + 3));
                        update_pid_weights();

                        uart_buffer[0] = 'k';
                        uart_buffer[1] = '\n';
                        uart_write_bytes(UART_NUM_0, (const char *) uart_buffer, 2);
                    }
                }else if(l >= 1 && uart_buffer[0] == 'l'){
                    state.relay_state = 1;
                    uart_buffer[0] = 'k';
                    uart_buffer[1] = '\n';
                    uart_write_bytes(UART_NUM_0, (const char *) uart_buffer, 2);
                }
            }
        }
#endif

        uint32_t dt;
        if(xQueueReceive(freq_counter.evt_queue, &dt, portMAX_DELAY)){
            state.current = dt;

#ifdef DEBUG_DT
#ifdef CONFIG_BG_ENABLE_UART
            uart_buffer[0] = 'd';
            uart_buffer[1] = dt >> 8;
            uart_buffer[2] = dt;
            uart_buffer[3] = '\n';
            uart_write_bytes(UART_NUM_0, (const char *) uart_buffer, 4);
#endif
#endif

            avg_agg += dt;
            avg_c++;

            if(avg_c == FREQ_COUNTER_SAMPLE_SIZE){
                state.current_avg = avg_agg / FREQ_COUNTER_SAMPLE_SIZE;
                avg_agg = 0;
                avg_c = 0;
            
#ifdef CONFIG_BG_ENABLE_UART
                uart_buffer[0] = 'a';
                uart_buffer[1] = state.current_avg >> 8;
                uart_buffer[2] = state.current_avg;
                uart_buffer[3] = '\n';
                uart_write_bytes(UART_NUM_0, (const char *) uart_buffer, 4);
#endif
                
                float next = 0.;
                bool close = false;
                if(!state.switch_state){
                    int diff = abs((int)state.current_avg - (int)state.setpoint);
                    close = diff * (int)(1. / ACCURACY) < state.setpoint;

                    next = pid_update(&state.pid,
                            state.current_avg, esp_timer_get_time());
                }
                driver_update(next);

#ifdef CONFIG_BG_ENABLE_UART
                int p_next = 10000 * next;
                uart_buffer[0] = 'f';
                uart_buffer[1] = p_next >> 8;
                uart_buffer[2] = p_next;
                uart_buffer[3] = '\n';
                uart_write_bytes(UART_NUM_0, (const char *) uart_buffer, 4);
#endif

                gpio_set_level(OUTPUT_STATUS_PIN0, freq_counter.off_detected);
                gpio_set_level(OUTPUT_STATUS_PIN1, close);
                gpio_set_level(OUTPUT_STATUS_PIN2, 1);
            }
        }
    }
}

static bool is_running(){
    return !freq_counter.off_detected;
}

static void relay(){
    if(state.relay_state == 0){
        state.relay_state = 1;
    }
}

static void server_connect_task(void* arg){

#ifdef CONFIG_BG_ENABLE_HTTP_SERVER
    server_connect(is_running, relay);
#endif

    vTaskDelete(NULL);
}

void do_nothing_putc1(char c){}

void app_main(){
    // TODO Disable logging to uart
    os_install_putc1(do_nothing_putc1);

    // Init
    freq_counter.evt_queue = xQueueCreate(10, sizeof(uint32_t));
    freq_counter.last = 0;
    freq_counter.off_detected = true;
    driver.current = 0;
    driver.current_pwm = DRIVER_PWM_PERIOD;

    gpio_config_t io_conf;

    // Setup PWM
    driver.current = 0.0;
    driver.current_pwm = 1000;
    static uint32_t pwm_pin = OUTPUT_MOTOR_PIN;
    pwm_init(DRIVER_PWM_PERIOD, &driver.current_pwm, 1, &pwm_pin);
    pwm_set_phase(0, 0.);
    pwm_start();

    // Setup interrupt
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << INPUT_MOTOR_TACHO_PIN);
    io_conf.pull_up_en = false;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(INPUT_MOTOR_TACHO_PIN, handle_isr, NULL);

    // Setup timer
    hw_timer_init(handle_timer, NULL);
    hw_timer_alarm_us(FREQ_COUNTER_PERIOD_OFF, true);

    // Setup status pins
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = false;

    io_conf.pin_bit_mask = (1ULL << OUTPUT_STATUS_PIN0);
    gpio_config(&io_conf);
    io_conf.pin_bit_mask = (1ULL << OUTPUT_STATUS_PIN1);
    gpio_config(&io_conf);
    io_conf.pin_bit_mask = (1ULL << OUTPUT_STATUS_PIN2);
    gpio_config(&io_conf);

    // Setup speed switch pin
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = true;
    io_conf.pin_bit_mask = (1ULL << INPUT_SWITCH_PIN);
    gpio_config(&io_conf);

    // Setup relay pin
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = false;
    io_conf.pin_bit_mask = (1ULL << OUTPUT_RELAY_PIN);
    gpio_config(&io_conf);

    gpio_set_level(OUTPUT_RELAY_PIN, false);

    state.relay_state = 0;


    // Setup PID
    // 33.3RPM -> 2330us
    state.setpoint = 2330;
    /* state.setpoint = 2330 * 33.3 / 45; */

    // PID Defaults
    //
    // Before timing bugfix (uart read)
    // Reasonable: Kp=-0.0001, Ti=1, Td=0
    // Second: Kp=-0.0002, Ti=2 Td=0.5
    //
    // After timing bugfix
    // First: Kp = -0.001 Ti=1 Td=0
    state.Kp = -0.002;
    state.Ti = 0.5;
    state.Td = 0.;
    pid_init(&state.pid, 1.0, 0.0, 0.0,
         // Setpoint
         state.setpoint,
         // Bounds
         0.0, 1.0);
    update_pid_weights();

#ifdef CONFIG_BG_ENABLE_UART
    // Setup UART
    uart_config_t uart_config = {
        .baud_rate = 74880,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_buffer = (uint8_t *) malloc(UART_BUF_SIZE);

#endif

    // Setup main task
    xTaskCreate(main_task, "main_task", 2048, NULL, 10, NULL);

    // Loop
    bool initial = true;
    for(;;){
        vTaskDelay(1000 / portTICK_RATE_MS);
        if(initial && freq_counter.off_detected){
            xTaskCreate(server_connect_task, "server_connect_task", 2048, NULL, 10, NULL);
        }
        initial = false;
    }
}
