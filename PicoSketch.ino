struct Axis {
  int pins[4];
  int digital_times_on[4];
  int digital_times_off[4];
  boolean analog = false;
  int x, y;
  int prevX, prevY;
};

struct Controller {
  int id;
  int button_count;
  int axis_count;
  boolean plugged_in;

  boolean prev_states[32];
  boolean button_states[32];
  int button_times_on[32];
  int button_times_off[32];
  int button_pins[32];
  Axis axis[4];
};

volatile Controller controllers[5];
volatile int battVal, fanVal = 0, bacVal = 255;
volatile boolean charging, systemOn;

#define LED_PIN 15
#define CHARGE_PIN 14
#define FAN_PIN 22
#define BAC_PIN 21
#define BATTERY_PIN A2

#define LED_MAX_VALUE 64
#define FAN_MIN_VALUE 125

#define DEBOUCE_TIME 20000

#define DEBUG true

void setup() {
  pinMode(LED_PIN, OUTPUT_8MA);
  pinMode(FAN_PIN, OUTPUT_4MA);
  pinMode(BAC_PIN, OUTPUT_4MA);
  pinMode(CHARGE_PIN, INPUT);

  systemOn = true;
  setFanSpeed(255);
  setBrightness(255);

  Serial1.setRX(17);
  Serial1.setTX(16);
  Serial1.begin(115200);

  controllers[0].button_count = 8;
  controllers[0].axis_count = 1;
  controllers[0].button_states[0] = false;

  Serial.begin(9600);
}

#define LED_POWER_ON 1
#define LED_POWER_OFF 2
#define LED_BATTERYLOW 3

#define LED_CHARGING10 4
#define LED_CHARGING25 5
#define LED_CHARGING50 6
#define LED_CHARGING75 7
#define LED_CHARGINGFULL 8

#define BATTERY_FULL 3000
#define BATTERY_EMPTY 2300

volatile int ledState = LED_BATTERYLOW;
volatile int counter = 0, systemOn_counter;
unsigned long counter_stamp, alive_stamp;

#define counter_delay 1000

byte readBuffer[256]; 
int readIndex = 0;
boolean reading = false;
long startTime = 0;

#define SERIAL_READ_TIMEOUT 100000
#define ALIVE_TIMEOUT 2000

