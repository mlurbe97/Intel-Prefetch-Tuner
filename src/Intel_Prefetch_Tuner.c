/*
## Intel Prefetch Tuner.
## Author: Manel Lurbe Sempere
## e-mail: malursem@gap.upv.es
## Year: 2022
*/

/*************************************************************
 **                  Includes                               **
 *************************************************************/

#include <wait.h>
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <err.h>
#include <sys/poll.h>
#include <sched.h>
#include "perf_util.h"
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>

/*************************************************************
 **                   Defines                               **
 *************************************************************/

#define N_MAX 8// Number of processor cores counting SMT logical cores.

/*************************************************************
 **                   Structs                               **
 *************************************************************/

typedef struct {
	char *events;
	int delay;//Quantum duration (ms)
	int pinned;
	int group;
	char *output_directory;
	int mode;//Dynamic 0, static 1
} options_t;

typedef struct {//Defines the variables of an application or process. Represents a core of the processor.
	pid_t pid;//Process id
	int benchmark;//Application to run
	int finished;//Have you completed the target instructions?
	int status;
	int core;	// Core where it runs
	uint64_t counters [45];
	perf_event_desc_t *fds;
	int num_fds;
	cpu_set_t mask;
	char *core_out;
	/*
		To save the information of each application in each quantum.
	*/
	uint64_t total_instructions;//Total instructions executed by the application.
	uint64_t total_cycles;//Total cycles executed by the application.
	uint64_t *my_Counters;//Counter array where the different counters are stored in each quantum.
	uint64_t instruction;//Instructions executed in the last quantum.
	uint64_t cycles;//Cycles executed in the last quantum.
	int actual_MSR;//Current msr configuration.
	float actual_IPC;//IPC of the measurement interval.
	float actual_BW;
	float sum_BW;

	//For the previous intervals we keep the CPI reached.
	float last_IPC_1;
	float last_IPC_2;
	float last_IPC_3;

	//For the previous intervals we save the consumed BW
	float last_BW_1;
	float last_BW_2;
	float last_BW_3;
} node;

/*************************************************************
 **                   Global Variables                      **
 *************************************************************/

node queue [N_MAX];//Queue where all running applications are stored. They are the cores of the processor.
static options_t options;

int end_experiment = 0;
int N;//Number of running applications
int workload;//Workload number

float sum_BW;

const int num_Counters = 3;//Indicates the number of counters that are programmed, 4 per quantum. Instructions and cycles go apart.
const float CPU_FREQ = 2500.0;//Processor frequency at which we launch the experiments.

//Reading and writing files.
FILE* core_Info;

// Overhead time
//clock_t overhead;

// Intel Prefetch register MSR prefetch configurations.
const uint32_t msr_prefetch_reg = 0x1a4;
// 0 All prefetchers enabled.
// 1 L2 Hardware prefetcher.
// 2 L2 adjacent cache line prefetcher.
// 3 DCU prefetcher.
// 4 DCU IP prefetcher.
// 5 All prefetchers disabled.
const uint64_t msr_stats [] = {0x0,0xE,0xD,0xB,0x7,0xF};

// Length of regvals.
const int prefetch_configs [] = {0,1,2,3,4,5};
const int num_prefetch_configs = 6;

/*************************************************************
 **                   Benchmarks SPEC2006-2017              **
 *************************************************************/

