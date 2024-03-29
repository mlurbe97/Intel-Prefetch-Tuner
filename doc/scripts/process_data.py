#!/usr/bin/python
# -*- coding: utf-8 -*-
import sys
import matplotlib.pyplot as plt
import numpy as np

program = sys.argv[0]
dir = sys.argv[1]
benchmarks = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47]
benchmarksNames = ["perlbench checkspam","bzip2","gcc","mcf","gobmk","hmmer","sjeng","libquantum","h264ref","omnetpp","astar","xalancbmk","bwaves","gamess",
"milc","zeusmp","gromacs","cactusADM","leslie3d","namd","microbench","soplex","povray","GemsFDTD","lbm",
"perlbench diffmail","calculix","AES-XTS","Text Compression","Image Compression","Navigation","HTML5","SQLite",
"PDF Rendering","Text Rendering","Clang","Camera","N-Body Physics","Rigid Body Physics","Gaussian Blur","Face Detection",
"Horizon Detection","Image Inpainting","HDR","Ray Tracing","Structure from Motion","Speech Recognition","Machine Learning"]
repetitions=[0]
configs = [0,1,2,3,4,5]
cores = [0]

# 0				1                   2		3				4		5		6
# instructions	total_instructions	cycles	total_cycles	cont0	cont1	cont2
for core in cores:
    for bench in benchmarks:
        results_ipc = open("data_bench["+str(bench)+"]core["+str(core)+"].csv", 'w')
        for rep in repetitions:
            values = []
            for conf in configs:
                instructions = 0.0
                cycles = 0.0
                try:
                    fitx = open(dir+"trabajo["+str(bench)+"]conf["+str(conf)+"]rep["+str(rep)+"]core["+str(core)+"].txt")
                except:
                    continue
                else:
                    primer = 0
                    for linea in fitx.read().split("\n"):
                        if primer == 0:#Evitar el texto de inicio
                            primer = 1
                        else:
                            columns = linea.split("\t")
                            try:
                                instructions = instructions + float(columns[0])
                                cycles = cycles + float(columns[2])
                            except:
                                continue
                    try:
                        ipc = float(instructions/cycles)
                        String = str(bench)+"\t"+str(conf)+"\t"+str(ipc)+"\n"
                        values.append(ipc)
                        results_ipc.write(String)
                    except:
                        continue
                    fitx.close()
            y_pos = np.arange(len(configs))
            plt.bar(y_pos, values, tick_label = configs, width = 0.8, color = ['red','blue','#b5ffb9', '#f9bc86','#a3acff','#2d7f5e'])

            # Layouts
            plt.ylabel('IPC',fontweight='bold')
            plt.xlabel('MSR',fontweight='bold')

            # Title
            title = "IPC : "+benchmarksNames[benchmarks.index(bench)]
            plt.title(title,fontweight='bold')

            # Auto layout
            plt.tight_layout()

            # Save graphic
            figName = title.replace(" ", "_")+'['+str(core)+'].png'
            figName = figName.replace("_:","")
            plt.savefig("./" + figName)
            plt.clf()
            plt.close()
            fitx.close()
                    
        results_ipc.close()

