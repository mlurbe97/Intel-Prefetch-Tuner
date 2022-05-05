#!/bin/bash

## Intel Prefetch Tuner.
## Author: Manel Lurbe Sempere
## e-mail: malursem@gap.upv.es
## Year: 2022

rm -rf ../libpfm4/perf_examples/Intel_Prefetch_Tuner*

if chmod +x launch_tuner.sh start_experiments.sh end_experiments.sh ; then
	echo "INFO: Execution permissions granted to launch_tuner.sh, start_experiments.sh and end_experiments.sh successfully."
else
	echo "ERROR: Failed to give execution permissions to launch_tuner.sh, start_experiments.sh and end_experiments.sh."
fi

if cp -rf launch_tuner.sh ../ ; then
   echo "INFO: launch_tuner.sh script copied to working dir."
else
   echo "ERROR: Failed to copy launch_tuner.sh script."
fi

if cp -rf start_experiments.sh ../ ; then
   echo "INFO: start_experiments.sh script copied to working dir."
else
   echo "ERROR: Failed to copy start_experiments.sh script."
fi

if cp -rf end_experiments.sh ../ ; then
   echo "INFO: end_experiments.sh script copied to working dir."
else
   echo "ERROR: Failed to copy end_experiments.sh script."
fi

if cp src/Makefile src/Intel_Prefetch_Tuner.c ../libpfm4/perf_examples/ ; then
	if cd ../libpfm4/perf_examples/ ; then
		if make ; then
			if cp -rf Intel_Prefetch_Tuner ../../ ; then
				echo "INFO: Process finished successfully."
			else
				echo "ERROR: Failed to copy programs."
			fi
		else
			echo "ERROR: Failed to compile the program."
		fi
	else
		echo "ERROR: Failed to change the directory."
	fi
else
	echo "ERROR: Failed to copy files."
fi

cd $HOME/working_dir/
OUTDIR=Intel_Prefetch_Tuner_res/
if mkdir $OUTDIR ; then
   echo "INFO: Directory for results created."
else
   echo "WARN: Failed to create directory for results. It may already exist."
fi
if mkdir $OUTDIR/singlecore ; then
   echo "INFO: Directory for singlecore results created."
else
   echo "WARN: Failed to create directory for singlecore results. It may already exist."
fi
if mkdir $OUTDIR/multicore ; then
   echo "INFO: Directory for multicore results created."
else
   echo "WARN: Failed to create directory for multicore results. It may already exist."
fi