char *benchmarks[][200] = {
  //* SPEC CPU 2006 *//
  // 0 -> perlbench checkspam
  {"spec2006-x86-bin/400.perlbench/perlbench_base.i386", "-ICPU2006/400.perlbench/data/all/input/lib", "CPU2006/400.perlbench/data/ref/input/checkspam.pl", "2500", "5", "11", "150", "1", "1", "1", "1", ">", "CPU2006/400.perlbench/data/ref/output/checkspam.2500.5.25.11.150.1.1.1.1.out", NULL},
  // 1 -> bzip2
  {"spec2006-x86-bin/401.bzip2/bzip2_base.i386", "CPU2006/401.bzip2/data/all/input/input.combined", "200", NULL},
  // 2 -> gcc
  {"spec2006-x86-bin/403.gcc/gcc_base.i386", "CPU2006/403.gcc/data/ref/input/scilab.i", "-o", "scilab.s", NULL},
  // 3 -> mcf
  {"spec2006-x86-bin/429.mcf/mcf_base.i386", "CPU2006/429.mcf/data/ref/input/inp.in", NULL},
  // 4 -> gobmk
  {"spec2006-x86-bin/445.gobmk/gobmk_base.i386", "--quiet", "--mode", "gtp", NULL},
  // 5 -> hmmer
  {"spec2006-x86-bin/456.hmmer/hmmer_base.i386", "--fixed", "0", "--mean", "500", "--num", "500000", "--sd", "350", "--seed", "0", "CPU2006/456.hmmer/data/ref/input/retro.hmm", NULL},
  // 6 -> sjeng
  {"spec2006-x86-bin/458.sjeng/sjeng_base.i386", "CPU2006/458.sjeng/data/ref/input/ref.txt", NULL},
  // 7 -> libquantum
  {"spec2006-x86-bin/462.libquantum/libquantum_base.i386", "1397", "8", NULL},
  // 8 -> h264ref
  {"spec2006-x86-bin/464.h264ref/h264ref_base.i386", "-d", "CPU2006/464.h264ref/data/ref/input/foreman_ref_encoder_baseline.cfg", NULL},
  // 9 -> omnetpp
  {"spec2006-x86-bin/471.omnetpp/omnetpp_base.i386", "CPU2006/471.omnetpp/data/ref/input/omnetpp.ini", NULL},
  // 10 -> astar
  {"spec2006-x86-bin/473.astar/astar_base.i386", "CPU2006/473.astar/data/ref/input/BigLakes2048.cfg", NULL},
  // 11 -> xalancbmk
  {"spec2006-x86-bin/483.xalancbmk/xalancbmk_base.i386", "-v", "CPU2006/483.xalancbmk/data/ref/input/t5.xml", "CPU2006/483.xalancbmk/data/ref/input/xalanc.xsl", NULL},
  // 12 -> bwaves
  {"spec2006-x86-bin/410.bwaves/bwaves_base.i386", NULL},
  // 13 -> gamess
  {"spec2006-x86-bin/416.gamess/gamess_base.i386", NULL},
  // 14 -> milc
  {"spec2006-x86-bin/433.milc/milc_base.i386", NULL},
  // 15 -> zeusmp
  {"spec2006-x86-bin/434.zeusmp/zeusmp_base.i386", NULL},
  // 16 -> gromacs
  {"spec2006-x86-bin/435.gromacs/gromacs_base.i386", "-silent", "-deffnm", "CPU2006/435.gromacs/data/ref/input/gromacs", "-nice", "0", NULL},
  // 17 -> cactusADM
  {"spec2006-x86-bin/436.cactusADM/cactusADM_base.i386", "CPU2006/436.cactusADM/data/ref/input/benchADM.par", NULL},
  // 18 -> leslie3d
  {"spec2006-x86-bin/437.leslie3d/leslie3d_base.i386", NULL},
  // 19 -> namd
  {"spec2006-x86-bin/444.namd/namd_base.i386", "--input", "CPU2006/444.namd/data/all/input/namd.input", "--iterations", "38", "--output", "namd.out", NULL},
  // 20 -> microbench
  {"microbench", "100", "0", "1024", "0"},
  // 21 -> soplex
  {"spec2006-x86-bin/450.soplex/soplex_base.i386", "-s1", "-e","-m45000", "CPU2006/450.soplex/data/ref/input/pds-50.mps", NULL},
  //22 -> povray
  {"spec2006-x86-bin/453.povray/povray_base.i386", "CPU2006/453.povray/data/ref/input/SPEC-benchmark-ref.ini", NULL},
  // 23 -> GemsFDTD
  {"spec2006-x86-bin/459.GemsFDTD/GemsFDTD_base.i386", NULL},
  // 24 -> lbm
  {"spec2006-x86-bin/470.lbm/lbm_base.i386", "300", "reference.dat", "0", "1", "CPU2006/470.lbm/data/ref/input/100_100_130_ldc.of", NULL},
  // 25 -> perlbench diffmail
  {"spec2006-x86-bin/400.perlbench/perlbench_base.i386", "-ICPU2006/400.perlbench/data/all/input/lib", "CPU2006/400.perlbench/data/all/input/diffmail.pl", "4", "800", "10", "17", "19", "300", ">", "CPU2006/400.perlbench/data/ref/output/diffmail.4.800.10.17.19.300.out", NULL},
  // 26 -> calculix
  {"spec2006-x86-bin/454.calculix/calculix_base.i386", "-i", "CPU2006/454.calculix/data/ref/input/hyperviscoplastic", NULL},
  //* Geekbench 5 *//
  // 27 -> geekbench5 AES-XTS
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "101", "--no-upload", NULL},
  // 28 -> geekbench5 Text Compression
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "201", "--no-upload", NULL},
  // 29 -> geekbench5 Image Compression
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "202", "--no-upload", NULL},
  // 30 -> geekbench5 Navigation
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "203", "--no-upload", NULL},
  // 31 -> geekbench5 HTML5
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "204", "--no-upload", NULL},
  // 32 -> geekbench5 SQLite
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "205", "--no-upload", NULL},
  // 33 -> geekbench5 PDF Rendering
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "206", "--no-upload", NULL},
  // 34 -> geekbench5 Text Rendering
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "207", "--no-upload", NULL},
  // 35 -> geekbench5 Clang
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "208", "--no-upload", NULL},
  // 36 -> geekbench5 Camera
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "209", "--no-upload", NULL},
  // 37 -> geekbench5 N-Body Physics
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "301", "--no-upload", NULL},
  // 38 -> geekbench5 Rigid Body Physics
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "302", "--no-upload", NULL},
  // 39 -> geekbench5 Gaussian Blur
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "303", "--no-upload", NULL},
  // 40 -> geekbench5 Face Detection
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "305", "--no-upload", NULL},
  // 41 -> geekbench5 Horizon Detection
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "306", "--no-upload", NULL},
  // 42 -> geekbench5 Image Inpainting
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "307", "--no-upload", NULL},
  // 43 -> geekbench5 HDR
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "308", "--no-upload", NULL},
  // 44 -> geekbench5 Ray Tracing
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "309", "--no-upload", NULL},
  // 45 -> geekbench5 Structure from Motion
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "310", "--no-upload", NULL},
  // 46 -> geekbench5 Speech Recognition
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "312", "--no-upload", NULL},
  // 47 -> geekbench5 Machine Learning
  {"Geekbench-5.4.4-Linux/geekbench5", "--arch", "64bit", "--section", "1", "--workload", "313", "--no-upload", NULL},
};

