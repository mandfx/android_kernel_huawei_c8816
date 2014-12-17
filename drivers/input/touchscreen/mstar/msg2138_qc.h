#ifndef _MSG2138_QC_H_
#define _MSG2138_QC_H_

#define __FIRMWARE_UPDATE__
#ifdef  __FIRMWARE_UPDATE__
#define __CHIP_MSG_2133A__
#ifndef __CHIP_MSG_2133A__
#define 	__CHIP_MSG_2133__
#endif

#endif

#ifndef CTP_PROXIMITY_FUN
#define CTP_PROXIMITY_FUN	0
#endif

#define N_BYTE_PER_TIME (8)
#define UPDATE_TIMES (1024/N_BYTE_PER_TIME)

#define UPDATE_INFO_BLK 0

#define FW_ADDR_MSG21XX   (0xC4>>1)
#define FW_ADDR_MSG21XX_TP   (0x4C>>1)

#define	CTP_ID_MSG21XX		1
#define	CTP_ID_MSG21XXA		2


#define u8 unsigned char
#define U8 unsigned char
#define u32 unsigned int
#define U32 unsigned int
#define s32 signed int
#define u16 ushort
#define U16 ushort
#define REPORT_PACKET_LENGTH  8
#define MSG21XX_INT_GPIO 1
#define MSG21XX_RESET_GPIO 0
#define FLAG_GPIO    122
#define ST_KEY_SEARCH 	217
#define MSG21XX_RETRY_COUNT 3
#define MS_TS_MSG20XX_X_MIN   0
#define MS_TS_MSG20XX_Y_MIN   0

#define MS_TS_MSG21XX_X_MAX   (480)
#define MS_TS_MSG21XX_Y_MAX   (800)
#define MS_I2C_VTG_MIN_UV	1800000
#define MS_I2C_VTG_MAX_UV	1800000
#define MS_VDD_VTG_MIN_UV   2850000
#define MS_VDD_VTG_MAX_UV   2850000
#define CFG_MAX_TOUCH_POINTS 	2	

#define CTP_AUTHORITY 0777

#define TP_ERR  1
#define TP_INFO 2
#define TP_DBG  3
#define TP_VDBG  4

#define MSTARALSPS_DEVICE_NAME "MStar-alsps-tpd-dev"
#define MSTARALSPS_INPUT_NAME  "proximity"
#define MSTARALSPS_IOCTL_MAGIC 0XDF
#define MSTARALSPS_IOCTL_PROX_ON _IOW(MSTARALSPS_IOCTL_MAGIC, 0,char *)
#define MSTARALSPS_IOCTL_PROX_OFF _IOW(MSTARALSPS_IOCTL_MAGIC, 1,char *)

#define PROXIMITY_TIMER_VAL   (300)
#define GTP_KEY_TAB	 {KEY_MENU, KEY_HOME, KEY_BACK, KEY_SEARCH}//KEY_HOMEPAGE

#define MAX_TOUCH_FINGER 2

#define FW_ADDR_MSG20XX_TP   	 (0x4C>>1) //device address of msg20xx    7Bit I2C Addr == 0x26;
#define FW_ADDR_MSG20XX      	 (0xC4>>1)  
#define FW_UPDATE_ADDR_MSG20XX   (0x92>>1)
#define DWIIC_MODE_ISP    0
#define DWIIC_MODE_DBBUS  1

typedef struct
{
    u16 X;
    u16 Y;
} TouchPoint_t;

typedef struct
{
    u8 nTouchKeyMode;
    u8 nTouchKeyCode;
    u8 nFingerNum;
    TouchPoint_t Point[MAX_TOUCH_FINGER];
} TouchScreenInfo_t;


typedef enum {
    SWID_OFILM=0,  
    SWID_EELY,
    SWID_TRULY, 
    SWID_NULL,
} SWID_ENUM;


extern int mstar_debug_mask;
#ifndef tp_log_err
#define tp_log_err(x...)                \
do{                                     \
    if( mstar_debug_mask >= TP_ERR )   \
    {                                   \
        printk(KERN_ERR "[Msg2138] " x); \
    }                                   \
                                        \
}while(0)
#endif

#ifndef tp_log_info
#define tp_log_info(x...)               \
do{                                     \
    if( mstar_debug_mask >= TP_INFO )  \
    {                                   \
        printk(KERN_ERR "[Msg2138] " x); \
    }                                   \
                                        \
}while(0)
#endif

#ifndef tp_log_debug
#define tp_log_debug(x...)              \
do{                                     \
    if( mstar_debug_mask >= TP_DBG )   \
    {                                   \
        printk(KERN_ERR "[Msg2138] " x); \
    }                                   \
                                        \
}while(0)
#endif

#ifndef tp_log_vdebug
#define tp_log_vdebug(x...)              \
do{                                     \
    if( mstar_debug_mask >= TP_VDBG )   \
    {                                   \
        printk(KERN_ERR "[Msg2138] " x); \
    }                                   \
                                        \
}while(0)
#endif

void msg2138_enable_irq(void);
void msg2138_disable_irq(void);

#endif
