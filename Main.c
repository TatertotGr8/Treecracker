MAIN.C
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#include "clock.h"


//Get resources from Trees.cu
#define SOURCE_FILENAME "trees.cu"
#define KERNEL_PRIM_NAME "filter_prim"
#define KERNEL_AUX_NAME "filter_aux"

//Results go in file treeseeds.txt
#define RESULTS_FILE_PATH "treeseeds.txt"
//Makes progress file 
#define PROGRESS_FILE_PATH "progress"
//Seedspace refers to the entire set of seeds that are "potential" candidates for Java Random’s //original seed.  In this case an unsigned long long shifted to the right by 44 bits, maximum seed //space of 20
#define SEEDSPACE_MAX (1LLU << 44)

// apparently THREAD_BATCH_SIZE can't be 2^32 because it overflows or something and 
// makes the kernel ids like 2^64 or something. 
#define THREAD_BATCH_SIZE (1LLU << 31)
#define BLOCK_SIZE (1LLU << 10)

#define RESULTS_PRIM_LEN (THREAD_BATCH_SIZE * sizeof(uint64_t) / 10)
#define RESULTS_PRIM_COUNT_LEN (sizeof(uint32_t))

#define RESULTS_AUX_LEN (THREAD_BATCH_SIZE * sizeof(uint64_t) / 100)
#define RESULTS_AUX_COUNT_LEN (sizeof(uint32_t))

void check(const char *fn, int err) {
 if (err != SUCCESS) {
 fprintf(stderr, "%s error: %d\n", fn, err);
 fflush(stdout);
 fflush(stderr);
 exit(-1);
 }
}