/*************************************************************
 **                   Name of the benchmarks	            **
 *************************************************************/

char *bench_Names [] = {
	"perlbench checkspam","bzip2","gcc","mcf","gobmk","hmmer","sjeng","libquantum",//0--7
	"h264ref","omnetpp","astar","xalancbmk","bwaves","gamess","milc","zeusmp",//8--15
	"gromacs","cactusADM","leslie3d","namd","microbench","soplex","povray","GemsFDTD",//16--23
	"lbm","perlbench diffmail","calculix","AES-XTS","Text Compression","Image Compression","Navigation","HTML5",//24--31
	"SQLite","PDF Rendering","Text Rendering","Clang","Camera","N-Body Physics","Rigid Body Physics","Gaussian Blur",//32--39
	"Face Detection","Horizon Detection","Image Inpainting","HDR","Ray Tracing","Structure from Motion","Speech Recognition","Machine Learning"//40--47
};

/*************************************************************
 **            Instructions to execute by benchmark		    **
 *************************************************************/

// 180 seconds (3 minutes)
unsigned long int bench_Instructions [] = {
	1619205006913,1200061142955,1463320412753,624319920683,1149822187650,1150033733701,1339142491224,2025499504943,//0--7
	2133497650136,510454602107,779666954202,884730388990,1947779886365,2320875771459,557671130651,1068441751620,//8--15
	1579500040113,398323762733,2019839187820,1946051008345,199994547221,820583157740,2093468615595,1302506404469,//16--23
	1341363523490,2490561507138,1809792889595,1470297856,604696974,1144752655,733831893,1446727555,//24--31
	1447257711,1230417460,1445692229,1229243327,1336661692,862638812,1229501274,819878364,//32--39
	1358862957,948528396,1358843831,970025216,1357382333,668089513,905371624,1551660190//40--47
};

/*************************************************************
 **						mix size							**
 *************************************************************/

int bench_mixes [] = { // Number of charges contained in the mix
	1,	// 0	DO NOT MODIFY selected application alone.
	4,	// 1	DO NOT MODIFY Benchmark and selected application.

	// Mixes of 6 apps
	4,	// 2
	4,	// 3
	4,	// 4
	4,	// 5
	4,	// 6
	4,	// 7
	4,	// 8
	4,	// 9
	4,	// 10
	4,	// 11
	4,	// 12
	4,	// 13
	4,	// 14
	4,	// 15
	4,	// 16
	4,	// 17
	4,	// 18
	4,	// 19
	4,	// 20
	4,	// 21

	// Mixes of 6 apps
	6,	// 22
	6,	// 23
	6,	// 24
	6,	// 25
	6,	// 26
	6,	// 27
	6,	// 28
	6,	// 29
	6,	// 30
	6,	// 31
	6,	// 32
	6,	// 33
	6,	// 34
	6,	// 35
	6,	// 36
	6,	// 37
	6,	// 38
	6,	// 39
	6,	// 40
	6,	// 41

	// Mixes of 8 apps
	8, // 42
	8, // 43
	8,	// 44
	8,	// 45
	8,	// 46
	8,	// 47
	8,	// 48
	8,	// 49
	8,	// 50
	8,	// 51
	8,	// 52
	8,	// 53
	8,	// 54
	8,	// 55
	8,	// 56
	8,	// 57
	8,	// 58
	8,	// 59
	8,	// 60
	8,	// 61

	//Microbech full bandwidth workload
	8, // 62

	//Validation workloads
	4, // 63
	4, // 64
	4, // 65
	4, // 66
	6, // 67
	6, // 68
	6, // 69
	6, // 70
	8, // 71
	8, // 72
	8, // 73
	8 // 74

	
};

