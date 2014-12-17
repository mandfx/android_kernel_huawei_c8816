/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <mach/gpiomux.h>
#include "msm_sensor.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#include "msm_camera_i2c_mux.h"
#include <misc/app_info.h>

#define OV2685_SENSOR_NAME "ov2685"
#define PLATFORM_DRIVER_NAME "msm_camera_ov2685"

#define CONFIG_MSMB_CAMERA_DEBUG
#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif


#define MIPI_SENSOR_CLOCK_FREQ    2400 /* 24MHz */


DEFINE_MSM_MUTEX(ov2685_mut);

void OV2685FullSizeCaptureSetting(struct msm_sensor_ctrl_t *s_ctrl);
static struct msm_sensor_ctrl_t ov2685_s_ctrl;

static struct msm_sensor_power_setting ov2685_power_setting[] = {
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 1,
	},
	
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 2,
	},
	
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},

	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 1,
	},

	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 1,
	},
	
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 2,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 1,
	},
};

static struct msm_sensor_power_setting ov2685_power_down_setting[] = {
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 1,
	},
	
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 1,
	},
	
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 1,
	},
	
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 20,
	},
};

static struct msm_camera_i2c_reg_conf ov2685_start_settings[] = {
	{0x0100, 0x01},  //{0x4202,0x00},	
};

static struct msm_camera_i2c_reg_conf ov2685_stop_settings[] = {
	{0x0100, 0x00}, 
};

static struct msm_camera_i2c_reg_conf ov2685_recommend_settings0[] = {
  	{0x0103, 0x01}, // soft reset
};

static struct msm_camera_i2c_reg_conf ov2685_recommend_settings[] = {
	{0x0100, 0x00}, 
	{0x3002, 0x00}, // gpio input, vsync input, fsin input
	{0x3016, 0x1c}, // drive strength of low speed = 1x, bypass latch of hs_enable
	{0x3018, 0x44}, // MIPI 1 lane, 10-bit mode
	{0x301d, 0xf0}, // enable clocks
	{0x3020, 0x00},// output raw
	{0x3082, 0x2c}, // PLL
	{0x3083, 0x03}, // PLL
	{0x3084, 0x0f}, // PLL
	{0x3085, 0x03},// PLL
	{0x3086, 0x00},// PLL
	{0x3087, 0x00},// PLL
	{0x3501, 0x26},// exposure M
	{0x3502, 0x40},// exposure L
	{0x3503, 0x03},// AGC manual, AEC manual
	{0x350b, 0x36},// Gain L
	{0x3600, 0xb4},// analog conrtol
	{0x3603, 0x35},//
	{0x3604, 0x24},//
	{0x3605, 0x00},//
	{0x3620, 0x26},//
	{0x3621, 0x37},//
	{0x3622, 0x04},//
	{0x3628, 0x10},// analog control
	{0x3705, 0x3c},// sensor control
	{0x370a, 0x23},//
	{0x370c, 0x50},//
	{0x370d, 0xc0},//
	{0x3717, 0x58},//
	{0x3718, 0x88},//
	{0x3720, 0x00},//
	{0x3721, 0x00},//
	{0x3722, 0x00},//
	{0x3723, 0x00},//
	{0x3738, 0x00},// sensor control
	{0x3781, 0x80},// PSRAM
	{0x3789, 0x60},// PSRAM
	{0x3800, 0x00}, // x start H
	{0x3801, 0x00},// x start L
	{0x3802, 0x00},// y start H
	{0x3803, 0x00},// y start L
	{0x3804, 0x06},// x end H
	{0x3805, 0x4f},// x end L
	{0x3806, 0x04},// y end H
	{0x3807, 0xbf},// y end L
	{0x3808, 0x03},// x output size H
	{0x3809, 0x20},// x output size L
	{0x380a, 0x02},// y output size H
	{0x380b, 0x58},// y output size L
	{0x380c, 0x06}, // HTS H
	{0x380d, 0xac},// HTS L
	{0x380e, 0x02}, // VTS H
	{0x380f, 0x84},// VTS L
	{0x3810, 0x00},// ISP x win H
	{0x3811, 0x04},// ISP x win L
	{0x3812, 0x00},// ISP y win H
	{0x3813, 0x04},// ISP y win L
	{0x3814, 0x31},// x inc
	{0x3815, 0x31},// y inc
	{0x3819, 0x04},// Vsync end row L

#ifdef OV2685_FLIP
#ifdef OV2685_MIRROR
	{0x3820, 0xc6}, // vsub48_blc on, vflip_blc on, Flip on, vbinf on,
	{0x3821, 0x05}, // Mirror on, hbin on
#else
	{0x3820, 0xc6}, // vsub48_blc on, vflip_blc on, Flip on, vbinf on,
	{0x3821, 0x01}, // hbin on
#endif
#else
#ifdef OV2685_MIRROR
	{0x3820, 0xc2}, // vsub48_blc on, vflip_blc on, vbinf on,
	{0x3821, 0x05}, // Mirror on, hbin on
#else
	{0x3820, 0xc2},  // vsub48_blc on, vflip_blc on, vbinf on,
	{0x3821, 0x01},  // hbin on
#endif
#endif

	//// IQ setting sta0xfrom, 0xhe,re
	{0x382a, 0x08}, // auto VTS
	
	// AEC
	{0x3a00, 0x43}, // night mode enable, band enable
	{0x3a02, 0x90},// 50Hz
	{0x3a03, 0x4e},// AEC target H
	{0x3a04, 0x40},// AEC target L
	{0x3a06, 0x00},// B50 H
	{0x3a07, 0xc1},// B50 L
	{0x3a08, 0x00},// B60 H
	{0x3a09, 0xa1},// B60 L
	{0x3a0a, 0x07},// max exp 50 H
	{0x3a0b, 0x8a},// max exp 50 L, 10 band
	{0x3a0c, 0x07},// max exp 60 H
	{0x3a0d, 0x8c},// max exp 60 L, 12 band
	{0x3a0e, 0x02},// VTS band 50 H
	{0x3a0f, 0x43},// VTS band 50 L
	{0x3a10, 0x02},// VTS band 60 H
	{0x3a11, 0x84},// VTS band 60 L
	{0x3a13, 0x80},// gain ceiling = 8x
	{0x4000, 0x81},// avg_weight = 8, mf_en on
	{0x4001, 0x40},// format_trig_beh on
	{0x4002, 0x00},// blc target
	{0x4003, 0x0c},// blc target
	{0x4008, 0x00},// bl_start
	{0x4009, 0x03},// bl_end
	{0x4300, 0x30},  //Benz // YUV 422
	{0x430e, 0x00},
	{0x4602, 0x02},// VFIFO R2, frame reset enable
	{0x4837, 0x1e},// MIPI global timing
	{0x5000, 0xff},// lenc_en, awb_gain_en, lcd_en, avg_en, bc_en, WC_en, blc_en
	{0x5001, 0x05},// avg after LCD
	{0x5002, 0x32},// dpc_href_s, sof_sel, bias_plus
	{0x5003, 0x04},// bias_man
	{0x5004, 0xff},// uv_dsn_en, rgb_dns_en, gamma_en, cmx_en, cip_en, raw_dns_en, strech_en, awb_en
	{0x5005, 0x12},// sde_en, rgb2yuv_en
	//{0x4202, 0x0f},// stream off


	// AWB
	{0x5180, 0xf4}, // AWB
	{0x5181, 0x11},//
	{0x5182, 0x41},//
	{0x5183, 0x42},//
	{0x5184, 0x82},// cwf_x
	{0x5185, 0x52},// cwf_y
	{0x5186, 0x86},// kx(cwf 2 a)x2y
	{0x5187, 0xa6},// Ky(cwf 2 day)y2x
	{0x5188, 0x0c},// cwf range
	{0x5189, 0x0e},// a range
	{0x518a, 0x0d},// day range
	{0x518b, 0x4f},// day limit
	{0x518c, 0x3c},// a limit
	{0x518d, 0xf8},//
	{0x518e, 0x04},//
	{0x518f, 0x7f},//
	{0x5190, 0x40},//
	{0x5191, 0x5f},//
	{0x5192, 0x40},//
	{0x5193, 0xff},//
	{0x5194, 0x40},//
	{0x5195, 0x06},//
	{0x5196, 0x8b},//
	{0x5197, 0x04},//
	{0x5198, 0x00},//
	{0x5199, 0x05},//
	{0x519a, 0x96},//
	{0x519b, 0x04},// AWB

