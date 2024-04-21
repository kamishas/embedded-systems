# BeagleBone Real-Time Stopwatch Application

This project is a real-time embedded application designed for the BeagleBone platform that implements a stopwatch with start/stop and reset functionalities. The application utilizes POSIX threads with assigned priorities to ensure responsive user inputs and accurate timekeeping, along with visual feedback through LED indicators.

## Table of Contents
- [Project Overview](#project-overview)
- [Getting Started](#getting-started)
- [Prerequisites](#prerequisites)
- [Hardware Setup](#hardware-setup)
- [Compilation Steps](#compilation-steps)
- [Running the Application](#running-the-application)
- [RTOS and Priority Preemption](#rtos-and-priority-preemption)
- [Contributing](#contributing)
- [Authors](#authors)
- [License](#license)
- [Acknowledgments](#acknowledgments)
- [Additional Resources](#additional-resources)
- [Contact](#contact)

## Project Overview

The application is structured into four main threads, each with a specific role and assigned priority to ensure the system is both responsive and reliable:

1. **Button Listener Thread**: Interprets start/stop and reset button presses.
2. **Stopwatch Counter Thread**: Manages the time counting mechanism.
3. **LED Controller Thread**: Manages LED state to indicate stopwatch status.
4. **Display Update Thread**: Updates the display with the current time.

## Getting Started

This section outlines the steps needed to set up and run the stopwatch application on a BeagleBone device.

### Prerequisites

- BeagleBone Black with Debian OS.
- Development tools like `gcc` installed on the BeagleBone.
- Hardware components such as buttons and LEDs connected to the BeagleBone.

### Hardware Setup

Ensure the buttons and LEDs are connected to the appropriate GPIO pins as per the source code. The exact pin numbers will be entered during the execution of the program.

### Compilation Steps

To compile the stopwatch application, navigate to the project directory and run the following command in your terminal:

```bash
gcc -o stopwatch stopwatch.c -lpthread

sudo ./stopwatch


RTOS and Priority Preemption
Priority assignments for the threads are as follows:

Button Listener Thread (Priority: 99)
Stopwatch Counter Thread (Priority: 70)
LED Controller Thread (Priority: 50)
Display Update Thread (Priority: 30)
These priorities ensure that user input is highly responsive, the timing mechanism is accurate, and the user interface updates do not interfere with more critical operations.

For a detailed explanation of the priority choices and scheduling, refer to the RTOS2 - Priority Thread Documentation.pdf.
