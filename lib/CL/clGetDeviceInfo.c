/* OpenCL runtime library: clGetDeviceInfo()

   Copyright (c) 2011-2012 Kalle Raiskila and Pekka Jääskeläinen
   
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include "pocl_cl.h"
#include <string.h>

#define POCL_RETURN_DEVICE_INFO(__TYPE__, __VALUE__)                \
  {                                                                 \
    size_t const value_size = sizeof(__TYPE__);                     \
    if (param_value)                                                \
      {                                                             \
        if (param_value_size < value_size) return CL_INVALID_VALUE; \
        *(__TYPE__*)param_value = __VALUE__;                        \
      }                                                             \
    if (param_value_size_ret)                                       \
      *param_value_size_ret = value_size;                           \
    return CL_SUCCESS;                                              \
  } 

/* A version for querying the info and in case the device returns 
   a zero, assume the device info query hasn't been implemented 
   for the device driver at hand. Warns about an incomplete 
   implementation. */
#define POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(__TYPE__, __VALUE__)    \
  {                                                                 \
    size_t const value_size = sizeof(__TYPE__);                     \
    if (param_value)                                                \
      {                                                             \
        if (param_value_size < value_size) return CL_INVALID_VALUE; \
        *(__TYPE__*)param_value = __VALUE__;                        \
        if (__VALUE__ == 0) POCL_WARN_INCOMPLETE();                 \
      }                                                             \
    if (param_value_size_ret)                                       \
      *param_value_size_ret = value_size;                           \
    return CL_SUCCESS;                                              \
  } 

#define POCL_RETURN_DEVICE_INFO_STR(__STR__)                        \
  {                                                                 \
    size_t const value_size = strlen(__STR__) + 1;                  \
    if (param_value)                                                \
      {                                                             \
        if (param_value_size < value_size) return CL_INVALID_VALUE; \
        memcpy(param_value, __STR__, value_size);                   \
      }                                                             \
    if (param_value_size_ret)                                       \
      *param_value_size_ret = value_size;                           \
    return CL_SUCCESS;                                              \
  }                                                                 \
    

  