	{0x5200, 0x09}, // stretch minimum = 30
	{0x5201, 0x00}, // stretch min low leve
	{0x5202, 0x06}, // stretch min low leve
	{0x5203, 0x20}, // stretch m0xhigh, 0xl
	{0x5204, 0x41}, // stretch step2, step1

    {0x5205, 0x16}, // stretch current low level
	{0x5206, 0x00}, // stretch curre0xhigh, 0xle,vel L
	{0x5207, 0x05}, // stretch curre0xhigh, 0xle,vel H
	{0x520b, 0x30}, // stretch_thres1 L
	{0x520c, 0x75}, // stretch_thres1 M
	{0x520d, 0x00}, // stretch_thres1 H
	{0x520e, 0x30}, // stretch_thres2 L
	{0x520f, 0x75}, // stretch_thres2 M
	{0x5210, 0x00}, // stretch_thres2 H
	
	// Raw de-noise auto +
	{0x5280, 0x15}, // m_nNoise YSlop = 5, Parameter noise and edgethre calculated by noise list
	{0x5281, 0x06}, // m_nNoiseList[0]
	{0x5282, 0x06}, // m_nNoiseList[1]
	{0x5283, 0x08}, // m_nNoiseList[2]
	{0x5284, 0x1c}, // m_nNoiseList[3]
	{0x5285, 0x1c}, // m_nNoiseList[4]
	{0x5286, 0x20}, // m_nNoiseList[5]
	{0x5287, 0x10}, // m_nMaxEdgeGThre
	
	// CIP Y de-noise, auto (default)
	{0x5300, 0xc5}, // m_bColorEdgeEnable, m_bAntiAlasing on, m_nNoise YSlop = 5
	{0x5301, 0xa0}, // m_nSharpenSlop = 1
	{0x5302, 0x06}, // m_nNoiseList[0]
	{0x5303, 0x08}, // m_nNoiseList[1]
	{0x5304, 0x10}, // m_nNoiseList[2]
	{0x5305, 0x20}, // m_nNoiseList[3]
	{0x5306, 0x30}, // m_nNoiseList[4]
	{0x5307, 0x60}, // m_nNoiseList[5]     

	// Sharpness
	{0x5308, 0x32}, // m_nMaxSarpenGain, m_nMinSharpenGain
	{0x5309, 0x00}, // m_nMinSharpen
	{0x530a, 0x2a}, // m_nMaxSharpen
	{0x530b, 0x02}, // m_nMinDetail                                                         
	{0x530c, 0x02}, // m_nMaxDetail                                                         
	{0x530d, 0x00}, // m_nDetailRatioList[0]                                                       
	{0x530e, 0x0c}, // m_nDetailRatioList[1]                                                       
	{0x530f, 0x14}, // m_nDetailRatioList[2]                                                       
	{0x5310, 0x1a}, // m_nSharpenNegEdgeRatio                                                      
	{0x5311, 0x20}, // m_nClrEdgeShT1                                                         
	{0x5312, 0x80}, // m_nClrEdgeShT2                                                         
	{0x5313, 0x4b}, // m_nClrEdgeShpSlop   
	
	// color matrix                                                         
	{0x5380, 0x01}, // nCCM_D[0][0] H                                                         
	{0x5381, 0x83}, // nCCM_D[0][0] L                                                         
	{0x5382, 0x00}, // nCCM_D[0][1] H                                                         
	{0x5383, 0x1f}, // nCCM_D[0][1] L                                                         
	{0x5384, 0x00}, // nCCM_D[1][0] H                                                         
	{0x5385, 0x88}, // nCCM_D[1][0] L                                                         
	{0x5386, 0x00}, // nCCM_D[1][1] H                                                         
	{0x5387, 0x82}, // nCCM_D[1][1] L                                                         
	{0x5388, 0x00}, // nCCM_D[2][0] H                                                         
	{0x5389, 0x40}, // nCCM_D[2][0] L                                                         
	{0x538a, 0x01}, // nCCM_D[2][1] H                                                         
	{0x538b, 0xb9}, // nCCM_D[2][1] L                                                         
	{0x538c, 0x10}, // Sing bit [2][1], [2][0], [1][1], [1][0], [0][1], [0][0]    
	
	// Gamma                                                           
	{0x5400, 0x0d}, // m_pCurveYList[0]                                                         
	{0x5401, 0x1a}, // m_pCurveYList[1]                                                         
	{0x5402, 0x32}, // m_pCurveYList[2]                                                         
	{0x5403, 0x59}, // m_pCurveYList[3] 

	{0x5404, 0x68}, // m_pCurveYList[4]                                                            
	{0x5405, 0x76}, // m_pCurveYList[5]                                                            
	{0x5406, 0x82}, // m_pCurveYList[6]                                                            
	{0x5407, 0x8c}, // m_pCurveYList[7]                                                            
	{0x5408, 0x94}, // m_pCurveYList[8]                                                            
	{0x5409, 0x9c}, // m_pCurveYList[9]                                                            
	{0x540a, 0xa9}, // m_pCurveYList[10]                                                            
	{0x540b, 0xb6}, // m_pCurveYList[11]                                                            
	{0x540c, 0xcc}, // m_pCurveYList[12]                                                            
	{0x540d, 0xdd}, // m_pCurveYList[13]                                                            
	{0x540e, 0xeb}, // m_pCurveYList[14]                                                            
	{0x540f, 0xa0}, // m_nMaxShadowHGain                                                            
	{0x5410, 0x6e}, // m_nMidTongHGain                                                            
	{0x5411, 0x06}, // m_nHighLightHGain    
	
	// RGB De-noise                                                            
	{0x5480, 0x19}, // m_nShadowExtraNoise = 12, m_bSmoothYEnable                                                            
	{0x5481, 0x00}, // m_nNoiseYList[1], m_nNoiseYList[0]                                                            
	{0x5482, 0x09}, // m_nNoiseYList[3], m_nNoiseYList[2]                                                            
	{0x5483, 0x12}, // m_nNoiseYList[5], m_nNoiseYList[4]                                                            
	{0x5484, 0x04}, // m_nNoiseUVList[0]                                                            
	{0x5485, 0x06}, // m_nNoiseUVList[1]                                                            
	{0x5486, 0x08}, // m_nNoiseUVList[2]                                                            
	{0x5487, 0x0c}, // m_nNoiseUVList[3]                                                            
	{0x5488, 0x10}, // m_nNoiseUVList[4]                                                            
	{0x5489, 0x18}, // m_nNoiseUVList[5]    
	
	// UV de-noise auto -1                                                            
	{0x5500, 0x00}, // m_nNoiseList[0]                                                            
	{0x5501, 0x01}, // m_nNoiseList[1]                                                            
	{0x5502, 0x02}, // m_nNoiseList[2]                                                            
	{0x5503, 0x03}, // m_nNoiseList[3]                                                            
	{0x5504, 0x04}, // m_nNoiseList[4]                                                            
	{0x5505, 0x05}, // m_nNoiseList[5]                                                            
	{0x5506, 0x00}, // uuv_dns_psra_man dis, m_nShadowExtraNoise = 0   
		
     
	// UV adjust
	{0x5600, 0x06}, // fixy off, neg off, gray off, fix_v off, fix_u off, contrast_en, saturation on
	{0x5603, 0x40}, // sat U
	{0x5604, 0x20}, // sat V
	{0x5608, 0x00},
	{0x5609, 0x40}, // uvadj_th1
	{0x560a, 0x80}, // uvadj_th2
	
