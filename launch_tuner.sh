#!/bin/bash

echo "Hello, "$USER".  This script will launch Intel Prefetch Tuner scheduler."
NUM_CORES=8
CPU_FREQ=4200000
read -p "Introduce the quantum length in ms [ENTER]: " quantum_time

echo "a-Dynamic."
echo "b-Static."
read -p "Select the mode of execution [ENTER]: " mode_ex
case "$mode_ex" in
   #case 1
   "a")  echo "Dynamic scheduling selected.";;
   #case 2
   "b")  echo "a-[0,1,2,3,4,5]."
         echo "b-[0]."
         echo "c-[1]."
         echo "d-[2]."
         echo "e-[3]."
         echo "f-[4]."
         echo "g-[5]."
         read -p "Select the mode of execution [ENTER]: " array_ex
         case "$array_ex" in
            #case 1
            "a")  dscr_array="0 1 2 3 4 5";;
            #case 2
            "b")  dscr_array="0";;
            #case 3
            "c")  dscr_array="1";;
            #case 4
            "d")  dscr_array="2";;
            #case 5
            "e")  dscr_array="3";;
            #case 6
            "f")  dscr_array="4";;
            #case 7
            "g")  dscr_array="5";;
            *) echo "Invalid entry for DSCR array."
               exit 1 # Exit script
         esac ;;

   #case 3
   *) echo "Invalid entry for mode_ex."
      exit 1# Exit script
esac

echo "a-Single-Core."
echo "b-Multi-Core."
read -p "Select type of execution [ENTER]: " type_ex
read -p "Introduce the name of the directory to save results [ENTER]: " dir_out_ex
case "$type_ex" in
   #case 1 #Single-core
   "a")  if mkdir Intel_Prefetch_Tuner_res/singlecore/${dir_out_ex} ; then
            echo \#Directory created working_dir/Intel_Prefetch_Tuner_res/singlecore/${dir_out_ex}\#
         else
            echo ""
            echo \#Error creating directory working_dir/Intel_Prefetch_Tuner_res/singlecore/${dir_out_ex}\#
            echo \#The directory may already exist\#
            echo ""
         fi
         echo "a-2006 apps & microbenchmark."
         echo "b-Geekbench5."
         echo "c-All apps."
         echo "d-microbenchmark."
         read -p "Select the applications to execute [ENTER]: " wk_ex
         case "$wk_ex" in
            #case 1
            "a") workloadArray="0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26";;## 2006 & microbenchmark applications.
            #case 2 
            "b") workloadArray="27" ;;# Geekbench5.
            #case 3 
            "c") workloadArray="0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27" ;;## All applications.
            #case 4 
            "d") workloadArray="20" ;;## microbenchmark.
            #case 5 
            *) echo "Invalid entry for workloadArray."
               exit 1 # Exit script
         esac
         # { sudo ./start_experiments.sh -c $NUM_CORES -f $CPU_FREQ; }
         case "$mode_ex" in
            "a") for workload in $workloadArray #Dynamic Single-core
                  do
                     sudo ./Intel_Prefetch_Tuner -m 0 -d ${quantum_time} -o "Intel_Prefetch_Tuner_res/singlecore/${dir_out_ex}/trabajo[${workload}]conf[auto]rep[0]core" -A 0 -B $workload -C 0 2>> Intel_Prefetch_Tuner_res/singlecore/${dir_out_ex}/outTrabajo[${workload}]conf[auto]rep[0].txt
                  done ;;
            "b") for workload in $workloadArray #Static Single-core
                  do
                     for dscr in $dscr_array
                     do
                        sudo ./Intel_Prefetch_Tuner -m 1 -d ${quantum_time} -o "Intel_Prefetch_Tuner_res/singlecore/${dir_out_ex}/trabajo[${workload}]conf[${dscr}]rep[0]core" -A 0 -B $workload -C $dscr 2>> Intel_Prefetch_Tuner_res/singlecore/${dir_out_ex}/outTrabajo[${workload}]conf[${dscr}]rep[0].txt
                     done
                  done ;;
            *) echo "Invalid entry to be executed."
               # { sudo ./end_experiments.sh -c $NUM_CORES; }
               exit 1 # Exit script
         esac ;;
   #case 2 #Multi-core
   "b")  if mkdir Intel_Prefetch_Tuner_res/multicore/${dir_out_ex} ; then
            echo \#Directory created working_dir/Intel_Prefetch_Tuner_res/multicore/${dir_out_ex}\#
         else
            echo ""
            echo \#Error creating directory working_dir/Intel_Prefetch_Tuner_res/multicore/${dir_out_ex}\#
            echo \#The directory may already exist\#
            echo ""
         fi
         echo "a-workloads 4-app."
         echo "b-worloads 6-app."
         echo "c-worloads 8-app."
         echo "d-validation workloads."
         echo "e-worloads all."
         read -p "Select the workloads to execute [ENTER]: " wk_ex
         case "$wk_ex" in
            #case 1
            "a") workloadArray="2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21";;## 4 applications workload.
            #case 2 
            "b") workloadArray="22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41" ;;# 6 applications workload.
            #case 3 
            "c") workloadArray="42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61" ;;## 8 applications workload.
            #case 4 
            "d") workloadArray="63 64 65 66 67 68 69 70 71 72 73 74" ;;## Validation workloads.
            #case 5 
            "e") workloadArray="2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74" ;;# Custom applications workload.
            #case 6 
            *) echo "Invalid entry for workloadArray."
               exit 1 # Exit script
         esac
         # { sudo ./start_experiments.sh -c $NUM_CORES -f $CPU_FREQ; }
         case "$mode_ex" in
            "a") for workload in $workloadArray #Dynamic Multi-core
                  do
                     sudo ./Intel_Prefetch_Tuner -m 0 -d ${quantum_time} -o "Intel_Prefetch_Tuner_res/multicore/${dir_out_ex}/trabajo[${workload}]conf[auto]rep[0]core" -A $workload 2>> Intel_Prefetch_Tuner_res/multicore/${dir_out_ex}/outTrabajo[${workload}]conf[auto]rep[0].txt
                  done ;;
            "b") for workload in $workloadArray #Static Multi-core
                  do
                     for dscr in $dscr_array
                     do
                        sudo ./Intel_Prefetch_Tuner -m 1 -d ${quantum_time} -o "Intel_Prefetch_Tuner_res/multicore/${dir_out_ex}/trabajo[${workload}]conf[${dscr}]rep[0]core" -A $workload -C $dscr 2>> Intel_Prefetch_Tuner_res/multicore/${dir_out_ex}/outTrabajo[${workload}]conf[${dscr}]rep[0].txt
                     done
                  done ;;
            *) echo "Invalid entry to be executed."
               # { sudo ./end_experiments.sh -c $NUM_CORES; }
               exit 1 # Exit script
         esac ;;
   #case 3
   *) echo "Invalid entry for type_ex."
      exit 1 # Exit script
esac

##END SCRIPT##
# { sudo ./end_experiments.sh -c $NUM_CORES; }