/*************************************************************
 **                 Mix composition			                **
 *************************************************************/

int workload_mixes [][12] = { // MIXES
	{-1},// 0	DO NOT MODIFY selected application alone.
	{-1,20,20,20},// 1	DO NOT MODIFY Benchmark and selected application.
	
	// Mixes of 4 apps
	{8,6,20,15}, // 2
	{1,19,15,23}, // 3
	{11,2,25,15}, // 4
	{21,16,14,5}, // 5
	{1,1,5,21}, // 6
	{10,13,18,20}, // 7
	{22,5,7,22}, // 8
	{14,14,3,12}, // 9
	{14,14,18,5}, // 10
	{19,10,9,4}, // 11
	{14,26,25,3}, // 12
	{5,21,11,24}, // 13
	{16,15,3,19}, // 14
	{19,23,19,20}, // 15
	{1,17,12,19}, // 16
	{20,18,10,26}, // 17
	{15,2,18,3}, // 18
	{14,8,10,8}, // 19
	{16,13,7,12}, // 20
	{19,3,25,8}, // 21

	// Mixes of 6 apps
	{12,26,24,13,2,14}, // 22
	{5,16,15,1,4,15}, // 23
	{2,20,19,2,18,4}, // 24
	{1,18,25,13,7,11}, // 25
	{16,18,13,24,19,20}, // 26
	{1,2,25,24,26,17}, // 27
	{9,22,6,22,20,1}, // 28
	{15,3,12,8,20,9}, // 29
	{4,8,11,20,26,10}, // 30
	{16,20,5,8,11,5}, // 31
	{21,8,24,17,8,1}, // 32
	{16,18,17,20,9,5}, // 33
	{16,17,26,7,23,18}, // 34
	{16,18,2,23,8,25}, // 35
	{17,9,7,25,7,1}, // 36
	{9,14,24,3,18,4}, // 37
	{4,17,9,23,26,5}, // 38
	{3,14,11,14,17,9}, // 39
	{5,14,16,22,23,6}, // 40
	{15,19,4,7,18,4}, // 41

	// Mixes of 8 apps
	{25,24,3,2,6,19,15,9}, // 42
	{1,19,15,9,14,25,4,12}, // 43
	{21,16,3,16,15,7,11,5}, // 44
	{10,9,3,22,9,17,22,3}, // 45
	{17,24,15,6,23,17,3,9}, // 46
	{14,10,11,4,3,2,2,5}, // 47
	{9,16,14,21,21,16,22,15}, // 48
	{24,25,6,16,25,8,1,16}, // 49
	{24,2,6,8,20,21,14,2}, // 50
	{24,25,10,6,21,9,24,13}, // 51
	{6,12,14,23,10,25,7,21}, // 52
	{8,18,9,13,9,2,8,11}, // 53
	{18,15,8,24,12,17,3,26}, // 54
	{6,3,15,18,17,21,13,7}, // 55
	{2,5,10,16,22,10,23,21}, // 56
	{22,6,13,17,22,24,23,23}, // 57
	{25,2,26,24,5,9,15,9}, // 58
	{10,8,4,10,25,24,21,15}, // 59
	{26,25,14,16,25,23,16,23}, // 60
	{22,15,12,20,11,7,26,5}, // 61

	//Microbech full bandwidth workload
	{20,20,20,20,20,20,20,20}, // 62

	//Validation workloads
	{21,5,14,25}, // 63
	{6,24,7,1}, // 64
	{4,8,13,10}, // 65
	{20,11,9,2}, // 66
	{1,17,1,1,16,11}, // 67
	{8,22,13,9,1,5}, // 68
	{7,7,26,9,20,7}, // 69
	{4,10,17,21,3,25}, // 70
	{10,13,21,10,23,7,10,14}, // 71
	{26,21,13,4,4,1,14,22}, // 72
	{20,1,11,26,4,19,12,15}, // 73
	{22,24,7,1,12,22,14,12}, // 74
};

/*************************************************************
 **                 wrmsr_on_cpu                             **
 *************************************************************/

