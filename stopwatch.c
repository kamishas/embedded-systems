#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sched.h>
#include <errno.h>  
#include <float.h>
#include <sys/types.h>
#include <time.h>
#include <sys/poll.h>
#include <signal.h>

// Define paths for GPIO export and unexport, and pins
#define GPIO_EXPORT_PATH "/sys/class/gpio/export"
#define GPIO_UNEXPORT_PATH "/sys/class/gpio/unexport"
#define BUTTON_START_STOP_PATH "/sys/class/gpio/gpio%s/value" //27
#define BUTTON_RESET_PATH "/sys/class/gpio/gpio%s/value" //66
#define LED_RED_PATH "/sys/class/gpio/gpio%s/value" //65
#define LED_GREEN_PATH "/sys/class/gpio/gpio%s/value" //67

// Function Declarations
void *buttonListener(void *arg);
void *stopwatchCounter(void *arg);
void *ledController(void *arg);
void *displayUpdate(void *arg);
void writeGPIO(const char *path, const char *value);
int readGPIO(const char *path, char *value, size_t size);
bool checkIfPinExported(const char *pin);
void exportPin(const char *pin);
void unexportPin(const char *pin);
void setPinEdgeDetection(const char *pin, const char *edge);
void setPinDirection(const char *pin, const char *direction);
void promptForPins();
void setupThreadWithPriority(pthread_t *thread, void *(*start_routine) (void *), int priority);
void sigintHandler(int sig_num);

// Global Variables for Stopwatch State
volatile bool stopwatchRunning = false;
volatile double stopwatchTime = 0.0;
pthread_mutex_t lock;

// GPIO pin numbers
char startStopButtonPin[4], resetButtonPin[4], redLEDPin[4], greenLEDPin[4];

int main() {
    // Register the signal handler for SIGINT
    signal(SIGINT, sigintHandler);
    // Explicitly set the initial state
    stopwatchRunning = false;
    stopwatchTime = 0.0;

    pthread_t threads[4];

    promptForPins();
    pthread_mutex_init(&lock, NULL);
    
    setupThreadWithPriority(&threads[0], buttonListener, 99);  // Highest priority
    setupThreadWithPriority(&threads[1], stopwatchCounter, 70);
    setupThreadWithPriority(&threads[2], ledController, 50);
    setupThreadWithPriority(&threads[3], displayUpdate, 30);   // Lowest priority

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&lock);
    return 0;
}

void promptForPins() {
    printf("Enter START/STOP Button GPIO pin number: ");
    scanf("%3s", startStopButtonPin);
    printf("Enter RESET Button GPIO pin number: ");
    scanf("%3s", resetButtonPin);
    printf("Enter Red LED GPIO pin number: ");
    scanf("%3s", redLEDPin);
    printf("Enter Green LED GPIO pin number: ");
    scanf("%3s", greenLEDPin);

    exportPin(startStopButtonPin);
    setPinDirection(startStopButtonPin, "in");
    setPinEdgeDetection(startStopButtonPin, "both"); // Set edge detection for start/stop button

    exportPin(resetButtonPin);
    setPinDirection(resetButtonPin, "in");
    setPinEdgeDetection(resetButtonPin, "both"); // Set edge detection for reset button

    exportPin(redLEDPin);
    setPinDirection(redLEDPin, "out");

    exportPin(greenLEDPin);
    setPinDirection(greenLEDPin, "out");
}

bool checkIfPinExported(const char *pin) {
    char path[35];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%s", pin);
    struct stat statbuf;
    return stat(path, &statbuf) == 0;
}

void exportPin(const char *pin) {
    if (!checkIfPinExported(pin)) {
        writeGPIO(GPIO_EXPORT_PATH, pin);
    }
}

void unexportPin(const char *pin) {
    if (checkIfPinExported(pin)) { // Make sure the pin is exported before attempting to unexport
        writeGPIO(GPIO_UNEXPORT_PATH, pin);
    }
}

void setPinEdgeDetection(const char *pin, const char *edge) {
    char path[50];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%s/edge", pin);
    writeGPIO(path, edge);
}

void writeGPIO(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("Unable to open GPIO path");
        exit(EXIT_FAILURE);
    }
    if (write(fd, value, strlen(value)) != (ssize_t)strlen(value)) {
        perror("Failed to write to GPIO path");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
}

int readGPIO(const char *path, char *value, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("Unable to open GPIO path for reading");
        return -1; // Return an error indicator
    }

    ssize_t bytesRead = read(fd, value, size);
    if (bytesRead == -1) {
        perror("Failed to read from GPIO path");
        close(fd);
        return -1;
    }

    close(fd);
    return 0; // Success
}

void setPinDirection(const char *pin, const char *direction) {
    char path[40];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%s/direction", pin);
    writeGPIO(path, direction);
}

