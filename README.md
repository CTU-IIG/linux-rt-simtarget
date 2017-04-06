# Support tools and enhancements of Simulink Linux Target (Raspberry Pi and others) 

is a set of Matlab Simulink patches that secures meeting real-time of Matlab-generated C code. It has been developed at the Czech Technical University in Prague, Faculty of Electrical Engineering, [Department of Control Engineering][l_dce]. 

## Software Requirements

Linux-rt-simtarget for Matlab Simulink currently supports MATLAB R2016b with [Raspberry Pi Support from Simulink][l_hw_rpi] running on Linux. 

## Installation

Download the patches and files from github (clone git repository or Downnload ZIP) and copy/unpack them into the Matlab root directory. It's a good idea to init a git repository in the Matlab root directory. Then it's recommanded to gitignore everything and git-add only files that are going to be modified. 

    	git init
	echo "*" > .gitignore
	git add -f `cat tracked_files`

Then apply the patches

	patch -p1 < x-<name>.patch #where 'x' stands for patch number

For example

	patch -p1 < 1-mlockall.patch 
	patch -p1 < 2-nanosleep.patch

The patches are supposed to be run along with RT_PREEMPT patched Linux Kernel. Recent version of patched kernel for Raspberry Pi can be found [here][l_ppisa_rpi_linux]. To build the kernel follow [Kernel building for Raspberry Pi][l_kernel_build]

##Documentation

These patches modify Matlab TLC and C files which provides ert c-code generation. Main intention is to improve RT-bahaviour and make more tranparent, how TLC files cooperate in code generation.
    
<b>1-mlockall.patch</b>
    
Added 'mlockall' directive to be generated to 'ert_main.c' to improve RT-behaviour. This system call ensures that no memory page of the program code is swapped out to the disk. Such an action would result in latency up to few milliseconds.
    
<b>2-nanosleep.patch</b>
    
Model sample loop timing modified. Model timing which uses 'waitForTimerEvent' which subsequently uses 'read' syscall replaced by 'clock_nanosleep'. This patch should improve RT capabilities of the running code.

[l_dce]: http://dce.fel.cvut.cz/en/
[l_ppisa_rpi_linux]:https://github.com/ppisa/linux-rpi/tree/rpi-4.9.y-rebase-rt-ppisa
[l_kernel_build]: https://www.raspberrypi.org/documentation/linux/kernel/building.md