void wrmsr_on_cpu(int msr_state, int cpu) {
    uint64_t data;
    int fd;
    char msr_file_name[64];
    sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
    fd = open(msr_file_name, O_WRONLY);
    if (fd < 0) {
        if (errno == ENXIO) {
            printf("wrmsr: No CPU %d found!\n", cpu);
            exit(2);
        } else if (errno == EIO) {
            printf("wrmsr: CPU %d doesn't support MSRs\n", cpu);
            exit(3);
        } else {
            perror("wrmsr: open");
            exit(127);
        }
    }
	data = msr_stats[msr_state];
    if (pwrite(fd, &data, sizeof(data), msr_prefetch_reg) != sizeof(data)) {
        if (errno == EIO) {
            printf("wrmsr: CPU %d cannot set MSR ""0x%08"PRIx32" to 0x%016"PRIx64"\n", cpu, msr_prefetch_reg, data);
            exit(4);
        } else {
            perror("wrmsr: pwrite");
            exit(127);
        }
    }
    close(fd);
}

/*************************************************************
 **                 initialize_events                       **
 *************************************************************/

void initialize_events(node *node) {
	int i, ret;

	// Configure events
	ret = perf_setup_list_events(options.events, &(node->fds), &(node->num_fds));
	if(ret || (node->num_fds == 0)) {
		exit(1);
	}
	node->fds[0].fd = -1;
	for(i=0; i<node->num_fds; i++) {
		node->fds[i].hw.disabled = 0;  /* start immediately */
		/* request timing information necessary for scaling counts */
		node->fds[i].hw.read_format = PERF_FORMAT_SCALE;
		node->fds[i].hw.pinned = !i && options.pinned;
		node->fds[i].fd = perf_event_open(&node->fds[i].hw, node->pid, -1, (options.group? node->fds[i].fd : -1), 0);
		if(node->fds[i].fd == -1) {
			fprintf(stderr, "ERROR: cannot attach event %s\n", node->fds[i].name);
		}
	}
}

/*************************************************************
 **                 finalize_events                         **
 *************************************************************/

void finalize_events(node *node) {
  int i;

  // Releases descriptors
  for(i=0; i < node->num_fds; i++) {
    close(node->fds[i].fd);
  }
  // Release the counters
  perf_free_fds(node->fds, node->num_fds);
  node->fds = NULL;
}

/*************************************************************
 **                 initialize_counters                     **
 *************************************************************/

void initialize_counters(node *node) {
  int i;
  for(i=0; i<45; i++) {
    node->counters[i] = 0;
  }
}

/*************************************************************
 **                 initialize_my_Counters                  **
 *************************************************************/

void initialize_my_Counters(node *node) {
	int j;
	for(j=0;j<num_Counters;j++){
		node->my_Counters[j] = 0;
	}
}

/*************************************************************
 **                  get_counts                             **
 *************************************************************/

static void get_counts(node *aux){
  ssize_t ret;
  int i,cont = 0;
	for(i=0; i < aux->num_fds; i++) {
		uint64_t val;
		ret = read(aux->fds[i].fd, aux->fds[i].values, sizeof(aux->fds[i].values));
		if(ret < (ssize_t)sizeof(aux->fds[i].values)) {
			if(ret == -1)
				fprintf(stderr, "ERROR: cannot read values event %s\n", aux->fds[i].name);
			else
				fprintf(stderr,"ERROR: could not read event %d\n", i);
		}
		val = aux->fds[i].values[0] - aux->fds[i].prev_values[0];
		aux->fds[i].prev_values[0] = aux->fds[i].values[0];
		aux->counters[i] += val;// Save all acumulated counters during the execution
		switch(i) {
			case 0:// Cycles
				aux->cycles = val;
				aux->total_cycles += val;
				//fprintf(stdout, "CASE 0 Cycles = %ld, Interval = %ld\n", aux->cycles,aux->interval_cycles);
				break;
			case 1:// Instructions
				aux->instruction = val;
				aux->total_instructions += val;
				//fprintf(stdout, "CASE 1 Instrucctions = %ld, Interval = %ld\n", aux->instruction,aux->interval_instructions);
				break;
			default:// Others
				aux->my_Counters[cont] = val;
				cont++;
				break;
		}
	}
}

/*************************************************************
 **                 launch_process                          **
 *************************************************************/

