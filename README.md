# Xemplar Picotroller
A uinput controller driver for the raspberry pi, that interfaces with a pico over the native serial port. Also draws overlays with https://github.com/The-Next-Guy/fbcp-nexus/

### Compiling
This program requires `libevdev-dev` to be installed, which can be simply installed with
```
sudo apt-get install libevdev-dev
```

It also requires lodepng.h and lodepng.cpp to be dropped in net to the rest of the source. Ensure the SMA IDs in `shared_memory.h` are the same as the ones set in fbcp-nexus. The you just
```
make
sudo ./joystick_emulator
```

To make it start on boot, edit `/etc/rc.local` and add
```
sudo /path/to/pictroller/joystick_emulator&
```