void loop() {
  readSerial();
  systemOn = (abs((long)(millis() - alive_stamp)) < ALIVE_TIMEOUT);
  setFanSpeed(fanVal);
  setBrightness(bacVal);

  if(charging){
    if(getBatteryPercent() > 0.9){
      ledState = LED_CHARGINGFULL;
    } else if(getBatteryPercent() > 0.75){
      ledState = LED_CHARGING75;
    } else if(getBatteryPercent() > 0.50){
      ledState = LED_CHARGING50;
    } else if(getBatteryPercent() > 0.25){
      ledState = LED_CHARGING25;
    } else {
      ledState = LED_CHARGING10;
    }
  } else {
    if(getBatteryPercent() > 0.25){
      ledState = systemOn ? LED_POWER_ON : LED_POWER_OFF;
    } else {
      ledState = LED_BATTERYLOW;
    }
  }

  int batt = getBatteryRaw();

  if(ledState == LED_POWER_ON){
    analogWrite(LED_PIN, LED_MAX_VALUE);
    counter = 0;
  }
  if(ledState == LED_POWER_OFF){
    analogWrite(LED_PIN, 0);
    counter = 0;
  }
  if(ledState == LED_CHARGING10){
    float value = 0;
    if(counter < 600) {
      value = 0.0;
    } else if(counter < 600) {
      value = (counter - 800) / (float)200;
    } else {
      value = 1.0 - ((counter - 800) / (float)200);
    }

    analogWrite(LED_PIN, (int)(value * LED_MAX_VALUE));
    if(counter >= 1000) counter = 0;
  }
  if(ledState == LED_CHARGING25){
    float value = 0;
    if(counter < 600) {
      value = 0.0;
    } else if(counter < 700) {
      value = (counter - 600) / (float)100;
    } else if(counter < 800) {
      value = 1.0;
    } else {
      value = 1.0 - ((counter - 800) / (float)200);
    }

    analogWrite(LED_PIN, (int)(value * LED_MAX_VALUE));
    if(counter >= 1000) counter = 0;
  }
  if(ledState == LED_CHARGING50){
    float value = 0;
    if(counter < 400) {
      value = 0.0;
    } else if(counter < 600) {
      value = (counter - 400) / (float)200;
    } else if(counter < 800) {
      value = 1.0;
    } else {
      value = 1.0 - ((counter - 800) / (float)200);
    }

    analogWrite(LED_PIN, (int)(value * LED_MAX_VALUE));
    if(counter >= 1000) counter = 0;
  }
  if(ledState == LED_CHARGING75){
    float value = 0;
    if(counter < 200) {
      value = 0.0;
    } else if(counter < 400) {
      value = (counter - 200) / (float)200;
    } else if(counter < 600) {
      value = 1.0;
    } else {
      value = 1.0 - ((counter - 800) / (float)200);
    }

    analogWrite(LED_PIN, (int)(value * LED_MAX_VALUE));
    if(counter >= 1000) counter = 0;
  }
  if(ledState == LED_CHARGINGFULL){
    float value = 0;
    if(counter < 200) {
      value = counter / (float)200;
    } else if(counter < 800) {
      value = 1.0;
    } else {
      value = 1.0 - ((counter - 800) / (float)200);
    }

    analogWrite(LED_PIN, (int)(value * LED_MAX_VALUE));
    if(counter >= 1000) counter = 0;
  }
  if(ledState == LED_BATTERYLOW){
    float ratio = 4095 / 3.3;
    float voltage = 4095 * ratio + 1.0; //getBatteryValue() * ratio + 1F;

    if(batt > BATTERY_FULL) batt = BATTERY_FULL; //Maximum that should be read
    batt -= BATTERY_EMPTY;                        //Minimum voltage that should be read
    if(batt < 100) batt = 100;

    float percent = (float)batt / (BATTERY_FULL - BATTERY_EMPTY);

    int delay = percent * percent * 2000;                    //Scale delay down so that it has more impact

    if(counter < delay){
      analogWrite(LED_PIN, 0);
    }
    if(counter > delay){
      analogWrite(LED_PIN, LED_MAX_VALUE);
    }
    if(counter > (delay << 1)) counter = 0;
  }

  if(abs((long long)(micros() - counter_stamp)) < counter_delay){
    return;
  }
  counter_stamp = micros();
  counter++;
}

void setFanSpeed(int speed){
  if(!systemOn){
    analogWrite(FAN_PIN, 0);
    return;
  }

  if(speed >= 255){
    fanVal = 255;
    analogWrite(FAN_PIN, 0);
    digitalWrite(FAN_PIN, 1);
  } else if(speed <= FAN_MIN_VALUE){
    if(speed < fanVal){
      fanVal = 0;
      analogWrite(FAN_PIN, 0);
      digitalWrite(FAN_PIN, 0);
    } else {
      fanVal = FAN_MIN_VALUE;
      analogWrite(FAN_PIN, fanVal);
    }
  } else {
    fanVal = speed;
    analogWrite(FAN_PIN, fanVal);
  }
}

void setBrightness(int brightness){
  if(!systemOn){
    digitalWrite(BAC_PIN, 0);
    analogWrite(BAC_PIN, 0);
    return;
  }

  if(brightness >= 255){
    bacVal = 255;
    analogWrite(BAC_PIN, 0);
    digitalWrite(BAC_PIN, 1);
  } else {
    bacVal = brightness;
    digitalWrite(BAC_PIN, 0);
    analogWrite(BAC_PIN, brightness);
  }
}