CL_API_ENTRY cl_int CL_API_CALL
clGetDeviceInfo(cl_device_id   device,
                cl_device_info param_name, 
                size_t         param_value_size, 
                void *         param_value,
                size_t *       param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
  switch (param_name)
  {
  case CL_DEVICE_IMAGE_SUPPORT: 
    POCL_RETURN_DEVICE_INFO(cl_bool, CL_TRUE);
  case CL_DEVICE_TYPE:
    POCL_RETURN_DEVICE_INFO(cl_device_type, device->type);   
  case CL_DEVICE_VENDOR_ID:
    POCL_RETURN_DEVICE_INFO(cl_uint, device->vendor_id);
  case CL_DEVICE_MAX_COMPUTE_UNITS:
    POCL_RETURN_DEVICE_INFO(cl_uint, device->max_compute_units);
  case CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS          :
    POCL_RETURN_DEVICE_INFO(cl_uint, device->max_work_item_dimensions);
  case CL_DEVICE_MAX_WORK_GROUP_SIZE               : 
    /* There is no "preferred WG size" device query, so we probably should
       return something more sensible than the CL_INT_MAX that seems
       to be the default in the pthread device. It should be computed from 
       the machine's vector width or issue width.

       Some OpenCL programs (e.g. the Dijkstra book sample) seem to scale 
       the work groups using this. 

       There's a kernel query CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE
       that can yield better heuristics for the good WG size and forms
       a basis for a higher level performance portability layer.
       
       Basically the size is now limited by the absence of work item
       loops. A huge unrolling factor explodes the instruction memory size (and
       compilation time) with usually no benefits.
    */
    {
      size_t max_wg_size = device->max_work_group_size;
      /* Allow overriding the max WG size to reduce compilation time
         for cases which use the maximum. This is needed until pocl has
         the WI loops.  */
      if (getenv ("POCL_MAX_WORK_GROUP_SIZE") != NULL)
        {
          size_t from_env = atoi (getenv ("POCL_MAX_WORK_GROUP_SIZE"));
          if (from_env < max_wg_size) max_wg_size = from_env;
        }
      POCL_RETURN_DEVICE_INFO(size_t, max_wg_size);
    }
  case CL_DEVICE_MAX_WORK_ITEM_SIZES:
    {
      /* We allocate a 3-elementa array for this in pthread.c */
      typedef struct { size_t size[3]; } size_t_3;
      POCL_RETURN_DEVICE_INFO(size_t_3, *(size_t_3 const *)device->max_work_item_sizes);
    }
    
  case CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR:
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->preferred_vector_width_char);   
    
  case CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT:
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->preferred_vector_width_short);
    
  case CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT:
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->preferred_vector_width_int);

  case CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG:
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->preferred_vector_width_long);
    
  case CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT:
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->preferred_vector_width_float);
    
  case CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE:
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->preferred_vector_width_double);

  case CL_DEVICE_MAX_CLOCK_FREQUENCY               :
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->max_clock_frequency);
  case CL_DEVICE_ADDRESS_BITS                      :
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->address_bits);
  case CL_DEVICE_MAX_READ_IMAGE_ARGS               : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->max_read_image_args);
  case CL_DEVICE_MAX_WRITE_IMAGE_ARGS              :
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->max_write_image_args);
  case CL_DEVICE_MAX_MEM_ALLOC_SIZE:
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_ulong, device->max_mem_alloc_size);
  case CL_DEVICE_IMAGE2D_MAX_WIDTH                 : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_ulong, device->image2d_max_width);
  case CL_DEVICE_IMAGE2D_MAX_HEIGHT                :
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_ulong, device->image2d_max_height);
  case CL_DEVICE_IMAGE3D_MAX_WIDTH                 : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_ulong, device->image3d_max_width);
  case CL_DEVICE_IMAGE3D_MAX_HEIGHT                : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_ulong, device->image3d_max_height);
  case CL_DEVICE_IMAGE3D_MAX_DEPTH                 :
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_ulong, device->image3d_max_depth);
  case CL_DEVICE_MAX_PARAMETER_SIZE                : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(size_t, device->max_parameter_size);
  case CL_DEVICE_MAX_SAMPLERS                      : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_ulong, device->max_samplers);
  case CL_DEVICE_MEM_BASE_ADDR_ALIGN               : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->mem_base_addr_align);
  case CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE          : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->min_data_type_align_size);
  case CL_DEVICE_SINGLE_FP_CONFIG                  : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_ulong, device->single_fp_config);
  case CL_DEVICE_GLOBAL_MEM_CACHE_TYPE             :
    POCL_RETURN_DEVICE_INFO(cl_uint, device->global_mem_cache_type);
  case CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE         : 
    POCL_RETURN_DEVICE_INFO(cl_uint, device->global_mem_cacheline_size);
  case CL_DEVICE_GLOBAL_MEM_CACHE_SIZE             : 
    POCL_RETURN_DEVICE_INFO(cl_ulong, device->global_mem_cache_size);
  case CL_DEVICE_GLOBAL_MEM_SIZE:
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_ulong, device->global_mem_size);
  case CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE          : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->max_constant_buffer_size);
  case CL_DEVICE_MAX_CONSTANT_ARGS                 : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->max_constant_args);
  case CL_DEVICE_LOCAL_MEM_TYPE                    :
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->local_mem_type);
  case CL_DEVICE_LOCAL_MEM_SIZE:
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_ulong, device->local_mem_size);
  case CL_DEVICE_ERROR_CORRECTION_SUPPORT          :
    POCL_RETURN_DEVICE_INFO(cl_uint, device->error_correction_support);
  case CL_DEVICE_PROFILING_TIMER_RESOLUTION        :
    POCL_RETURN_DEVICE_INFO(cl_uint, device->profiling_timer_resolution);
  case CL_DEVICE_ENDIAN_LITTLE                     :
    POCL_RETURN_DEVICE_INFO(cl_uint, device->endian_little);
  case CL_DEVICE_AVAILABLE                         :
    POCL_RETURN_DEVICE_INFO(cl_uint, device->available);
  case CL_DEVICE_COMPILER_AVAILABLE                :
    POCL_RETURN_DEVICE_INFO(cl_uint, device->compiler_available);
  case CL_DEVICE_EXECUTION_CAPABILITIES            :
    POCL_RETURN_DEVICE_INFO(cl_uint, device->execution_capabilities);
  case CL_DEVICE_QUEUE_PROPERTIES                  :
    POCL_RETURN_DEVICE_INFO(cl_uint, device->queue_properties);
   
  case CL_DEVICE_NAME:
    POCL_RETURN_DEVICE_INFO_STR(device->name);
   
  case CL_DEVICE_VENDOR                            : 
    POCL_RETURN_DEVICE_INFO_STR("unknown"); /* TODO: CPUID */

  case CL_DRIVER_VERSION:
    POCL_RETURN_DEVICE_INFO_STR(device->driver_version);
  case CL_DEVICE_PROFILE                           : break;
  case CL_DEVICE_VERSION                           : 
    POCL_RETURN_DEVICE_INFO_STR("unknown"); /* TODO: CPUID */

  case CL_DEVICE_EXTENSIONS                        : 
    POCL_RETURN_DEVICE_INFO_STR("cl_khr_fp16");
  case CL_DEVICE_PLATFORM                          : break;
  case CL_DEVICE_DOUBLE_FP_CONFIG                  : break;
  case CL_DEVICE_HALF_FP_CONFIG                    : break;
  case CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF       : break;
  case CL_DEVICE_HOST_UNIFIED_MEMORY               : break;
    /* TODO: figure out what the difference between preferred and native
       widths are. And why there is no struct fields 'native_vector...' */
  case CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR          : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->preferred_vector_width_char);
  case CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT         :
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->preferred_vector_width_short);
  case CL_DEVICE_NATIVE_VECTOR_WIDTH_INT           : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->preferred_vector_width_int);
  case CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG          : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->preferred_vector_width_long);
  case CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT         : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->preferred_vector_width_float);
  case CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE        : 
    POCL_RETURN_DEVICE_INFO_WITH_IMPL_CHECK(cl_uint, device->preferred_vector_width_double);
  case CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF          : 
    /* TODO: why is there no preferred_vector_width_half? */
    POCL_RETURN_DEVICE_INFO(cl_uint, 0);
  case CL_DEVICE_OPENCL_C_VERSION                  :
    POCL_RETURN_DEVICE_INFO_STR("1.2");
  }
  return CL_INVALID_VALUE;
}
