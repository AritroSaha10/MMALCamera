# MMALCamera
C++ library for using MMAL (MultiMedia Abstraction Layer) to control a CSI camera on the Raspberry Pi. 

## Current Features
- Show a fullscreen preview to the screen **solely** using the GPU

## Planned Features
- Fetching images
- Adjusting camera settings (ex. AWB, brightness, etc)

## Compilation and Execution
```
cmake .
make
./MMALCameraSample
```

## Usage as Library
Just import the two files `MMALCamera.cpp` and `MMALCamera.h` into your project and import it into your code using `#include "MMALCamera.h"`.

## Note
Since this uses VideoCore libraries that are only available on the RPi, it will likely be very difficult (if not impossible) to cross-compile from another machine.