void processCommand(int func, int value){
  switch(func){
    case 1: //Read Controller value, index is supplied number + 1;
      sendFullState((value % 4) + 1);
      break;
    case 2: //Read Battery Value
      controllers[0].button_states[0] = charging;
      controllers[0].button_states[1] = false;
      controllers[0].button_states[2] = false;
      controllers[0].axis[0].x = controllers[0].axis[0].y = getBatteryRaw();
      sendFullState(0);
      break;

    case 4: //Read Fan Value
      controllers[0].button_states[0] = charging;
      controllers[0].button_states[1] = true;
      controllers[0].button_states[2] = false;
      controllers[0].axis[0].x = controllers[0].axis[0].y = fanVal;
      sendFullState(0);
      break;
    case 5: //Write Fan Value
      setFanSpeed(value);
      controllers[0].button_states[0] = charging;
      controllers[0].button_states[1] = true;
      controllers[0].button_states[2] = false;
      controllers[0].axis[0].x = controllers[0].axis[0].y = fanVal;
      sendFullState(0);
      break;

    case 6: //Read Backlight Value
      controllers[0].button_states[0] = charging;
      controllers[0].button_states[1] = false;
      controllers[0].button_states[2] = true;
      controllers[0].axis[0].x = controllers[0].axis[0].y = bacVal;
      sendFullState(0);
      break;
    case 7: //Write Backlight Value
      setBrightness(value);
      controllers[0].button_states[0] = charging;
      controllers[0].button_states[1] = false;
      controllers[0].button_states[2] = true;
      controllers[0].axis[0].x = controllers[0].axis[0].y = bacVal;
      sendFullState(0);
      break;  
  }
}

void readSerial(){
  if(abs((int)(micros() - startTime)) > SERIAL_READ_TIMEOUT){
    reading = false;
  }

  while(Serial1.available() > 0){
    alive_stamp = millis();

    int r = Serial1.read();
    if(r == -1) return;

    byte b = (byte) r;
    if(!reading && b == '{'){
      reading = true;
      readIndex = 0;
      startTime = micros();
    }

    if(reading){
      readBuffer[readIndex] = b;
      readIndex++;
      
      startTime = micros();
    }

    if(reading && b == '}' && readIndex > 6){
      reading = false;
      int func = readBuffer[1];
      int data = readBuffer[2];
      data |= readBuffer[3] << 8;
      data |= readBuffer[4] << 16;
      data |= readBuffer[5] << 24;

      processCommand(func, data);
    }
  }
}

int getBatteryRaw(){
  return battVal;
}

float getBatteryPercent(){
  int batt = getBatteryRaw();
  if(batt > BATTERY_FULL) batt = BATTERY_FULL; //Maximum that should be read
  batt -= BATTERY_EMPTY;                        //Minimum voltage that should be read
  if(batt < 1) batt = 1;

  return (float)batt / (BATTERY_FULL - BATTERY_EMPTY);
}

void setup1(){
  pinMode(BATTERY_PIN, INPUT);
  analogReadResolution(12);

  //Setup Controller 1 (0)
  controllers[1].plugged_in = true;
  controllers[1].axis_count = 1;

  int c0_axis0_pins[4] = {10, 11, 12, 13};
  for(int i = 0; i < 4; i++){
    controllers[1].axis[0].pins[i] = c0_axis0_pins[i];
  }

  int c0_buttons[8] = {2, 3, 4, 5,  6, 7, 8, 9};
  controllers[1].button_count = 8;
  for(int i = 0; i < controllers[1].button_count; i++){
    controllers[1].button_pins[i] = c0_buttons[i];
  }
  

  //Setup Controller Pins
  for(int c = 0; c < 4; c++){
    controllers[c].id = c + 1;
    for(int i = 0; i < controllers[c].button_count; i++){
      pinMode(controllers[c].button_pins[i], INPUT_PULLUP);
    }
    for(int i = 0; i < controllers[c].axis_count; i++){
      for(int p = 0; p < 4; p++){
        pinMode(controllers[c].axis[i].pins[p], INPUT_PULLUP);
      }
    }
  }

}