	// lens correction
	{0x5800, 0x03}, // red x0 H
	{0x5801, 0x08}, // red x0 L
	{0x5802, 0x02}, // red y0 H
	{0x5803, 0x40}, // red y0 L
	{0x5804, 0x48}, // red a1
	{0x5805, 0x05}, // red a2
	{0x5806, 0x12}, // red b1
	{0x5807, 0x05}, // red b2
	{0x5808, 0x03}, // green x0 H
	{0x5809, 0x08}, // green x0 L
	{0x580a, 0x02}, // green y0 H
	{0x580b, 0x38}, // green y0 L
	{0x580c, 0x2f}, // green a1
	{0x580d, 0x05}, // green a2
	{0x580e, 0x52}, // green b1
	{0x580f, 0x06}, // green b2
	{0x5810, 0x02}, // blue x0 H
	{0x5811, 0xfc}, // blue x0 L
	{0x5812, 0x02}, // blue y0 H

	{0x5813, 0x30}, // blue y0 L
	{0x5814, 0x2c}, // bule a1
	{0x5815, 0x05}, // blue a2
	{0x5816, 0x42}, // blue b1
	{0x5817, 0x06}, // blue b2
	{0x5818, 0x0d}, // rst_seed on, md_en, coef_m off, gcoef_en
	{0x5819, 0x40}, // lenc_coef_th
	{0x581a, 0x04}, // lenc_gain_thre1
	{0x581b, 0x0c}, // lenc_gain_thre2
	{0x3503, 0x00}, // AEC auto, AGC auto
	//{0x0100, 0x01},// wake up
};

static struct v4l2_subdev_info ov2685_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt	= 1,
		.order	= 0,
	},
};

/* preview */
static struct msm_camera_i2c_reg_conf ov2685_svga_settings[] = {
	 // 800x600 preview
	// 30-10fps
	// MCLK=24Mhz, SysClk=33Mhz, MIPI 1 lane 528Mbps
	//0x0100, 0x00, // sleep
	//{0x4202, 0x0f}, // stream off
	{0x3501, 0x26}, // exposure M
	{0x3502, 0x40}, // exposure L
	{0x3620, 0x26}, // analog control
	{0x3621, 0x37}, //
	{0x3622, 0x04}, // analog control
	{0x370a, 0x23}, // sensor control
	{0x3718, 0x88}, //
	{0x3721, 0x00}, //
	{0x3722, 0x00}, //
	{0x3723, 0x00}, //
	{0x3738, 0x00}, // sensor control
	{0x3801, 0x00}, // x start L
	{0x3803, 0x00}, // y start L
	{0x3804, 0x06}, // x end H
	{0x3805, 0x4f}, // x end L
	{0x3806, 0x04}, // y end H
	{0x3807, 0xbf}, // y end L
	{0x3808, 0x03}, // x output size H
	{0x3809, 0x20}, // x output size L
	{0x380a, 0x02}, // y output size H
	{0x380b, 0x58}, // y output size L
	{0x380c, 0x06}, // HTS H
	{0x380d, 0xac}, // HTS L
	{0x380e, 0x02}, // VTS H
	{0x380f, 0x84}, // VTS L
	{0x3811, 0x04}, // ISP x win L
	{0x3813, 0x04}, // ISP y win L
	{0x3814, 0x31}, // x inc
	{0x3815, 0x31}, // y inc

#ifdef OV2685_FLIP  /* Benz */
#ifdef OV2685_MIRROR
	{0x3820, 0xc6}, // vsub48_blc on, vflip_blc on, Flip on, vbinf on,
	{0x3821, 0x05}, // Mirror on, hbin on
#else
	{0x3820, 0xc6},// vsub48_blc on, vflip_blc on, Flip on, vbinf on,
	{0x3821, 0x01},// hbin on
#endif
#else
#ifdef OV2685_MIRROR
	{0x3820, 0xc2}, // vsub48_blc on, vflip_blc on, vbinf on,
	{0x3821, 0x05}, // Mirror on, hbin on
#else
	{0x3820, 0xc2}, // vsub48_blc on, vflip_blc on, vbinf on,
	{0x3821, 0x01},  // hbin on
#endif
#endif

	{0x382a, 0x08}, // auto VTS
	{0x3a00, 0x43}, // nig mode on, band on
	{0x3a07, 0xc1}, // B50 L
	{0x3a09, 0xa1}, // B60 L
	{0x3a0a, 0x07}, // max exp 50 H
	{0x3a0b, 0x8a}, // max exp 50 L
	{0x3a0c, 0x07}, // max exp 60 H
	{0x3a0d, 0x8c}, // max exp 60 L
	{0x3a0e, 0x02}, // VTS band 50, H
	{0x3a0f, 0x43}, // VTS band 50, L
	{0x3a10, 0x02}, // VTS band 60, H
	{0x3a11, 0x84}, // VTS band 60, L
	{0x3a13, 0x80}, // gain ceiling = 8x
	{0x4008, 0x00}, // bl_start
	{0x4009, 0x03}, // bl_end
	{0x3503, 0x00}, // AEC auto, AGC auto
};

/* capture */
static struct msm_camera_i2c_reg_conf ov2685_uxga_settings[] = {
// UXGA Capture
	// OV2685_UXGA_YUV7.5 fps
	// MCLK=24Mhz, SysClk=33Mhz, MIPI 1 lane 528Mbps
	//0x0100, 0x00, // sleep
	//{0x4202, 0x0f}, // stream off
	{0x3503, 0x03}, // AEC manual, AGC manual
	{0x3501, 0x4e},// exposure M
	{0x3502, 0xe0},// exposure L
	{0x3620, 0x24},// analog control
	{0x3621, 0x34},//
	{0x3622, 0x03},// analog control
	{0x370a, 0x21},// sensor control
	{0x3718, 0x80},//
	{0x3721, 0x09},//
	{0x3722, 0x06},//
	{0x3723, 0x59},//
	{0x3738, 0x99},// sensor control
	{0x3801, 0x00},// x start L
	{0x3803, 0x00},// y start L
	{0x3804, 0x06},// x end H
	{0x3805, 0x4f},// x end L
	{0x3806, 0x04},// y end H
	{0x3807, 0xbf},// y end L
	{0x3808, 0x06},// x output size H
	{0x3809, 0x40},// x output size L
	{0x380a, 0x04},// y output size H
	{0x380b, 0xb0},// y output size L
	{0x380c, 0x06},// HTS H
	{0x380d, 0xa4},// HTS L
	{0x380e, 0x0a},// VTS H
	{0x380f, 0x1c},// VTS L
	{0x3811, 0x08},// ISP x win L
	{0x3813, 0x08},// ISP y win L
	{0x3814, 0x11},// x inc
	{0x3815, 0x11},// y inc

#ifdef OV2685_FLIP
#ifdef OV2685_MIRROR
	{0x3820, 0xc4}, // vsub48_blc on, vflip_blc on, Flip on, vbinf off,
	{0x3821, 0x04}, // Mirror on, hbin off
#else
	{0x3820, 0xc4},// vsub48_blc on, vflip_blc on, Flip on, vbinf off,
	{0x3821, 0x00},// hbin off
#endif
#else
#ifdef OV2685_MIRROR
	{0x3820, 0xc0}, // vsub48_blc on, vflip_blc on, vbinf off,
	{0x3821, 0x04},// Mirror on, hbin off
#else
	{0x3820, 0xc0},// vsub48_blc on, vflip_blc on, vbinf off,
	{0x3821, 0x00},// hbin off
#endif
#endif

	{0x382a, 0x00}, // fixed VTS
	{0x3a00, 0x41}, // nig0xmode, 0xof,f, band on
	{0x3a07, 0xc2}, // B50 L
	{0x3a09, 0xa1}, // B60 L
	{0x3a0a, 0x09}, // max exp 50 H
	{0x3a0b, 0xda}, // max exp 50 L
	{0x3a0c, 0x0a}, // max exp 60 H
	{0x3a0d, 0x10}, // max exp 60 L
	{0x3a0e, 0x04}, // V0xband, 0x50, H
	{0x3a0f, 0x8c}, // V0xband, 0x50, L
	{0x3a10, 0x05}, // V0xband, 0x60, H
	{0x3a11, 0x08}, // V0xband, 0x60, L
	{0x3a13, 0x60}, // gain ceiling = 6x
	{0x4008, 0x02}, // bl_start
	{0x4009, 0x09}, // bl_end
};



