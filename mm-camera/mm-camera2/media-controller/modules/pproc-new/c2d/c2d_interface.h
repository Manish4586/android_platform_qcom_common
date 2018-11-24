/*============================================================================

  Copyright (c) 2013 - 2015 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

============================================================================*/

#ifndef __PPROC_INTERFACE_H
#define __PPROC_INTERFACE_H

#include <unistd.h>
#include <linux/msm_ion.h>
#include <media/msmb_pproc.h>
#include "mtype.h"
#include "cam_intf.h"
#include "c2d2.h"
#include "c2dExt.h"

#define BUFF_SIZE_255 255
//#define MAX_PLANES 3
#if 0
#define PPROC_H_FLIP 1 << 0
#define PPROC_V_FLIP 1 << 1

typedef enum {
  PPROC_ASF_OFF,
  PPROC_ASF_DUAL_FILTER,
  PPROC_ASF_EMBOSS,
  PPROC_ASF_SKETCH,
  PPROC_ASF_NEON,
} pproc_interface_asf_mode;

typedef enum {
  PPROC_PLANE_CBCR,
  PPROC_PLANE_CRCB,
  PPROC_PLANE_Y,
  PPROC_PLANE_CB,
  PPROC_PLANE_CR,
} pproc_interface_plane_fmt;

struct cpp_asf_info {
  int32_t sp;
  uint8_t neg_abs_y1;
  uint8_t dyna_clamp_en;
  uint8_t sp_eff_en;
  int16_t clamp_h_ul;
  int16_t clamp_h_ll;
  int16_t clamp_v_ul;
  int16_t clamp_v_ll;
  float clamp_scale_max;
  float clamp_scale_min;
  uint16_t clamp_offset_max;
  uint16_t clamp_offset_min;
  uint32_t nz_flag;
  float sobel_h_coeff[16];
  float sobel_v_coeff[16];
  float hpf_h_coeff[16];
  float hpf_v_coeff[16];
  float lpf_coeff[16];
  float lut1[24];
  float lut2[24];
  float lut3[12];
};

typedef struct {
  uint32_t frame_id;
  //uint32_t out_buff_idx;
  uint32_t src_width;
  uint32_t src_height;
  uint32_t src_stride;
  uint32_t src_scanline;
  //uint32_t dst_width;
  //uint32_t dst_height;
  uint32_t dst_stride;
  uint32_t dst_scanline;
  uint32_t process_window_first_pixel;
  uint32_t process_window_first_line;
  uint32_t process_window_width;
  uint32_t process_window_height;
  uint16_t rotation;
  uint32_t mirror;
  double h_scale_ratio;
  double v_scale_ratio;
  double noise_profile[3][4];
  double weight[3][4];
  double denoise_ratio[3][4];
  double edge_softness[3][4];
  pproc_interface_asf_mode asf_mode;
  int32_t sharpness;
  uint32_t denoise_enable;
  int in_frame_fd;
  int out_frame_fd;
  pproc_interface_plane_fmt in_plane_fmt;
  pproc_interface_plane_fmt out_plane_fmt;
  struct cpp_asf_info asf_info;
} pproc_frame_input_params_t;


/** _pproc_intf_buff_data:
 *    @native_buf: TRUE VFE allocated buf.
 *    @vaddr: virtual address for VFE buf.
 *
 * Replica for isp_buf_divert_t
 **/
typedef struct _pproc_intf_buff_data {
  boolean native_buf;        /* TRUE VFE allocated buf */
  void *vaddr;               /* NULL if not native buf */
  int fd;                    /* not used if not native buf */
  struct v4l2_buffer v4l2_buffer_obj; /* v4l2 buffer */
  boolean is_locked;
  boolean ack_flag;
  boolean is_buf_dirty;
  uint32_t identity;
} pproc_intf_buff_data_t;

typedef struct {
  //uint32_t                   frameid;
  //uint32_t                   in_buff_idx;
  //uint32_t                   client_id;
  uint32_t                   mct_event_identity;
  int32_t                    out_buff_idx;
  pproc_intf_buff_data_t     isp_divert_buffer;
  pproc_frame_input_params_t frame_params;
  void                      *buf;
} pproc_interface_frame_divert_t;

typedef struct _pproc_callback_params {
  void                          *module;
  pproc_interface_frame_divert_t divert_frame;
} pproc_interface_callback_params_t;

typedef int32_t (*pproc_client_framedone_ack_t)(uint32_t frame_skip,
  void *data);
typedef int32_t (*pproc_client_create_frame_info_t)(void *data,
  pproc_interface_frame_divert_t *divert_frame);