int launch_process(node *node) {
	FILE *fitxer;
	pid_t pid;
	
	pid = fork();
	switch(pid) {
		case -1: // ERROR
			fprintf(stderr, "ERROR: Couldn't create the child.\n");
			exit(-3);

		case 0: // Child
        	// Descriptors for those who have input for standard input
        	switch(node->benchmark) {
				case 4: // [Doesn't work]
					close(0);
					fitxer = fopen("CPU2006/445.gobmk/data/ref/input/13x13.tst", "r");
					if(fitxer == NULL) {
					fprintf(stderr,"ERROR. The file could not be opened: CPU2006/445.gobmk/data/ref/input/13x13.tst.\n");
					return -1;
					}
					break;

				case 9:
					system("cp omnetpp_2006.ini omnetpp.ini  >/dev/null 2>&1");
					break;

				case 13:
					close(0);
					fitxer = fopen("CPU2006/416.gamess/data/ref/input/h2ocu2+.gradient.config", "r");
					if(fitxer == NULL) {
					fprintf(stderr,"ERROR. The file could not be opened: CPU2006/416.gamess/data/ref/input/h2ocu2+.gradient.config.\n");
					return -1;
					}
					break;

				case 14:
					close(0);
					fitxer = fopen("CPU2006/433.milc/data/ref/input/su3imp.in", "r");
					if(fitxer == NULL) {
					fprintf(stderr,"ERROR. The file could not be opened: CPU2006/433.milc/data/ref/input/su3imp.in.\n");
					return -1;
					}
					break;

				case 18:
					close(0);
					fitxer = fopen("CPU2006/437.leslie3d/data/ref/input/leslie3d.in", "r");
					if(fitxer == NULL) {
					fprintf(stderr,"ERROR. The file could not be opened: CPU2006/437.leslie3d/data/ref/input/leslie3d.in.\n");
					return -1;
					}
					break;

				case 22:
					close(2);
					fitxer = fopen("povray.sal", "w");
					if(fitxer == NULL) {
					fprintf(stderr,"ERROR. The file could not be opened: povray.sal\n");
					return -1;
					}
					break;
    		}
			execv(benchmarks[node->benchmark][0], benchmarks[node->benchmark]);
			fprintf(stderr, "ERROR: Couldn't launch the program %d.\n",node->benchmark);
			exit(-2);

		default:// Parent
			usleep(100); // Wait 200 ms
			// We stop the process
			kill(pid, 19);
			// We see that it has not failed
			waitpid(pid, &(node->status), WUNTRACED);
			if(WIFEXITED(node->status)) {
				fprintf(stderr, "ERROR: command process %d exited too early with status %d\n", pid, WEXITSTATUS(node->status));
				return -2;
			}
			// The pid is assigned
			node->pid = pid;
			if(sched_setaffinity(node->pid, sizeof(node->mask), &node->mask) != 0) {
				fprintf(stderr,"ERROR: Sched_setaffinity %d.\n", errno);
				exit(1);
			}
			// Put the prefetcher configuration value of the process
			wrmsr_on_cpu(node->actual_MSR,node->core);
			
			return 1;
	}
}

/************************************************************
**                  Print results			               **
*************************************************************/

void print_results(node *node) {

	core_Info = fopen(node->core_out, "w");
	fprintf(core_Info,"%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n",node->instruction,node->total_instructions,node->cycles,node->total_cycles,node->my_Counters[0],node->my_Counters[1],node->my_Counters[2]);
	fclose(core_Info);
}

/*************************************************************
 **                 measure                                 **
 *************************************************************/

int measure() {
	int i, ret;

	// Free processes (SIGCONT-Continue a paused process)
	for(i=0; i<N; i++) {
		if(queue[i].pid > 0) {
			kill(queue[i].pid, 18);
			//fprintf(stdout, "INFO: Process %d runing with status %d\n", queue[i].pid, WEXITSTATUS(queue[i].status));
		}
	}
	// Check that everyone has performed the operation
	for(i=0; i<N; i++) {
		waitpid(queue[i].pid, &(queue[i].status), WCONTINUED);
		if(WIFEXITED(queue[i].status)) {
			fprintf(stdout, "ERROR: command process %d_%d exited too early with status %d\n", queue[i].benchmark, queue[i].pid, WEXITSTATUS(queue[i].status));
		}
	}
	// We run for a quantum
	usleep(options.delay*1000);
	// Pause processes (SIGSTOP-Pause a process)
	ret = 0;
	for(i=0; i<N; i++) {
		if(queue[i].pid > 0) {
			kill(queue[i].pid, 19);
			//fprintf(stdout, "INFO: Process %d paused with status %d\n", queue[i].pid, WEXITSTATUS(queue[i].status));
		}
	}
	// Check that no process has died
	for(i=0; i<N; i++) {
		waitpid(queue[i].pid, &(queue[i].status), WUNTRACED);
		if(WIFEXITED(queue[i].status)) {
			//fprintf(stderr, "INFO: Process %d_%d finished with status %d\n", queue[i].mid, queue[i].pid, WEXITSTATUS(queue[i].status));
			ret++;
			queue[i].pid = -1;
		}
	}
	//sum_BW = 0.0;
	// Pick up counters for each process
	for(i=0; i<N; i++) {
		get_counts(&(queue[i]));
		
		/*
			UPDATE THE IPC OF PREVIOUS INTERVALS
		*/
		queue[i].last_IPC_3 = queue[i].last_IPC_2;
		queue[i].last_IPC_2 = queue[i].last_IPC_1;
		queue[i].last_IPC_1 = queue[i].actual_IPC;

		/*
			CALCULATE THE CURRENT IPC OF THE INTERVAL
		*/
		if(queue[i].cycles != 0){//IPC can be updated every quantum because the counters are updated every quantum
			queue[i].actual_IPC =  (float) queue[i].instruction / (float) queue[i].cycles;
		}else{
			queue[i].actual_IPC = 0.0;
		}
		/*
			CALCULATE CURRENT BANDWIDTH OF INTERVAL
		*/
		// queue[i].last_BW_3 = queue[i].last_BW_2;
		// queue[i].last_BW_2 = queue[i].last_BW_1;
		// queue[i].last_BW_1 = queue[i].actual_BW;
		
		// if(queue[i].cycles != 0){
		// 	queue[i].actual_BW = (float) ((float) (queue[i].my_Counters[PM_MEM_PREF]+queue[i].my_Counters[LLC_LOAD_MISSES])*CPU_FREQ) / (float) queue[i].cycles;
		// }else{
		// 	queue[i].actual_BW = 0.0;
		// }
		// sum_BW += queue[i].actual_BW;
	}
	
	for(i=0; i<N; i++){
		print_results(&(queue[i]));
	}
	return ret;
}