static struct msm_camera_i2c_reg_conf OV2685_reg_antibanding[4][26] = {};

static struct msm_camera_i2c_reg_conf OV2685_reg_effect_normal[] = {
	{0x3208, 0x00},
	{0x5600, 0x06},
	{0x5603, 0x40},
	{0x5604, 0x28},
	{0x3208, 0x10},
	{0x3208, 0xa0},
};

static struct msm_camera_i2c_reg_conf OV2685_reg_effect_black_white[] = {
	{0x3208, 0x00},
	{0x5600, 0x1c},
	{0x5603, 0x80},
	{0x5604, 0x80},
	{0x3208, 0x10},
	{0x3208, 0xa0},
};

static struct msm_camera_i2c_reg_conf OV2685_reg_effect_negative[] = {
	{0x3208, 0x00},
	{0x5600, 0x46},
	{0x5603, 0x40},
	{0x5604, 0x28},
	{0x3208, 0x10},
	{0x3208, 0xa0},
};

static struct msm_camera_i2c_reg_conf OV2685_reg_effect_old_movie[] = {
	{0x3208, 0x00},
	{0x5600, 0x1c},
	{0x5603, 0x40},
	{0x5604, 0xa0},
	{0x3208, 0x10},
	{0x3208, 0xa0},
};

static struct msm_camera_i2c_reg_conf OV2685_reg_effect_aqua[] = {
	{0x3208, 0x00},
	{0x5600, 0x1c},
	{0x5603, 0xa0},
	{0x5604, 0x40},
	{0x3208, 0x10},
	{0x3208, 0xa0},
};

static struct msm_camera_i2c_reg_conf OV2685_reg_wb_auto[] = {
	{0x3208, 0x00},//start group 1
	{0x5180, 0xf4},
	{0x3208, 0x10},//end group 1
	{0x3208, 0xa0},//launch group 1	
};

static struct msm_camera_i2c_reg_conf OV2685_reg_wb_sunny[] = {
	{0x3208, 0x00},//start group 1 
	{0x5180, 0xf6},                
	{0x5195, 0x07},//R Gain        
	{0x5196, 0x9c},//              
	{0x5197, 0x04},//G Gain        
	{0x5198, 0x00},//              
	{0x5199, 0x05},//B Gain        
	{0x519a, 0xf3},//              
	{0x3208, 0x10},//end group 1   
	{0x3208, 0xa0},//launch group 1
};

static struct msm_camera_i2c_reg_conf OV2685_reg_wb_cloudy[] = {
	{0x3208, 0x00},//start group 1
	{0x5180, 0xf6}, 
	{0x5195, 0x07},//R Gain
	{0x5196, 0xdc},//
	{0x5197, 0x04},//G Gain
	{0x5198, 0x00},//
	{0x5199, 0x05},//B Gain
	{0x519a, 0xd3},//
	{0x3208, 0x10},//end group 1
	{0x3208, 0xa0},//launch group 1	
};

static struct msm_camera_i2c_reg_conf OV2685_reg_wb_office[] = {
	{0x3208, 0x00},//start group 1
	{0x5180, 0xf6}, 
	{0x5195, 0x06},//R Gain
	{0x5196, 0xb8},//
	{0x5197, 0x04},//G Gain
	{0x5198, 0x00},//
	{0x5199, 0x06},//B Gain
	{0x519a, 0x5f},//
	{0x3208, 0x10},//end group 1
	{0x3208, 0xa0},//launch group 1	
};

static struct msm_camera_i2c_reg_conf OV2685_reg_wb_home[] = {
	{0x3208, 0x00},//start group 1
	{0x5180, 0xf6}, 
	{0x5195, 0x04},//R Gain
	{0x5196, 0x90},//
	{0x5197, 0x04},//G Gain
	{0x5198, 0x00},//
	{0x5199, 0x09},//B Gain
	{0x519a, 0x20},//
	{0x3208, 0x10},//end group 1
	{0x3208, 0xa0},//launch group 1
};

uint32_t OV2685_get_sysclk(struct msm_sensor_ctrl_t *s_ctrl)
{
	// calculate sysclk
	uint16_t temp1, temp2,Div_loop;
	uint32_t sysclk;
	uint32_t Pre_div0, Pre_div2x, SP_Div, SysDiv, VCO;
	uint32_t Pre_div2x_map[] = {
	2, 3, 4, 5, 6, 8, 12, 16};
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x3088,&temp1, MSM_CAMERA_I2C_BYTE_DATA);
	temp2 = (temp1>>4) & 0x01;
	Pre_div0 = temp2 + 1;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x3080,&temp1, MSM_CAMERA_I2C_BYTE_DATA);
	temp2 = temp1 & 0x07;
	Pre_div2x = Pre_div2x_map[temp2];
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x3081,&temp1, MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x3082,&Div_loop,MSM_CAMERA_I2C_BYTE_DATA);
	Div_loop = ((temp1 & 0x03)<<8) + Div_loop;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x3086,&temp1, MSM_CAMERA_I2C_BYTE_DATA);
	temp2 = temp1 & 0x07;
	SP_Div = temp2 + 1;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x3084,&temp1, MSM_CAMERA_I2C_BYTE_DATA);
	temp2 = temp1 & 0x0f;
	SysDiv = temp2 + 1;
	VCO = MIPI_SENSOR_CLOCK_FREQ * Div_loop * 2 / Pre_div0 / Pre_div2x;
	sysclk = VCO / SP_Div / SysDiv ;
	
	// Check sysclk
	//temp1 = sysclk & 0xff;
	//temp2 = sysclk>>8;
	//OV2685_write_i2c(0x500b, temp1);
	//OV2685_write_i2c(0x500d, temp2);
	
	return sysclk;
}

uint16_t OV2685_get_HTS(struct msm_sensor_ctrl_t *s_ctrl)
{
	// read HTS from register settings
	uint16_t HTS;
	uint16_t temp;
	
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x380c,&temp, MSM_CAMERA_I2C_BYTE_DATA);
	temp = (temp<<8);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x380d,&HTS,MSM_CAMERA_I2C_BYTE_DATA);
	HTS =  HTS + temp;
	return HTS;
}

uint16_t OV2685_get_VTS(struct msm_sensor_ctrl_t *s_ctrl)
{
	// read VTS from register settings
	uint16_t VTS,temp;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x380e,&temp,MSM_CAMERA_I2C_BYTE_DATA);
	temp = temp<<8;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x380f,&VTS,MSM_CAMERA_I2C_BYTE_DATA);
	VTS = temp + temp;
	return VTS;
}

void OV2685_set_bandingfilter(struct msm_sensor_ctrl_t *s_ctrl)
{
	uint16_t preview_VTS,preview_sysclk,preview_HTS;
	uint16_t band_step60, band_step50;
	// read preview PCLK
	preview_sysclk = OV2685_get_sysclk(s_ctrl);
	// read preview HTS
	preview_HTS = OV2685_get_HTS(s_ctrl);
	// read preview VTS
	preview_VTS = OV2685_get_VTS(s_ctrl);
	// calculate banding filter
	// 60Hz
	band_step60 = preview_sysclk * 100/preview_HTS * 100/120;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x3a08, (band_step60 >> 8),MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x3a09, (band_step60 & 0xff),MSM_CAMERA_I2C_BYTE_DATA);
	// 50Hz
	band_step50 = preview_sysclk * 100/preview_HTS;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x3a06, (band_step50 >> 8),MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x3a07, (band_step50 & 0xff),MSM_CAMERA_I2C_BYTE_DATA);
}