int main(int argc, char** argv) {

 // source file variables
 FILE *source_file;
 char *source_str;
 size_t source_size;

 // load program source
 source_file = fopen(SOURCE_FILENAME, "r");
 if (source_file == NULL) {
 perror("file error");
 exit(-1);
 }
 // get file size
 fseek(source_file, 0, SEEK_END);
 source_size = ftell(source_file);
 fseek(source_file, 0, SEEK_SET);
 source_str = malloc(source_size + 1);
 memeset(source_str, 0, source_size + 1);
 fread(source_str, 1, source_size, source_file);
 fclose(source_file);

 //variables
 int err;

 uint num_platforms;
 platform_id platform;
 device_id device;
 context context;
 command_queue queue;

 program program;

 // host & device memory objects for
 // primary filter results
 mem d_results_prim;
// uint64_t *results_prim = malloc(RESULTS_PRIM_LEN);
 // number of primary filter results
 mem d_results_prim_count;
 uint32_t results_prim_count;
 // filter results
 mem d_results_aux;
 uint64_t *results_aux = malloc(RESULTS_AUX_LEN);
 // number of auxiliary filter results
 mem d_results_aux_count;
 uint32_t results_aux_count;
 mem d_kernel_offset;
 uint64_t kernel_offset = 0;

 uint64_t total_results = 0;

 // if there was a kernel in progress that was interrupted,
 // restore its progress
 FILE *results_file;
 FILE *progress_file;

 progress_file = fopen(PROGRESS_FILE_PATH, "rb");
 int restored = 0;
 if (progress_file != NULL) {
 restored = 1;
 fread(&kernel_offset, sizeof(kernel_offset), 1, progress_file);
 fclose(progress_file);
 remove("progress");
 }

 if (restored) {
 results_file = fopen(RESULTS_FILE_PATH, "ab");
 } else if (fopen(RESULTS_FILE_PATH, "r") != NULL) {
 printf("existing results file found. please delete or rename it to continue\n");
 exit(1);
 } else {
 // if there was no existing results file
 // we just want to create the file and write straight to it
 results_file = fopen(RESULTS_FILE_PATH, "wb");
 }

 progress_file = fopen(PROGRESS_FILE_PATH, "wb");

  check("GetPlatformIDs", GetPlatformIDs(1, &platform, NULL));
 check("GetDeviceIDs", GetDeviceIDs(platform, DEVICE_TYPE, 1, &device, NULL));
 context = CreateContext(NULL, 1, &device, NULL, NULL, &err);
 check("CreateContext", err);
 queue = CreateCommandQueueWithProperties(context, device, 0, &err);
 check("CreateCommandQueue", err);

 // compile kernel
 program = CreateProgramWithSource(context, 1, (const char **) &source_str, NULL, &err);
 check("CreateProgramWithSource", err);
 err = BuildProgram(program, 0, NULL, NULL, NULL, NULL);
 // print build log if there was an error
 if (err != SUCCESS) {
 size_t log_size;
 GetProgramBuildInfo(program, device,PROGRAM_BUILD_LOG, 0, NULL, &log_size);
 char *log = malloc(log_size);
 GetProgramBuildInfo(program, device, PROGRAM_BUILD_LOG, log_size, log, NULL);
 fprintf(stderr, "%s\n", log);
 fflush(stdout);
 fflush(stderr);
 exit(-1);
 }

 // create primary & auxiliary filter kernel
 kernel_prim = CreateKernel(program, KERNEL_PRIM_NAME, &err);
 check("CreateKernel prim", err);
 kernel_aux = CreateKernel(program, KERNEL_AUX_NAME, &err);
 check("CreateKernel aux", err);

 // create buffer for primary results
 d_results_prim = CreateBuffer(context, MEM_WRITE_ONLY, RESULTS_PRIM_LEN, NULL, &err);
 check("results prim create", err);
 d_results_prim_count = CreateBuffer(context, MEM_READ_WRITE, sizeof(results_prim_count), NULL, &err);
 check("results prim count create", err);
 // and aux
 d_results_aux = CreateBuffer(context, MEM_WRITE_ONLY, RESULTS_AUX_LEN, NULL, &err);
 check("results aux create", err);
 d_results_aux_count = CreateBuffer(context, MEM_READ_WRITE, sizeof(results_aux_count), NULL, &err);
 check("results aux count create", err);

 d_kernel_offset = CreateBuffer(context, MEM_READ_ONLY, sizeof(kernel_offset), NULL, &err);
 check("kernel offset create", err);

 // main kernel loop
 uint64_t time_global_start = clock();
 uint64_t time_last;
 uint64_t time_batch_start;
 // slightly cursed for loop, we don't actually set the kernel offset here
 // because we initialized it to zero before, and if we restore progress
 // we just set it to the saved value
 for (; kernel_offset < SEEDSPACE_MAX; kernel_offset += THREAD_BATCH_SIZE) {
 time_batch_start = clock();
 time_last = clock();

 // zero out counters
 results_prim_count = 0;
 results_aux_count = 0;
 check("reset prim counter", EnqueueWriteBuffer(queue, d_results_prim_count, TRUE, 0, sizeof(results_prim_count), &results_prim_count, 0, NULL, NULL));
 check("reset aux counter", EnqueueWriteBuffer(queue, d_results_aux_count, TRUE, 0, sizeof(results_aux_count), &results_aux_count, 0, NULL, NULL));
 check("write kernel offset", EnqueueWriteBuffer(queue, d_kernel_offset, TRUE, 0, sizeof(kernel_offset), &kernel_offset, 0, NULL, NULL));

 // queue the primary filter kernel for execution
 check("kernel arg set 0", SetKernelArg(kernel_prim, 0, sizeof(d_kernel_offset), &d_kernel_offset));
 check("kernel arg set 1", SetKernelArg(kernel_prim, 1, sizeof(d_results_prim), &d_results_prim));
 check("kernel arg set 2", SetKernelArg(kernel_prim, 2, sizeof(d_results_prim_count), &d_results_prim_count));
 size_t global_dimensions = THREAD_BATCH_SIZE;
 size_t block_size = BLOCK_SIZE;
 check("EnqueueNDRangeKernel prim", EnqueueNDRangeKernel(
 queue, // command queue
 kernel_prim, // kernel
 1, // work dimensions
 NULL, // global work offset
// global work size
 &global_dimensions, 
// local work size (NULL = auto)
 &block_size, 
// number of events in wait list
 0, 
//event wait list
 NULL, 
//event
 NULL 
 ));

 // wait for primary kernel to finish
 check("Flush", Flush(queue));
 check("Finish prim kernel queue", Finish(queue));

 // read results counts 
 check("EnqueueReadBuffer queue read prim results count", EnqueueReadBuffer(queue, d_results_prim_count, TRUE, 0, sizeof(results_prim_count), &results_prim_count, 0, NULL, NULL));

 // measure primary kernel time
 double kernel_prim_time = (clock() - time_last) / 1e9;
 printf("primary kernel batch took %.6f\n", kernel_prim_time);

 printf("got %8u results from primary batch\n", results_prim_count);

 // run the aux kernel only if we got results from the initial filter
 if (results_prim_count > 0) {
 time_last = clock();
 checkcl("kernel arg set 0", clSetKernelArg(kernel_aux, 0, sizeof(d_results_prim), &d_results_prim));
 checkcl("kernel arg set 1", clSetKernelArg(kernel_aux, 1, sizeof(d_results_prim_count), &d_results_prim_count));
 checkcl("kernel arg set 2", clSetKernelArg(kernel_aux, 2, sizeof(d_results_aux), &d_results_aux));
 checkcl("kernel arg set 3", clSetKernelArg(kernel_aux, 3, sizeof(d_results_aux_count), &d_results_aux_count));
 global_dimensions = results_prim_count;
 block_size = BLOCK_SIZE;
 checkcl("clEnqueueNDRangeKernel aux", clEnqueueNDRangeKernel(
 queue, // command queue
 kernel_aux, // kernel
 1, // work dimensions
 NULL, // global work offset
 &global_dimensions, // global work size
 NULL, // local work size (NULL = auto)
 0, // number of events in wait list
 NULL, // event wait list
 NULL // event
 ));

 // wait for aux kernel to finish
 check("Flush", Flush(queue));
 check("Finish aux kernel queue", Finish(queue));

 // measure aux kernel time
 double kernel_aux_time = (clock() - time_last) / 1e9;
 printf("aux kernel batch took %.6f\n", kernel_aux_time);
 time_last = clock();

 // read aux results count
 check("EnqueueReadBuffer queue read aux results count", EnqueueReadBuffer(queue, d_results_aux_count, TRUE, 0, sizeof(results_aux_count), &results_aux_count, 0, NULL, NULL));

 printf("got %8u results from aux batch\n", results_aux_count);

 // queue aux result read
 check("EnqueueReadBuffer queue read aux results", EnqueueReadBuffer(queue, d_results_aux, FALSE, 0, results_aux_count * sizeof(uint64_t), results_aux, 0, NULL, NULL));

 // wait for the results to be read
 check("Flush", Flush(queue));
 check("Finish read aux results",Finish(queue));

 // measure result read time
 printf("mem read took %.6f\n", (clock() - time_last) / 1e9);
 time_last = clock();

 // write results to file
 total_results += results_aux_count;
 for (size_t i = 0; i < results_aux_count; i++) {
 uint64_t result = results_aux[i];
 // binary write
// fwrite(&result, sizeof(uint64_t), 1, results_file);
 // string write
 fprintf(results_file, "%llu\n", result);
 }








