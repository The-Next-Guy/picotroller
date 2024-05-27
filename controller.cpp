#include "controller.h"
#include <iostream>
#include <cstdio>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <sys/reboot.h>

#define SELECT_BUTTON 6
#define START_BUTTON 5

#define L_BUTTON 7
#define R_BUTTON 4
#define A_BUTTON 1
#define B_BUTTON 2
#define X_BUTTON 0
#define Y_BUTTON 3

#define STATUS_TIMEOUT 100

using String = std::string;

ControllerManager::ControllerManager(JoyCallback callback) {
	setup_uinput_device(0);
    //for(int i = 0; i < 4; ++i) {
    //    setup_uinput_device(i + 1);  // Joystick IDs 1 to 4
	//	controllers[i].id = i;
    //}
	this->callback = callback;
}

ControllerManager::~ControllerManager() {
    for(int i = 0; i < fd_count; ++i) {
        ioctl(fds[i], UI_DEV_DESTROY);
        close(fds[i]);
    }
	
    close(serial_fd);
}

void ControllerManager::setup_uinput_device(int joystick_id) {
    struct uinput_setup usetup;
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if(fd < 0) {
        perror("Unable to open /dev/uinput");
        exit(EXIT_FAILURE);
    }

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
	
    ioctl(fd, UI_SET_KEYBIT, BTN_A);
    ioctl(fd, UI_SET_KEYBIT, BTN_B);
    ioctl(fd, UI_SET_KEYBIT, BTN_X);
    ioctl(fd, UI_SET_KEYBIT, BTN_Y);
    ioctl(fd, UI_SET_KEYBIT, BTN_START);
    ioctl(fd, UI_SET_KEYBIT, BTN_SELECT);
    ioctl(fd, UI_SET_KEYBIT, BTN_TL);
    ioctl(fd, UI_SET_KEYBIT, BTN_TR);
	
    ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_UP);
    ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_DOWN);
    ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_LEFT);
    ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_RIGHT);

    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x5348; // Sample Vendor
    usetup.id.product = 0x0100 + joystick_id; // Unique product ID for each joystick
    snprintf(usetup.name, UINPUT_MAX_NAME_SIZE, "Xemplar PicoTroller %d", joystick_id);

    ioctl(fd, UI_DEV_SETUP, &usetup);
    ioctl(fd, UI_DEV_CREATE);

    fds[fd_count++] = fd;
}

int ControllerManager::initConnection(std::string port) {
	serial_fd = openSerialPort(port.c_str());
	if(serial_fd == -1) {
        return 1;
    }
	
	statusUpdate();
	
	return 0;
}