uint16_t OV2685_get_gain16(struct msm_sensor_ctrl_t *s_ctrl)
{
	uint16_t gain16,temp1,temp2;
	
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x350a,&temp1,MSM_CAMERA_I2C_BYTE_DATA);
	temp1 = (temp1 & 0x07) <<8;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x350b,&temp2,MSM_CAMERA_I2C_BYTE_DATA);
	gain16 = temp1 + temp2;
	return gain16;
}

uint32_t OV2685_get_binning_factor(struct msm_sensor_ctrl_t *s_ctrl)
{
	return 1;
}

void OV2685_set_night_mode(uint32_t iNightMode,struct msm_sensor_ctrl_t *s_ctrl)
{
	if (iNightMode)   
	{
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x3a00,0x43,MSM_CAMERA_I2C_BYTE_DATA);
	}
	else
	{
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x3a00,0x41,MSM_CAMERA_I2C_BYTE_DATA);
	}

	return;
}

uint16_t OV2685_get_light_frequency(struct msm_sensor_ctrl_t *s_ctrl)
{
	// get light frequency
	uint16_t temp, light_frequency;
	
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x3a02,&temp,MSM_CAMERA_I2C_BYTE_DATA);
	
	if (temp & 0x80) 
	{
		// 50Hz
		light_frequency = 50;
	}
	else 
	{
		// 60Hz
		light_frequency = 60;
	}
	return light_frequency;
}

uint32_t OV2685_set_gain16(int gain16,struct msm_sensor_ctrl_t *s_ctrl)
{
	// write gain, 16 = 1x
	uint16_t temp;
	gain16 = gain16 & 0x7ff;
	temp = gain16 & 0xff;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x350b, temp,MSM_CAMERA_I2C_BYTE_DATA);
	temp = gain16>>8;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x350a, temp,MSM_CAMERA_I2C_BYTE_DATA);
	return 0;
}

uint32_t OV2685_set_VTS(uint32_t VTS,struct msm_sensor_ctrl_t *s_ctrl)
{
	// write VTS to registers
	uint16_t temp;
	temp = VTS & 0xff;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x380f, temp,MSM_CAMERA_I2C_BYTE_DATA);
	temp = VTS>>8;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x380e, temp,MSM_CAMERA_I2C_BYTE_DATA);
	return 0;
}

uint32_t OV2685_set_shutter(int shutter,struct msm_sensor_ctrl_t *s_ctrl)
{
	// write shutter, in number of line period
	uint16_t temp;
	
	shutter = shutter & 0xffff;
	temp = shutter & 0x0f;
	temp = temp<<4;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x3502, temp,MSM_CAMERA_I2C_BYTE_DATA);
	temp = shutter & 0xfff;
	temp = temp>>4;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x3501, temp,MSM_CAMERA_I2C_BYTE_DATA);
	temp = shutter>>12;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x3500, temp,MSM_CAMERA_I2C_BYTE_DATA);
	return 0;
}

uint16_t OV2685_get_shutter(struct msm_sensor_ctrl_t *s_ctrl)
{
	// read shutter, in number of line period
	uint16_t shutter,temp1,temp2,temp3;
	
	shutter = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x03500,&temp1,MSM_CAMERA_I2C_BYTE_DATA);
	temp1 = (temp1 & 0x0F)<<8;
	
	 s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x3501,&temp2,MSM_CAMERA_I2C_BYTE_DATA);
	 temp2 = (temp1 + temp2)<<4;
	 
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x3502,&temp3,MSM_CAMERA_I2C_BYTE_DATA);
	shutter = temp2 + (temp3 >>4);
	
	return shutter;
}

void OV2685_stream_on(struct msm_sensor_ctrl_t *s_ctrl)
{
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x0100,0x01,MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x4202,0x00,MSM_CAMERA_I2C_BYTE_DATA);
}

void OV2685Capture(struct msm_sensor_ctrl_t *s_ctrl)
{
	uint32_t preview_shutter, preview_gain16, preview_binning;
	uint32_t capture_shutter, capture_gain16, capture_sysclk, capture_HTS, capture_VTS;
	uint32_t light_frequency, capture_bandingfilter, capture_max_band;
	uintptr_t capture_gain16_shutter;
	uint32_t preview_sysclk, preview_HTS;

	// read preview shutter
	preview_shutter = OV2685_get_shutter(s_ctrl);

	// read preview gain
	preview_gain16 = OV2685_get_gain16(s_ctrl);

	// get preview binning factor
	preview_binning = OV2685_get_binning_factor(s_ctrl);

	// turn off night mode for capture
	OV2685_set_night_mode(0,s_ctrl);

       // read Preview sysclk
	preview_sysclk = OV2685_get_sysclk(s_ctrl);

	//read preview HTS
	preview_HTS =  OV2685_get_HTS(s_ctrl);

	// write Reg 
	OV2685FullSizeCaptureSetting(s_ctrl);
	
	// read capture sysclk
	capture_sysclk = OV2685_get_sysclk(s_ctrl);

	// read capture HTS
	capture_HTS = OV2685_get_HTS(s_ctrl);

	// read capture VTS
	capture_VTS = OV2685_get_VTS(s_ctrl);

	// calculate capture banding filter
	light_frequency = OV2685_get_light_frequency(s_ctrl);

	if (light_frequency == 60) 
	{
		// 60Hz
		capture_bandingfilter = capture_sysclk * 100/capture_HTS * 100/120;
	}
	else 
	{
		// 50Hz
		capture_bandingfilter = capture_sysclk * 100/capture_HTS;
	}
	
	capture_max_band = (uint32_t)((capture_VTS - 4)/capture_bandingfilter);
	
	// gain to shutter
	capture_gain16_shutter =  \
	preview_gain16 * preview_shutter * preview_binning * capture_sysclk/preview_sysclk *preview_HTS/capture_HTS;
	
	if(capture_gain16_shutter < (capture_bandingfilter * 16)) 
	{
		// shutter < 1/100
		capture_shutter = capture_gain16_shutter/16;
		if(capture_shutter<1) capture_shutter = 1;
		capture_gain16 = capture_gain16_shutter/capture_shutter;
		if(capture_gain16 < 16) capture_gain16 = 16;
	}
	else 
	{
		if(capture_gain16_shutter > (capture_bandingfilter*capture_max_band*16)) 
		{
			// exposure reach max
			capture_shutter = capture_bandingfilter*capture_max_band;
			capture_gain16 = capture_gain16_shutter / capture_shutter;
		}
		else 
		{
			// 1/100 < capture_shutter < max, capture_shutter = n/100
			capture_shutter = ((uint32_t) (capture_gain16_shutter/16/capture_bandingfilter)) * capture_bandingfilter;
			capture_gain16 = capture_gain16_shutter / capture_shutter;
		}
	}
	
	// write capture gain
	OV2685_set_gain16(capture_gain16,s_ctrl);
	
	// write capture shutter
	if (capture_shutter > (capture_VTS - 4)) 
	{
		capture_VTS = capture_shutter + 4;
		OV2685_set_VTS(capture_VTS,s_ctrl);
	}
	
	OV2685_set_shutter(capture_shutter,s_ctrl);
	 // skip 2 vysnc
	 mdelay(10); // 2/7.5

    //  OV2685_Buffer_on(s_ctrl);
	// start capture at 3rd vsync
	
	printk("[OV2685]OV2685Capture exit :\n ");
	return; 

}

static const struct i2c_device_id ov2685_i2c_id[] = {
	{OV2685_SENSOR_NAME, (kernel_ulong_t)&ov2685_s_ctrl},
	{ }
};

static int32_t msm_ov2685_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int rc;
	rc = msm_sensor_i2c_probe(client, id, &ov2685_s_ctrl);
	return rc;
}

