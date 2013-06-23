VS1000D Toolchain for Linux
===========================

Overview
--------
This is a toolchain for programming the VS1000D library from Linux. It has been tested on Ubuntu 13.04 using a [Sparkfun VS1000D break-out board](https://www.sparkfun.com/products/8849). The VS1000D is a license-free Ogg-Vorbis chip manufactured by [VLSI](http://www.vlsi.fi). The recommended way of programming the VS1000D is using the VSIDE API, offered by VLSI [here](http://www.vlsi.fi/en/support/software/vside.html). However, this assumes that you are running some sort of Windows environment, or you are prepared to run the application from within a windows emulator, like [WINE](http://www.winehq.com), or within a virtual machine. The goal of this project is to provide a toolchain to help bootstrap development from within a pure Linux environment. One limit is that a USB <-> serial programmer is needed.

This toolchain is the result of merging the following packages:

* The VSKIT 1.3 Linux toolchain from [here](http://www.vlsi.fi/fileadmin/software/VS10XX/vskit130_linux_free_i386.tar.gz)
* The VSKIT 1.34b from [here](http://www.vlsi.fi/fileadmin/software/VS1000/vskit134b.zip)
* The uartcontrol.c example from [here](http://www.vlsi.fi/fileadmin/software/VS1000/uartcontrol030.zip)
* The uspspk.c control example from [here](http://www.vlsi.fi/fileadmin/software/VS1000/usbspk.c)

Please note that some of the files in this repository are covered by a software license. Please refer the VLSI website for further information. 

<b> Flashing the VS1000D in Linux currently requires a suitable USB <-> Serial interface, like an FTDI cable.</b> I use a [3.3V FTDI breakout](https://www.sparkfun.com/products/9873). This is nice because it breaks out a 5V VCC pin, which can be used to power the VS1000D. If you don't own such a device, you can flash the device over the USB in Windows.

Compiling code
--------------
Firstly, you'll need the appropriate build tools. For Debian / Ubuntu run:
 ```
sudo apt-get install build-essential
 ```
Secondly, checkout and compile the code

 ```
git clone git://github.com/asymingt/vs1000d-linux.git
cd vs1000d-linux
make
 ```
If the build is successful, you should see the foillowing
 ```
...
NandType: 3 Large-Page 5-byte addr, 128kB blocks, 256MB flash
I: 0x0050-0x065f In: 6214, out: 6214
X: 0x1fa0-0x2006 In:  212, out:  212
Y: 0x1b32-0x1b33 In:   10, out:   10
I: 0x0040-0x004b In:   54, out:   54
I: 0x0025-0x0025 In:   10, out:   10
In: 6500, out: 6506
size 13
 ```
The output of this command is an image called <b>boot.img</b>. You will need to flash this image to the device using the instructions in the next sections.

Flashing boot.img
-----------------

If you have a USB <-> serial interface, connect its TX-O and RX-I lines to the RX and TX lines of the VS1000D respectively. If you only have a 3.3V cable, you may need to power the device using the onboard USB via a separate USB cable.

If this is the first time that you've used a USB <-> serial cable in Ubuntu, you'll need to add yourself to the group <b>dialout</b>. To do this, type the command below:

 ```
sudo usermod -a -G dialout <your_username>
 ```
 
If you had to type the command above, don't forget to log out and login again.

When the FTDI cable is plugged in, udev will automagically create an interface like <b>/dev/ttyUSB0</b>. Unfortunately, the vs3emu program we will use to flash the VS1000D expects a /dev/ttyS0 location by default, which cannot be changed. We'll therefore create symbolic link mapping /dev/ttyS0 -> /dev/ttyUSB0.


 ```
sudo rm /dev/ttyS0
sudo ln -s /dev/ttyUSB0 /dev/ttyS0
 ```
 
The command above assumes that you don't have any meaningful serial port already attached to /dev/ttyS0. If you do, then you'll need to select a different tty (like /dev/ttyS1) and use the <b>-p</b> switch in vs3emu to select this alternative port.


For both the serial method, you will need to put the VS1000D in a special mode. This involves connecting CS1 to GND (see the excellent [Watterott Article](https://github.com/watterott/KnowledgeBase/wiki/SparkFun-VS1000-Breakout) for precise instructions).

1. Disconnect power from the VS1000D
2. Connect CS1 to GND (see Watterott article for instruction)
3. Apply power to the device
4. Push the POWER / PAUSE switch

A solid amber LED will indicate that you are in the correct mode. If connected to a computer via a USB cable, you will see a partition with a single file 'NO_FLASH'.

Then, in the vs1000d-linux directory type
 ```
make flash
 ```
 
If this worked, you should see something like this:

 ```
 ./toolchain/bin/vs3emu -chip vs1000 -s 115200 -l flasher.bin -c run.cmd
VSEMU 2.2 Nov 12 2010 16:45:47(c)1995-2010 VLSI Solution Oy
Clock 11999 kHz
Using serial port 1, COM speed 115200
Waiting for a connection to the board...

Caused interrupt
Chip version "1000"
Stack pointer 0x19e0, bpTable 0x7d4d
User program entry address 0x4083
flasher.bin: includes optional header, 8 sections, 497 symbols
Section 1: code        page:0 start:80 size:3 relocs:1 fixed
Section 2: puthex      page:0 start:83 size:54 relocs:5
Section 3: const_x     page:1 start:8096 size:110 relocs:0
Section 4: PrintSector  page:0 start:137 size:44 relocs:6
Section 5: main        page:0 start:181 size:202 relocs:66
Section 6: const_y     page:2 start:6962 size:17 relocs:0
Section 7: VS_stdiolib  page:0 start:383 size:262 relocs:83
Section 8: VS_stdiolib$0  page:0 start:645 size:110 relocs:32
info ok
0003 =type
564c 5349 0003 0813 005a 0cbf =size
Erasing
000d =blocks
000d =Writing
000d fclose()
restart
 ```

