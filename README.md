# mopidy-gpiovolume

Mopidy volume control and standby control for a hacked Logitech Z-680 through GPIO

Hardware description can be found here:

  https://hackaday.io/project/21792-logitech-z-680-music-streamer

# Installation

I'll try to describe everything i've used to install and make this work, i'm working with Raspbian Jessie. I'm assuming you downloaded this repository in your pi home directory and are running from that directory.

## Instal the cpp service

Install the kernel headers (don't know if the headers is used where allready there, so maybe this can be skipped)
```
$ sudo apt-get install raspberrypi-kernel-headers
```

Install WiringPi (used to drive the GPIO)
```
$ sudo apt-get install wiringpi
```

Compile volume control program:
```
$ gcc -Wall -o gpiovolume mopidy-gpiovolume/gpiovolume.cpp -lwiringPi -lrt
```

Create a service file in /etc/systemd/system/gpiovolume.service
```
[Unit]
Description=Volume Control by GPIO
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/
ExecStart=/home/pi/gpiovolume
Restart=on-abort

[Install]
WantedBy=multi-user.target
```
You might want to change the location for the process to be in /usr/sbin. I left it in the home so i can easily recompile it.

Start the service
```
$ sudo systemctl start gpiovolume.service
```

You can check if it is running with
```
ps -aux | grep gpiovolume
```

There should be a gpiovolume entry in /dev/shm this is the shared memory file that holds the volume and standby status. The cpp service is following those values.


## Install the mopidy plug in

Install the Python dev tools (needed to install possix_ipc)
```
$ sudo apt-get install python-dev
```

Install posix_ipc
```
$ sudo pip install posix_ipc
```

Install gpiovolume
```
$ sudo pip install mopidy-gpiovolume
```

Edit the Mopidy configuration to use the volume mixer in /etc/mopidy/mopidy.conf
```
$ [audio]
mixer = gpiovolume
mixer_volume = 15
```

Restart mopidy and everything should be working