static struct i2c_driver ov2685_i2c_driver = {
	.id_table = ov2685_i2c_id,
	.probe  = msm_ov2685_i2c_probe,
	.driver = {
		.name = OV2685_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov2685_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id ov2685_dt_match[] = {
	{.compatible = "shinetech,ov2685", .data = &ov2685_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, ov2685_dt_match);

static struct platform_driver ov2685_platform_driver = {
	.driver = {
		.name = "shinetech,ov2685",
		.owner = THIS_MODULE,
		.of_match_table = ov2685_dt_match,
	},
};

static void ov2685_i2c_write_table(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_camera_i2c_reg_conf *table,
		int num)
{
	int i = 0;
	int rc = 0;
	for (i = 0; i < num; ++i) {
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write(
			s_ctrl->sensor_i2c_client, table->reg_addr,
			table->reg_data,
			MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			msleep(100);
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write(
				s_ctrl->sensor_i2c_client, table->reg_addr,
				table->reg_data,
				MSM_CAMERA_I2C_BYTE_DATA);
		}
		table++;
	}
}
void OV2685FullSizeCaptureSetting(struct msm_sensor_ctrl_t *s_ctrl)
{
	ov2685_i2c_write_table(s_ctrl, &ov2685_uxga_settings[0],
	ARRAY_SIZE(ov2685_uxga_settings));		
}

static int32_t ov2685_sensor_enable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_INIT, NULL);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_CFG, (void *)&i2c_conf->i2c_mux_mode);
	return 0;
}

static int32_t ov2685_sensor_disable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
				VIDIOC_MSM_I2C_MUX_RELEASE, NULL);
	return 0;
}

int32_t ov2685_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	int32_t index = 0;
	struct msm_sensor_power_setting_array *power_setting_array = NULL;
	struct msm_sensor_power_setting *power_setting = NULL;

	struct msm_sensor_power_setting_array *power_down_setting_array = NULL;
	struct msm_camera_sensor_board_info *data = s_ctrl->sensordata;
	power_setting_array = &s_ctrl->power_setting_array;
	power_down_setting_array = &s_ctrl->power_down_setting_array;

	CDBG("%s:%d enter\n", __func__, __LINE__);

	if (data->gpio_conf->cam_gpiomux_conf_tbl != NULL) {
		pr_err("%s:%d mux install\n", __func__, __LINE__);
		msm_gpiomux_install(
			(struct msm_gpiomux_config *)
			data->gpio_conf->cam_gpiomux_conf_tbl,
			data->gpio_conf->cam_gpiomux_conf_tbl_size);
	}

	rc = msm_camera_request_gpio_table(
		data->gpio_conf->cam_gpio_req_tbl,
		data->gpio_conf->cam_gpio_req_tbl_size, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}
	for (index = 0; index < power_setting_array->size; index++) {
		CDBG("%s index %d\n", __func__, index);
		power_setting = &power_setting_array->power_setting[index];
		CDBG("%s type %d\n", __func__, power_setting->seq_type);
		CDBG("%s seq_val %d\n", __func__, power_setting->seq_val);
		switch (power_setting->seq_type) {
		case SENSOR_CLK:
			if (power_setting->seq_val >= s_ctrl->clk_info_size) {
				pr_err("%s clk index %d >= max %d\n", __func__,
					power_setting->seq_val,
					s_ctrl->clk_info_size);
				goto power_up_failed;
			}
			if (power_setting->config_val)
				s_ctrl->clk_info[power_setting->seq_val].
					clk_rate = power_setting->config_val;

			rc = msm_cam_clk_enable(s_ctrl->dev,
				&s_ctrl->clk_info[0],
				(struct clk **)&power_setting->data[0],
				s_ctrl->clk_info_size,
				1);
			CDBG("%s powerup clock num %d", __func__, s_ctrl->clk_info_size);
			if (rc < 0) {
				pr_err("%s: clk enable failed\n",
					__func__);
				goto power_up_failed;
			}

#ifdef CONFIG_HUAWEI_KERNEL_CAMERA
			/*store data[0] for the use of power down*/
			if(s_ctrl->power_down_setting_array.power_setting){
				int32_t i = 0;
				int32_t j = 0;
				struct msm_sensor_power_setting *power_down_setting = NULL;
				for(i=0; i<power_down_setting_array->size; i++){
					power_down_setting = &power_down_setting_array->power_setting[i];
					if(power_setting->seq_val == power_down_setting->seq_val){
						for(j=0; j<s_ctrl->clk_info_size; j++)
						{
							power_down_setting->data[j] = power_setting->data[j];
							CDBG("%s clkptr %p \n", __func__,power_setting->data[j]);
						}
					}
				}
			}
#endif

			break;
		case SENSOR_GPIO:
			if (power_setting->seq_val >= SENSOR_GPIO_MAX ||
				!data->gpio_conf->gpio_num_info) {
				pr_err("%s gpio index %d >= max %d\n", __func__,
					power_setting->seq_val,
					SENSOR_GPIO_MAX);
				goto power_up_failed;
			}
			pr_debug("%s:%d gpio set val %d\n", __func__, __LINE__,
				data->gpio_conf->gpio_num_info->gpio_num
				[power_setting->seq_val]);
			gpio_set_value_cansleep(
				data->gpio_conf->gpio_num_info->gpio_num
				[power_setting->seq_val],
				power_setting->config_val);		
			break;
		case SENSOR_VREG:
			if (power_setting->seq_val >= CAM_VREG_MAX) {
				pr_err("%s vreg index %d >= max %d\n", __func__,
					power_setting->seq_val,
					SENSOR_GPIO_MAX);
				goto power_up_failed;
			}

			msm_camera_config_single_vreg(s_ctrl->dev,
				&data->cam_vreg[power_setting->seq_val],
				(struct regulator **)&power_setting->data[0],
				1);

#ifdef CONFIG_HUAWEI_KERNEL_CAMERA
			/*store data[0] for the use of power down*/
			if(s_ctrl->power_down_setting_array.power_setting){
				int32_t i = 0;
				struct msm_sensor_power_setting *power_down_setting = NULL;
				for(i=0;i<power_down_setting_array->size; i++){
					power_down_setting = &power_down_setting_array->power_setting[i];
					if(power_setting->seq_val == power_down_setting->seq_val){
						power_down_setting->data[0] = power_setting->data[0];
					}
				}
			}
#endif

			break;
		case SENSOR_I2C_MUX:
			if (data->i2c_conf && data->i2c_conf->use_i2c_mux)
				ov2685_sensor_enable_i2c_mux(data->i2c_conf);
			break;
		default:
			pr_err("%s error power seq type %d\n", __func__,
				power_setting->seq_type);
			break;
		}
		if (power_setting->delay > 20) {
			msleep(power_setting->delay);
		} else if (power_setting->delay) {
			usleep_range(power_setting->delay * 1000,
				(power_setting->delay * 1000) + 1000);
		}
	}

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_INIT);
		if (rc < 0) {
			pr_err("%s cci_init failed\n", __func__);
			goto power_up_failed;
		}
	}

	if (s_ctrl->func_tbl->sensor_match_id)
		rc = s_ctrl->func_tbl->sensor_match_id(s_ctrl);
	else
		rc = msm_sensor_match_id(s_ctrl);
	if (rc < 0) {
		pr_err("%s:%d match id failed rc %d\n", __func__, __LINE__, rc);
		goto power_up_failed;
	}

	CDBG("%s exit\n", __func__);
	return 0;
