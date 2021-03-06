/* prototypes.inc - macros for generating the driver layer function prototypes

   Copyright (c) 2012 Pekka Jääskeläinen / Tampere University of Technology
   
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

#define GEN_PROTOTYPES(__DRV__)                                         \
  _Pragma ("GCC visibility push(hidden)")                               \
  void pocl_##__DRV__##_init_device_infos(struct _cl_device_id* dev);  \
  void pocl_##__DRV__##_init_device_ops(struct pocl_device_ops* ops);  \
  void pocl_##__DRV__##_uninit (cl_device_id device);                   \
  void pocl_##__DRV__##_init (cl_device_id device, const char* parameters); \
  void *pocl_##__DRV__##_malloc (void *data, cl_mem_flags flags,            \
                             size_t size, void *host_ptr);              \
  void *pocl_##__DRV__##_create_sub_buffer (void *device_data, void* buffer, \
                                            size_t origin, size_t size); \
  void pocl_##__DRV__##_free (void *data, cl_mem_flags flags, void *ptr);   \
  void pocl_##__DRV__##_read (void *data, void *host_ptr,                   \
                          const void *device_ptr, size_t cb);           \
  void pocl_##__DRV__##_read_rect (void *data, void *host_ptr, void *device_ptr, \
                               const size_t *buffer_origin,             \
                               const size_t *host_origin,               \
                               const size_t *region,                    \
                               size_t buffer_row_pitch,                 \
                               size_t buffer_slice_pitch,               \
                               size_t host_row_pitch,                   \
                               size_t host_slice_pitch);                \
  void pocl_##__DRV__##_write (void *data, const void *host_ptr, void *device_ptr, size_t cb); \
  void pocl_##__DRV__##_write_rect (void *data, const void *host_ptr, void *device_ptr, \
                                const size_t *buffer_origin,            \
                                const size_t *host_origin,              \
                                const size_t *region,                   \
                                size_t buffer_row_pitch,                \
                                size_t buffer_slice_pitch,              \
                                size_t host_row_pitch,                  \
                                size_t host_slice_pitch);               \
  void pocl_##__DRV__##_copy (void *data, const void *src_ptr,              \
                          void *__restrict__ dst_ptr, size_t cb);       \
  void pocl_##__DRV__##_copy_rect (void *data, const void *src_ptr,         \
                               void *dst_ptr,                           \
                               const size_t *src_origin,                \
                               const size_t *dst_origin,                \
                               const size_t *region,                    \
                               size_t src_row_pitch,                    \
                               size_t src_slice_pitch,                  \
                               size_t dst_row_pitch,                    \
                               size_t dst_slice_pitch);                 \
  void pocl_##__DRV__##_fill_rect (void *data,           \
                           void *__restrict__ const device_ptr, \
                           const size_t *__restrict__ const buffer_origin,  \
                           const size_t *__restrict__ const region,         \
                           size_t const buffer_row_pitch,      \
                           size_t const buffer_slice_pitch,    \
                           void *fill_pixel,    \
                           size_t pixel_size);  \
  void pocl_##__DRV__##_compile_submitted_kernels (_cl_command_node *node);  \
  void pocl_##__DRV__##_run (void *data, _cl_command_node* cmd);        \
  void pocl_##__DRV__##_run_native (void *data, _cl_command_node* cmd); \
  void* pocl_##__DRV__##_map_mem (void *data, void *buf_ptr,                      \
                        size_t offset, size_t size, void *host_ptr); \
  cl_ulong pocl_##__DRV__##_get_timer_value(void *data); \
  int pocl_##__DRV__##_build_program (void *data,                      \
                                    const char *source_fn,             \
                                    const char *binary_fn,             \
                                    const char *default_cmd,           \
                                    const char *user_opts,             \
                                    const char *dev_tmpdir);           \
  char* pocl_##__DRV__##_init_build (void *data, \
                                         const char *dev_tmpdir); \
  cl_int pocl_##__DRV__##_get_supported_image_formats (cl_mem_flags flags,\
                                         const cl_image_format **image_formats,\
                                         cl_int *num_image_formats);\
_Pragma ("GCC visibility pop")