typedef struct {
  pproc_client_create_frame_info_t create_frame;
  pproc_client_framedone_ack_t     framedone_ack;
  void                            *data;
} pproc_client_callback_t;
#endif
typedef enum {
  PPROC_IFACE_INIT,
  PPROC_IFACE_SET_CALLBACK,
  PPROC_IFACE_GET_CAPABILITY,
  PPROC_IFACE_PROCESS_FRAME,
  PPROC_IFACE_FRAME_DONE,
  PPROC_IFACE_FRAME_DIVERT,
  PPROC_IFACE_SET_SHARPNESS,
  PPROC_IFACE_STOP_STREAM,
  PPROC_IFACE_CREATE_SURFACE,
  PPROC_IFACE_DESTROY_SURFACE,
} pproc_interface_event_t;

typedef struct {
  /* Open func for pproc library
     return status -> success / failure */
  int32_t (*open)(void **);
  /* Set param for pproc library
     return status -> success / failure */
  int32_t (*process)(void *, pproc_interface_event_t, void *);
  /* close func for pproc library
     return status -> success / failure */
  int32_t (*close)(void *);
} pproc_interface_func_tbl_t;

typedef struct {
  pproc_interface_func_tbl_t *func_tbl;
  void                       *lib_private_data;
} pproc_interface_lib_params_t;

typedef struct {
  /* ref count to make sure pproc sub module is singleton */
  uint32_t                      refcount;
  /* handle to CPP / C2D library opened using dlopen() */
  void                         *lib_handle;
  /* pointer to static structure never try to malloc/free */
  pproc_interface_lib_params_t *lib_params;
  uint32_t                      ref_count;
} pproc_interface_t;

typedef struct {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
} c2d_roi_cfg_t;

typedef struct _c2d_lens_correction_t {
  int    use_LC;
  float *transform_mtx;
  unsigned int transform_type;
}c2d_lens_correction_t;

typedef struct _c2d_frame_sp {
  /* phy addr of the buffer */
  unsigned long  phy_addr;
  uint32_t       y_off;
  uint32_t       cbcr_off;
  /* buffer length */
  uint32_t       length;
  int32_t        fd;
  uint32_t       addr_offset;
  /* mapped addr */
  unsigned long  vaddr;
} c2d_frame_sp;

typedef struct _c2d_frame_mp {
  /* phy addr of the plane */
  unsigned long  phy_addr;
  /* offset of plane data */
  uint32_t       data_offset;
  /* plane length */
  uint32_t       length;
  int32_t        fd;
  uint32_t       addr_offset;
  /* mapped addr */
  unsigned long  vaddr;
} c2d_frame_mp;

typedef struct _c2d_frame {
  uint32_t                   frame_id;
  uint32_t                   num_planes; /* 1 for sp */
  struct ion_allocation_data ion_alloc[MAX_PLANES];
  struct ion_fd_data         fd_data[MAX_PLANES];
  c2d_roi_cfg_t              roi_cfg;
  c2d_lens_correction_t      lens_correction_cfg;
  cam_format_t               format;
  uint32_t                   width;
  uint32_t                   height;
  uint32_t                   stride;
  uint32_t                   x_border;
  uint32_t                   y_border;
  union {
    c2d_frame_sp             sp;
    c2d_frame_mp             mp[MAX_PLANES];
  };
} c2d_frame;

typedef enum {
  C2D_FLIP_H = 1 << 0,
  C2D_FLIP_V = 1 << 1,
} c2d_flip_type;

typedef struct _c2d_gpu_buf_t{
  uint16_t num_planes;
  union {
    uint32_t sp_gAddr;
    uint32_t mp_gAddr[MAX_PLANES];
  };
} c2d_gpu_buf_t;

typedef struct _c2d_module_libparams {
  uint32_t id;
  C2D_YUV_SURFACE_DEF surface_def;
  C2D_SURFACE_BITS surface_type;
  c2d_gpu_buf_t gpu_buf;
  C2D_YUV_FORMAT format;
  C2D_SURFACE_BITS surface_bit;
} c2d_libparams;


typedef struct _c2d_process_frame_buffer {
  c2d_frame            *c2d_input_buffer;
  c2d_frame            *c2d_output_buffer;
  uint32_t              rotation;
  c2d_flip_type         flip;
  c2d_libparams         c2d_input_lib_params;
  c2d_libparams         c2d_output_lib_params;
} c2d_process_frame_buffer;

pproc_interface_lib_params_t *pproc_library_init(void);

#endif