power_up_failed:
	CDBG("%s:%d failed\n", __func__, __LINE__);
	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_RELEASE);
	}

	if(s_ctrl->power_down_setting_array.power_setting){
		power_setting_array = &s_ctrl->power_down_setting_array;
		index = power_setting_array->size;
		CDBG("power down use the power down settings array\n");
	}
	for (index = 0; index < power_setting_array->size; index++){
		CDBG("%s index %d\n", __func__, index);
		power_setting = &power_setting_array->power_setting[index];
		CDBG("%s type %d\n", __func__, power_setting->seq_type);
		switch (power_setting->seq_type) {
		case SENSOR_CLK:
			msm_cam_clk_enable(s_ctrl->dev,
				&s_ctrl->clk_info[0],
				(struct clk **)&power_setting->data[0],
				s_ctrl->clk_info_size,
				0);
			CDBG("%s powerdown clock num %d\n", __func__, s_ctrl->clk_info_size);
			break;
		case SENSOR_GPIO:
			gpio_set_value_cansleep(
				data->gpio_conf->gpio_num_info->gpio_num
				[power_setting->seq_val], GPIOF_OUT_INIT_LOW);
			break;
		case SENSOR_VREG:
			msm_camera_config_single_vreg(s_ctrl->dev,
				&data->cam_vreg[power_setting->seq_val],
				(struct regulator **)&power_setting->data[0],
				0);
			break;
		case SENSOR_I2C_MUX:
			if (data->i2c_conf && data->i2c_conf->use_i2c_mux)
				ov2685_sensor_disable_i2c_mux(data->i2c_conf);
			break;
		default:
			pr_err("%s error power seq type %d\n", __func__,
				power_setting->seq_type);
			break;
		}
		if (power_setting->delay > 20) {
			msleep(power_setting->delay);
		} else if (power_setting->delay) {
			usleep_range(power_setting->delay * 1000,
				(power_setting->delay * 1000) + 1000);
		}
	}
	msm_camera_request_gpio_table(
		data->gpio_conf->cam_gpio_req_tbl,
		data->gpio_conf->cam_gpio_req_tbl_size, 0);
	return rc;
}

int32_t ov2685_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t index = 0;
	struct msm_sensor_power_setting_array *power_setting_array = NULL;
	struct msm_sensor_power_setting *power_setting = NULL;
	struct msm_camera_sensor_board_info *data = s_ctrl->sensordata;
	s_ctrl->stop_setting_valid = 0;
	power_setting_array = &s_ctrl->power_down_setting_array;
	
	CDBG("%s:%d enter\n", __func__, __LINE__);
	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_RELEASE);
	}

	for (index = 0; index < power_setting_array->size; index++) {
		CDBG("%s index %d\n", __func__, index);
		power_setting = &power_setting_array->power_setting[index];
		CDBG("%s type %d\n", __func__, power_setting->seq_type);
		switch (power_setting->seq_type) {
		case SENSOR_CLK:
			msm_cam_clk_enable(s_ctrl->dev,
				&s_ctrl->clk_info[0],
				(struct clk **)&power_setting->data[0],
				s_ctrl->clk_info_size,
				0);
			CDBG("%s powerdown clock num %d", __func__, s_ctrl->clk_info_size);
			break;
		case SENSOR_GPIO:
			if (power_setting->seq_val >= SENSOR_GPIO_MAX ||
				!data->gpio_conf->gpio_num_info) {
				pr_err("%s gpio index %d >= max %d\n", __func__,
					power_setting->seq_val,
					SENSOR_GPIO_MAX);
				continue;
			}
			gpio_set_value_cansleep(
				data->gpio_conf->gpio_num_info->gpio_num
				[power_setting->seq_val], power_setting->config_val);
			break;
		case SENSOR_VREG:
			if (power_setting->seq_val >= CAM_VREG_MAX) {
				pr_err("%s vreg index %d >= max %d\n", __func__,
					power_setting->seq_val,
					SENSOR_GPIO_MAX);
				continue;
			}
			msm_camera_config_single_vreg(s_ctrl->dev,
				&data->cam_vreg[power_setting->seq_val],
				(struct regulator **)&power_setting->data[0],
				0);
			break;
		case SENSOR_I2C_MUX:
			if (data->i2c_conf && data->i2c_conf->use_i2c_mux)
				ov2685_sensor_disable_i2c_mux(data->i2c_conf);
			break;
		default:
			pr_err("%s error power seq type %d\n", __func__,
				power_setting->seq_type);
			break;
		}
		if (power_setting->delay > 20) {
			msleep(power_setting->delay);
		} else if (power_setting->delay) {
			usleep_range(power_setting->delay * 1000,
				(power_setting->delay * 1000) + 1000);
		}
	}
	msm_camera_request_gpio_table(
		data->gpio_conf->cam_gpio_req_tbl,
		data->gpio_conf->cam_gpio_req_tbl_size, 0);
	CDBG("%s exit\n", __func__);

	return 0;
}

static int32_t ov2685_platform_probe(struct platform_device *pdev)
{
	int32_t rc;
	const struct of_device_id *match;
	match = of_match_device(ov2685_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init ov2685_init_module(void)
{
	int32_t rc;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&ov2685_platform_driver,
		ov2685_platform_probe);
	if (!rc)
		return rc;
	return i2c_add_driver(&ov2685_i2c_driver);
}

static void __exit ov2685_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (ov2685_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ov2685_s_ctrl);
		platform_driver_unregister(&ov2685_platform_driver);
	} else
		i2c_del_driver(&ov2685_i2c_driver);
	return;
}

static int32_t ov2685_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;

	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
		s_ctrl->sensor_i2c_client,
		s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
		&chipid, MSM_CAMERA_I2C_BYTE_DATA);
	    CDBG("%s read id: %x expected id %x:\n", __func__, chipid,
		s_ctrl->sensordata->slave_info->sensor_id);
	
	if (rc < 0) {
		pr_err("%s: %s:ov2685 read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	CDBG("%s: read id: %x expected id %x:\n", __func__, chipid,
		s_ctrl->sensordata->slave_info->sensor_id);
	if (chipid != s_ctrl->sensordata->slave_info->sensor_id) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}