void *buttonListener(void *arg) {
    char startStopPath[40], resetPath[40], value[2] = {'0', '\0'};
    int startStopFd, resetFd, ret;

    // Format the paths
    snprintf(startStopPath, sizeof(startStopPath), BUTTON_START_STOP_PATH, startStopButtonPin);
    snprintf(resetPath, sizeof(resetPath), BUTTON_RESET_PATH, resetButtonPin);

    // Open the files for reading button states
    startStopFd = open(startStopPath, O_RDONLY | O_NONBLOCK);
    resetFd = open(resetPath, O_RDONLY | O_NONBLOCK);

    if (startStopFd < 0 || resetFd < 0) {
        perror("Failed to open GPIO value file");
        return NULL;
    }

    struct pollfd fds[2] = {
        {startStopFd, POLLPRI, 0},
        {resetFd, POLLPRI, 0}
    };

    while (1) {
        // Dummy reads to clear any initial conditions
        pread(startStopFd, value, 1, 0);
        pread(resetFd, value, 1, 0);
        int timeout = 10;

        ret = poll(fds, 2, timeout); // 10ms timeout for poll (checking button input every 10ms)
        if (ret > 0) {
            if (fds[0].revents & POLLPRI) {
                pread(fds[0].fd, value, 1, 0); // Read to clear the event
                pthread_mutex_lock(&lock);
                stopwatchRunning = !stopwatchRunning;
                pthread_mutex_unlock(&lock);
                usleep(200000); // Debounce delay
            }
            if (fds[1].revents & POLLPRI) {
                pread(fds[1].fd, value, 1, 0); // Read to clear the event
                pthread_mutex_lock(&lock);
                stopwatchTime = 0.0;
                pthread_mutex_unlock(&lock);
                usleep(200000); // Debounce delay
            }
        }
    }

    close(startStopFd);
    close(resetFd);
    return NULL;
}

void *stopwatchCounter(void *arg) {
    while (1) {
        if (stopwatchRunning) {
            pthread_mutex_lock(&lock);
            stopwatchTime += 0.01; // Increment by 10ms
            if (stopwatchTime >= DBL_MAX) {
                stopwatchTime = 0.0; // Reset if approaching DBL_MAX
            }
            pthread_mutex_unlock(&lock);
        }
        usleep(10000); // 10ms pause
    }
    return NULL;
}


void *ledController(void *arg) {
    char redLEDPath[40], greenLEDPath[40];
    snprintf(redLEDPath, sizeof(redLEDPath), "/sys/class/gpio/gpio%s/value", redLEDPin);
    snprintf(greenLEDPath, sizeof(greenLEDPath), "/sys/class/gpio/gpio%s/value", greenLEDPin);

    while (1) {
        pthread_mutex_lock(&lock);
        if (stopwatchRunning) {
            writeGPIO(redLEDPath, "0");  // Turn off the red LED
            writeGPIO(greenLEDPath, "1"); // Turn on the green LED
        } else {
            writeGPIO(redLEDPath, "1");  // Turn on the red LED
            writeGPIO(greenLEDPath, "0"); // Turn off the green LED
        }
        pthread_mutex_unlock(&lock);
        usleep(100000); // Sleep for 100ms
    }
}

void *displayUpdate(void *arg) {
    while (1) {
        pthread_mutex_lock(&lock);
        if (stopwatchRunning) {
            // Running: Display with a resolution of 100ms
            printf("\rStopwatch Time: %3.3f seconds ", stopwatchTime);
        } else {
            // Stopped: Display with a resolution of 10ms
            printf("\rStopwatch Time: %3.2f seconds ", stopwatchTime);
        }
        fflush(stdout);
        pthread_mutex_unlock(&lock);
        usleep(100000); // Refresh rate of 100ms to match display requirement
    }
    return NULL;
}

void setupThreadWithPriority(pthread_t *thread, void *(*start_routine) (void *), int priority) {
    pthread_attr_t attr;
    struct sched_param param;
    int ret;

    // Initialize thread attributes
    pthread_attr_init(&attr);

    // Set the scheduling policy to FIFO
    ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    if (ret != 0) {
        fprintf(stderr, "Failed to set thread scheduling policy: %s\n", strerror(ret));
        exit(EXIT_FAILURE);
    }

    // Set the thread priority
    param.sched_priority = priority;
    ret = pthread_attr_setschedparam(&attr, &param);
    if (ret != 0) {
        fprintf(stderr, "Failed to set thread priority: %s\n", strerror(ret));
        exit(EXIT_FAILURE);
    }

    // Ensure the attribute is actually applied
    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if (ret != 0) {
        fprintf(stderr, "Failed to set inherit scheduler attribute: %s\n", strerror(ret));
        exit(EXIT_FAILURE);
    }

    // Create the thread with the specified attributes
    ret = pthread_create(thread, &attr, start_routine, NULL);
    if (ret != 0) {
        fprintf(stderr, "Failed to create thread: %s\n", strerror(ret));
        exit(EXIT_FAILURE);
    }

    // Destroy the thread attributes object since it is no longer needed
    pthread_attr_destroy(&attr);
}

void sigintHandler(int sig_num) {
    printf("\nCaught SIGINT (Ctrl-C). Cleaning up and exiting...\n");
    // Turn off the LEDs before exiting
    char redLEDPath[40], greenLEDPath[40];
    snprintf(redLEDPath, sizeof(redLEDPath), "/sys/class/gpio/gpio%s/value", redLEDPin);
    snprintf(greenLEDPath, sizeof(greenLEDPath), "/sys/class/gpio/gpio%s/value", greenLEDPin);
    writeGPIO(redLEDPath, "0");
    writeGPIO(greenLEDPath, "0");
    //unexportPin(redLEDPin); Commented out due to unexpected error when unexporting
    //unexportPin(greenLEDPin);
    //unexportPin()
    // Exit the program
    exit(0);
}
