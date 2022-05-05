#!/bin/bash

## Intel Prefetch Tuner.
## Author: Manel Lurbe Sempere
## e-mail: malursem@gap.upv.es
## Year: 2022

helpFunction()
{
   echo ""
   echo "usage: sudo ./end_experiments.sh -c num_cores"
   exit 1 # Exit script after printing help
}

# Get the arguments
while getopts "c:" opt
do
   case "$opt" in
	  c ) NUM_CORES="$OPTARG" ;;
      ? ) helpFunction ;; # Print helpFunction in case parameter is non-existent
   esac
done

# Print helpFunction in case parameters are empty
if [ -z "$NUM_CORES" ]
then
   echo "Missing arguments.";
   helpFunction
fi

ONE=1
let num_procesors=$NUM_CORES-$ONE
echo "Available cpu cores" $NUM_CORES
for cpu in $(seq 0 $num_procesors);
do
	{ sudo cpufreq-set -g ondemand -c $cpu; }
	echo "Core" $cpu "free..."
done;

echo "CPU Free..."
