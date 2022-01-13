# TimecodeFPS
AviSynth plugin to convert VFR to CFR using MKV timecodes

## Documentation
* http://www.avisynth.nl/index.php/TimecodeFPS
* http://tasvideos.org/forum/viewtopic.php?p=313741#313741

### Copyright 2012 Nicholai Main
See timecodefps.txt for more.

### Requirements

- Microsoft VisualC++ Redistributable Package 2022 (can be downloaded from [here](https://github.com/abbodi1406/vcredist/releases))

### Changelog
* v1.1.3 2022-01-13
    - Set duration of the latest frame when specified. (Asd-g)
* v1.1.2 2021-09-28
    - Created Visual Studio 2019 solution and removed GCC .mak files. (TomArrow)
    - Changed to C++ and adapted a few lines of code that didn't work in C++. (TomArrow)
    - Can now pass multiple timecode files by separating them with a semicolon, for example if you are loading multiple videos with absolute timestamp values and concatenating them and you want to preserve a continuous FPS across multiple files. (TomArrow)
* v1.1.1 2021-06-04
    - Added support for timestamp files created from mkvtoolnix. (Asd-g)
* v1.1 2020-04-15
    - Add 64-bit binary. (Binjohn)
    - Update to AviSynth+ 3.5 library. (Binjohn)
* 2018-02-17
    - Add "start" parameter
* 2012-04-16
    - Initial release