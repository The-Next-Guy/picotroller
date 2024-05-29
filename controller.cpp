#include "controller.h"
#include <iostream>
#include <cstdio>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <fcntl.h>

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
}

void ControllerManager::setMonitor(Monitor* monitor){
	this->monitor = monitor;
	this->monitor->callback = [this](unsigned char a, unsigned char b, const unsigned char* c, unsigned char d, const short int* e) {
        this->monitorJoystick(a, b, c, d, e);
    };
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

int ControllerManager::initMonitor() {
	return monitor->init() ? 0 : 1;
}


void ControllerManager::loop() {
	while(true) {
        monitor->update();
		callback(controllers, 0);
        std::this_thread::sleep_for(std::chrono::microseconds(500));
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
	
	if(controllers[controller_index].button_count < button_count || controllers[controller_index].axis_count < axis_count) {
		controllers[controller_index].button_count = button_count;
		controllers[controller_index].axis_count = axis_count;
	}
	for(int i = 0; i < button_count; i++) {
		byte byte_index = i / 8;
        byte bit_index = i % 8;
        byte button_state = (button_values[byte_index] >> bit_index) & 1;
			
		controllers[controller_index].button_states[i] = button_state == 1;
	}
	for(byte i = 0; i < axis_count; i++) {
        controllers[controller_index].axis[i].x = axis_values[i * 2];
        controllers[controller_index].axis[i].y = axis_values[i * 2 + 1];
    }
	
	if(callback(controllers, controller_index)) return;
	
	controller_index++;
	emulateJoystick(controller_index, button_count, button_values, axis_count, axis_values);
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

void ControllerManager::monitorRequest(byte func, unsigned int value){
	monitor->request(func, value);
}