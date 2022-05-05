#!/bin/bash

## Intel Prefetch Tuner.
## Author: Manel Lurbe Sempere
## e-mail: malursem@gap.upv.es
## Year: 2022

helpFunction()
{
   echo ""
   echo "usage: sudo ./start_experiments.sh -c num_cores -f cpu_freq"
   exit 1 # Exit script after printing help
}

# Get the arguments
while getopts "c:f:" opt
do
   case "$opt" in
	  c ) NUM_CORES="$OPTARG" ;;
	  f ) CPU_FREQ="$OPTARG" ;;
      ? ) helpFunction ;; # Print helpFunction in case parameter is non-existent
   esac
done

# Print helpFunction in case parameters are empty
if [ -z "$CPU_FREQ" ] || [ -z "$NUM_CORES" ]
then
   echo "Missing arguments.";
   helpFunction
fi

ONE=1
let num_procesors=$NUM_CORES-$ONE
echo "Available cpu cores" $NUM_CORES
for cpu in $(seq 0 $num_procesors);
do
	{ sudo cpufreq-set -g userspace -c $cpu; }
	echo "Core" $cpu "ready..."
done;

{ sudo cpufreq-set -f $CPU_FREQ; }

echo "CPU Ready..."