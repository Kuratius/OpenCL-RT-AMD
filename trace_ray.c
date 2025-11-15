#define PROGRAM_FILE "trace_ray.cl"
#define KERNEL_FUNC "trace_ray"
#define ARRAY_SIZE 64
#define CL_TARGET_OPENCL_VERSION (300)

#define radv_bvh_node_triangle 0
#define radv_bvh_node_box16    4
#define radv_bvh_node_box32    5
#define radv_bvh_node_instance 6
#define radv_bvh_node_aabb     7

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef MAC
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
struct radv_bvh_triangle_node {
   float coords[3][3];
   uint32_t reserved[3];
   uint32_t triangle_id;
   /* flags in upper 4 bits */
   uint32_t geometry_id_and_flags;
   uint32_t reserved2;
   uint32_t id;
};

struct radv_bvh_aabb_node {
   uint32_t primitive_id;
   /* flags in upper 4 bits */
   uint32_t geometry_id_and_flags;
   uint32_t reserved[14];
};

struct radv_bvh_instance_node {
   uint64_t bvh_ptr; /* pre-shifted/masked to serve as node base */

   /* lower 24 bits are the custom instance index, upper 8 bits are the visibility mask */
   uint32_t custom_instance_and_mask;
   /* lower 24 bits are the sbt offset, upper 8 bits are VkGeometryInstanceFlagsKHR */
   uint32_t sbt_offset_and_flags;

   mat3x4 wto_matrix;

   uint32_t instance_id;
   uint32_t bvh_offset;
   uint32_t reserved[2];

   /* Object to world matrix transposed from the initial transform. */
   mat3x4 otw_matrix;
};



/* Find a GPU or CPU associated with the first available platform 

The `platform` structure identifies the first platform identified by the 
OpenCL runtime. A platform identifies a vendor's installation, so a system 
may have an NVIDIA platform and an AMD platform. 

The `device` structure corresponds to the first accessible device 
associated with the platform. Because the second parameter is 
`CL_DEVICE_TYPE_GPU`, this device must be a GPU.
*/
cl_device_id create_device() {

   cl_platform_id platform;
   cl_device_id dev;
   int err;

   /* Identify a platform */
   err = clGetPlatformIDs(1, &platform, NULL);
   if(err < 0) {
      perror("Couldn't identify a platform");
      exit(1);
   } 

   // Access a device
   // GPU
   err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &dev, NULL);
   if(err == CL_DEVICE_NOT_FOUND) {
      // CPU
      err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &dev, NULL);
   }
   if(err < 0) {
      perror("Couldn't access any devices");
      exit(1);   
   }

   return dev;
}

/* Create program from a file and compile it */
cl_program build_program(cl_context ctx, cl_device_id dev, const char* filename) {

   cl_program program;
   FILE *program_handle;
   char *program_buffer, *program_log;
   size_t program_size, log_size;
   int err;

   /* Read program file and place content into buffer */
   program_handle = fopen(filename, "r");
   if(program_handle == NULL) {
      perror("Couldn't find the program file");
      exit(1);
   }
   fseek(program_handle, 0, SEEK_END);
   program_size = ftell(program_handle);
   rewind(program_handle);
   program_buffer = (char*)malloc(program_size + 1);
   program_buffer[program_size] = '\0';
   fread(program_buffer, sizeof(char), program_size, program_handle);
   fclose(program_handle);

   /* Create program from file 

   Creates a program from the source code in the add_numbers.cl file. 
   Specifically, the code reads the file's content into a char array 
   called program_buffer, and then calls clCreateProgramWithSource.
   */
   program = clCreateProgramWithSource(ctx, 1, 
      (const char**)&program_buffer, &program_size, &err);
   if(err < 0) {
      perror("Couldn't create the program");
      exit(1);
   }
   free(program_buffer);

   /* Build program 

   The fourth parameter accepts options that configure the compilation. 
   These are similar to the flags used by gcc. For example, you can 
   define a macro with the option -DMACRO=VALUE and turn off optimization 
   with -cl-opt-disable.
   */
   err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
   if(err < 0) {

      /* Find size of log and print to std output */
      clGetProgramBuildInfo(program, dev, CL_PROGRAM_BUILD_LOG, 
            0, NULL, &log_size);
      program_log = (char*) malloc(log_size + 1);
      program_log[log_size] = '\0';
      clGetProgramBuildInfo(program, dev, CL_PROGRAM_BUILD_LOG, 
            log_size + 1, program_log, NULL);
      printf("%s\n", program_log);
      free(program_log);
      exit(1);
   }

   return program;
}





