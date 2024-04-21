#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <sys/utsname.h>
#define MAX_GPIO 100
#define MAX_PWM 10

// Mocked global arrays for GPIO and PWM values
int simulated_gpio_values[MAX_GPIO] = {0};
int simulated_pwm_values[MAX_PWM] = {0};

char sensor1Pin[4], sensor2Pin[4] , sensor3Pin[4] , sensor4Pin[4];
char led1Pin[4] , led2Pin[4], buzzerPin[4];
int pwm_chip, pwm_number ;

// Prototypes for mock functions and signal handling
void mocked_write_to_file(const char *path, const char *value);
int mocked_read_sensor(const char *pin);

// Prototypes for GPIO and PWM control functions
void write_to_file(const char *path, const char *value);
void initialize_gpio(const char* pin, const char* direction);
void cleanup_gpio(const char* pin);
void write_to_gpio(const char* pin, const char* value);
int read_sensor(const char* pin);
void initialize_pwm(int pwm_chip, int pwm_number);
void cleanup_pwm(int pwm_chip, int pwm_number);
void set_pwm_duty_cycle(int pwm_chip, int pwm_number, const char *duty_cycle);
void set_servo_angle(int pwm_chip, int pwm_number, int angle);

// Prototypes for setup, cleanup, and threaded functions
void setup_signal_handler(void);
void prompt_user_for_gpio_pins(void);
void initialize_all_hardware(void);
void cleanup_resources(void);
void* handle_train_sensors(void* arg);
void* handle_crossing_guard(void* arg);
void* handle_warning_lights(void* arg);
void* handle_operator_intervention(void* arg);
void handle_sigint(int sig);

// Implementations
void mocked_write_to_file(const char *path, const char *value) {
    printf("Mock write: %s = %s\n", path, value);
}

int mocked_read_sensor(const char *pin) {
    int pinNum = atoi(pin);
    return simulated_gpio_values[pinNum];
}

void write_to_file(const char *path, const char *value) {
    mocked_write_to_file(path, value);
}

void initialize_gpio(const char* pin, const char* direction) {
    printf("Initializing GPIO pin %s as %s\n", pin, direction);
}

void cleanup_gpio(const char* pin) {
    printf("Cleaning up GPIO pin %s\n", pin);
}

void write_to_gpio(const char* pin, const char* value) {
    int pinNum = atoi(pin);
    simulated_gpio_values[pinNum] = atoi(value);
    printf("GPIO %s set to %s\n", pin, value);
}

int read_sensor(const char* pin) {
    return mocked_read_sensor(pin);
}

void initialize_pwm(int pwm_chip, int pwm_number) {
    printf("Initializing PWM chip %d, number %d\n", pwm_chip, pwm_number);
}

void cleanup_pwm(int pwm_chip, int pwm_number) {
    printf("Cleaning up PWM chip %d, number %d\n", pwm_chip, pwm_number);
}

void set_pwm_duty_cycle(int pwm_chip, int pwm_number, const char *duty_cycle) {
    simulated_pwm_values[pwm_number] = atoi(duty_cycle);
    printf("PWM %d:%d duty cycle set to %s\n", pwm_chip, pwm_number, duty_cycle);
}

void set_servo_angle(int pwm_chip, int pwm_number, int angle) {
    printf("Setting servo at PWM %d:%d to angle %d\n", pwm_chip, pwm_number, angle);
}

void handle_sigint(int sig) {
    printf("SIGINT received, cleaning up...\n");
    cleanup_resources();
    exit(EXIT_SUCCESS);
}

void setup_signal_handler(void) {
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
}

void prompt_user_for_gpio_pins(void) {
    // For simulation purposes, pins are already set. Adjust as necessary for actual input.
    printf("Enter GPIO pin  number for sensor 1  (train approach from left):  ");
    scanf("%3s", sensor1Pin);
    printf("Enter GPIO pin number for sensor 2 (train leave from left): ");
    scanf("%3s", sensor2Pin);
    printf("Enter GPIO pin number for sensor 3 (train approach from right): ");
    scanf("%3s", sensor3Pin);
    printf("Enter GPIO pin number for sensor 4 (train leave from right): ");
    scanf("%3s", sensor4Pin);
    printf("Enter GPIO pin number for LED 1: ");
    scanf("%3s", led1Pin);
    printf("Enter GPIO pin number for LED 2: ");
    scanf("%3s", led2Pin);
    printf("Enter GPIO pin number for the buzzer: ");
    scanf("%3s", buzzerPin);
    printf("Enter PWM chip number (e.g., 0 for pwmchip0): ");
    scanf("%d", &pwm_chip);
    printf("Enter PWM number (e.g., 0 for pwm0): ");
    scanf("%d", &pwm_number);
}

void initialize_all_hardware(void) {
    initialize_gpio(sensor1Pin
, "in");
initialize_gpio(sensor2Pin, "in");
initialize_gpio(sensor3Pin, "in");
initialize_gpio(sensor4Pin, "in");
initialize_gpio(led1Pin, "out");
initialize_gpio(led2Pin, "out");
initialize_gpio(buzzerPin, "out");
// Initialize PWM for the servo control
initialize_pwm(pwm_chip, pwm_number);
}

void cleanup_resources(void) {
cleanup_gpio(sensor1Pin);
cleanup_gpio(sensor2Pin);
cleanup_gpio(sensor3Pin);
cleanup_gpio(sensor4Pin);
cleanup_gpio(led1Pin);
cleanup_gpio(led2Pin);
cleanup_gpio(buzzerPin);
cleanup_pwm(pwm_chip, pwm_number);
}

void* handle_train_sensors(void* arg) {
return NULL;
}

void* handle_crossing_guard(void* arg) {
return NULL;
}

void* handle_warning_lights(void* arg) {
return NULL;
}

void* handle_operator_intervention(void* arg) {

return NULL;
}

int main() {
   struct utsname sysInfo;
    uname(&sysInfo); // Get system information

    // Display machine name and node name
    printf("Machine name: %s\n", sysInfo.machine);
    printf("Node name: %s\n", sysInfo.nodename);

    // Additional information
    printf("Group 7\n");
    printf("Team members:\nHarjot Singh, Sai Nitesh, Shashank, Nishanth Varma, Venkateshwar Rao\n");
setup_signal_handler();
prompt_user_for_gpio_pins();
initialize_all_hardware();

pthread_t sensor_thread, crossing_guard_thread, warning_lights_thread, intervention_thread;
pthread_create(&sensor_thread, NULL, handle_train_sensors, NULL);
pthread_create(&crossing_guard_thread, NULL, handle_crossing_guard, NULL);
pthread_create(&warning_lights_thread, NULL, handle_warning_lights, NULL);
pthread_create(&intervention_thread, NULL, handle_operator_intervention, NULL);

// Join threads and perform cleanup
pthread_join(sensor_thread, NULL);
pthread_join(crossing_guard_thread, NULL);
pthread_join(warning_lights_thread, NULL);
pthread_join(intervention_thread, NULL);
cleanup_resources();

printf("Program has completed execution.\n");
return 0;
}
