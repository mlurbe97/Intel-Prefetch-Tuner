# Intel Prefetch Tuner

## Author

* Manel Lurbe Sempere (malursem@gap.upv.es)

## Abstract

Prefetch tuner dinamically when executes an application on Intel procesors.

## Install, compile and run

- Install pre-requisites.
    ```
    sudo apt update && sudo apt upgrade
    sudo apt install gcc g++ build-essential cpufrequtils cpuid
    ```
- To run a 32-bit executable file on a 64-bit multi-architecture:
    ```
    sudo dpkg --add-architecture i386
    sudo apt-get update
    sudo apt-get install libc6:i386 libncurses5:i386 libstdc++6:i386
    sudo apt-get install multiarch-support
    ```
- Check the number of HPC available on the processor cores in your system, type this command and you will find.
    ```
    cpuid -1 | grep counters
    ```
- Check available processor cores in your system, type this command and you will find.
    ```
    nproc
    ```
    For me the output was:
    ```
    6
    ```
    This means that I have 6 processor cores from 0 to 5.

- Check maximum cpu frequency of your processor cores.
    ```
    cpufreq-info
    ```
    You will find something like this, that is auto explanatory:
    ```
    hardware limits: 800 MHz - 4.10 GHz
    available cpufreq governors: performance, powersave
    current policy: frequency should be within 800 MHz and 4.10 GHz.
    ```

- Disable intel_pstate on the kernel boot to control the frequency of the CPU.
    ```
    sudo nano /etc/default/grub
    ```
    Add the following line to the file:
    ```
    GRUB_CMDLINE_LINUX_DEFAULT="intel_pstate=disable"
    ```
    Then run the command:
    ```
    sudo update-grub
    ```
    Finally reboot the machine:
    ```
    sudo reboot
    ```

- Prepare the workspace.
    ```
    mkdir $HOME/woriking_dir/
    cd $HOME/woriking_dir/
    ```

- Install SPEC CPU applications inside the workspace on the folders.
    ```
    $working_dir/spec2006-x86-bin/
    $working_dir/spec2017-x86-bin/
    ```

- Place SPEC CPU refence inputs inside the workspace on the folders.
    ```
    $working_dir/CPU2006/
    $working_dir/CPU2017/
    ```

- Install libpfm4 library.
    ```
    cd $HOME/woriking_dir/
    git clone https://github.com/wcohen/libpfm4.git
    cd libpfm4/
    make
    sudo make install
    ```

- Clone the repo inside the workspace.
    ```
    cd $HOME/woriking_dir/
    git clone https://github.com/mlurbe97/Intel-Prefetch-Tuner.git
    ```

- Compile the scheduler.
    ```
    cd $HOME/woriking_dir/Intel-Prefetch-Tuner/
    chmod +x compile.sh
    ./compile.sh
    ```
- Run the scheduler.
    ```
    cd $HOME/woriking_dir/Intel-Prefetch-Tuner/
    chmod +x launch_tuner.sh
    cp launch_tuner.sh $HOME/working_dir/
    cd $HOME/working_dir/
    sudo ./launch_tuner.sh
    ```
