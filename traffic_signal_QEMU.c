#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/utsname.h>

char green1[4], yellow1[4], red1[4], green2[4], yellow2[4], red2[4];

void writeToFile(const char *path, const char *value) {
    printf("Simulated: Writing '%s' to %s\n", value, path);
}

void initPin(const char *pin) {
    printf("Simulated: Initializing GPIO pin %s as output\n", pin); // Simulate initializing pin
}

void setPinValue(const char *pin, const char *value) {
    if (strcmp(value, "1") == 0) {
        printf("Simulated: Setting GPIO pin %s to HIGH\n", pin);
    } else {
        printf("Simulated: Setting GPIO pin %s to LOW\n", pin);
    }
}

void unexportPin(const char *pin) {
    printf("Simulated: Unexporting GPIO pin %s\n", pin); // Simulate unexporting pin
}

void cleanup() {
    // Placeholder for GPIO pin names
    char *pins[] = {green1, yellow1, red1, green2, yellow2, red2};
    for(int i = 0; i < 6; i++) {
        unexportPin(pins[i]);
    }
}

void signalHandler(int sig) {
    cleanup();
    printf("\nSimulated: GPIO pins 'unexported'. Exiting.\n");
    exit(0);
}

// Define pin variables globally
char green1[4], yellow1[4], red1[4], green2[4], yellow2[4], red2[4];

int main() {
    // Register signal handler for SIGINT
    signal(SIGINT, signalHandler);
    struct utsname unameData;
    uname(&unameData); // Get system information
    printf("Machine name: %s\n", unameData.nodename); // Display machine name
    // Collect user inputs for GPIO pin numbers and duration
    printf("Group 7 \n");
    printf("Team members: \n Harjot Singh, Sai Nitesh, Shashank, Nishanth Varma, Venkateshwar Rao \n");
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

    // Simulate initializing all the pins
    initPin(green1);
    initPin(yellow1);
    initPin(red1);
    initPin(green2);
    initPin(yellow2);
    initPin(red2);

    // Simulate one cycle of traffic light changes
    while (1) {
        // Simulate setting 1st signal to green and 2nd to red
        setPinValue(green1, "1");
        setPinValue(red1, "0");
        setPinValue(green2, "0");
        setPinValue(red2, "1");
        sleep(greenTime);

        // Simulate transition 1st signal to yellow
        setPinValue(green1, "0");
        setPinValue(yellow1, "1");
        sleep(5);

        // Simulate setting 1st signal to red and preparing 2nd signal
        setPinValue(yellow1, "0");
        setPinValue(red1, "1");
        setPinValue(yellow2, "1");
        sleep(2);

        // Simulate transitioning 2nd signal to green
        setPinValue(yellow2, "0");
        setPinValue(red2, "0");
        setPinValue(green2, "1");
        sleep(greenTime);

        // Simulate transition 2nd signal to yellow
        setPinValue(green2, "0");
        setPinValue(yellow2, "1");
        sleep(5);

        // Simulate setting 2nd signal to red and preparing 1st signal
        setPinValue(yellow2, "0");
        setPinValue(red2, "1");
        setPinValue(yellow1, "1");
        sleep(2);

        // Now transition 1st signal to green again
        setPinValue(yellow1, "0");
        setPinValue(red1, "0");
        setPinValue(green1, "1");
    }

    // Cleanup is handled by signalHandler
    return 0;
}
