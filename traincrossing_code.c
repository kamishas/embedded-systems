#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

#define GPIO_EXPORT_PATH "/sys/class/gpio/export"
#define GPIO_UNEXPORT_PATH "/sys/class/gpio/unexport"
#define GPIO_DIRECTION_PATH "/sys/class/gpio/gpio%s/direction"
#define GPIO_VALUE_PATH "/sys/class/gpio/gpio%s/value"
#define TRAIN_COLLISION (1<<4) // Adjust as necessary
#define SERVO_OPEN "1000000"    // Adjust to your servo's specification
#define SERVO_CLOSED "2000000"  // Adjust to your servo's specification

// PWM Definitions
// Explained in the readme as well as the report, we had to hardcode the PWM chip and number to avoid errors.
// Checked that the above was okay with the Professor, as long as it is explained.
#define PWM_EXPORT_PATH "/sys/class/pwm/pwmchip4/export"
#define PWM_UNEXPORT_PATH "/sys/class/pwm/pwmchip4/unexport"
#define PWM_PERIOD_PATH "/sys/class/pwm/pwmchip4/pwm-4:0/period"
#define PWM_DUTY_CYCLE_PATH "/sys/class/pwm/pwmchip4/pwm-4:0/duty_cycle"
#define PWM_ENABLE_PATH "/sys/class/pwm/pwmchip4/pwm-4:0/enable"

// Train status flags
#define TRAIN_APPROACH_LEFT 1
#define TRAIN_APPROACH_RIGHT 2
#define TRAIN_LEAVE_LEFT 4
#define TRAIN_LEAVE_RIGHT 8

char sensor1Pin[4], sensor2Pin[4], sensor3Pin[4], sensor4Pin[4];
char led1Pin[4], led2Pin[4], buzzerPin[4];
int pwm_chip, pwm_number;

// Train status and mutex
volatile int trainStatus = 0;
volatile int collisionDetected = 0;
volatile int clearCollision = 0; // Operator can set this to clear the collision state
volatile int manualReset = 0; // Add this flag to indicate a manual reset is requested
pthread_mutex_t trainStatusMutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
void* handle_train_sensors(void* arg);
void* handle_crossing_guard(void* arg);
void* handle_warning_lights(void* arg);
void* handle_operator_intervention(void* arg);
void setup_signal_handler();
void prompt_user_for_gpio_pins();
void initialize_all_hardware();
void cleanup_resources();
void handle_sigint(int sig);
int read_sensor(const char* pin);
void initialize_gpio(const char* pin, const char* direction);
void cleanup_gpio(const char* pin);
void initialize_pwm();
void cleanup_pwm();
void write_to_file(const char* path, const char* value);
int is_gpio_exported(const char* pin);
void write_to_gpio(const char* pin, const char* value);
void set_pwm_duty_cycle(int pwm_chip, int pwm_number, const char *duty_cycle);
void set_servo_angle(int pwm_chip, int pwm_number, int angle);

int main() {
    // Setup signal handler, prompt user for GPIO pins, and initialize hardware
    setup_signal_handler();
    prompt_user_for_gpio_pins();
    initialize_all_hardware();

    // Create threads for various components of the railway crossing system
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

    return 0;
}

