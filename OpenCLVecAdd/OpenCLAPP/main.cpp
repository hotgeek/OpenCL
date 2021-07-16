//
//  main.cpp
//  OpenCLAPP
//
//  Created by Rahul Borkar on 7/16/21.
//

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <OpenCL/opencl.h>
#include "helper_timer.h"

//Global OpenCL Variables
cl_int ret_ocl;
cl_platform_id oclPlatformID;
cl_device_id oclComputeDeviceID;
cl_context oclContext;
cl_command_queue oclCommandQueue;
cl_program oclProgram;
cl_kernel oclKernel;

char *oclSourceCode = NULL;
size_t sizeKernelCodeLength;

int iNumberOfArrayElements = 114447771;
size_t localWorkSize;
size_t globalWorkSize;

float *hostInput1 = NULL;
float *hostInput2 = NULL;
float *hostOutput = NULL;
float *gold = NULL;

cl_mem deviceInput1 = NULL;
cl_mem deviceInput2 = NULL;
cl_mem deviceOutput = NULL;

float timeOnGPU;
float timeOnCPU;


int main(void) {
    //functions
    void fillFloatArrayWithRandomNumbers(float *, int);
    size_t roundGlobalSizeToNearestMultipleLocalSize(int, unsigned int);
    void vecAddHost(const float*, const float *, float *, int);
    char* loadOclProgramSource(const char *, const char *, size_t *);
    void cleanup(void);
    
    //allocate host memory
    //allocate host memory
    hostInput1 = (float*)malloc(sizeof(float) * iNumberOfArrayElements);
    if (hostInput1 == NULL) {
        printf("CPU memory fatal error: Cannot allocate memory for hostInput1\n");
        cleanup();
        exit(EXIT_FAILURE);
    }

    hostInput2 = (float*)malloc(sizeof(float) * iNumberOfArrayElements);
    if (hostInput2 == NULL) {
        printf("CPU memory fatal error: Cannot allocate memory for hostInput2\n");
        cleanup();
        exit(EXIT_FAILURE);
    }

    hostOutput = (float*)malloc(sizeof(float) * iNumberOfArrayElements);
    if (hostOutput == NULL) {
        printf("CPU memory fatal error: Cannot allocate memory for hostOutput\n");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    gold = (float*)malloc(sizeof(float) * iNumberOfArrayElements);
    if (gold == NULL) {
        printf("Unable to allocate memory for GOLD\n");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    fillFloatArrayWithRandomNumbers(hostInput1, iNumberOfArrayElements);
    fillFloatArrayWithRandomNumbers(hostInput2, iNumberOfArrayElements);
    
    //Get openCL supported PlatForm IDs
        
    ret_ocl = clGetPlatformIDs(1, &oclPlatformID, NULL);
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clGetPlatformIDs() Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    //Get OpenCL supporting GPU device's ID
    ret_ocl = clGetDeviceIDs(oclPlatformID, CL_DEVICE_TYPE_GPU, 2, &oclComputeDeviceID, NULL);
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clGetDeviceIDs() Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    char gpu_name[255];
    clGetDeviceInfo(oclComputeDeviceID, CL_DEVICE_NAME, sizeof(gpu_name), &gpu_name, NULL);
    printf("%s\n", gpu_name);
    
    
    //Create OpenCL Compute Context
    oclContext = clCreateContext(NULL, 1, &oclComputeDeviceID, NULL, NULL, &ret_ocl);
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clCreateContext() Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    //create Command Queue
    oclCommandQueue = clCreateCommandQueue(oclContext, oclComputeDeviceID, 0, &ret_ocl);
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clCreateCommandQueue() Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    //Create OpenCL program from .cl
    oclSourceCode = loadOclProgramSource("VecAdd.cl", "", &sizeKernelCodeLength);
    
    printf("******OCL Source code: %s\n", oclSourceCode);
    
    //cl_int status = 0;
    oclProgram = clCreateProgramWithSource(oclContext, 1, (const char **)&oclSourceCode, &sizeKernelCodeLength, &ret_ocl);
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clCreateCommandQueue() Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    //build OpenCL Program
    ret_ocl = clBuildProgram(oclProgram, 0, NULL, NULL, NULL, NULL);
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clBuildProgram() Failed : %d, Exitting Now ... \n", ret_ocl);
        
        size_t  len;
        char buffer[2048];
        clGetProgramBuildInfo(oclProgram, oclComputeDeviceID, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
        
        printf("\nOpenCL Program Build log : %s\n", buffer);
        
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    oclKernel = clCreateKernel(oclProgram, "vecAdd", &ret_ocl);
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clCreateKernel() Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    int size = iNumberOfArrayElements * sizeof(cl_float);
    //allocate device memory
    deviceInput1  = clCreateBuffer(oclContext, CL_MEM_READ_ONLY, size, NULL, &ret_ocl);
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clCreateBuffer() for deviceInput1 Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    
    //allocate device memory
    deviceInput2  = clCreateBuffer(oclContext, CL_MEM_READ_ONLY, size, NULL, &ret_ocl);
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clCreateBuffer() for deviceInput1 Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    //allocate device memory
    deviceOutput = clCreateBuffer(oclContext, CL_MEM_WRITE_ONLY, size, NULL, &ret_ocl);
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clCreateBuffer() for deviceOutput Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    //set openCL kernel arguments, our OpenCL kernel has 4 arguments 0, 1, 2, 3
    //set 0 based 0th argument i.e. deviceInput1
    ret_ocl = clSetKernelArg(oclKernel, 0, sizeof(cl_mem), (void *)&deviceInput1);
    //deviceInput1 maps tp in1 param of kernel function in .cl file
    
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clCreateBuffer() for deviceInput1 Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    //set 0 based 1st argument i.e. deviceInput2
    ret_ocl = clSetKernelArg(oclKernel, 1, sizeof(cl_mem), (void *)&deviceInput2);
    //deviceInput1 maps tp in1 param of kernel function in .cl file
    
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clSetKernelArg() for deviceInput2 Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
   //set 0 based 2nd argument i.e. deviceOutput
    ret_ocl = clSetKernelArg(oclKernel, 2, sizeof(cl_mem), (void *)&deviceOutput);
    //deviceOutput maps to in1 param of kernel function in .cl file
    
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clSetKernelArg() for deviceOutput Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    //set 0 based 3rd argument ie. lenth
    ret_ocl = clSetKernelArg(oclKernel, 3, sizeof(cl_int), (void *) &iNumberOfArrayElements);
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clSetKernelArg() for iNumberOfArrayElements Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    //write above input device buffer to Device memory
    ret_ocl = clEnqueueWriteBuffer(oclCommandQueue, deviceInput1, CL_FALSE, 0, size, hostInput1, 0, NULL, NULL);
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clEnqueueWriteBuffer() for hostInput1 Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    //write above input device buffer to Device memory
    ret_ocl = clEnqueueWriteBuffer(oclCommandQueue, deviceInput2, CL_FALSE, 0, size, hostInput2, 0, NULL, NULL);
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clEnqueueWriteBuffer() for hostInput2 Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    //run the kernel
    globalWorkSize = roundGlobalSizeToNearestMultipleLocalSize((int)localWorkSize, iNumberOfArrayElements);
    
    //start timer
    StopWatchInterface *timer = NULL;
    sdkCreateTimer(&timer);
    sdkStartTimer(&timer);
    
    ret_ocl = clEnqueueNDRangeKernel(oclCommandQueue, oclKernel, 1, NULL, &globalWorkSize, NULL, 0, NULL, NULL);
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clEnqueueNDRangeKernel() for Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    //finish openCL command queue
    clFinish(oclCommandQueue);
    
    //stop timer
    sdkStopTimer(&timer);
    timeOnGPU = sdkGetTimerValue(&timer);
    sdkDeleteTimer(&timer);
    
    //Read back result from device (from deviceOutput) into CPU variable i.e. hostOutput
    
    ret_ocl = clEnqueueReadBuffer(oclCommandQueue, deviceOutput, CL_TRUE, 0, size, hostOutput, 0, NULL, NULL);
    if(ret_ocl != CL_SUCCESS) {
        printf("OpenCL Error: clEnqueueReadBuffer() for Failed : %d, Exitting Now ... \n", ret_ocl);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    vecAddHost(hostInput1, hostInput2, gold, iNumberOfArrayElements);
    //comapre results for golden hosts
    
    //compare results of Golden host
    const float epsilon = 0.000001f;
    bool bAccuracy = true;
    int breakValue = 0;
    int i = 0;
    for (i = 0; i < iNumberOfArrayElements; i++) {
        float val1 = gold[i];
        float val2 = hostOutput[i];

        if (fabs(val1 - val2) > epsilon) {
            bAccuracy = false;
            breakValue = i;
            break;
        }
    }
    
    if (bAccuracy == false) {
      printf("Break Value = %d\n", breakValue);
    }

    char str[125];
    if (bAccuracy == true)
        sprintf(str, "%s", "Compariosn of Output arrays on CPU and GPU Are accurate within the limit of 0.000001\n");
    else
        sprintf(str, "%s", "Not All comparisons are accurate withiun the limit of 0.000001\n");


    printf("1st array is from 0th element %.6f to %dth element %.6f\n", hostInput1[0], iNumberOfArrayElements - 1, hostInput1[iNumberOfArrayElements - 1]);
    printf("2nd array is from 0th element %.6f to %dth element %.6f\n", hostInput2[0], iNumberOfArrayElements - 1, hostInput2[iNumberOfArrayElements - 1]);
    printf("Global Work Size = %u And Local Work Size = %u \n", (unsigned int) globalWorkSize, (unsigned int) localWorkSize);
    printf("Sum of Each element from 2 arrays create 3rd array as: \n");
    printf("3rd array if from 0th element %.6f to %dth element %.6f\n", hostOutput[0], iNumberOfArrayElements - 1, hostOutput[iNumberOfArrayElements - 1]);

    printf("Time Taken for addition on CPU is = %.6f(ms)\n", timeOnCPU);
    printf("Time Taken for addition on GPU is = %.6f(ms)\n", timeOnGPU);

    printf("%s\n", str);

    cleanup();
    return(0);
}


void fillFloatArrayWithRandomNumbers(float* pFloatArray, int size) {
    //code
    int i;

    const float fScale = 1.0f / (float)RAND_MAX;
    for (i = 0; i < size; i++) {
        pFloatArray[i] = fScale * rand();
    }
}

//Golden vector addition
void vecAddHost(const float* pFloatData1, const float* pFloatData2, float* pFloatResult, int iNumElements) {
    int i;

    StopWatchInterface* timer = NULL;
    
    sdkCreateTimer(&timer);
    sdkStartTimer(&timer);

    for (i = 0; i < iNumElements; i++) {
        pFloatResult[i] = pFloatData1[i] + pFloatData2[i];
    }

    sdkStopTimer(&timer);

    timeOnCPU = sdkGetTimerValue(&timer);

    sdkDeleteTimer(&timer);
}

size_t roundGlobalSizeToNearestMultipleLocalSize(int local_size, unsigned int global_size) {
    //code
    unsigned int r = 0;
    
    if(local_size > 0)
      r = global_size % local_size;
    
    if(r == 0){
        return global_size;
    } else {
        return global_size + local_size + r;
    }
}

char* loadOclProgramSource(const char *fileName, const char *preamble, size_t *sizeFinalLength) {
    //locals
    FILE *pFile = NULL;
    size_t sizeSourceLength;
    
    pFile = fopen("/Users/rahulbrokar/VecAdd.cl", "rb");
    if(pFile == NULL)
        return NULL;
    
    size_t sizePreambleLength = (size_t) strlen (preamble);
    
    //get the length of Source Code
    fseek(pFile, 0, SEEK_END);
    sizeSourceLength = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);
    
    //allocate a buffer for the source code string and read it in
    char *sourceString = (char *) malloc (sizeSourceLength + sizePreambleLength + 1);
    memcpy(sourceString, preamble, sizePreambleLength);
    
    if(fread((sourceString) + sizePreambleLength, sizeSourceLength, 1, pFile) != 1) {
        fclose(pFile);
        free(sourceString);
        return(0);
    }
    
    //close the file and return the total length of the combined (preamble + source) string
    fclose(pFile);
    if(sizeFinalLength != 0) {
        *sizeFinalLength = sizeSourceLength + sizePreambleLength;
    }
    
    sourceString[sizeSourceLength + sizePreambleLength] = '\0';
    return(sourceString);
}

void cleanup(void) {
    //code
    
    if(oclSourceCode) {
        free((void *) oclSourceCode);
        oclSourceCode = NULL;
    }
    
    if(oclKernel) {
        clReleaseKernel(oclKernel);
        oclKernel = NULL;
    }
    
    if(oclProgram) {
        clReleaseProgram(oclProgram);
        oclProgram = NULL;
    }
    
    if(oclCommandQueue) {
        clReleaseCommandQueue(oclCommandQueue);
        oclCommandQueue = NULL;
    }
    
    if(oclContext) {
        clReleaseContext(oclContext);
        oclContext = NULL;
    }
    
    //free allocated device memory
    if (deviceInput1)
    {
        clReleaseMemObject(deviceInput1);
        deviceInput1 = NULL;
    }

    if (deviceInput2) {
        clReleaseMemObject(deviceInput2);
        deviceInput2 = NULL;
    }

    if (deviceOutput) {
        clReleaseMemObject(deviceOutput);
        deviceOutput = NULL;
    }

    //Free allocated host memory
    if (hostInput1) {
        free(hostInput1);
        hostInput1 = NULL;
    }

    if (hostInput2) {
        free(hostInput2);
        hostInput2 = NULL;
    }

    if (hostOutput) {
        free(hostOutput);
        hostOutput = NULL;
    }

    if (gold) {
        free(gold);
        gold = NULL;
    }
}
