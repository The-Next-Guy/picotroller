#include <iostream>
#include <cstdio>
#include <thread>
#include "controller.h"
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>

#include "overlay.h"

#define VERSION "0.1"

using String = std::string;

bool joyCheck(int, int, int, std::string);
std::string getLocalFile(const std::string& filename);

ControllerManager manager(joyCheck);
OverlayManager overlay;

volatile int overlay_counter = 0, fanCounter = 0;
int last_vol, overlay_id = -1, overlay_dir;

#define OVERLAY_TIMEOUT 20
#define STATUS_TIMEOUT 10
#define FAN_MIN_VALUE 125

#define VOLUME_STEP_SIZE 1
#define FAN_STEP_SIZE 5
#define BACK_STEP_SIZE 5

#define AXIS_DEADZONE 100
#define RESISTOR_RATIO 1.589
#define VREF_ADC 3.238
#define VOFF 0.1

void controllerLoop() {
    manager.loop();
}

void picoProcess(int data) {
	std::cout << "Battery Value: " << manager.batteryValue << std::endl;
}

float batt_diffs[11] = {4.05, 4.00, 3.95, 3.92, 3.87, 3.82, 3.79, 3.75, 3.72, 3.65, 3.20};
int   batt_percs[11] = {100, 90, 80, 70, 60, 50, 40, 30, 20, 10, 1};

int getBatteryPercentage(float voltage) {
	int perc = 0;
	int diff_index = 0;
    for(int i = 0; i < 11; i++) {
		if(voltage > batt_diffs[i]) {
			perc = batt_percs[i];
			diff_index = i;
			break;
		}
	}
	
	if(perc == 0 || perc == 100) return perc;
	
	float delta = (voltage - batt_diffs[diff_index]) / (batt_diffs[diff_index - 1] - batt_diffs[diff_index]);
	perc += (int)(delta * 10);
	
	return perc;
}

void drawVolumeOverlay(int dir, int value) {
	overlay.clearScreen();
	overlay.drawPNG(getLocalFile("overlay_rp.png").c_str(), 0, 0);
	
	float ratio = (float)value / 255;
	float percent = ratio * 100;
	
	if(percent > 70){
		overlay.drawPNG(getLocalFile("volume_full.png").c_str(), 289, 39);
	} else if(percent > 40){
		overlay.drawPNG(getLocalFile("volume_med.png").c_str(), 289, 39);
	}  else if(percent > 1){
		overlay.drawPNG(getLocalFile("volume_low.png").c_str(), 289, 39);
	} else {
		overlay.drawPNG(getLocalFile("volume_mute.png").c_str(), 289, 39);
	}
	
	std::string dStr = std::to_string((int)(percent));
	int xOff = 293;
	
	int strLen = dStr.length();
	if(strLen == 1) xOff += 5;
	if(strLen == 2) xOff += 2;
	
	overlay.drawString(dStr.c_str(), xOff, 190, 0xFFFF, 0xF0);
	
	int h = (118 * ratio);
	
	overlay.drawRect(290, 65, 22, 120, 0xFFFF, 0xFF);
	overlay.fillRect(291, 66 + (118 - h), 20, h, 0xFFFF, 0xFF);
	
	overlay.commit();
}

void drawFanOverlay(int value, int change) {
	overlay.clearScreen();
	overlay.drawPNG(getLocalFile("overlay_rp.png").c_str(), 0, 0);
	
	fanCounter = (fanCounter+1) % 8;
	int fanFrame = fanCounter;
	
	std::string path = getLocalFile("fan_" + std::to_string(fanFrame) + ".png");
	overlay.drawPNG(path.c_str(), 289, 39);
	
	value -= FAN_MIN_VALUE;
	if(value < 0) value = 0;
	
	float ratio = (float)value / (255 - FAN_MIN_VALUE);
	float percent = ratio * 100;
	
	std::string dStr = std::to_string((int)(percent));
	int xOff = 293;
	
	int strLen = dStr.length();
	if(strLen == 1) xOff += 5;
	if(strLen == 2) xOff += 2;
	
	overlay.drawString(dStr.c_str(), xOff, 190, 0xFFFF, 0xF0);
	
	int h = (118 * ratio);
	
	overlay.drawRect(290, 65, 22, 120, 0xFFFF, 0xFF);
	overlay.fillRect(291, 66 + (118 - h), 20, h, 0xFFFF, 0xFF);
		
	overlay.commit();
}

void drawBacklightOverlay(int value, int change) {
	overlay.clearScreen();
	overlay.drawPNG(getLocalFile("overlay_rp.png").c_str(), 0, 0);
	
	if(value < 0) value = 0;
	
	float ratio = (float)value / 255;
	float percent = ratio * 100;
	
	if(percent > 75){
		overlay.drawPNG(getLocalFile("brightness_full.png").c_str(), 289, 39);
	} else if(percent > 25){
		overlay.drawPNG(getLocalFile("brightness_half.png").c_str(), 289, 39);
	} else {
		overlay.drawPNG(getLocalFile("brightness_none.png").c_str(), 289, 39);
	}
	
	std::string dStr = std::to_string((int)(percent));
	int xOff = 293;
	
	int strLen = dStr.length();
	if(strLen == 1) xOff += 5;
	if(strLen == 2) xOff += 2;
	
	overlay.drawString(dStr.c_str(), xOff, 190, 0xFFFF, 0xF0);
	
	int h = (118 * ratio);
	
	overlay.drawRect(290, 65, 22, 120, 0xFFFF, 0xFF);
	overlay.fillRect(291, 66 + (118 - h), 20, h, 0xFFFF, 0xFF);
		
	overlay.commit();
}

