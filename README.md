# Xemplar Picotroller
A uinput controller driver for the raspberry pi, that interfaces with a pico over the native serial port, or using gpio pins. Also draws overlays with https://github.com/The-Next-Guy/fbcp-nexus/

### Compiling
This program requires `libevdev-dev` to be installed, which can be simply installed with
```
sudo apt-get install libevdev-dev
```

It also requires lodepng.h and lodepng.cpp to be dropped in next to the rest of the source. Ensure the SMA IDs in `shared_memory.h` are the same as the ones set in fbcp-nexus. Create a `config.prop` file next to the source, or else root will and it will throw an error. To fix, chown the file to pi. Then you just
```
make
sudo ./joystick_emulator
```
This will take you through the setup, you can choose the gpio monitor, or the pico monitor. You can even write your own by extending Monitor in Monitor.h. You will have to modify the main file to add the option.

If you choose the pico monitor, it will it up and strat running right away on `/dev/serial0` but you can change this by changing the `interface` property.

If you choose the gpio monitor, it will as a series of questions about which pins you want to check, how many joysticks, if you have a dpad, pull up/downs, and then it will start configuring buttons. Each button it will ask you to hold it, then release.

Once the monitor is setup, it will ask you to setup PicoTroller's buttons, depending on the features provided by the monitor.

Lastly, it will ask you to setup the audio device that it will be using, just enter it like it is. It could be `PCM`, `Headphone`, `HDMI`, or something else.

This whole setup can be ran from ssh, without a keyboard, provided that you have your pico or gpio pins wired up. Who knows, maybe you'll make a wifi client.

To make it start on boot, edit `/etc/rc.local` and add
```
sudo /path/to/pictroller/joystick_emulator&
```