// Function to handle train sensors
void* handle_train_sensors(void* arg) {
    int lastSensorState[4] = {0};
    int sequenceStage = 0; // Tracks the current stage of the sensor sequence

    while (1) {
        pthread_mutex_lock(&trainStatusMutex);

        if (manualReset) {
            pthread_mutex_unlock(&trainStatusMutex);
            usleep(100000); // Wait before checking again
            continue;
        }

        int currentSensorState[4] = {
            read_sensor(sensor1Pin),
            read_sensor(sensor2Pin),
            read_sensor(sensor3Pin),
            read_sensor(sensor4Pin)
        };

        for (int i = 0; i < 4; ++i) {
            if (currentSensorState[i] != lastSensorState[i] && currentSensorState[i] == 1) {
                printf("Sensor %d activated.\n", i + 1);

                // Handle sequence logic
                switch (sequenceStage) {
                    case 0: // No sensors have been activated yet
                        if (i == 0) sequenceStage = 1; // Start of left-to-right
                        else if (i == 2) sequenceStage = 3; // Start of right-to-left
                        else collisionDetected = 1; // Any other sensor first is a collision
                        break;
                    case 1: // Left-to-right, Sensor 1 activated
                        if (i == 1) sequenceStage = 2; // Sensor 2 follows Sensor 1
                        else collisionDetected = 1;
                        break;
                    case 2: // Left-to-right, Sensors 1 and 2 activated
                        if (i == 3) sequenceStage = 7; // Sensor 4 follows Sensors 1 and 2
                        else collisionDetected = 1;
                        break;
                    case 3: // Right-to-left, Sensor 3 activated
                        if (i == 3) sequenceStage = 4; // Sensor 4 follows Sensor 3
                        else collisionDetected = 1;
                        break;
                    case 4: // Right-to-left, Sensors 3 and 4 activated
                        if (i == 1) sequenceStage = 8; // Sensor 2 follows Sensors 3 and 4
                        else collisionDetected = 1;
                        break;
                    case 7: // Left-to-right, Sensors 1, 2, and 4 activated
                        if (i == 2) sequenceStage = 10; // Complete sequence with Sensor 3 last
                        else collisionDetected = 1;
                        break;
                    case 8: // Right-to-left, Sensors 3, 4, and 2 activated
                        if (i == 0) sequenceStage = 9; // Complete sequence with Sensor 1 last
                        else collisionDetected = 1;
                        break;
                }

                if (collisionDetected) {
                    printf("Collision detected.\n");
                    trainStatus = 0;
                    clearCollision = 0;
                    sequenceStage = 0;
                    collisionDetected = 1; // Reset for next iteration
                } else if (sequenceStage == 9 || sequenceStage == 10) {
                    printf("Full sequence completed. Crossing guard up, LEDs stopped flashing.\n");
                    clearCollision = 1;
                    trainStatus = 0;
                    sequenceStage = 0; // Reset for next train
                } else if (sequenceStage == 2 || sequenceStage == 4) {
                    printf("Half sequence completed. Crossing guard down, LEDs flashing.\n");
                    // Do not change trainStatus here to keep guard down and LEDs flashing
                    trainStatus |= (sequenceStage == 2) ? TRAIN_APPROACH_LEFT : TRAIN_APPROACH_RIGHT;
                }
            }
        }

        memcpy(lastSensorState, currentSensorState, sizeof(lastSensorState));

        pthread_mutex_unlock(&trainStatusMutex);
        usleep(100000); // Sleep to reduce CPU usage
    }
    return NULL;
}


// Function to handle the crossing guard
void* handle_crossing_guard(void* arg) {
    while (1) {
        pthread_mutex_lock(&trainStatusMutex);
        if (trainStatus & (TRAIN_APPROACH_LEFT | TRAIN_APPROACH_RIGHT)) {
            // Close the servo arm to block the crossing
            set_servo_angle(pwm_chip, pwm_number, 90); // Assume 90 degrees is closed
        } else if (trainStatus == 0) {
            // Open the servo arm to unblock the crossing after a 1-second delay
            usleep(1000000); // Sleep for 1 second
            set_servo_angle(pwm_chip, pwm_number, 180); // Assume 180 degrees is open
        }
        pthread_mutex_unlock(&trainStatusMutex);
        usleep(10000); // Check every 10 milliseconds
    }
    return NULL;
}

// Function to handle warning lights
void* handle_warning_lights(void* arg) {
    int led_state = 0; // To toggle between LEDs

    while (1) {
        pthread_mutex_lock(&trainStatusMutex);

        // Collision detected behavior
        if (collisionDetected) {
            // Activate buzzer and keep it on during the collision indication
            write_to_gpio(buzzerPin, "1"); // Activate buzzer
            // Adjust the loop for a distinct and faster flashing pattern during collision
            for (int i = 0; i < 10; i++) { // Increase the number of flashes
                write_to_gpio(led1Pin, "1");
                write_to_gpio(led2Pin, "1");
                usleep(100000); // Shorten the time for a faster flash
                write_to_gpio(led1Pin, "0");
                write_to_gpio(led2Pin, "0");
                usleep(100000); // Shorten the pause between flashes
            }
            // After the collision sequence, turn off the buzzer and LEDs
            write_to_gpio(buzzerPin, "0"); // Deactivate buzzer
            write_to_gpio(led1Pin, "0"); // Ensure LEDs are off
            write_to_gpio(led2Pin, "0");
        } else {
            // Normal operation for warning lights during train approach
            if (trainStatus & (TRAIN_APPROACH_LEFT | TRAIN_APPROACH_RIGHT)) {
                // Alternate flashing LEDs to indicate train approach
                led_state = !led_state;
                write_to_gpio(led1Pin, led_state ? "1" : "0");
                write_to_gpio(led2Pin, led_state ? "0" : "1");
                usleep(500000); // Standard rate for alternating flash
            } else {
                // No train detected or after reset - LEDs should be off
                write_to_gpio(led1Pin, "0");
                write_to_gpio(led2Pin, "0");
                write_to_gpio(buzzerPin, "0"); // Ensure buzzer is off
            }
        }

        pthread_mutex_unlock(&trainStatusMutex);
        usleep(100000); // Brief pause before checking the state again
    }
    return NULL; // This return statement is never reached
}

