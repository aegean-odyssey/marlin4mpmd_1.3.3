# marlin4mpdm_1.3.3
command-line build for Marlin4MPMD by @mcheah (release 1.3.3)

## Background

I bought a Monoprice MP Mini Delta printer and it's been great fun. Now that I've used the machine for some months, I thought I'd tinker with the firmware. The Marlin4MPMD project makes this possible, but I wanted to use a slightly different build system (emacs, make, gcc) to work on the firmware.

So, the mission was to create a Makefile that could successfully build the Marlin4MPMD firmware from the command line -- without changing any files in the original repository.

Here is the result.

## Build System

The build system is linux based, specifically [Debian "buster"](https://www.debian.org/releases/buster/) with the following additional packages installed:

*(ARM bare metal compiler, libraries, and tools)*

```sh
$ apt-get install build-essential gcc-arm-none-eabi binutils-arm-none-eabi 
$ apt-get install libnewlib-arm-none-eabi libnewlib-dev libstdc++\-arm-none-eabi-newlib 
$ apt-get install gdb-multiarch openocd telnet emacs-nox
```

*(basic git -- for GitHub, ssh, ...)*

```sh
$ apt-get install git quilt patchutils openssh-client ca-certificates gnupg wget curl
```

*(optional, but I find useful)*

```sh
$ apt-get install sudo rename xsltproc picocom zip unzip p7zip-full p7zip-rar
```

## Compiling

Generate two (2) "firmware.bin" files -- one for use with a 60watt power adapter (05Alimit) and one for use with a 120watt power adapter (10Alimit). 

```sh
$ make all
```

Generate a single "firmware.bin" file with the prevailing configuration of the original Marlin4MPDM source (currently matches the 10Alimit configuration).

```sh
$ make
```

## Bugs

If you come across a bug in this marlin4mpmd_1.3.3 firmware that is *__NOT__* present in the original Marlin4MPMD 1.3.3 release of the firmware, please let me know. In this project I do not change any of the original source files, so it will be interesting to see how the compiled versions differ in practice.

## Epilog

I put together what I consider a simple set of development tools to "hack" firmware for the Monoprice MP Mini Delta 3d printer. The build process seems to work and it appears to produce smaller binaries (compared to the Marlin4MPDM binaries).

Of course, success here is only possible because of the enormous amount of effort and dedication in the mcheah/Marlin4MPMD project and the wealth of information collected in that project. Kudos to @mcheah. It is a fabulous resource for MP Mini Delta owners. **Don't just believe me, take a look! :)**

> *visit* [**mcheah/Marlin4MPMD**](https://github.com/mcheah/Marlin4MPMD) (GitHub)

Now that I can successfully build firmware for the Monoprice MP Mini Delta, I may attempt a port of the Marlin firmware directly. I am not sure, though, if there is any advantage over the Marlin4MPMD firmware in doing so. The Mini Delta's motherboard does not offer a large program space, nor any additional i/o, nor otherwise "untapped" hidden features -- so extra capabilities of a later version of Marlin firmware may not be usable on this machine. Perhaps, I'll find out.
