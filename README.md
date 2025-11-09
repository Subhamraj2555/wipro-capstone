# wipro-capstone
Linux System Monitor
A system monitor for Linux developed in C++ 17 like the htop utility based on the ncurses display.

This implementation is based on the starter code for System Monitor Project in the Object Oriented Programming Course of the Udacity C++ Nanodegree Program.

System Monitor

ncurses
ncurses is a library that facilitates text-based graphical output in the terminal. This project relies on ncurses for display output.

You can install ncurses within your own Linux environment: sudo apt install libncurses5-dev libncursesw5-dev

How to build
Clone the project repository: git clone https://github.com/itornaza/cpp-system-monitor.git

Build the project: make build

Run the resulting executable: ./build/monitor

Make
This project uses Make. The Makefile has four targets:

build compiles the source code and generates an executable
format applies ClangFormat to style the source code
debug compiles the source code and generates an executable, including debugging symbols
clean deletes the build/ directory, including all of the build artifacts
System information
System information for the process manager is derived from the following system files:

Kernel information - /proc/version
Operating system - /etc/os-release
Memory utilization - /proc/meminfo
Total processes - /proc/meminfo
Running processes - /proc/meminfo
Up time - /proc/uptime
CPU usage - /proc/stat
Processes information also resides mainly in the /proc/ directory:

PID - /proc/[pid] where pid is in any directory having an integer for its name
UID - /proc/[pid]/status
Username - /etc/passwd
Processor utilization - /proc/[pid]/stat
Memory utilization - /proc/[pid]/stat
Command - /proc/[pid]/cmdline
More information about proc in the man page or enter man proc at the command line.
