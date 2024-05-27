#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <cstdlib>

speed_t getBaudRate(int baudRate) {
    switch (baudRate) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        default: return B115200;  // Default to 115200 if invalid
    }
}

int main(int argc, char *argv[]) {
    const char *serialPort = "/dev/serial0";  // Adjust if needed, e.g., "/dev/ttyAMA0"
    int baudRate = 115200;  // Default baud rate

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            baudRate = std::atoi(argv[++i]);
        }
    }

    int serial_fd = open(serialPort, O_RDWR | O_NOCTTY);
    if (serial_fd == -1) {
        std::cerr << "Failed to open serial port " << serialPort << ": " << strerror(errno) << std::endl;
        return 1;
    }

    // Configure serial port
    struct termios options;
    if (tcgetattr(serial_fd, &options) != 0) {
        std::cerr << "Error getting terminal attributes: " << strerror(errno) << std::endl;
        close(serial_fd);
        return 1;
    }

    speed_t speed = getBaudRate(baudRate);
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    options.c_cflag |= (CLOCAL | CREAD);  // Enable receiver and set local mode
    options.c_cflag &= ~PARENB;  // No parity
    options.c_cflag &= ~CSTOPB;  // 1 stop bit
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;  // 8 data bits
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  // Raw input mode
    options.c_iflag &= ~(IXON | IXOFF | IXANY);  // Disable software flow control
    options.c_oflag &= ~OPOST;  // Raw output mode

    // Apply the settings
    if (tcsetattr(serial_fd, TCSANOW, &options) != 0) {
        std::cerr << "Error setting terminal attributes: " << strerror(errno) << std::endl;
        close(serial_fd);
        return 1;
    }

    char buffer[256];
    int bytesRead;

    std::cout << "Reading from serial port at baud rate " << baudRate << "..." << std::endl;

    while (true) {
        bytesRead = read(serial_fd, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';  // Null-terminate the string
            // Remove carriage returns
            for (int i = 0; i < bytesRead; ++i) {
                if (buffer[i] != '\r') {
                    std::cout << buffer[i];
                }
            }
            std::cout.flush();
        } else if (bytesRead < 0) {
            std::cerr << "Error reading from serial port: " << strerror(errno) << std::endl;
            break;
        }
        usleep(100000);  // Sleep for 100 ms to avoid busy waiting
    }

    close(serial_fd);
    return 0;
}

