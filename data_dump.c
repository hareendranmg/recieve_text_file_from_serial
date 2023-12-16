#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>

#define BUFFER_SIZE 1024

volatile sig_atomic_t stop_flag = 0;

void handle_signal(int signum) {
    printf("Caught signal %d\n", signum);
    if (signum == SIGINT || signum == SIGTERM) {
        printf("Stopping...\n");
        stop_flag = 1;
        printf("Stopped\n");
    }
    printf("Exiting signal handler\n");
}

void cleanup(int serial_fd, FILE *outputFile) {
    printf("Cleaning up...\n");
    close(serial_fd);
    fclose(outputFile);
    printf("Cleaned up\n");
}

int main(int argc, char *argv[]) {
    // print argc and argv
    printf("argc: %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d]: %s\n", i, argv[i]);
    }
    if (argc != 7) {
        printf("Usage: %s <serial_port> <output_file> <baud_rate> <parity> <data_bits> <stop_bits>\n");
        return EXIT_FAILURE;
    }

    const char *serialPortName = argv[1];
    const char *outputFileName = argv[2];
    int baudRate = atoi(argv[3]);
    char parity = argv[4][0]; // Assuming the user provides a single character for parity ('N', 'E', or 'O')
    int dataBits = atoi(argv[5]);
    int stopBits = atoi(argv[6]);

    // Open the serial port
    int serial_fd = open(serialPortName, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (serial_fd == -1) {
        printf("Error opening serial port");
        return EXIT_FAILURE;
    }


    // close serial port and again open 
    close(serial_fd);
    serial_fd = open(serialPortName, O_RDWR | O_NOCTTY | O_NONBLOCK);

    printf("Opened serial port %s\n", serialPortName);

    // Set serial port parameters
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(serial_fd, &tty) != 0) {
        printf("Error getting serial port attributes");
        close(serial_fd);
        return EXIT_FAILURE;
    }
    
    cfsetospeed(&tty, baudRate);
    cfsetispeed(&tty, baudRate);

    // Set the parameters based on command-line arguments
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;

    // Set parity
    if (parity == 'N') {
        tty.c_cflag &= ~PARODD; // No parity
    } else if (parity == 'E') {
        tty.c_cflag |= PARODD; // Even parity
    } else if (parity == 'O') {
        tty.c_cflag |= (PARENB | PARODD); // Odd parity
    }

    // Set data bits
    tty.c_cflag &= ~CSIZE;
    if (dataBits == 8) {
        tty.c_cflag |= CS8;
    } else if (dataBits == 7) {
        tty.c_cflag |= CS7;
    } else {
        printf("Invalid number of data bits\n");
        close(serial_fd);
        return EXIT_FAILURE;
    }

    // Set stop bits
    if (stopBits == 1) {
        tty.c_cflag &= ~CSTOPB; // 1 stop bit
    } else if (stopBits == 2) {
        tty.c_cflag |= CSTOPB; // 2 stop bits
    } else {
        printf("Invalid number of stop bits\n");
        close(serial_fd);
        return EXIT_FAILURE;
    }

    if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
        printf("Error setting serial port attributes");
        close(serial_fd);
        return EXIT_FAILURE;
    }

    // Open the output file
    FILE *outputFile = fopen(outputFileName, "w");
    if (!outputFile) {
        printf("Error opening output file");
        close(serial_fd);
        return EXIT_FAILURE;
    }

    // Register signal handler for graceful termination
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    volatile __uint8_t new_event = 0;
    volatile __uint32_t cnt = 0;
    char buffer[BUFFER_SIZE];
    // Main loop for reading from the serial port and writing to the file

    printf("starting...\n");
    while (!stop_flag) {

        ssize_t bytesRead = read(serial_fd, buffer, sizeof(buffer)); //

        if (bytesRead > 0)
        {
            new_event = 1;  
           fwrite(buffer, 1, bytesRead, outputFile);
            fflush(outputFile);
        }        
    }

    printf("Stopping...\n");

    // Cleanup and close resources before exiting
    cleanup(serial_fd, outputFile);

    return EXIT_SUCCESS;
}