int main() {

   /* OpenCL structures */
   cl_device_id device;
   cl_context context;
   cl_program program;
   cl_kernel kernel;
   cl_command_queue queue;
   cl_int i, j, err;
   size_t local_size, global_size;

   /* Data and buffers    */

   float sum[2], total, actual_sum;
   cl_mem input_buffer, sum_buffer;
   cl_int num_groups;


struct aabb {float minX; float minY; float minZ; float maxX; float maxY; float maxZ;};

struct radv_bvh_box32_node {
   uint32_t children[4];
   struct aabb coords[4];
   /* VK_BVH_BOX_FLAG_* indicating if all/no children are opaque */
   uint32_t flags;
   uint32_t reserved[3];
};


    struct radv_bvh_box32_node node[4];
    memset(&node[0], 0, sizeof(node));
    node[0].children[0]=0x5;
    node[0].children[1]=(sizeof(node[0])/64)<<3 | 0x5;
    node[0].children[2]=((2*sizeof(node[0]))/64)<<3 | 0x5;
    node[0].children[3]=((3*sizeof(node[0]))/64)<<3 | 0x5;
    node[0].coords[0].minX=-0x1.0p1;
    node[0].coords[0].minY=-0x1.0p0;
    node[0].coords[0].minZ=-0x1.0p0;
    node[0].coords[0].maxX=0x1.0p0;
    node[0].coords[0].maxY=0x1.0p0;
    node[0].coords[0].maxZ=0x1.0p0;
    node[0].coords[1].minX=-0x1.0p-1;
    node[0].coords[1].minY=-0x1.0p-1;
    node[0].coords[1].minZ=-0x1.0p-1;
    node[0].coords[1].maxX=0x1.0p-1;
    node[0].coords[1].maxY=0x1.0p-1;
    node[0].coords[1].maxZ=0x1.0p-1;
    node[0].coords[2].minX=-0x1.0p-2;
    node[0].coords[2].minY=-0x1.0p-2;
    node[0].coords[2].minZ=-0x1.0p-2;
    node[0].coords[2].maxX=0x1.0p-2;
    node[0].coords[2].maxY=0x1.0p-2;
    node[0].coords[2].maxZ=0x1.0p-2;
    node[0].coords[3].minX=-0x1.0p-3;
    node[0].coords[3].minY=-0x1.0p-3;
    node[0].coords[3].minZ=-0x1.0p-3;
    node[0].coords[3].maxX=0x1.0p-3;
    node[0].coords[3].maxY=0x1.0p-3;
    node[0].coords[3].maxZ=0x1.0p-3;
    node[0].flags=0;
    node[0].reserved[0]=0;
    node[0].reserved[1]=0;
    node[0].reserved[2]=0;
    struct radv_bvh_box32_node * data=aligned_alloc(256,sizeof(node));
    memcpy(data, &node[0] , sizeof(node));

    printf("size of node is %zu\n", sizeof(node));


   /* Initialize data */

   /* Create device and context 

   Creates a context containing only one device — the device structure 
   created earlier.
   */
   device = create_device();
   context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
   if(err < 0) {
      perror("Couldn't create a context");
      exit(1);   
   }

   /* Build program */
   program = build_program(context, device, PROGRAM_FILE);

   /* Create data buffer 

   • `global_size`: total number of work items that will be 
      executed on the GPU (e.g. total size of your array)
   • `local_size`: size of local workgroup. Each workgroup contains 
      several work items and goes to a compute unit 

   In this example, the kernel is executed by eight work-items divided into 
   two work-groups of four work-items each. Returning to my analogy, 
   this corresponds to a school containing eight students divided into 
   two classrooms of four students each.   

     Notes: 
   • Intel recommends workgroup size of 64-128. Often 128 is minimum to 
   get good performance on GPU
   • On NVIDIA Fermi, workgroup size must be at least 192 for full 
   utilization of cores
   • Optimal workgroup size differs across applications
   */
   global_size = 8; // WHY ONLY 8?
   local_size = 4; 
   num_groups = global_size/local_size;

   float dataf[ARRAY_SIZE];
   /* Initialize data */
   for(i=0; i<ARRAY_SIZE; i++) {
      dataf[i] = 1.0f*i;
   }

   input_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY |
         CL_MEM_COPY_HOST_PTR, sizeof(node), data, &err); // <=====INPUT
   sum_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE |
         CL_MEM_COPY_HOST_PTR, num_groups * sizeof(float), sum, &err); // <=====OUTPUT
   if(err < 0) {
      perror("Couldn't create a buffer");
      exit(1);   
   };

   /* Create a command queue 

   Does not support profiling or out-of-order-execution
   */
   queue = clCreateCommandQueueWithProperties(context, device, 0, &err);
   if(err < 0) {
      perror("Couldn't create a command queue");
      exit(1);   
   };

   /* Create a kernel */
   kernel = clCreateKernel(program, KERNEL_FUNC, &err);
   if(err < 0) {
      perror("Couldn't create a kernel");
      exit(1);
   };

   /* Create kernel arguments */
   err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &input_buffer); // <=====INPUT
   err |= clSetKernelArg(kernel, 1, local_size * sizeof(float), NULL);
   err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &sum_buffer); // <=====OUTPUT
   if(err < 0) {
      perror("Couldn't create a kernel argument");
      exit(1);
   }

   /* Enqueue kernel 

   At this point, the application has created all the data structures 
   (device, kernel, program, command queue, and context) needed by an 
   OpenCL host application. Now, it deploys the kernel to a device.

   Of the OpenCL functions that run on the host, clEnqueueNDRangeKernel 
   is probably the most important to understand. Not only does it deploy 
   kernels to devices, it also identifies how many work-items should 
   be generated to execute the kernel (global_size) and the number of 
   work-items in each work-group (local_size).
   */
   err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, 
         &local_size, 0, NULL, NULL); 
   if(err < 0) {
      perror("Couldn't enqueue the kernel");
      exit(1);
   }

   /* Read the kernel's output    */
   err = clEnqueueReadBuffer(queue, sum_buffer, CL_TRUE, 0, 
         sizeof(sum), sum, 0, NULL, NULL); // <=====GET OUTPUT
   if(err < 0) {
      perror("Couldn't read the buffer");
      exit(1);
   }

   /* Check result */
   total = 0.0f;
   for(j=0; j<num_groups; j++) {
      total += sum[j];
   }

   /* Deallocate resources */
   clReleaseKernel(kernel);
   clReleaseMemObject(sum_buffer);
   clReleaseMemObject(input_buffer);
   clReleaseCommandQueue(queue);
   clReleaseProgram(program);
   clReleaseContext(context);

   free(data);
   return 0;
}
