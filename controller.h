#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <cstdint>
#include <termios.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/uinput.h>

// Define DEBUG as a boolean
#define DEBUG false
#define ONE_LINE false


#define EVENT_BUTTON  1
#define EVENT_AXIS    2
#define EVENT_SPECIAL 3

#define SPECIAL_BATTERY    1
#define SPECIAL_VOLUME     2
#define SPECIAL_VOLUME_ADJ 3

#define SPECIAL_FAN        4
#define SPECIAL_FAN_ADJ 5

#define SPECIAL_BACK       6
#define SPECIAL_BACK_ADJ   7

#define SAMPLE_SIZE 100

typedef uint8_t byte;

struct AxisData {
  int16_t x, y;
};

struct ControllerData {
  int id;
  int button_count;
  int axis_count;
  bool plugged_in;

  bool button_states[32];
  AxisData axis[4];
};

class ControllerManager {
public:
    int batteryValue, fanValue, backlightValue;
	bool pluggedIn;
	
	typedef bool (*JoyCallback)(int,int,int,std::string);
	JoyCallback callback;

    ControllerManager(JoyCallback callback);
    ~ControllerManager();
	int initConnection(std::string port);
	void loop();
    void readSerial();
	void picoRequest(int func, int value);
	
	float getBatteryAverage();
	
private:
    int fds[4];  // File descriptors for the virtual joysticks
    int fd_count = 0, update_counter, special_counter;
	int serial_fd;
	
	bool controlMode = false;
    byte buffer[2048];  // Buffer to store incomplete messages
    size_t buffer_len = 0;
	ControllerData controllers[4];
	
	int sample_buffer[SAMPLE_SIZE];
	int sample_index = 0;
	int sample_count = 0;
	long sample_sum = 0;

    void setup_uinput_device(int joystick_id);
	int openSerialPort(const char* portName);
	void statusUpdate();
	bool checkSpecial(int c);
	void addSample(int newSample);
	
	uint16_t crc16_ccitt_xmodem(const uint8_t* data, size_t length);
	
    void processBuffer();
    void emulateJoystick(byte controller_index, byte button_count, const byte* button_values, byte axis_count, const int16_t* axis_values);
    void monitorJoystick(byte controller_index, byte button_count, const byte* button_values, byte axis_count, const int16_t* axis_values);
    void sendEvent(int fd, int type, int code, int16_t value);
};

#endif // CONTROLLER_H