void readControllers(){
  for(int c = 0; c < 4; c++){
    if(!controllers[c].plugged_in) continue;

    for(int i = 0; i < controllers[c].button_count; i++){
      boolean state = !digitalRead(controllers[c].button_pins[i]);

      if(state){ //Read Button States
        controllers[c].button_times_on[i]++;
      } else {
        controllers[c].button_times_off[i]++;
      }
    }

    for(int i = 0; i < controllers[c].axis_count; i++){
      int prevX = controllers[c].axis[i].x;
      int prevY = controllers[c].axis[i].y;

      if(controllers[c].axis[i].analog){
        controllers[c].axis[i].x = 32000 * (analogRead(controllers[c].axis[i].pins[0]) / (float)(4096 / 2) - 1.0);
        controllers[c].axis[i].y = 32000 * (analogRead(controllers[c].axis[i].pins[1]) / (float)(4096 / 2) - 1.0);
      } else {
        for(int b = 0; b < 4; b++){
          if(digitalRead(controllers[c].axis[i].pins[b])){
            controllers[c].axis[i].digital_times_on[b]++;
          } else {
            controllers[c].axis[i].digital_times_off[b]++;
          }
        }
      }
    }
  }
}

void calcDebouce(){
  for(int c = 0; c < 4; c++){
    boolean changed = false;
    for(int i = 0; i < controllers[c].button_count; i++){
      controllers[c].button_states[i] = controllers[c].button_times_on[i] > controllers[c].button_times_off[i];
      controllers[c].button_times_on[i] = 0;
      controllers[c].button_times_off[i] = 0;
      changed |= (controllers[c].button_states[i] != controllers[c].prev_states[i]);
      controllers[c].prev_states[i] = controllers[c].button_states[i];
    }
    for(int i = 0; i < controllers[c].axis_count; i++){
      if(controllers[c].axis[i].analog) continue;

      controllers[c].axis[i].x = 0;
      controllers[c].axis[i].y = 0;

      controllers[c].axis[i].x += 32000 * ((controllers[c].axis[i].digital_times_on[1] > controllers[c].axis[i].digital_times_off[1]) ? 1 : 0);
      controllers[c].axis[i].x -= 32000 * ((controllers[c].axis[i].digital_times_on[3] > controllers[c].axis[i].digital_times_off[3]) ? 1 : 0);

      controllers[c].axis[i].y += 32000 * ((controllers[c].axis[i].digital_times_on[0] > controllers[c].axis[i].digital_times_off[0]) ? 1 : 0);
      controllers[c].axis[i].y -= 32000 * ((controllers[c].axis[i].digital_times_on[2] > controllers[c].axis[i].digital_times_off[2]) ? 1 : 0);

      
      changed |= (controllers[c].axis[i].x != controllers[c].axis[i].prevX);
      changed |= (controllers[c].axis[i].y != controllers[c].axis[i].prevY);

      controllers[c].axis[i].prevX = controllers[c].axis[i].x;
      controllers[c].axis[i].prevY = controllers[c].axis[i].y;

      for(int b = 0; b < 4; b++){
        controllers[c].axis[i].digital_times_on[b] = 0;
        controllers[c].axis[i].digital_times_off[b] = 0;
      }
    }

    if(changed) sendFullState(c);
  }
}

long long last_update = -DEBOUCE_TIME;
void loop1(){
  battVal = analogRead(BATTERY_PIN);
  charging = digitalRead(CHARGE_PIN);

  readControllers();
  if(abs((micros() - last_update)) > DEBOUCE_TIME){
    last_update = micros();

    calcDebouce();
  }
}

void sendAxisEvent(int controller, int axis, boolean isY, int value){
  if(DEBUG){
    Serial.print("Joystick ");
    Serial.print(controller);
    Serial.print(": Axis ");
    Serial.print(axis);
    Serial.print(isY ? "Y: " : "X: ");
    Serial.println(value);/**/
  }
}