// Function to handle operator intervention
void* handle_operator_intervention(void* arg) {
    char userInput;
    while (1) {
        if (collisionDetected) {
            printf("\nCollision detected! Press 'r' to reset: ");
            scanf(" %c", &userInput);
            if (userInput == 'r') {
                pthread_mutex_lock(&trainStatusMutex);
                collisionDetected = 0; // Clear collision state
                trainStatus = 0; // Clear train status flags
                manualReset = 1; // Indicate a manual reset
                printf("Manual reset initiated. Please wait...\n");
                pthread_mutex_unlock(&trainStatusMutex);

                // Wait a bit to ensure other parts of the system can react to the reset
                usleep(500000); // Adjust timing as needed for your system

                pthread_mutex_lock(&trainStatusMutex);
                clearCollision = 1; // Signal to reset other components, if needed
                manualReset = 0; // Reset completed, clear manual reset flag
                pthread_mutex_unlock(&trainStatusMutex);
                
                printf("System reset. Resuming normal operation...\n");
            }
        }
        // Reduce CPU usage when not handling a collision
        usleep(1000000);
    }
    return NULL;
}

// Function to setup signal handler for SIGINT
void setup_signal_handler() {
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
}

// Function to prompt the user for GPIO pins
void prompt_user_for_gpio_pins() {
    printf("Enter GPIO pin  number for sensor 1 (train approach from left): ");
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

// Function to initialize all hardware components
void initialize_all_hardware() {
    // Initialize sensors as inputs
    initialize_gpio(sensor1Pin, "in");
    initialize_gpio(sensor2Pin, "in");
    initialize_gpio(sensor3Pin, "in");
    initialize_gpio(sensor4Pin, "in");
    
    // Initialize LEDs and buzzer as outputs
    initialize_gpio(led1Pin, "out");
    initialize_gpio(led2Pin, "out");
    initialize_gpio(buzzerPin, "out");
    
    // Initialize PWM for servo control
    initialize_pwm(pwm_chip, pwm_number);
    
    // Set the PWM period for the servo's expected frequency (usually 50Hz for servos)
    char buffer[64];
    strcpy(buffer, PWM_PERIOD_PATH);

    write_to_file(buffer, "20000000"); // Period of 20ms
    
    // Start the PWM signal
    snprintf(buffer, sizeof(buffer), PWM_ENABLE_PATH, pwm_chip, pwm_number);
    write_to_file(buffer, "1");
    
    // Set the servo to an initial position, if needed
    set_servo_angle(pwm_chip, pwm_number, 180); // Initial position at 180 degrees
}


// Function to perform cleanup of resources
void cleanup_resources() {
    // Clean up all GPIOs
    cleanup_gpio(sensor1Pin);
    cleanup_gpio(sensor2Pin);
    cleanup_gpio(sensor3Pin);
    cleanup_gpio(sensor4Pin);
    cleanup_gpio(led1Pin);
    cleanup_gpio(led2Pin);
    cleanup_gpio(buzzerPin);
    
    // Clean up PWM
    cleanup_pwm(pwm_chip, pwm_number);
}


// Signal handler function for SIGINT
void handle_sigint(int sig) {
    printf("SIGINT received, cleaning up...\n");
    cleanup_resources();
    cleanup_pwm(pwm_chip, pwm_number);
    exit(EXIT_SUCCESS);
}

// Function to read from a sensor
int read_sensor(const char* pin) {
    char path[64], value_str[3];
    snprintf(path, sizeof(path), GPIO_VALUE_PATH, pin);
    //printf("Reading sensor value from GPIO pin %s\n", pin); // Log reading action
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Unable to open GPIO value file %s: %s\n", path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (read(fd, value_str, sizeof(value_str) - 1) == -1) {
        fprintf(stderr, "Error reading GPIO value file %s: %s\n", path, strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }
    value_str[2] = '\0'; // Ensure null-termination
    close(fd);
    return atoi(value_str);
}

// Function to check if a GPIO pin is already exported
int is_gpio_exported(const char* pin) {
    char path[64];
    struct stat statbuf;
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%s", pin);
    // Use stat() to check if the directory exists
    return stat(path, &statbuf) == 0;
}

int is_pwm_exported(int pwm_chip, int pwm_number) {
    char path[64];
    // Construct the path to check if the PWM device is already exported
    snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm%d", pwm_chip, pwm_number);
    struct stat statbuf;
    // Use stat to check if the directory exists
    if (stat(path, &statbuf) == 0) {
        // The directory exists, meaning the PWM device is already exported
        return 1; // True - it's exported
    } else {
        // The directory does not exist, PWM device is not exported
        return 0; // False - not exported
    }
}


// Function to initialize a GPIO pin
void initialize_gpio(const char* pin, const char* direction) {
    printf("Initializing GPIO pin %s as %s\n", pin, direction); // Log the action
    if (!is_gpio_exported(pin)) {
        printf("Exporting GPIO pin %s\n", pin); // Log exporting action
        write_to_file(GPIO_EXPORT_PATH, pin);
    } else {
        printf("GPIO pin %s already exported\n", pin); // Log already exported
    }

    char direction_path[64];
    snprintf(direction_path, sizeof(direction_path), GPIO_DIRECTION_PATH, pin);
    printf("Setting GPIO pin %s direction to %s\n", pin, direction); // Log direction setting
    write_to_file(direction_path, direction);
}

// Function to cleanup a GPIO pin
void cleanup_gpio(const char* pin) {
    printf("Cleaning up GPIO pin %s\n", pin); // Log cleanup action
    write_to_file(GPIO_UNEXPORT_PATH, pin);
}

// Function to initialize a PWM signal
void initialize_pwm(int pwm_chip, int pwm_number) {
    printf("Initializing PWM on chip %d, number %d\n", pwm_chip, pwm_number); // Log PWM initialization
    char buffer[64];

    snprintf(buffer, sizeof(buffer), PWM_PERIOD_PATH, pwm_chip, pwm_number);
    write_to_file(buffer, "20000000"); // Set to 20ms for 50Hz

    // Disable PWM before setting duty cycle
    snprintf(buffer, sizeof(buffer), PWM_ENABLE_PATH, pwm_chip, pwm_number);
    write_to_file(buffer, "0");

    // Set PWM duty cycle
    snprintf(buffer, sizeof(buffer), PWM_DUTY_CYCLE_PATH, pwm_chip, pwm_number);
    write_to_file(buffer, "1500000"); // Set to some initial duty cycle

    // Enable PWM after setting duty cycle
    snprintf(buffer, sizeof(buffer), PWM_ENABLE_PATH, pwm_chip, pwm_number);
    write_to_file(buffer, "1");
}


// Function to cleanup a PWM signal
void cleanup_pwm(int pwm_chip, int pwm_number) {
    printf("Cleaning up PWM on chip %d, number %d\n", pwm_chip, pwm_number); // Log cleanup action
    // Disable PWM
    write_to_file(PWM_ENABLE_PATH, "0");

    // Unexport the PWM
    write_to_file(PWM_UNEXPORT_PATH, "0");
}

void write_to_file(const char* path, const char* value) {
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Unable to open file %s for writing: %s\n", path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (write(fd, value, strlen(value)) == -1) {
        fprintf(stderr, "Error writing to file %s: %s\n", path, strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
}

void write_to_gpio(const char* pin, const char* value) {
    char path[64];
    // Construct the path to the GPIO value file
    snprintf(path, sizeof(path), GPIO_VALUE_PATH, pin);
    
    // Use the write_to_file function to write the value to the GPIO value file
    write_to_file(path, value);
}

// Function to set the PWM duty cycle for the servo
void set_pwm_duty_cycle(int pwm_chip, int pwm_number, const char *duty_cycle) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), PWM_DUTY_CYCLE_PATH, pwm_chip, pwm_number);
    write_to_file(buffer, duty_cycle);
}

// Function that calculates the duty cycle based on the angle for the servo
void set_servo_angle(int pwm_chip, int pwm_number, int angle) {
    // Assuming a servo that operates between 0 (1ms) and 180 (2ms) degrees
    // Servo's period is generally 20ms (20000000ns)
    long duty_cycle_ns = 1000000 + (angle * 1000000 / 180);
    char duty_cycle_str[16];
    snprintf(duty_cycle_str, sizeof(duty_cycle_str), "%ld", duty_cycle_ns);
    set_pwm_duty_cycle(pwm_chip, pwm_number, duty_cycle_str);
}
