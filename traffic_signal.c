//CS 692 Traffic Signal Assignment
// Harjot Singh, Sai Nitesh Bachupally, Shasank Kamineni, Nishanth Varma Kosuri, Venkateswara Rao Narra

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#define LED_PATH "/sys/class/gpio/gpio"
#define EXPORT_PATH "/sys/class/gpio/export"
#define UNEXPORT_PATH "/sys/class/gpio/unexport"

char green1[4], yellow1[4], red1[4], green2[4], yellow2[4], red2[4];

void writeToFile(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        perror("Unable to open file");
        exit(1);
    }
    if (write(fd, value, strlen(value)) == -1)
    {
        perror("Error writing to file");
        close(fd); // Close fd on error before exit
        exit(1);
    }
    close(fd);
}

void initPin(const char *pin)
{
    char path[35];
    sprintf(path, "%s%s/direction", LED_PATH, pin);
    writeToFile(EXPORT_PATH, pin);
    writeToFile(path, "out");
}

void setPinValue(const char *pin, const char *value)
{
    char path[35];
    sprintf(path, "%s%s/value", LED_PATH, pin);
    writeToFile(path, value);
}

void unexportPin(const char *pin)
{
    writeToFile(UNEXPORT_PATH, pin);
}

void cleanup()
{
    unexportPin(green1);
    unexportPin(yellow1);
    unexportPin(red1);
    unexportPin(green2);
    unexportPin(yellow2);
    unexportPin(red2);
}

void signalHandler(int sig)
{
    cleanup();
    printf("\nGPIO pins unexported. Exiting.\n");
    exit(0);
}

int main()
{
    // Register signal handler for SIGINT
    signal(SIGINT, signalHandler);

    printf("Enter GPIO pin for 1st green LED: ");
    scanf("%3s", green1);
    printf("Enter GPIO pin for 1st yellow LED: ");
    scanf("%3s", yellow1);
    printf("Enter GPIO pin for 1st red LED: ");
    scanf("%3s", red1);
    printf("Enter GPIO pin for 2nd green LED: ");
    scanf("%3s", green2);
    printf("Enter GPIO pin for 2nd yellow LED: ");
    scanf("%3s", yellow2);
    printf("Enter GPIO pin for 2nd red LED: ");
    scanf("%3s", red2);
    float greenDuration;
    printf("Enter duration in minutes for green light (can be fractional): ");
    scanf("%f", &greenDuration);

    // Convert minutes to seconds for sleep durations
    int greenTime = (int)(greenDuration * 60);

    // Initialize all the pins
    initPin(green1);
    initPin(yellow1);
    initPin(red1);
    initPin(green2);
    initPin(yellow2);
    initPin(red2);

    while (1)
    {
        // Set 1st signal to green and 2nd to red
        setPinValue(green1, "1");
        setPinValue(red1, "0");
        setPinValue(green2, "0");
        setPinValue(red2, "1");
        sleep(greenTime);

        // Transition 1st signal to yellow
        setPinValue(green1, "0");
        setPinValue(yellow1, "1");
        sleep(5);

        // Set 1st signal to red and prepare 2nd signal by turning yellow on while red is still on
        setPinValue(yellow1, "0");
        setPinValue(red1, "1");
        setPinValue(yellow2, "1"); // Turn on yellow along with red for 2nd signal
        sleep(2);

        // Now transition 2nd signal to green
        setPinValue(yellow2, "0");
        setPinValue(red2, "0");
        setPinValue(green2, "1");
        sleep(greenTime);

        // Transition 2nd signal to yellow
        setPinValue(green2, "0");
        setPinValue(yellow2, "1");
        sleep(5);

        // Set 2nd signal to red and prepare 1st signal by turning yellow on while red is still on
        setPinValue(yellow2, "0");
        setPinValue(red2, "1");
        setPinValue(yellow1, "1"); // Turn on yellow along with red for 1st signal
        sleep(2);

        // Now transition 1st signal to green
        setPinValue(yellow1, "0");
        setPinValue(red1, "0");
        setPinValue(green1, "1");
    }

    // Cleanup is handled by signalHandler
    return 0;
}