static void ov2685_set_effect(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	CDBG("%s %d\n", __func__, value);
	switch (value) {
	case MSM_CAMERA_EFFECT_MODE_OFF: {
		ov2685_i2c_write_table(s_ctrl, &OV2685_reg_effect_normal[0],
			ARRAY_SIZE(OV2685_reg_effect_normal));
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_MONO: {
		ov2685_i2c_write_table(s_ctrl, &OV2685_reg_effect_black_white[0],
			ARRAY_SIZE(OV2685_reg_effect_black_white));
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_NEGATIVE: {
		ov2685_i2c_write_table(s_ctrl, &OV2685_reg_effect_negative[0],
			ARRAY_SIZE(OV2685_reg_effect_negative));
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_SEPIA: {
		ov2685_i2c_write_table(s_ctrl, &OV2685_reg_effect_old_movie[0],
			ARRAY_SIZE(OV2685_reg_effect_old_movie));
		break;
	} 
	case MSM_CAMERA_EFFECT_MODE_AQUA: {
		ov2685_i2c_write_table(s_ctrl, &OV2685_reg_effect_aqua[0],
			ARRAY_SIZE(OV2685_reg_effect_aqua));
		break;
	}
	default:
		ov2685_i2c_write_table(s_ctrl, &OV2685_reg_effect_normal[0],
			ARRAY_SIZE(OV2685_reg_effect_normal));
	}
}

static void ov2685_set_antibanding(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	CDBG("%s %d\n", __func__, value);
	ov2685_i2c_write_table(s_ctrl, &OV2685_reg_antibanding[value][0],
		ARRAY_SIZE(OV2685_reg_antibanding[value]));
}

static void ov2685_set_white_balance_mode(struct msm_sensor_ctrl_t *s_ctrl,
	int value)
{
	CDBG("%s %d\n", __func__, value);
	switch (value) {
	case MSM_CAMERA_WB_MODE_AUTO: {
		ov2685_i2c_write_table(s_ctrl, &OV2685_reg_wb_auto[0],
			ARRAY_SIZE(OV2685_reg_wb_auto));
		break;
	}
	case MSM_CAMERA_WB_MODE_INCANDESCENT: {
		ov2685_i2c_write_table(s_ctrl, &OV2685_reg_wb_home[0],
			ARRAY_SIZE(OV2685_reg_wb_home));
		break;
	}
	case MSM_CAMERA_WB_MODE_DAYLIGHT: {
		ov2685_i2c_write_table(s_ctrl, &OV2685_reg_wb_sunny[0],
			ARRAY_SIZE(OV2685_reg_wb_sunny));
		break;
	}
	case MSM_CAMERA_WB_MODE_FLUORESCENT: {
		ov2685_i2c_write_table(s_ctrl, &OV2685_reg_wb_office[0],
			ARRAY_SIZE(OV2685_reg_wb_office));
		break;
	}
	case MSM_CAMERA_WB_MODE_CLOUDY_DAYLIGHT: {
		ov2685_i2c_write_table(s_ctrl, &OV2685_reg_wb_cloudy[0],
			ARRAY_SIZE(OV2685_reg_wb_cloudy));
		break;
	}
	default:
		ov2685_i2c_write_table(s_ctrl, &OV2685_reg_wb_auto[0],
		ARRAY_SIZE(OV2685_reg_wb_auto));
	}
}

int32_t ov2685_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	long rc = 0;
	int32_t i = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++)
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);
		break;
	case CFG_SET_INIT_SETTING:
		CDBG("init setting\n");
		ov2685_i2c_write_table(s_ctrl,
				&ov2685_recommend_settings0[0],
				ARRAY_SIZE(ov2685_recommend_settings0));
        mdelay(5);
		ov2685_i2c_write_table(s_ctrl,
				&ov2685_recommend_settings[0],
				ARRAY_SIZE(ov2685_recommend_settings));
		CDBG("init setting X\n");
		break;
	case CFG_SET_RESOLUTION: {
		int val = 0;
		CDBG("svga/xvga setting\n");
		if (copy_from_user(&val,
			(void *)cdata->cfg.setting, sizeof(int))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		/* capture */
		if (val == 0)
		{
			OV2685Capture(s_ctrl);
		}
		else if (val == 1)
		{
			ov2685_i2c_write_table(s_ctrl, &ov2685_svga_settings[0],
			ARRAY_SIZE(ov2685_svga_settings));
			OV2685_set_bandingfilter(s_ctrl);
			mdelay(150);
		}
		else
		{
			CDBG("svga/xvga setting Error. %d X\n",val);
		}
		
		CDBG("svga/xvga setting %d X\n",val);
		break;
	}
	case CFG_SET_STOP_STREAM:
		CDBG("stop stream\n");
		ov2685_i2c_write_table(s_ctrl,
			&ov2685_stop_settings[0],
			ARRAY_SIZE(ov2685_stop_settings));
		CDBG("stop stream X\n");
		break;
	case CFG_SET_START_STREAM:
		CDBG("start stream\n");
		ov2685_i2c_write_table(s_ctrl,
			&ov2685_start_settings[0],
			ARRAY_SIZE(ov2685_start_settings));
		CDBG("start stream x\n");
		break;
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params =
			*s_ctrl->sensordata->sensor_init_params;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
	case CFG_SET_SLAVE_INFO: {
		struct msm_camera_sensor_slave_info sensor_slave_info;
		struct msm_sensor_power_setting_array *power_setting_array;
		int slave_index = 0;
		if (copy_from_user(&sensor_slave_info,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_sensor_slave_info))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		/* Update sensor slave address */
		if (sensor_slave_info.slave_addr) {
			s_ctrl->sensor_i2c_client->cci_client->sid =
				sensor_slave_info.slave_addr >> 1;
		}

		/* Update sensor address type */
		s_ctrl->sensor_i2c_client->addr_type =
			sensor_slave_info.addr_type;

		/* Update power up / down sequence */
		s_ctrl->power_setting_array =
			sensor_slave_info.power_setting_array;
		power_setting_array = &s_ctrl->power_setting_array;
		power_setting_array->power_setting = kzalloc(
			power_setting_array->size *
			sizeof(struct msm_sensor_power_setting), GFP_KERNEL);
		if (!power_setting_array->power_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(power_setting_array->power_setting,
			(void *)
			sensor_slave_info.power_setting_array.power_setting,
			power_setting_array->size *
			sizeof(struct msm_sensor_power_setting))) {
			kfree(power_setting_array->power_setting);
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		s_ctrl->free_power_setting = true;
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.slave_addr);
		CDBG("%s sensor addr type %d\n", __func__,
			sensor_slave_info.addr_type);
		CDBG("%s sensor reg %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id_reg_addr);
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id);
		for (slave_index = 0; slave_index <
			power_setting_array->size; slave_index++) {
			CDBG("%s i %d power setting %d %d %ld %d\n", __func__,
			slave_index,
			power_setting_array->power_setting[slave_index].
			seq_type,
			power_setting_array->power_setting[slave_index].
			seq_val,
			power_setting_array->power_setting[slave_index].
			config_val,
			power_setting_array->power_setting[slave_index].
			delay);
		}
		kfree(power_setting_array->power_setting);
		break;
	}
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &conf_array);
		kfree(reg_setting);
		break;
	}
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_seq_reg_array)),
			GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_seq_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_seq_table(s_ctrl->sensor_i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}

	case CFG_POWER_UP:
		if (s_ctrl->func_tbl->sensor_power_up)
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		if (s_ctrl->func_tbl->sensor_power_down)
			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		if (copy_from_user(stop_setting, (void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = stop_setting->reg_setting;
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
			(void *)reg_setting, stop_setting->size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(stop_setting->reg_setting);
			stop_setting->reg_setting = NULL;
			stop_setting->size = 0;
			rc = -EFAULT;
			break;
		}
		break;
	} 
	case CFG_SET_EFFECT: {
		int32_t effect_mode;
		if (copy_from_user(&effect_mode, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s: Effect mode is %d\n", __func__, effect_mode);
		CDBG("Set effect\n");
		ov2685_set_effect(s_ctrl, effect_mode);
		CDBG("Set effect X\n");
		break;
	}
	case CFG_SET_ANTIBANDING: {
		int32_t antibanding_mode;
		if (copy_from_user(&antibanding_mode,
			(void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s: anti-banding mode is %d\n", __func__,
			antibanding_mode);
		CDBG("Set antibanding\n");
		ov2685_set_antibanding(s_ctrl, antibanding_mode);
		CDBG("Set antibanding X\n");
		break;
	} 
	case CFG_SET_WHITE_BALANCE: {
		int32_t wb_mode;
		if (copy_from_user(&wb_mode, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s: white balance is %d\n", __func__, wb_mode);
		CDBG("Set wb\n");
		ov2685_set_white_balance_mode(s_ctrl, wb_mode);
		CDBG("Set wb X\n");
		break;
	}
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

/****************************************************************************
* FunctionName: ov2685_add_project_name;
* Description : add the project name and app_info display;
***************************************************************************/
static int ov2685_add_project_name(struct msm_sensor_ctrl_t *s_ctrl)
{
	/*Todo: check the module before cp project name, when we use two sensors with the same IC*/

	/*add project name for the project menu*/
	strncpy(s_ctrl->sensordata->sensor_info->sensor_project_name, "23060151FF-OV-S", strlen("23060151FF-OV-S")+1);

	/*add the app_info*/
	app_info_set("camera_main", "ov2685_sunny");

	pr_info("ov2685_add_project_name OK\n");

	return 0;
}

static struct msm_sensor_fn_t ov2685_sensor_func_tbl = {
	.sensor_config = ov2685_sensor_config,
	.sensor_power_up = ov2685_sensor_power_up,
	.sensor_power_down = ov2685_sensor_power_down,
	.sensor_match_id = ov2685_sensor_match_id,
#ifdef CONFIG_HUAWEI_KERNEL_CAMERA
	.sensor_add_project_name = ov2685_add_project_name,
#endif
};

static struct msm_sensor_ctrl_t ov2685_s_ctrl = {
	.sensor_i2c_client = &ov2685_sensor_i2c_client,
	.power_setting_array.power_setting = ov2685_power_setting,
	.power_setting_array.size = ARRAY_SIZE(ov2685_power_setting),
	.msm_sensor_mutex = &ov2685_mut,
	.sensor_v4l2_subdev_info = ov2685_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov2685_subdev_info),
	.func_tbl = &ov2685_sensor_func_tbl,
#ifdef CONFIG_HUAWEI_KERNEL_CAMERA
	.power_down_setting_array.power_setting = ov2685_power_down_setting,
	.power_down_setting_array.size = ARRAY_SIZE(ov2685_power_down_setting),
#endif
};

module_init(ov2685_init_module);
module_exit(ov2685_exit_module);
MODULE_DESCRIPTION("Ov2685 2MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