void ControllerManager::loop() {
	while(true) {
        readSerial();
		if(update_counter == 0) statusUpdate(); 
		
		update_counter++;
		if(update_counter > STATUS_TIMEOUT) update_counter = 0; 
		
		checkSpecial(0);
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
}

void ControllerManager::readSerial() {
    byte temp_buffer[512];
    ssize_t len;
    while((len = read(serial_fd, temp_buffer, sizeof(temp_buffer))) > 0) {
        if(buffer_len + len > sizeof(buffer)) {
            // Handle overflow
            std::cerr << "Buffer overflow" << std::endl;
            buffer_len = 0;
            continue;
        }
        memcpy(buffer + buffer_len, temp_buffer, len);
        buffer_len += len;
        processBuffer();
    }
}

/**
void ControllerManager::processBuffer() {
	int message_start = -1;
	
	for(size_t i = 0; i < buffer_len; i++) {
	    if(buffer[i] == '{') {
			message_start = i + 1;
			break;
		}
	}
	
	if(message_start == -1) {
		buffer_len = 0;
		return;
	}
	
	if(buffer_len - message_start < 7) {
		return;
	}
	
    byte* message = buffer + message_start;
	
	int controller_id = message[0];
    byte button_count = message[1];
	
    size_t button_value_count = (button_count + 7) / 8;
    byte axis_count = message[2 + button_value_count];
    size_t expected_size = 2 + button_value_count + 1 + axis_count * 4;
	
	if(controller_id > 3 || button_count > 32 || axis_count > 4 || expected_size > 30 || message[expected_size] != '}') { //MalFormed
		buffer_len -= message_start;
		memmove(buffer, buffer + message_start, buffer_len);
		return;
	}
	
	if(buffer_len < message_start + expected_size + 2) { //Not enough data yet
		return;
	}
	
	uint16_t calc_crc = crc16_ccitt_xmodem(message - 1, expected_size + 2);
	uint16_t sent_crc = message[expected_size + 1] | (message[expected_size + 2] << 8);
	
	if(calc_crc != sent_crc) { //MalFormed
		buffer_len -= message_start;
		memmove(buffer, buffer + message_start, buffer_len);
		return;
	}
	
    byte* button_values = message + 2;
    int16_t axis_values[axis_count * 2];
    for(size_t j = 0; j < axis_count; ++j) {
        axis_values[j * 2] = static_cast<int16_t>((message[3 + button_value_count + j * 4] << 8) | message[4 + button_value_count + j * 4]);
        axis_values[j * 2 + 1] = static_cast<int16_t>((message[5 + button_value_count + j * 4] << 8) | message[6 + button_value_count + j * 4]);
    }
    monitorJoystick(controller_id, button_count, button_values, axis_count, axis_values);
	
	buffer_len -= expected_size;
	memmove(buffer, buffer + message_start + expected_size, buffer_len);
}
**/

uint16_t ControllerManager::crc16_ccitt_xmodem(const uint8_t* data, size_t length) {
    uint16_t crc = 0x0000; // Initial value
    const uint16_t polynomial = 0x1021; // Polynomial used in CRC-16-CCITT

    for(size_t i = 0; i < length; ++i) {
        crc ^= (data[i] << 8); // XOR byte into the high order byte of CRC

        for(uint8_t j = 0; j < 8; ++j) {
            if(crc & 0x8000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc = crc << 1;
            }
        }
    }

    return crc;
}

void ControllerManager::processBuffer() {
    size_t i = 0;
    while(i < buffer_len) {
        // Find the start of a message
        if(buffer[i] != '{') {
            if(i < 64) {
                ++i;
                continue;
            }
            // Shift the buffer to remove processed part
            buffer_len -= i;
            memmove(buffer, buffer + i, buffer_len);
            i = 0;
            continue;
        }

        // Check if there is enough space for at least the header
        size_t message_start = i + 1;
        if(message_start + 2 >= buffer_len) break;

        byte button_count = buffer[message_start + 1];
        size_t button_value_count = (button_count + 7) / 8;
        size_t axis_count_pos = message_start + 2 + button_value_count;

        // Check if there is enough space for the button values and axis count
        if(axis_count_pos >= buffer_len) break;

        byte axis_count = buffer[axis_count_pos];
        size_t message_end = axis_count_pos + 1 + axis_count * 4;

        // Check if there is enough space for the entire message and if it ends with '}' with a 2 byte crc
        if(message_end + 2 >= buffer_len) break;
		if(buffer[message_end] != '}') {
			buffer_len -= 3;
            memmove(buffer, buffer + 3, buffer_len);
            continue;
		}

		uint16_t calc_crc = crc16_ccitt_xmodem(buffer + message_start - 1, message_end + 2 - message_start);
		uint16_t sent_crc = buffer[message_end + 1] + (buffer[message_end + 2] << 8);
		
		if(calc_crc != sent_crc) {
			buffer_len -=1;
			memmove(buffer, buffer + 1, buffer_len);
		}

        // Valid message found, extract and process it
        size_t message_len = message_end - message_start;
        byte* message = buffer + message_start;

        if(DEBUG) {
            if(ONE_LINE) std::cout << "\033[A";
            std::cout << "\rExtracted message: ";
            for(size_t j = 0; j < message_len; ++j) {
                std::cout << std::hex << static_cast<int>(message[j]) << " " << std::dec;
            }
            std::cout << std::endl;
        }

        // Ensure there's enough data for a valid message
        if(message_len >= 3) {
            byte controller_index = message[0];
            byte button_count = message[1];
            size_t button_value_count = (button_count + 7) / 8;
            byte* button_values = message + 2;
            byte axis_count = message[2 + button_value_count];

            if(message_len >= 2 + button_value_count + 1 + axis_count * 4) {
                int16_t axis_values[axis_count * 2];
                for(size_t j = 0; j < axis_count; ++j) {
                    axis_values[j * 2] = static_cast<int16_t>((message[3 + button_value_count + j * 4] << 8) | message[4 + button_value_count + j * 4]);
                    axis_values[j * 2 + 1] = static_cast<int16_t>((message[5 + button_value_count + j * 4] << 8) | message[6 + button_value_count + j * 4]);
                }
                monitorJoystick(controller_index, button_count, button_values, axis_count, axis_values);
            }
        }

        // Shift remaining buffer
        buffer_len -= (message_end + 1);
        memmove(buffer, buffer + message_end + 1, buffer_len);
        i = 0;
    }
}

void ControllerManager::monitorJoystick(byte controller_index, byte button_count, const byte* button_values, byte axis_count, const int16_t* axis_values) {
	if(controller_index < 0 || controller_index > fd_count) return; // Invalid controller index
	if(controller_index == 0) {
		pluggedIn =  (button_values[0] & 0x01) > 0;
		bool isFan = (button_values[0] & 0x02) > 0;
		bool isBac = (button_values[0] & 0x04) > 0;
		if(isFan) {
		  fanValue = axis_values[0];
		} else if(isBac) {
		  backlightValue = axis_values[0];
		} else {
		  batteryValue = axis_values[0];
		  addSample(batteryValue);
		}
		return;
	}
	controller_index--;
	
	if(controllers[controller_index].button_count < button_count) {
		controllers[controller_index].button_count = button_count;
		controllers[controller_index].axis_count = axis_count;
	}
	for(int i = 0; i < button_count; i++) {
		byte byte_index = i / 8;
        byte bit_index = i % 8;
        byte button_state = (button_values[byte_index] >> bit_index) & 1;
			
		controllers[controller_index].button_states[i] = button_state == 1;
	}
	for(byte i = 0; i < axis_count; ++i) {
        controllers[controller_index].axis[i].x = axis_values[i * 2];
        controllers[controller_index].axis[i].y = axis_values[i * 2 + 1];
    }
	
	if(checkSpecial(controller_index)) return;
	
	controller_index++;
	emulateJoystick(controller_index, button_count, button_values, axis_count, axis_values);
}

bool ControllerManager::checkSpecial(int c) {
	bool selectPressed = controllers[c].button_states[SELECT_BUTTON];
    bool startPressed = controllers[c].button_states[START_BUTTON];
	
    bool rPressed = controllers[c].button_states[R_BUTTON];
    bool lPressed = controllers[c].button_states[L_BUTTON];
    bool yPressed = controllers[c].button_states[Y_BUTTON];
    bool xPressed = controllers[c].button_states[X_BUTTON];
    bool bPressed = controllers[c].button_states[B_BUTTON];
    bool aPressed = controllers[c].button_states[A_BUTTON];
	
    int axisValue = controllers[c].axis[0].y;
	
	if(selectPressed && startPressed && rPressed && lPressed) {
		special_counter++;
		std::cout << special_counter << std::endl;
		if(special_counter >= 100) {
		    sync(); // Flush filesystem buffers
            reboot(RB_AUTOBOOT);
			exit(0);
		}
		return true;
	} else if(selectPressed && xPressed && rPressed && lPressed) {
		special_counter++;
		if(special_counter >= 100) {
	        sync(); // Flush filesystem buffers
            reboot(RB_POWER_OFF);
			exit(0);
		}
        return true;
	}
	
	if(selectPressed && bPressed) {
		if(special_counter == 0) {
	        callback(EVENT_SPECIAL, SPECIAL_BATTERY, 0, "");
		    special_counter++;
		}
        return true;
	} else if(selectPressed && aPressed && axisValue != 0) {
		int start_delay = 100;
		int cont_delay = 10;
		if(special_counter <= 1) {
            callback(EVENT_SPECIAL, SPECIAL_FAN_ADJ, axisValue, "");
		}
		special_counter++;
		if(special_counter == start_delay) {
            callback(EVENT_SPECIAL, SPECIAL_FAN_ADJ, axisValue, "");
		}
		if(special_counter >= (start_delay + cont_delay)) {
			special_counter = start_delay - 1;
		}
        return true;
	} else if(selectPressed && aPressed) {
		if(special_counter == 0) {
		    callback(EVENT_SPECIAL, SPECIAL_FAN, 0, "");
		}
		special_counter = 1;
        return true;
	} else if(selectPressed && xPressed && axisValue != 0) {
		int start_delay = 100;
		int cont_delay = 10;
		if(special_counter == 0) {
		    callback(EVENT_SPECIAL, SPECIAL_VOLUME_ADJ, axisValue, "");
		}
		special_counter++;
		if(special_counter == start_delay) {
		    callback(EVENT_SPECIAL, SPECIAL_VOLUME_ADJ, axisValue, "");
		}
		if(special_counter >= (start_delay + cont_delay)) {
			special_counter = start_delay - 1;
		}
        return true;
    } else if(selectPressed && xPressed) {
		if(special_counter == 0) {
		    callback(EVENT_SPECIAL, SPECIAL_VOLUME, 0, "");
		}
		special_counter = 1;
        return true;
	} else if(selectPressed && yPressed && axisValue != 0) {
		int start_delay = 100;
		int cont_delay = 10;
		if(special_counter == 0) {
		    callback(EVENT_SPECIAL, SPECIAL_BACK_ADJ, axisValue, "");
		}
		special_counter++;
		if(special_counter == start_delay) {
		    callback(EVENT_SPECIAL, SPECIAL_BACK_ADJ, axisValue, "");
		}
		if(special_counter >= (start_delay + cont_delay)) {
			special_counter = start_delay - 1;
		}
        return true;
    } else if(selectPressed && yPressed) {
		if(special_counter == 0) {
		    callback(EVENT_SPECIAL, SPECIAL_BACK, 0, "");
		}
		special_counter = 1;
        return true;
	}
	
	special_counter = 0;
	return false;
}

void ControllerManager::emulateJoystick(byte controller_index, byte button_count, const byte* button_values, byte axis_count, const int16_t* axis_values) {
    int fd = fds[controller_index - 1]; // Convert to 0-based index
    sendEvent(fd, EV_SYN, SYN_REPORT, 0);

    for(byte i = 0; i < button_count; ++i) {
		int code;
        switch (i + 1) {
            case 1: code = BTN_A; break;
            case 2: code = BTN_B; break;
            case 3: code = BTN_X; break;
            case 4: code = BTN_Y; break;
            case 5: code = BTN_START; break;
            case 6: code = BTN_SELECT; break;
            case 7: code = BTN_TL; break;
            case 8: code = BTN_TR; break;
            default: continue;
        }
        sendEvent(fd, EV_KEY, code, controllers[controller_index - 1].button_states[i]);
    }
	
    sendEvent(fd, EV_SYN, SYN_REPORT, 0);
	
	if(controllers[controller_index - 1].axis[0].x > 100) {
        sendEvent(fd, EV_KEY, BTN_DPAD_RIGHT, 1);
        sendEvent(fd, EV_KEY, BTN_DPAD_LEFT, 0);
	} else if(controllers[controller_index - 1].axis[0].x < -100) {
        sendEvent(fd, EV_KEY, BTN_DPAD_RIGHT, 0);
        sendEvent(fd, EV_KEY, BTN_DPAD_LEFT, 1);
	} else {
        sendEvent(fd, EV_KEY, BTN_DPAD_LEFT, 0);
        sendEvent(fd, EV_KEY, BTN_DPAD_RIGHT, 0);
	}
	
	if(controllers[controller_index - 1].axis[0].y > 100) {
        sendEvent(fd, EV_KEY, BTN_DPAD_DOWN, 1);
        sendEvent(fd, EV_KEY, BTN_DPAD_UP, 0);
	} else if(controllers[controller_index - 1].axis[0].y < -100) {
        sendEvent(fd, EV_KEY, BTN_DPAD_UP, 1);
        sendEvent(fd, EV_KEY, BTN_DPAD_DOWN, 0);
	} else {
        sendEvent(fd, EV_KEY, BTN_DPAD_UP, 0);
        sendEvent(fd, EV_KEY, BTN_DPAD_DOWN, 0);
	}

    sendEvent(fd, EV_SYN, SYN_REPORT, 0);

    if(DEBUG) {
        std::cout << "\rAxes: ";
        for(byte i = 0; i < axis_count * 2; ++i) {
            std::cout << std::setw(3) << static_cast<int>(i) << ":" << std::setw(6) << controllers[controller_index - 1].axis[i].x << " " << controllers[controller_index - 1].axis[i].y << " ";
        }
        std::cout << "Buttons: ";
        for(byte i = 0; i < button_count; ++i) {
            std::cout << std::setw(2) << static_cast<int>(i) << ":" << (controllers[controller_index - 1].button_states[i] ? "on " : "off ");
        }
        std::cout << std::flush;
		if(!ONE_LINE) std::cout << std::endl;
    }
}

void ControllerManager::sendEvent(int fd, int type, int code, int16_t value) {
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.code = code;
    ev.value = value;
    if(write(fd, &ev, sizeof(ev)) < 0) {
        perror("Failed to write event");
    }
}

void ControllerManager::addSample(int newSample) {
    sample_sum -= sample_buffer[sample_index];

    sample_buffer[sample_index] = newSample;
    sample_sum += newSample;

    sample_index = (sample_index + 1) % SAMPLE_SIZE;

    if(sample_count < SAMPLE_SIZE) {
        sample_count++;
    }
}

float ControllerManager::getBatteryAverage() {
    if(sample_count == 0) {
        return 0;
    }
    return (float)sample_sum / sample_count;
}

void ControllerManager::picoRequest(int func, int value) {
	byte buffer[7];
	buffer[0] = '{';
	buffer[1] = (byte)(func & 0xFF);
	buffer[2] = (byte)((value >> 0) & 0xFF);
	buffer[3] = (byte)((value >> 8) & 0xFF);
	buffer[4] = (byte)((value >> 16) & 0xFF);
	buffer[5] = (byte)((value >> 24) & 0xFF);
	buffer[6] = '}';
	write(serial_fd, buffer, sizeof(buffer));
}

void ControllerManager::statusUpdate() {
	uint8_t write_data[8];
	write_data[0] = '{';
	write_data[1] = 2; //Read Battery Value
	write_data[2] = 0;
	write_data[3] = 0;
	write_data[4] = 0;
	write_data[5] = 0;
	write_data[6] = '}';
	write(serial_fd, write_data, sizeof(write_data));
	
	write_data[1] = 4; //Read Fan Value
	write(serial_fd, write_data, sizeof(write_data));
	
	write_data[1] = 6; //Read Backlight Value
	write(serial_fd, write_data, sizeof(write_data));
	
	for(int i = 0; i < 4; i++) {
		picoRequest(1, i); //Read Controller States
	}
}

int ControllerManager::openSerialPort(const char* portName) {
    int fd = open(portName, O_RDWR | O_NOCTTY | O_NDELAY);
    if(fd == -1) {
        perror("openSerialPort: Unable to open port");
        return -1;
    }

    struct termios options;
    tcgetattr(fd, &options);

    // Set baud rate to 115200
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);

    // Set 8 data bits, no parity, 1 stop bit
    options.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
    options.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit (most common)
    options.c_cflag &= ~CSIZE; // Clear all bits that set the data size
    options.c_cflag |= CS8; // 8 bits per byte (most common)
    options.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
    options.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

    // Set raw input
    cfmakeraw(&options);

    // Set the new options for the port
    tcsetattr(fd, TCSANOW, &options);

    return fd;
}