void drawBatteryOverlay(int value) {
	overlay.clearScreen();
	overlay.drawPNG(getLocalFile("battery_overlay.png").c_str(), 0, 0);
	
	float voltage = (((value / (float)4096) * VREF_ADC) + VOFF) * RESISTOR_RATIO;
	int percent = getBatteryPercentage(voltage);
	
	char buffer[20];
    sprintf(buffer, "%d%% (%.3fv)", percent, voltage);
	
	std::string dStr = std::string(buffer) + (manager.pluggedIn ? " Charging" : " Battery");
	int xOff = (320 - overlay.getStringWidth(dStr.c_str())) / 2;
	//xOff += (3 - dStr.length()) * 3;
	overlay.drawString(dStr.c_str(), xOff, 24, 0xFFFF, 0xFF);
	
	
	int width = (float)232 / 100 * percent;
	overlay.drawRect(43, 2, 234, 16, 0xFFFF, 0xFF);
	overlay.fillRect(44 + (232 - width), 3, width, 14, 0xFFFF, 0xFF);
		
	overlay.commit();
}

void controlBrightness(int value) {
	if(value > AXIS_DEADZONE) { //Adjust Down
		manager.picoRequest(7, manager.backlightValue - BACK_STEP_SIZE);
	} else if(value < -AXIS_DEADZONE) { //Adjust Up
		manager.picoRequest(7, manager.backlightValue + BACK_STEP_SIZE);
	}
}

void controlFan(int value) {
	if(value > AXIS_DEADZONE) { //Adjust Down
		manager.picoRequest(5, manager.fanValue - FAN_STEP_SIZE);
	} else if(value < -AXIS_DEADZONE) { //Adjust Up
		manager.picoRequest(5, manager.fanValue + FAN_STEP_SIZE);
	}
}

void controlVolume(int value) {
    std::string command;
    if(value < -AXIS_DEADZONE) {
        command = "amixer sset 'PCM',0 " + std::to_string(VOLUME_STEP_SIZE) + "+";
    } else if(value > AXIS_DEADZONE) {
        command = "amixer sset 'PCM',0 " + std::to_string(VOLUME_STEP_SIZE) + "-";
    } else {
        command = "amixer sset 'PCM',0 0+";
    }

    // Execute the command and handle the output
    FILE* pipe = popen(command.c_str(), "r");
    if(!pipe) return;

    char buffer[128];
    std::string result = "";
    while(fgets(buffer, 128, pipe) != NULL) {
        result += buffer;
    }
    pclose(pipe);

    // Parse the output to find the current volume level
    size_t pos = result.find("Front Left:");
    int currVol = 0;
    if(pos != std::string::npos) {
        std::sscanf(result.c_str() + pos, "Front Left: %d", &currVol);
    }
	last_vol = currVol;
}

bool joyCheck(int func, int reg, int value, std::string data) {
	if(func == EVENT_SPECIAL) {
		if(reg == SPECIAL_VOLUME) {
			overlay_id = 1;
			overlay_counter = 0;
			return true;
		}
	    if(reg == SPECIAL_VOLUME_ADJ) {
			controlVolume(value);
			overlay_id = 1;
			overlay_counter = 0;
			return true;
		}
		if(reg == SPECIAL_BATTERY) {
			overlay_id = 0;
			overlay_counter = 0;
			return true;
		}
		if(reg == SPECIAL_FAN) {
			overlay_id = 2;
			overlay_counter = 0;
			return true;
		}
		if(reg == SPECIAL_FAN_ADJ) {
			controlFan(value);
			overlay_id = 2;
			overlay_counter = 0;
			return true;
		}
		if(reg == SPECIAL_BACK) {
			overlay_id = 4;
			overlay_counter = 0;
			return true;
		}
		if(reg == SPECIAL_BACK_ADJ) {
			controlBrightness(value);
			overlay_id = 4;
			overlay_counter = 0;
			return true;
		}
	} else if(func == EVENT_SPECIAL) {
		
	} else {
		
	}
	return false;
}

void drawOverlay(){
	switch(overlay_id){
		case 0: //Battery
			drawBatteryOverlay(manager.getBatteryAverage());
		    break;
		case 1: //Volume
			drawVolumeOverlay(0, last_vol);
		    break;
		case 2: //Fan
			drawFanOverlay(manager.fanValue, 0);
		    break;
		case 4: //Brightness
			drawBacklightOverlay(manager.backlightValue, 0);
		    break;
	}
}

int main() {
    std::cout << "Xemplar PicoTroller v" << VERSION << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;
	
	manager.initConnection("/dev/serial0");

    std::thread controllerThread(controllerLoop);
	
	controlVolume(0); //Obtains initial volume
	
	overlay.clearScreen();
	overlay.commit();

    // Main loop
    while(true) {
		if(overlay_counter > -1) overlay_counter++;
		if(overlay_counter >= OVERLAY_TIMEOUT) {
			overlay_id = -1;
			overlay_counter = -1;
			overlay.clearScreen();
			overlay.commit();
		}
		
		drawOverlay();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    controllerThread.join();  // Join the thread before exiting (not reached in this example)
    return 0;
}

std::string getLocalFile(const std::string& filename) {
    // Buffer to store the path of the executable
    char exePath[1024];
    ssize_t count = readlink("/proc/self/exe", exePath, sizeof(exePath));
    if (count == -1) {
        perror("readlink");
        return "";
    }
    exePath[count] = '\0'; // Null-terminate the path

    // Get the directory name of the executable
    std::string dirPath = dirname(exePath);

    // Construct the full file path
    std::string filePath = dirPath + "/" + filename;

    return filePath;
}