void sendButtonEvent(int controller, int button, boolean state){
  if(DEBUG){
    Serial.print("Joystick ");
    Serial.print(controller);
    Serial.print(": Button ");
    Serial.print(button);
    Serial.print(" was ");
    Serial.println(state ? "pressed." : "released.");/**/
  }
}

void sendFullState(int c){
  sendFullStateRaw(c);
  if(!DEBUG || true) return;

  int count = controllers[c].button_count;
  
  
  Serial.print("{");
  Serial.print(c);
  Serial.print(" ");

  byte data;
  data = count & 0xFF;
  Serial.print(data);
  Serial.print(" ");

  for(int i = 0; i < count; i++){
    if(i % 8 == 0) data = 0;
    data += (controllers[c].button_states[i] ? 1 : 0) << (i % 8);

    if(i % 8 == 7) {
      Serial.print(data, HEX);
    }
  }

  count = controllers[c].axis_count;
  data = count & 0xFF;
  Serial.print("  ");
  Serial.print(data);
  Serial.print(" ");

  for(int i = 0; i < count; i++){
    byte up, low;
    up  = (controllers[c].axis[i].x >> 8) & 0xFF;
    low = (controllers[c].axis[i].x) & 0xFF;

    if(up < 0x10) Serial.print("0");
    Serial.print(up, HEX);
    if(low < 0x10) Serial.print("0");
    Serial.print(low, HEX);


    up  = (controllers[c].axis[i].y >> 8) & 0xFF;
    low = (controllers[c].axis[i].y) & 0xFF;
    
    if(up < 0x10) Serial.print("0");
    Serial.print(up, HEX);
    if(low < 0x10) Serial.print("0");
    Serial.print(low, HEX);
  }

  Serial.println("}");
}

uint16_t crc16_ccitt_xmodem(const uint8_t* data, size_t length) {
    uint16_t crc = 0x0000; // Initial value
    const uint16_t polynomial = 0x1021; // Polynomial used in CRC-16-CCITT

    for (size_t i = 0; i < length; ++i) {
        crc ^= (data[i] << 8); // XOR byte into the high order byte of CRC

        for (uint8_t j = 0; j < 8; ++j) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc = crc << 1;
            }
        }
    }

    return crc;
}

void add_crc(uint8_t* message, size_t length) {
    uint16_t crc = crc16_ccitt_xmodem(message, length);
    message[length] = crc & 0xFF; // Low byte of CRC
    message[length + 1] = (crc >> 8) & 0xFF; // High byte of CRC
}

void sendFullStateRaw(int c){
  int count = controllers[c].button_count;
  byte msg[32];
  int msg_len = 0;
  
  msg[msg_len++] = '{';
  msg[msg_len++] = c;


  byte data;
  data = count & 0xFF;
  msg[msg_len++] = data;
  
  msg_len = 3;

  for(int i = 0; i < count; i++){
    if(i % 8 == 0) data = 0;
    data += (controllers[c].button_states[i] ? 1 : 0) << (i % 8);

    if(i % 8 == 7) {
      msg[msg_len++] = data;
    }
  }

  count = controllers[c].axis_count;
  data = count & 0xFF;
  msg[msg_len++] = data;

  for(int i = 0; i < count; i++){
    byte up, low;
    up  = (controllers[c].axis[i].x >> 8) & 0xFF;
    low = (controllers[c].axis[i].x) & 0xFF;

    msg[msg_len++] = up;
    msg[msg_len++] = low;

    up  = (controllers[c].axis[i].y >> 8) & 0xFF;
    low = (controllers[c].axis[i].y) & 0xFF;
    
    msg[msg_len++] = up;
    msg[msg_len++] = low;
  }
  msg[msg_len++] = '}';
  add_crc(msg, msg_len);

  Serial1.write(msg, msg_len + 2); //Print and add crc length;
}