/*************************************************************
 **                 Usage                                   **
 *************************************************************/

static void usage(void) {
  printf("\nUso:  Intel_Prefetch_Tuner\n");
  printf("-d quanta duration(ms)\n");
  printf("[-o output_directory for the output of the prediction function (it will be divided into files for each core)]\n");
  printf("[-h [help]]\n");
  printf("-A workload [0-> Individual, 1-> Individual with 3 instances of the microbenchmark]\n");
  printf("[-B benchmark [Only if -A is 1 or 0]]\n");
  printf("[-C prefetch configuration [Only if -A is 1 or 0] ]\n");
  printf("[-S Stride [Stride that will be used on the microbenchmark. Recomended value 256]]\n");
  printf("[-N Nops [Numero de nops que ejecuta el microbenchmark, para regular la carga.]]\n");
}

/*************************************************************
 **                     MAIN PROGRAM                        **
 *************************************************************/

int main(int argc, char **argv) {

	printf("\nIntel Prefetch Tuner. Author: Manel Lurbe Sempere e-mail: malursem@gap.upv.es Year: 2022\n");

	int c, i, ret, quantums = 0;
	int individualBench = -1;
	int individualMSR = 0;// Default configuration. ALL prefetchers enabled
	// Set initial events to measure
	options.events = strdup("cycles,instructions,OFFCORE_REQUESTS_OUTSTANDING.ALL_DATA_RD,OFFCORE_REQUESTS_OUTSTANDING.CYCLES_WITH_DATA_RD,OFFCORE_REQUESTS_OUTSTANDING.CYCLES_WITH_DEMAND_RFO");
	options.delay = 200;
	end_experiment = 0;
	N = -1;

	while((c=getopt(argc, argv,"d:o:hA:B:C:PgS:N:")) != -1) {
		switch(c) {
			case 'd':
				options.delay = atoi(optarg);
				break;
			case 'o':
				options.output_directory = strdup(optarg);
				break;
			case 'h':
				usage();
				exit(0);
			case 'A':
				workload = atoi(optarg);
				N = bench_mixes[workload];
				break;
			/* For Single-core application only*/	
			case 'B'://Selected benchmark.
				individualBench = atoi(optarg);
				break;
			case 'C'://Selected MSR.
				individualMSR = atoi(optarg);
				int config_found = -1;
				for(i=0;i<num_prefetch_configs;i++){
					if(prefetch_configs[i] == individualMSR){
						config_found = 1;
						break;
					}
				}
				if(config_found == -1){
					fprintf(stderr, "ERROR: Configuration %d doesn't exist.\n",individualMSR);
					exit(-1);
				}
				break;
			/* libpfm options */
			case 'P':
				options.pinned = 1;
				break;
			case 'g':
				options.group = 1;
				break;
			/* Microbenchmark options */
			case 'S':// Microbenchmark Stride
				benchmarks[20][3] = optarg;
				break;
			case 'N':// Nop
				benchmarks[20][2] = optarg;
				break;
			default:
				fprintf(stderr, "ERROR: Unknown error\n");
		}
	}

	for(i=0; i<N_MAX; i++) {
		/*
			Application status
		*/
		queue[i].benchmark = -1;
		queue[i].finished = 0;
		queue[i].pid = -1;
		queue[i].core = -1;
		/*
			Array where to store the counters to be measured
		*/
		queue[i].my_Counters = (uint64_t *) malloc(num_Counters * sizeof(uint64_t));
		initialize_my_Counters(&(queue[i]));
		/*
			Accumulated for each application
		*/
		queue[i].total_instructions = 0;
		queue[i].total_cycles = 0;
		/*
			From the current quantum of the application
		*/
		queue[i].instruction = 0;
		queue[i].cycles = 0;
		/*
			Values to store
		*/
		queue[i].actual_MSR = individualMSR;
		queue[i].actual_IPC = 0;
		queue[i].actual_BW = 0;
		queue[i].sum_BW = 0.0;

		// For the previous intervals we keep the IPC reached.
		queue[i].last_IPC_1 = 0;
		queue[i].last_IPC_2 = 0;
		queue[i].last_IPC_3 = 0;

		// For the previous intervals we save the consumed BW
		queue[i].last_BW_1 = 0;
		queue[i].last_BW_2 = 0;
		queue[i].last_BW_3 = 0;

		// Select predefined cores for N_MAX
		queue[i].core = i;
	}

	if(N < 0) {
		fprintf(stderr, "ERROR: Number of processes not specified.\n");
		return -1;
	}
	if(!options.output_directory){
		fprintf(stderr, "ERROR: The output directory was not specified.\n");
		return -1;
	}
	if(options.delay < 1) {
		options.delay = 200;
	}
	for(i=0; i<N; i++) {
		queue[i].benchmark = workload_mixes[workload][i];
		queue[i].core_out = malloc(sizeof(char) * 1024);
		snprintf(queue[i].core_out, sizeof(char) * 1024, "%s%s%d%s", options.output_directory, "[", queue[i].core, "].txt");
	}
	// It looks if the load has been put 0 (Only one application) or 1 (application next to the microbenchmark)
	if(workload==0 || workload==1){
		if(individualBench<0 || individualMSR<0){
			fprintf(stderr, "ERROR: No application or prefetch configuration specified.\n");
			return -1;
		}
		queue[0].benchmark = individualBench;
		queue[0].actual_MSR = individualMSR;
	}
	// See if any benchmark to assign or core is missing
	for(i=0; i<N; i++) {
		if(queue[i].benchmark < 0) {
			fprintf(stderr, "ERROR: Some process to assign benchmark is missing.\n");
			return -1;
		}
		if(queue[i].core < 0) {
			fprintf(stderr, "ERROR: Some core to assign.\n");
			return -1;
		}
	}
	// Start counters
	for(i=0; i<N; i++) {
		initialize_counters(&(queue[i]));
	}
	// Assign cores
	for(i=0; i<N; i++) {
		/*
			Each core prepares its results file, to later in the post-processing know what core the data is.
		*/
		core_Info = fopen(queue[i].core_out, "w");
		fprintf(core_Info,"instructions\ttotal_instructions\tcycles\ttotal_cycles\tALL_DATA_RD\tCYCLES_WITH_DATA_RD\tCYCLES_WITH_DEMAND_RFO\n");
		fclose(core_Info);
		CPU_ZERO(&(queue[i].mask));	
		CPU_SET(queue[i].core, &(queue[i].mask));
	}
	// Initialize libpfm
	if(pfm_initialize() != PFM_SUCCESS) {
		fprintf(stderr,"ERROR: libpfm initialization failed\n");
	}
	for(i=0; i<N; i++) {
		launch_process(&(queue[i]));
		initialize_events(&(queue[i]));
	}
	do {
		// Run a quantum and collect the values of that quantum
		ret = measure();
		quantums++;
		// If any process has ended
		if(ret) {
			// Look at which of the applications has ended
			for(i=0; i<N; i++) {
				if(queue[i].pid == -1) {
					// The counters are read before finalizing it
					get_counts(&(queue[i]));
					finalize_events(&(queue[i]));
					// If the instructions to be executed have been completed
					if(queue[i].total_instructions < bench_Instructions[queue[i].benchmark]){
						launch_process(&(queue[i]));
						initialize_events(&(queue[i]));
					}
				}
			}
		}
		for(i=0; i<N; i++) {
			if(queue[i].total_instructions >= bench_Instructions[queue[i].benchmark]){
				// If you have not finished any time yet
				if(!queue[i].finished){
					end_experiment++;
					queue[i].finished = 1;
				}
				// If you are alive kill because you have passed the instructions of the experiment
				if(queue[i].pid != -1){
					kill(queue[i].pid, 9);
					queue[i].pid = -1;
					finalize_events(&(queue[i]));
				}
				queue[i].total_instructions = 0;
				// Should not print on the screen
				launch_process(&(queue[i]));
				initialize_events(&(queue[i]));
			}
		}
	}while(end_experiment < N);
	// End any process that may be pending
	for(i=0; i<N; i++) {
		if(queue[i].pid > 0) {
			kill(queue[i].pid, 9);
			finalize_events(&(queue[i]));
		}
	}
	//Free libpfm resources cleanly
	pfm_terminate();
	for(i=0; i<N_MAX; i++) {
		free(queue[i].core_out);
		free(queue[i].my_Counters);
	}
	return 0;
}
