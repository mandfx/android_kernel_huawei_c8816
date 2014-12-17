#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/timer.h>
#include <linux/gpio.h>

#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <mach/gpio.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <linux/syscalls.h>

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/string.h>
#include <asm/unistd.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/earlysuspend.h>
#include <linux/miscdevice.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <tpd_custom_msg2133.h>
#include <linux/of_gpio.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/input/mt.h>

#include "msg2138_scap_test.h"
#include "msg2138_qc.h"

#include <misc/app_info.h>
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

#ifdef CONFIG_GET_HARDWARE_INFO
#include <mach/hardware_info.h>
static char tmp_str[50];
#endif

/* move define to msg2138_qc.h */
static u8 curr_ic_type=CTP_ID_MSG21XXA;

static int msg21xx_irq = 0;
static struct input_dev *input=NULL;
static struct i2c_client *msg21xx_i2c_client;
static struct mutex msg21xx_mutex;
static struct work_struct msg21xx_wq;

static struct i2c_client *i2c_client = NULL;

int mstar_debug_mask = TP_INFO;
module_param_named(mstar_debug_mask, mstar_debug_mask, int, 0664);

#if CTP_PROXIMITY_FUN
struct class *msg2133_class;
char msg2133_name[10];
struct cdev cdev;
static dev_t msg2133_dev_number;

static int ps_state = 1;
static int mstar2133_ps_opened=0;
static bool ps_Enable_status = false;

struct input_dev *input_dev;
struct hrtimer ps_timer;
static struct wake_lock pls_delayed_work_wake_lock;
static void  Msg21XX_proximity_enable(bool enableflag);
ktime_t proximity_poll_delay;

#endif

static const u16 touch_key_array[] = GTP_KEY_TAB;

SWID_ENUM sw_id=SWID_NULL;

static unsigned short curr_ic_major=0;
static unsigned short curr_ic_minor=0;

#ifdef __FIRMWARE_UPDATE__

#define __AUTO_UPDATE__
#ifdef	__AUTO_UPDATE__
u8 update_flag=0;
 
u16 update_bin_major=0;
u16 update_bin_minor=0;
#endif	


static  char *fw_version;

#ifdef __CHIP_MSG_2133A__
static int FwDataCnt;
typedef enum
{
	EMEM_MAIN = 0,
	EMEM_INFO,
	EMEM_ALL,
}EMEM_TYPE_t;


static u8 temp[94][1024];
static int FwDataCnt;

struct class *firmware_class;
struct device *firmware_cmd_dev;
#endif
#endif

struct msg21xx_ts_data {
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	int use_irq;
	struct hrtimer timer;
	struct work_struct  work;
	int (*power)(int on);
	int (*get_int_status)(void);
	void (*reset_ic)(void);
	unsigned irq_gpio;
    	unsigned reset_gpio;
	u32 irq_flags;
    	u32 reset_flags;
	struct regulator *vdd;
	struct regulator *vcc_i2c;
};

typedef struct
{
    u32 u32X;       /* X coordinate, 0~2047 */
    u32 u32Y;       /* Y coordinate, 0~2047 */
    u8  u8Finger;   /* Number of fingers in the touch event */
    u32 u32Dis_X;   /* X distance between two fingers */
    u32 u32Dis_Y;   /* Y distance between two fingers. This value could be negative */
} touch_data_st;

static int __devinit msg21xx_ts_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int __devexit msg21xx_ts_remove(struct i2c_client *client);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void msg21xx_ts_early_suspend(struct early_suspend *h);
static void msg21xx_ts_late_resume(struct early_suspend *h);
#endif
static int msg21xx_ts_suspend(struct i2c_client *client, pm_message_t mesg);
static int msg21xx_ts_resume(struct i2c_client *client);

static const struct i2c_device_id msg21xx_ts_id[] =
{
    { "ms-msg21xx", 0x26 },
    { }
};
#ifdef CONFIG_OF
static struct of_device_id mastr_match_table[] =
{
    { .compatible = "mstar,ms-msg21xx",},
    { },
};
#else
#define mastr_match_table NULL
#endif

MODULE_DEVICE_TABLE(i2c, msg21xx_ts_id);
static struct i2c_driver msg21xx_ts_driver =
{
    .driver = {
        .name = "ms-msg21xx",
        .of_match_table = mastr_match_table,
    },
    .probe = msg21xx_ts_probe,
    .remove = __devexit_p(msg21xx_ts_remove),
    .id_table = msg21xx_ts_id,
    .suspend = msg21xx_ts_suspend,
    .resume = msg21xx_ts_resume,
#ifdef CONFIG_HAS_EARLYSUSPEND
    .suspend = msg21xx_ts_early_suspend,
    .resume = msg21xx_ts_late_resume,
#endif
};

void HalTscrCReadI2CSeq(u8 addr, u8* read_data, u8 size)
{
    int rc;

    struct i2c_msg msgs[] =
    {
        {
            .addr = addr,
            .flags = I2C_M_RD,
            .len = size,
            .buf = read_data,
        },
    };

    rc = i2c_transfer(msg21xx_i2c_client->adapter, msgs, 1);
    if (rc < 0)
    {
        tp_log_debug("%s %d:HalTscrCReadI2CSeq error %d\n", __func__, __LINE__, rc);
    }
}

int HalTscrCDevWriteI2CSeq(u8 addr, u8* data, u16 size)
{
    int rc;

    struct i2c_msg msgs[] =
    {
        {
            .addr = addr,
            .flags = 0,
            .len = size,
            .buf = data,
        },
    };
    rc = i2c_transfer(msg21xx_i2c_client->adapter, msgs, 1);
    if (rc < 0)
    {
        tp_log_debug("%s %d:HalTscrCDevWriteI2CSeq error %d\n", __func__, __LINE__, rc);
        return -1;
    }

    return 0;
}

void msg2138_disable_irq(void)
{
	disable_irq_nosync(msg21xx_irq);
}

void msg2138_enable_irq(void)
{
	enable_irq(msg21xx_irq);
}

void dbbusDWIICEnterSerialDebugMode(void)
{
    U8 data[5];

    // Enter the Serial Debug Mode
    data[0] = 0x53;
    data[1] = 0x45;
    data[2] = 0x52;
    data[3] = 0x44;
    data[4] = 0x42;
	mdelay(15);
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG20XX, data, 5);
}

void dbbusDWIICStopMCU(void)
{
    U8 data[1];

    // Stop the MCU
    data[0] = 0x37;
	mdelay(15);
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG20XX, data, 1);
}

void dbbusDWIICIICUseBus(void)
{
    U8 data[1];

    // IIC Use Bus
    data[0] = 0x35;
	mdelay(15);
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG20XX, data, 1);
}

void dbbusDWIICIICReshape(void)
{
    U8 data[1];

    // IIC Re-shape
    data[0] = 0x71;
	mdelay(15);
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG20XX, data, 1);
}

void dbbusDWIICIICNotUseBus(void)
{
    U8 data[1];

    // IIC Not Use Bus
    data[0] = 0x34;
	mdelay(15);
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG20XX, data, 1);
}

void dbbusDWIICNotStopMCU(void)
{
    U8 data[1];

    // Not Stop the MCU
    data[0] = 0x36;
	mdelay(15);
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG20XX, data, 1);
}

void dbbusDWIICExitSerialDebugMode(void)
{
    U8 data[1];

    // Exit the Serial Debug Mode
    data[0] = 0x45;
	mdelay(15);
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG20XX, data, 1);
}

void drvISP_EntryIspMode(void)
{
    U8 bWriteData[5] =
    {
        0x4D, 0x53, 0x54, 0x41, 0x52
    };

    mdelay(15);
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG20XX, bWriteData, 5);
}

#ifdef __CHIP_MSG_2133A__

void drvDB_WriteReg(u8 bBank, u8 bAddr, u16 bData)
{
    u8 bWriteData[5];
    bWriteData[0]=0x10;
    bWriteData[1] = bBank;
    bWriteData[2] = bAddr;
    bWriteData[4] = bData>>8;
    bWriteData[3] = bData&0xFF;
    mdelay(15);	
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG20XX, bWriteData, 5);
}

void drvDB_WriteReg8Bit(u8 bBank, u8 bAddr, u8 bData)
{
	u8 bWriteData[5];
	bWriteData[0] = 0x10;
	bWriteData[1] = bBank;
	bWriteData[2] = bAddr;
	bWriteData[3] = bData;
	mdelay(15);
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG20XX, bWriteData, 4);
}

u16 drvDB_ReadReg(u8 bBank,u8 bAddr)
{
    u16 val = 0;
    u8 bWriteData[3] = {0x10, bBank, bAddr};
    u8 bReadData[2] = {0x00, 0x00};
    mdelay(15);
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG20XX, bWriteData, 3);

    mdelay(15);
    HalTscrCReadI2CSeq(FW_ADDR_MSG20XX, &bReadData[0], 2);

    val = (bReadData[1] << 8) | (bReadData[0]);

    return val;
}

u32 Reflect(u32 ref, char ch)//unsigned int Reflect(unsigned int ref, char ch)
{
	u32 value=0;
	u32 i=0;

	for(i = 1; i < (ch + 1); i++)
	{
		if(ref & 1)
			value |= 1 << (ch - i);
		ref >>= 1;
	}
	return value;
}

static void Init_CRC32_Table ( u32 *crc32_table )
{
    u32 magicnumber = 0x04c11db7;
    u32 i = 0, j;

    for ( i = 0; i <= 0xFF; i++ )
    {
        crc32_table[i] = Reflect ( i, 8 ) << 24;
        for ( j = 0; j < 8; j++ )
        {
            crc32_table[i] = ( crc32_table[i] << 1 ) ^ ( crc32_table[i] & ( 0x80000000L ) ? magicnumber : 0 );
        }
        crc32_table[i] = Reflect ( crc32_table[i], 32 );
    }
}

static u32 Get_CRC ( u32 text, u32 prevCRC, u32 *crc32_table )
{
    u32  ulCRC = prevCRC;
    ulCRC = ( ulCRC >> 8 ) ^ crc32_table[ ( ulCRC & 0xFF ) ^ text];
    return ulCRC ;
}

void _HalTscrHWReset(void)
{
    gpio_set_value(MSG21XX_RESET_GPIO, 1);

    mdelay(20);  /* Note that the RST must be in LOW 10ms at least */
    gpio_set_value(MSG21XX_RESET_GPIO, 0);
    mdelay(20);
    gpio_set_value(MSG21XX_RESET_GPIO, 1);

    /* Enable the interrupt service thread/routine for INT after 50ms */
    msleep(300);
}

static int drvTP_erase_emem_c33 ( void )
{
    tp_log_debug("%s %d.\n", __func__, __LINE__);
    // stop mcu
    drvDB_WriteReg ( 0x0F, 0xE6, 0x0001 );

    //disable watch dog
    drvDB_WriteReg8Bit ( 0x3C, 0x60, 0x55 );
    drvDB_WriteReg8Bit ( 0x3C, 0x61, 0xAA );

    // set PROGRAM password
    drvDB_WriteReg8Bit ( 0x16, 0x1A, 0xBA );
    drvDB_WriteReg8Bit ( 0x16, 0x1B, 0xAB );

    //proto.MstarWriteReg(F1.loopDevice, 0x1618, 0x80);
    drvDB_WriteReg8Bit ( 0x16, 0x18, 0x80 );

    drvDB_WriteReg8Bit ( 0x16, 0x18, 0x40 );

    drvDB_WriteReg8Bit ( 0x16, 0x18, 0x80 );

    // erase trigger
    drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x04 ); //erase main

    return ( 1 );
}

#endif
static void _GetMainFwVersion(void)
{
    u8 smbus_tx_data[3] = {0};
    u8 smbus_rx_data[4] = {0};

    smbus_tx_data[0] = 0x53;
    smbus_tx_data[1] = 0x00;
    if (curr_ic_type == CTP_ID_MSG21XXA)
    {
        smbus_tx_data[2] = 0x2A;
    }
    else if (curr_ic_type == CTP_ID_MSG21XX)
    {
        smbus_tx_data[2] = 0x74;
    }
    else
    {
        smbus_tx_data[2] = 0x2A;
    }
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX_TP, &smbus_tx_data[0], 3);
    mdelay(100);
    HalTscrCReadI2CSeq(FW_ADDR_MSG21XX_TP, &smbus_rx_data[0], 4);

    curr_ic_major = (smbus_rx_data[1] << 8) + smbus_rx_data[0];
    curr_ic_minor = (smbus_rx_data[3] << 8) + smbus_rx_data[2];

}


static u16 _GetVendorID ( EMEM_TYPE_t emem_type )
{
    u8 i;
    u16 ret = 0;
    u8  dbbus_tx_data[5] = {0};
    u8  dbbus_rx_data[4] = {0};
    u16 reg_data = 0;
    u16 addr_id = 0;

    _HalTscrHWReset();

    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 100 );

    //Stop MCU
    drvDB_WriteReg ( 0x0F, 0xE6, 0x0001 );

    // Stop Watchdog
    drvDB_WriteReg8Bit ( 0x3C, 0x60, 0x55 );
    drvDB_WriteReg8Bit ( 0x3C, 0x61, 0xAA );

    //cmd
    drvDB_WriteReg ( 0x3C, 0xE4, 0xA4AB );
    drvDB_WriteReg ( 0x1E, 0x04, 0x7d60 );

    // TP SW reset
    drvDB_WriteReg ( 0x1E, 0x04, 0x829F );

    //MCU run
    drvDB_WriteReg ( 0x0F, 0xE6, 0x0000 );

    //polling 0x3CE4
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x5B58 );

    if (emem_type == EMEM_MAIN)
    {
        addr_id = 0x7F55;
    }
    else if (emem_type == EMEM_INFO)
    {
        addr_id = 0x8300;
    }

    dbbus_tx_data[0] = 0x72;
    dbbus_tx_data[1] = (addr_id >> 8) & 0xFF;
    dbbus_tx_data[2] = addr_id & 0xFF;
    dbbus_tx_data[3] = 0x00;
    dbbus_tx_data[4] = 0x04;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP , &dbbus_tx_data[0], 5 );

    // recive info data
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX_TP, &dbbus_rx_data[0], 4 );

    for (i = 0; i < 4; i++)
    {
        tp_log_debug("%s %d:[21xxA]:Vendor id dbbus_rx_data[%d]=0x%x,%d\n",
            			__func__, __LINE__, i, dbbus_rx_data[i], (dbbus_rx_data[i] - 0x30));
    }

    if ((dbbus_rx_data[0] >= 0x30 && dbbus_rx_data[0] <= 0x39)
        && (dbbus_rx_data[1] >= 0x30 && dbbus_rx_data[1] <= 0x39)
        && (dbbus_rx_data[2] >= 0x31 && dbbus_rx_data[2] <= 0x39))
    {
        ret = (dbbus_rx_data[0] - 0x30) * 100 + (dbbus_rx_data[1] - 0x30) * 10 + (dbbus_rx_data[2] - 0x30);
    }

    if (emem_type == EMEM_MAIN)
    {
        tp_log_debug("%s %d:[21xxA]:Vendor id from main block=%d\n", __func__, __LINE__, ret);
    }
    else if (emem_type == EMEM_INFO)
    {
        tp_log_debug("%s %d:[21xxA]:Vendor id from info. block=%d\n", __func__, __LINE__, ret);
    }

    return (ret);
}

#ifdef __CHIP_MSG_2133A__

static void firmare_update_c33(void)
{
    u8 n = 0, i = 0, flag = 2; //2:main blk    1:all blk    3: info blk

    u32 j = 0;
    u32 *crc_tab = NULL;
    u32 crc_main = 0, crc_main_tp, crc_temp = 0;
    u32 crc_info = 0, crc_info_tp = 0;
    u16 reg_data = 0;

    crc_main = 0xffffffff;
    crc_info = 0xffffffff;

	crc_tab = (u32 *)devm_kzalloc(&msg21xx_i2c_client->dev, sizeof(u32) * 256, GFP_KERNEL);
    if (NULL == crc_tab)
    {
		tp_log_err("%s %d:devm_kzalloc failed, firmware update fail!\n", __func__, __LINE__);
        return;
    }
    
	_HalTscrHWReset();

    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();

    //erase main
    drvTP_erase_emem_c33 ( );
    //MSG2133A_DBG("[21xxA]:EMEM ERASE FINISH!\n");
    mdelay ( 100 );

    // Program
    _HalTscrHWReset();

    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();

    //polling 0x3CE4 is 0x1C70
    if ( ( flag == 1 ) || ( flag == 2 ) )
    {
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0x1C70 );
        tp_log_info("%s %d:EMEM UPDATE -> polling 0x3CE4 is 0x1C70\n", __func__, __LINE__);
    }

    switch ( flag )
    {
        case 1:
            drvDB_WriteReg ( 0x3C, 0xE4, 0xE38F );  // for all-blocks
            break;

        case 2:
            drvDB_WriteReg ( 0x3C, 0xE4, 0x7731 );  // for main block
            break;
        case 3:
            drvDB_WriteReg ( 0x3C, 0xE4, 0x7731 );  // for info block

            drvDB_WriteReg8Bit ( 0x0F, 0xE6, 0x01 );

            drvDB_WriteReg8Bit ( 0x3C, 0xE4, 0xC5 ); //
            drvDB_WriteReg8Bit ( 0x3C, 0xE5, 0x78 ); //

            drvDB_WriteReg8Bit ( 0x1E, 0x04, 0x9F );
            drvDB_WriteReg8Bit ( 0x1E, 0x05, 0x82 );

            drvDB_WriteReg8Bit ( 0x0F, 0xE6, 0x00 );
            mdelay ( 100 );
            break;
    }

    // polling 0x3CE4 is 0x2F43
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x2F43 );

    // calculate CRC 32
    Init_CRC32_Table ( &crc_tab[0] );

    for ( i = 0; i < 32; i++ ) // total  33 KB : main blk 32K
    {

        if ( flag == 3 )// info blk
        { 
            i = 32; 
        }
        
        if ( i < 32 )   //emem_main
        {
            if ( i == 31 )
            {
                temp[i][1014] = 0x5A; //Fmr_Loader[1014]=0x5A;
                temp[i][1015] = 0xA5; //Fmr_Loader[1015]=0xA5;

                for ( j = 0; j < 1016; j++ )
                {
                    crc_main = Get_CRC ( temp[i][j], crc_main, &crc_tab[0] );
                }

                crc_temp = crc_main;
                crc_temp = crc_temp ^ 0xffffffff;

                for (j = 0; j < 4; j++)
                {
                    temp[i][1023 - j] = (crc_temp >> 8 * j) & 0xFF;
                    tp_log_info("%s %d:Upate crc32 into bin buffer temp[%d][%d]=%x\n", 
                        			__func__, __LINE__, i, (1020 + j), temp[i][1020 + j]);
                }
            }
            else
            {
                for ( j = 0; j < 1024; j++ )
                {
                    crc_main = Get_CRC ( temp[i][j], crc_main, &crc_tab[0] );
                }
            }
        }

        for (n = 0; n < UPDATE_TIMES; n++)
        {
            HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP, temp[i] + n * N_BYTE_PER_TIME, N_BYTE_PER_TIME );
        }

        // polling 0x3CE4 is 0xD0BC
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0xD0BC );
        tp_log_info("%s %d:[21xxA]:EMEM UPDATE -> polling 0x3CE4 is 0xD0BC, CNT=%d\n", __func__, __LINE__, i);

        drvDB_WriteReg ( 0x3C, 0xE4, 0x2F43 );
    }

    if ( ( flag == 1 ) || ( flag == 2 ) )
    {
        // write file done and check crc
        drvDB_WriteReg ( 0x3C, 0xE4, 0x1380 );
    }

    if ( ( flag == 1 ) || ( flag == 2 ) )
    {
        // polling 0x3CE4 is 0x9432
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0x9432 );
        tp_log_info("%s %d:EMEM UPDATE -> polling 0x3CE4 is 0x9432\n", __func__, __LINE__);
    }

    crc_main = crc_main ^ 0xffffffff;
    crc_info = crc_info ^ 0xffffffff;

    if ( ( flag == 1 ) || ( flag == 2 ) )
    {
        // CRC Main from TP
        crc_main_tp = drvDB_ReadReg ( 0x3C, 0x80 );
        crc_main_tp = ( crc_main_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0x82 );
        tp_log_info("%s %d:crc_main=0x%x, crc_main_tp=0x%x, \n", __func__, __LINE__, crc_main, crc_main_tp);

        // CRC Info from TP
        if ( flag == 1 )
        {
            crc_info_tp = drvDB_ReadReg ( 0x3C, 0xA0 );
            crc_info_tp = ( crc_info_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0xA2 );
            tp_log_info("%s %d:crc_info=0x%x, crc_info_tp=0x%x, \n", __func__, __LINE__, crc_info, crc_info_tp);
            if ( crc_info_tp != crc_info )
            {
                tp_log_info("%s %d:update fail, info crc error! \n", __func__, __LINE__);
            }
        }
    }

    _HalTscrHWReset();
    _GetMainFwVersion();

    devm_kfree(&msg21xx_i2c_client->dev, crc_tab);
}

static ssize_t firmware_update_store( struct device *dev, struct device_attribute *attr,
                                     const char *buf, size_t size)
{
    firmare_update_c33();
    return size;
}
#endif
static ssize_t firmware_update_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", fw_version);
}

static ssize_t firmware_clear_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", fw_version);
}

static ssize_t firmware_clear_store(struct device *dev,
                                     struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}

static ssize_t firmware_version_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
    tp_log_debug("%s %d:firmware_version_show fw_version = %s\n", __func__, __LINE__, fw_version);
    return sprintf(buf, "%s\n", fw_version);
}

static ssize_t firmware_version_store(struct device *dev,
                                      struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned char dbbus_tx_data[3];
    unsigned char dbbus_rx_data[4] ;
    unsigned short major = 0, minor = 0;

    fw_version = kzalloc(sizeof(char), GFP_KERNEL);

#ifdef __CHIP_MSG_2133A__
    dbbus_tx_data[0] = 0x53;
    dbbus_tx_data[1] = 0x00;
    dbbus_tx_data[2] = 0x2A;
#else
    dbbus_tx_data[0] = 0x53;
    dbbus_tx_data[1] = 0x00;
    dbbus_tx_data[2] = 0x74;
#endif
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG20XX_TP, &dbbus_tx_data[0], 3);
    HalTscrCReadI2CSeq(FW_ADDR_MSG20XX_TP, &dbbus_rx_data[0], 4);

    major = (dbbus_rx_data[1] << 8) + dbbus_rx_data[0];
    minor = (dbbus_rx_data[3] << 8) + dbbus_rx_data[2];

    tp_log_debug("%s %d:major = %d, minor = %d\n", __func__, __LINE__, major, minor);
    sprintf(fw_version, "%03d%03d", major, minor);
    tp_log_debug("%s %d:fw_version = %s\n", __func__, __LINE__, fw_version);

    return size;
}

static ssize_t firmware_data_show(struct device *dev,
                                  struct device_attribute *attr, char *buf)
{
    return FwDataCnt;
}

static ssize_t firmware_data_store(struct device *dev,
                                   struct device_attribute *attr, const char *buf, size_t size)
{
	tp_log_debug("%s %d:FwDataCnt = %d\n", __func__, __LINE__, FwDataCnt);
    memcpy(temp[FwDataCnt], buf, 1024);
    FwDataCnt++;
    return size;
}

static DEVICE_ATTR(update, CTP_AUTHORITY, firmware_update_show, firmware_update_store);
static DEVICE_ATTR(clear, CTP_AUTHORITY, firmware_clear_show, firmware_clear_store);
static DEVICE_ATTR(version, CTP_AUTHORITY, firmware_version_show, firmware_version_store);
static DEVICE_ATTR(data, CTP_AUTHORITY, firmware_data_show, firmware_data_store);

#if CTP_PROXIMITY_FUN
static ssize_t firmware_psensor_show(struct device* dev,
                                     struct device_attribute* attr, char* buf)
{
    if (ps_Enable_status)
    { 
        return sprintf(buf, "on\n"); 
    }
    else
    { 
        return sprintf(buf, "off\n"); 
    }
}

static ssize_t firmware_psensor_store(struct device* dev,
                                      struct device_attribute* attr, const char* buf, size_t size)
{
    if (ps_Enable_status)
    {
        Msg21XX_proximity_enable(false);
    }
    else
    {
        Msg21XX_proximity_enable(true);

    }

    return size;
}

static DEVICE_ATTR(psensor, CTP_AUTHORITY, firmware_psensor_show, firmware_psensor_store);
#endif

touch_data_st touch_data;
static int msg21xx_i2c_rx_data(char *buf, int len)
{
    uint8_t i;
    struct i2c_msg msg[] =
    {
        {
            .addr	= msg21xx_i2c_client->addr,
            .flags	= I2C_M_RD,
            .len	= len,
            .buf	= buf,
        }
    };

    for (i = 0; i < MSG21XX_RETRY_COUNT; i++)
    {
        if (i2c_transfer(msg21xx_i2c_client->adapter, msg, 1) >= 0)
        {
            break;
        }
        
        mdelay(10);
    }

    if (i >= MSG21XX_RETRY_COUNT)
    {
        tp_log_debug("%s %d:retry over %d\n", __func__, __LINE__, MSG21XX_RETRY_COUNT);
        return -EIO;
    }
    
    return 0;
}

static int msg21xx_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
    int i = 0;
#if CTP_PROXIMITY_FUN
    if (mstar2133_ps_opened)
    {
        tp_log_debug("%s %d:mstar2133_ps_opened ! return\n", __func__, __LINE__);
        return 0;
    }
#endif
    disable_irq(msg21xx_irq);/*Added by liumx for tp 2013.10.29*/
    for (i = 0; i < 2; i++)
    {
        input_mt_slot(input, i);
        input_mt_report_slot_state(input, MT_TOOL_FINGER, 0);
    }
    
    input_report_key(input, BTN_TOUCH, 0);
    input_sync(input);

    gpio_direction_output(MSG21XX_RESET_GPIO, 0);
    gpio_set_value(MSG21XX_RESET_GPIO, 0);
    return 0;
}

static int msg21xx_ts_resume(struct i2c_client *client)
{
#if CTP_PROXIMITY_FUN
	if(mstar2133_ps_opened)
	{
		tp_log_debug("%s %d:mstar2133_ps_opened ! return\n", __func__, __LINE__);
		return 0;
	}
#endif
	gpio_direction_output(MSG21XX_RESET_GPIO, 1);/* Enable the interrupt service thread/routine for INT after 50ms */
	gpio_set_value(MSG21XX_RESET_GPIO, 1);
	msleep(60);
	enable_irq(msg21xx_irq);
	return 0;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void msg21xx_ts_early_suspend(struct early_suspend *h)
{
#if CTP_PROXIMITY_FUN
    if (mstar2133_ps_opened)
    {
        tp_log_debug("%s %d:mstar2133_ps_opened ! return\n", __func__, __LINE__);
        return;
    }
#endif

    tp_log_debug("%s %d\n", __FUNCTION__, __LINE__);
    disable_irq_nosync(msg21xx_irq);
    gpio_direction_output(MSG21XX_RESET_GPIO, 0);
    gpio_set_value(MSG21XX_RESET_GPIO, 0);
}

static void msg21xx_ts_late_resume(struct early_suspend *h)
{
#if CTP_PROXIMITY_FUN
    if (mstar2133_ps_opened)
    {
        gpio_direction_output(MSG21XX_RESET_GPIO, 1);
        gpio_set_value(MSG21XX_RESET_GPIO, 1);
        msleep(30);
        enable_irq(msg21xx_irq);
        Msg21XX_proximity_enable(true);
        tp_log_debug("%s %d:mstar2133_ps_opened ! return\n", __func__, __LINE__);
        return;
    }
#endif
    tp_log_debug("%s %d\n", __FUNCTION__, __LINE__);
    gpio_direction_output(MSG21XX_RESET_GPIO, 1);
    gpio_set_value(MSG21XX_RESET_GPIO, 1);
    msleep(30);
    enable_irq(msg21xx_irq);
}
#endif
u8 Calculate_8BitsChecksum( u8 *msg, s32 s32Length )
{
    s32 s32Checksum = 0;
    s32 i;

    for ( i = 0 ; i < s32Length; i++ )
    {
        s32Checksum += msg[i];
    }

    return (u8)( ( -s32Checksum ) & 0xFF );
}

#if CTP_PROXIMITY_FUN

/**
* @fn static void Msg21XX_proximity_enable(
*         bool enableflag
*         )
* @brief function description.
* @param u8 u8Enableflag [I/O]:param description.
* @return void return variable description.
*/
static void Msg21XX_proximity_enable(bool enableflag)//  1:Enable or 0:Disable  CTP Proximity function¡ê?
{
    unsigned char dbbus_tx_data[4];

    tp_log_debug("%s %d:Msg21XX_proximity_enable\n", __func__, __LINE__);
    dbbus_tx_data[0] = 0x52;
    dbbus_tx_data[1] = 0x00;
#ifndef __CHIP_MSG_2133A__
    dbbus_tx_data[2] = 0x62;
#else
    dbbus_tx_data[2] = 0x4A;
#endif
    ps_Enable_status = enableflag;

    if (enableflag)
    {dbbus_tx_data[3] = 0xA0;}
    else
    {dbbus_tx_data[3] = 0xA1;}
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG20XX_TP, &dbbus_tx_data[0], 4);
}

/**
* @fn static u8 Read_ProximityFun_State(
*         void
*         )
* @brief function description.
* @param void [I/O]:param description.
* @return u8 return variable description.
*/
#if 0 /* Commented by MarkLi //2013-Jan-11 10:21:37 */
static u8 Read_ProximityFun_State(void)//Read CTP ProximityFun  function State :Enable or Disable¡ê?
{
	unsigned char dbbus_tx_data[3];
	unsigned char dbbus_rx_data[2];
	dbbus_tx_data[0] = 0x53;
	dbbus_tx_data[1] = 0x00;
	dbbus_tx_data[2] = 0x62;
    	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG20XX_TP, &dbbus_tx_data[0], 3);
    	HalTscrCReadI2CSeq(FW_ADDR_MSG20XX_TP, &dbbus_rx_data[0], 2);
	if(dbbus_rx_data[0]==0xA0) 
	{ return 1;}
	else
	return 0;
}
#endif /* Commented by MarkLi*/

/**
* @fn static u8 Proximity_Status_Check(
*         void
*         )
* @brief function description.
* @param void [I/O]:param description.
* @return u8 return variable description.
*/
#if 0 /* Commented by MarkLi //2013-Jan-11 11:51:18 */
static u8 Proximity_Status_Check(void)
{
  u8 TempP;
  TempP=ps_state;
  //ps_state=0;//
   return TempP;
}
#endif /* Commented by MarkLi*/
/**
* @fn static void mstar2133_pls_irq_handler(
*         void
*         )
* @brief function description.
* @param void [I/O]:param description.
* @return void return variable description.
*/
static enum hrtimer_restart mstar2133_pls_timer_func(struct hrtimer* timer)
{
    tp_log_debug("%s %d:Mstar21xx_pls_irq_handler ps_state = %d\n", __func__, __LINE__, ps_state);
    input_report_abs(input_dev, ABS_DISTANCE, ps_state);
    input_sync(input_dev);
    hrtimer_forward_now(&ps_timer, proximity_poll_delay);
    return HRTIMER_RESTART;
}
/**
* @fn static int mstar2133_pls_enable(
*         void
*         )
* @brief function description.
* @param void [I/O]:param description.
* @return int return variable description.
*/
static int mstar2133_pls_enable(void)
{
    tp_log_debug("%s %d:\n", __func__, __LINE__);
    
    Msg21XX_proximity_enable(true);
    mstar2133_ps_opened = 1;
    input_report_abs(input_dev, ABS_DISTANCE, 1);
    input_sync(input_dev);
    hrtimer_start(&ps_timer, proximity_poll_delay, HRTIMER_MODE_REL);
    return 0;
}
/**
* @fn static int mstar2133_pls_disable(
*         void
*         )
* @brief function description.
* @param void [I/O]:param description.
* @return int return variable description.
*/
static int mstar2133_pls_disable(void)
{
    tp_log_debug("%s %d:\n", __func__, __LINE__);
    
    Msg21XX_proximity_enable(false);
    mstar2133_ps_opened = 0;
    hrtimer_cancel(&ps_timer);
    return 0;
}
/**
* @fn static int mstar2133_pls_open(
*         struct inode *inode,
*          struct file *file
*         )
* @brief function description.
* @param struct inode *inode [I/O]:param description.
* @param  struct file *file [I/O]:param description.
* @return int return variable description.
*/
static int mstar2133_pls_open(struct inode* inode, struct file* file)
{
    tp_log_debug("%s %d:mstar2133_pls_open()\n", __func__, __LINE__);
    if (mstar2133_ps_opened)
    { 
        return -EBUSY; 
    }
    
    return 0;
}

/**
* @fn static int mstar2133_pls_release(
*         struct inode *inode,
*          struct file *file
*         )
* @brief function description.
* @param struct inode *inode [I/O]:param description.
* @param  struct file *file [I/O]:param description.
* @return int return variable description.
*/
static int mstar2133_pls_release(struct inode* inode, struct file* file)
{
    tp_log_debug("%s %d:mstar2133_pls_release()\n", __func__, __LINE__);
    return 0;
}
/**
* @fn static long mstar2133_pls_ioctl(
*         struct inode *inode,
*          struct file *file,
*          unsigned int cmd,
*          unsigned long arg
*         )
* @brief function description.
* @param struct inode *inode [I/O]:param description.
* @param  struct file *file [I/O]:param description.
* @param  unsigned int cmd [I/O]:param description.
* @param  unsigned long arg [I/O]:param description.
* @return long return variable description.
*/
 static long mstar2133_pls_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    tp_log_debug("%s %d:cmd %d", __func__, __LINE__, _IOC_NR(cmd));
    switch (cmd)
    {
        case MSTARALSPS_IOCTL_PROX_ON:
            tp_log_debug("%s %d:MSTARALSPS_IOCTL_PROX_ON\n", __func__, __LINE__);
            wake_lock(&pls_delayed_work_wake_lock);
            mstar2133_pls_enable();
            break;
        case MSTARALSPS_IOCTL_PROX_OFF:
            tp_log_debug("%s %d:MSTARALSPS_IOCTL_PROX_OFF\n", __func__, __LINE__);
            mstar2133_pls_disable();
            wake_unlock(&pls_delayed_work_wake_lock);
            break;
        default:
            tp_log_debug("%s %d:invalid cmd %d\n", __func__, __LINE__, _IOC_NR(cmd));
            return -EINVAL;
    }
    
    return 0;
}

 static struct file_operations mstar2133_pls_fops = {
	 .owner 			 = THIS_MODULE,
	 .open				 = mstar2133_pls_open,
	 .release			 = mstar2133_pls_release,
	 //.ioctl 			 = mstar2133_pls_ioctl,
	 .unlocked_ioctl     = mstar2133_pls_ioctl,
 };
#endif


#if 1
static u32 Distance(u16 X,u16 Y,u16 preX,u16 preY)
{
    u32 temp = 0;
    temp = (((X - preX) * (X - preX)) + ((Y - preY) * (Y - preY)));
    return temp;
}

static void msg21xx_do_work(struct work_struct *work)
{

    u8 val[8] = {0};
    u8 Checksum = 0;
    u8 i;
    u32 delta_x = 0, delta_y = 0;
    u32 u32X = 0;
    u32 u32Y = 0;
    u8 touchkeycode = 0;
    static TouchScreenInfo_t* touchData = NULL;
    static u32 preKeyStatus = 0;
    u32	result = 0;
    static u8 preTouchStatus; //Previous Touch VA Status;
    u8 press[2] = {0, 0};
    static u8 prepress[2] = {0, 0};
    static u8 preTouchNum = 0;
    static u16 preX[2] = {0xffff, 0xffff}, preY[2] = {0xffff, 0xffff};
    u16 XX[2] = {0, 0}, YY[2] = {0, 0};
    u16 temp;
    u8 changepoints = 0;
#ifdef SWAP_X_Y
    u16 tempy;
    u16 	tempx;
#endif

    if (touchData == NULL)
    {
        touchData = kzalloc(sizeof(TouchScreenInfo_t), GFP_KERNEL);
    }

    memset(touchData, 0, sizeof(TouchScreenInfo_t));
    mutex_lock(&msg21xx_mutex);
    result = msg21xx_i2c_rx_data( &val[0], REPORT_PACKET_LENGTH);
    if (result < 0 )
    {
        tp_log_debug("%s %d:msg21xx_i2c_rx_data() return = %d\n", __func__, __LINE__, result);
        enable_irq(msg21xx_irq);
        mutex_unlock(&msg21xx_mutex);
        return;
    }
    
    Checksum = Calculate_8BitsChecksum(&val[0], (REPORT_PACKET_LENGTH - 1)); //calculate checksum

    tp_log_debug("%s %d:primary:val[0]=%d,val[1]=%d,val[2]=%d,val[3]=%d,val[4]=%d,val[5]=%d,val[6]=%d,val[7]=%d\n",
                 __func__, __LINE__, 
                 val[0], val[1], val[2], val[3], val[4], val[5], val[6], val[7]);
    if ((Checksum == val[7]) && (val[0] == 0x52))   //check the checksum  of packet
    {
        u32X = (((val[1] & 0xF0) << 4) | val[2]);   //parse the packet to coordinates
        u32Y = (((val[1] & 0x0F) << 8) | val[3]);

        delta_x = (((val[4] & 0xF0) << 4) | val[5]);
        delta_y = (((val[4] & 0x0F) << 8) | val[6]);

        tp_log_debug("%s %d:u32X=%d,u32Y=%d\n", __func__, __LINE__, u32X, u32Y);
        tp_log_debug("%s %d:[HAL] u32X = %x, u32Y = %x\n", __func__, __LINE__, u32X, u32Y);
        tp_log_debug("%s %d:[HAL] delta_x = %x, delta_y = %x\n", __func__, __LINE__, delta_x, delta_y);

        if ((val[1] == 0xFF) && (val[2] == 0xFF) && (val[3] == 0xFF) && (val[4] == 0xFF) && (val[6] == 0xFF))
        {
            touchData->Point[0].X = 0; // final X coordinate
            touchData->Point[0].Y = 0; // final Y coordinate

            if ((val[5] == 0x0) || (val[5] == 0xFF) || (preTouchStatus == 1)) // if((val[5]==0x0)||(val[5]==0xFF))
            {
                touchData->nFingerNum = 0; //touch end
                touchData->nTouchKeyCode = 0; //TouchKeyMode
                touchData->nTouchKeyMode = 0; //TouchKeyMode
            }
            else
            {
                touchData->nTouchKeyMode = 1; //TouchKeyMode
                touchData->nTouchKeyCode = val[5]; //TouchKeyCode
                touchData->nFingerNum = 1;
                tp_log_debug("%s %d:useful key code report touch key code = %d\n", 
                    			__func__, __LINE__, touchData->nTouchKeyCode);
            }
            
            preTouchStatus = 0;

        }
        else
        {
            touchData->nTouchKeyMode = 0; //Touch on screen...
            if ((delta_x == 0) && (delta_y == 0))
            {
                touchData->nFingerNum = 1; //one touch
                touchData->Point[0].X = (u32X * MS_TS_MSG21XX_X_MAX) / 2048;
                touchData->Point[0].Y = (u32Y * MS_TS_MSG21XX_Y_MAX ) / 2048;//1781;

                press[0] = 1;
                press[1] = 0;

                /* Calibrate if the touch panel was reversed in Y */
            }
            else
            {
                u32 x2, y2;

                touchData->nFingerNum = 2; //two touch

                /* Finger 1 */
                touchData->Point[0].X = (u32X * MS_TS_MSG21XX_X_MAX) / 2048;
                touchData->Point[0].Y = (u32Y * MS_TS_MSG21XX_Y_MAX ) / 2048;//1781;

                /* Finger 2 */
                if (delta_x > 2048)     //transform the unsigh value to sign value
                {
                    delta_x -= 4096;
                }
                if (delta_y > 2048)
                {
                    delta_y -= 4096;
                }

                x2 = (u32)(u32X + delta_x);
                y2 = (u32)(u32Y + delta_y);

                touchData->Point[1].X = (x2 * MS_TS_MSG21XX_X_MAX) / 2048;
                touchData->Point[1].Y = (y2 * MS_TS_MSG21XX_Y_MAX ) / 2048; // 1781;

                press[0] = 1;
                press[1] = 1;

                /* Calibrate if the touch panel was reversed in Y */
            }
#if 1//check swap two points by sam
            if (preTouchStatus == 1)
            {
                for (i = 0; i < 2; i++)
                {
                    XX[i] = touchData->Point[i].X;
                    YY[i] = touchData->Point[i].Y;
                }

                if (/*(touchData->nFingerNum==1)&&*/(preTouchNum == 2))
                {
                    if (Distance(XX[0], YY[0], preX[0], preY[0]) > Distance(XX[0], YY[0], preX[1], preY[1]))
                    { changepoints = 1; }
                }

                if ((touchData->nFingerNum == 2) && (preTouchNum == 1))
                {
                    if (prepress[0] == 1)
                    {
                        if (Distance(XX[0], YY[0], preX[0], preY[0]) > Distance(XX[1], YY[1], preX[0], preY[0]))
                        { changepoints = 1; }
                    }
                    else
                    {
                        if (Distance(XX[0], YY[0], preX[1], preY[1]) < Distance(XX[1], YY[1], preX[1], preY[1]))
                        { changepoints = 1; }
                    }
                }

                if ((touchData->nFingerNum == 1) && (preTouchNum == 1))
                {
                    if (press[0] != prepress[0])
                    { changepoints = 1; }
                }
                if ((touchData->nFingerNum == 2) && (preTouchNum == 2))
                {
                    //
                }

                if (changepoints == 1)
                {
                    temp = press[0];
                    press[0] = press[1];
                    press[1] = temp;

                    temp = touchData->Point[0].X;
                    touchData->Point[0].X = touchData->Point[1].X;
                    touchData->Point[1].X = temp;

                    temp = touchData->Point[0].Y;
                    touchData->Point[0].Y = touchData->Point[1].Y;
                    touchData->Point[1].Y = temp;
                }

            }

            //save current status
            for (i = 0; i < 2; i++)
            {
                prepress[i] = press[i];
                preX[i] = touchData->Point[i].X;
                preY[i] = touchData->Point[i].Y;
            }
            preTouchNum = touchData->nFingerNum;
            //end of save current status
#endif
#ifdef SWAP_X_Y
            tempy = touchData->Point[0].X;
            tempx = touchData->Point[0].Y;
            touchData->Point[0].Y = tempx;
            touchData->Point[0].X = tempy;

            tempy = touchData->Point[1].X;
            tempx = touchData->Point[1].Y;
            touchData->Point[1].Y = tempx;
            touchData->Point[1].X = tempy;
#endif

#ifdef REVERSE_X
            touchData->Point[0].X = MS_TS_MSG21XX_X_MAX - touchData->Point[0].X;
            touchData->Point[1].X = MS_TS_MSG21XX_X_MAX - touchData->Point[1].X;
#endif

#ifdef REVERSE_Y
            touchData->Point[0].Y = MS_TS_MSG21XX_Y_MAX - touchData->Point[0].Y ;
            touchData->Point[1].Y  = MS_TS_MSG21XX_Y_MAX - touchData->Point[1].Y ;
#endif

            preTouchStatus = 1;
        }
        tp_log_debug("%s %d:touchData->nTouchKeyMode[%d]\n", __func__, __LINE__, touchData->nTouchKeyMode);
        
        //report...
        if (touchData->nTouchKeyMode)
        {
            switch (touchData->nTouchKeyCode)
            {
                case 1:
                    touchkeycode = KEY_BACK;
                    break;
                case 2:
                    touchkeycode = KEY_MENU;
                    break;
                case 4:
                    touchkeycode = KEY_HOME ;//KEY_HOMEPAGE
                    break;
                //case 8:
                    //touchkeycode = KEY_SEARCH;
                    //break;
#if CTP_PROXIMITY_FUN
                case 0x40://leave.....  proximity
                    tp_log_debug("%s %d:proximity leave-\n", __func__, __LINE__);
                    ps_state = 1;
                    enable_irq(msg21xx_irq);
                    mutex_unlock(&msg21xx_mutex);
                    return;
                case 0x80://close.....
                    tp_log_debug("%s %d:proximity close-\n", __func__, __LINE__);
                    ps_state = 0;
                    enable_irq(msg21xx_irq);
                    mutex_unlock(&msg21xx_mutex);
                default:
                    tp_log_debug("%s %d:proximity default-\n", __func__, __LINE__);
                    enable_irq(msg21xx_irq);
                    mutex_unlock(&msg21xx_mutex);
                    return;

#endif
            }

            if (preKeyStatus != touchkeycode)
            {
                preKeyStatus = touchkeycode;
                tp_log_debug("%s %d:useful key code report touch key code = %d,touchkeycode=%d\n",
                    			__func__, __LINE__, touchData->nTouchKeyCode,touchkeycode);

                input_report_key(input, touchkeycode, 1);
                input_sync(input);
                preTouchStatus = 0;
            }
        }
        else
        {
            if ((touchData->nFingerNum) == 0)  //touch end
            {
                if (preKeyStatus != 0)
                { 
                    input_report_key(input, preKeyStatus, 0); 
                }
                else
                {
                    for (i = 0; i < 2; i++)
                    {
                        input_mt_slot(input, i);
                        input_report_abs(input, ABS_MT_TRACKING_ID, -1);
                    }
                }
                input_sync(input);

            }
            else //touch on screen
            {
                if (touchData->nFingerNum)
                {
                    for (i = 0; i < 2; i++)
                    {
                        input_mt_slot(input, i);
                        if (press[i])
                        {
                            tp_log_debug("%s %d:report:u32X=%d,u32Y=%d,i=%d\n",
                                			__func__, __LINE__, touchData->Point[i].X,touchData->Point[i].Y,i);
                            input_report_abs(input, ABS_MT_TRACKING_ID, i);
                            input_report_abs(input, ABS_MT_POSITION_X, touchData->Point[i].X);
                            input_report_abs(input, ABS_MT_POSITION_Y, touchData->Point[i].Y);
                        }
                        else
                        { input_report_abs(input, ABS_MT_TRACKING_ID, -1); }

                    }
                }

                input_sync(input);
            }

            preKeyStatus = 0; //clear key status..
        }
    }

    enable_irq(msg21xx_irq);
    mutex_unlock(&msg21xx_mutex);
}
#endif

static int msg21xx_ts_open(struct input_dev *dev)
{
	return 0;
}

static void msg21xx_ts_close(struct input_dev *dev)
{
	tp_log_debug("%s %d:msg21xx_ts_close.\n", __func__, __LINE__);
}


static int msg21xx_init_input(void)
{
    u8 index;
    int err;

    input = input_allocate_device();
    input->name = msg21xx_i2c_client->name;
    input->phys = "I2C";
    input->id.bustype = BUS_I2C;
    input->dev.parent = &msg21xx_i2c_client->dev;
    input->open = msg21xx_ts_open;
    input->close = msg21xx_ts_close;
    input->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;

    __set_bit(INPUT_PROP_DIRECT, input->propbit);
    input_mt_init_slots(input, CFG_MAX_TOUCH_POINTS);
    input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, 255, 0, 0);
    input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 2, 0, 0);
    input_set_abs_params(input, ABS_MT_POSITION_X, 0, MS_TS_MSG21XX_X_MAX, 0, 0);
    input_set_abs_params(input, ABS_MT_POSITION_Y, 0, MS_TS_MSG21XX_Y_MAX, 0, 0);
    for (index = 0; index < 3; index++)
    {
        input_set_capability(input, EV_KEY, touch_key_array[index]);
    }

    err = input_register_device(input);
    if (err)
    { 
   		tp_log_err("%s %d:failed to alloc input device.\n", __func__, __LINE__);
        goto fail_alloc_input; 
    }

fail_alloc_input:

    return 0;
}
static irqreturn_t msg21xx_interrupt(int irq, void *dev_id)
{
    disable_irq_nosync(msg21xx_irq);
    schedule_work(&msg21xx_wq);
    return IRQ_HANDLED;
}

static int msg21xx_parse_dt(struct device *dev,
				struct msg21xx_ts_data *msg21xx_pdata)
{
    int ret_value = 0;
    struct device_node* np = dev->of_node;
    /* reset, irq gpio info */

    ret_value = of_get_named_gpio_flags(np, "mstar,reset-gpio", 0,
                                        &msg21xx_pdata->reset_flags);
    if (ret_value >= 0)
    {
        msg21xx_pdata->reset_gpio = ret_value;
    }
    else
    {
        tp_log_err("%s %d:Get reset gpio number error.\n", __func__, __LINE__);
        return ret_value;
    }

    ret_value = of_get_named_gpio_flags(np, "mstar,irq-gpio", 0,
                                        &msg21xx_pdata->irq_flags);
    if (ret_value >= 0)
    {
        msg21xx_pdata->irq_gpio = ret_value;
    }
    else
    {
        tp_log_err("%s %d:Get irq gpio number error.\n", __func__, __LINE__);
        return ret_value;
    }

    return 0;
}

#define FW_ADDR_MSG21XX   (0xC4>>1)
#define FW_ADDR_MSG21XX_TP   (0x4C>>1)
static int Check_Device(void)
{
    int ret;
    u8 dbbus_tx_data[4];
    unsigned char dbbus_rx_data[2] = {0};
    u8 curr_ic_type;

    _HalTscrHWReset();
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 100 );

    // Disable the Watchdog
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x60;
    dbbus_tx_data[3] = 0x55;
    ret = HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG20XX, dbbus_tx_data, 4 );
    if (ret < 0)
    { 
        tp_log_err("%s %d:HalTscrCDevWriteI2CSeq() fail, ret = %d.\n", __func__, __LINE__, ret);
        return ret; 
    }
    
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x61;
    dbbus_tx_data[3] = 0xAA;
    ret = HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG20XX, dbbus_tx_data, 4 );
    if (ret < 0)
    { 
        tp_log_err("%s %d:HalTscrCDevWriteI2CSeq() fail. ret = %d.\n", __func__, __LINE__, ret);
        return ret; 
    }
    
    // Stop MCU
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x0F;
    dbbus_tx_data[2] = 0xE6;
    dbbus_tx_data[3] = 0x01;
    ret = HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG20XX, dbbus_tx_data, 4 );
    if (ret < 0)
    { 
        tp_log_err("%s %d:HalTscrCDevWriteI2CSeq() fail, ret = %d.\n", __func__, __LINE__, ret);
        return ret; 
    }

    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0xCC;

    msg21xx_i2c_client->addr = FW_ADDR_MSG20XX;
    ret = i2c_master_send(msg21xx_i2c_client, dbbus_tx_data, 3);
    if (ret <= 0)
    {
        msg21xx_i2c_client->addr = FW_ADDR_MSG20XX_TP;
        tp_log_err("%s %d:Device error %d,addr = %d\n", __func__, __LINE__, ret, FW_ADDR_MSG20XX);
        return -1;
    }

    ret = i2c_master_recv(msg21xx_i2c_client, dbbus_rx_data, 2);
    msg21xx_i2c_client->addr = FW_ADDR_MSG20XX_TP;
    if (ret <= 0)
    {
        tp_log_err("%s %d:Device error %d,addr = %d\n", __func__, __LINE__, ret, FW_ADDR_MSG20XX);
        return -1;
    }
#ifdef __FIRMWARE_UPDATE__
    if ( dbbus_rx_data[0] == 2 )
    {
        curr_ic_type = CTP_ID_MSG21XXA;
    }
    else
    {
        curr_ic_type = CTP_ID_MSG21XX;
    }
    
    tp_log_debug("%s %d:CURR_IC_TYPE = %d \n", __func__, __LINE__, curr_ic_type);
#endif
    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();

    return 1;
}

#ifdef	__AUTO_UPDATE__
static u32 _CalMainCRC32(void)
{
    u32 ret = 0;
    u16  reg_data = 0;

    _HalTscrHWReset();

    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 100 );

    //Stop MCU
    drvDB_WriteReg ( 0x0F, 0xE6, 0x0001 );

    // Stop Watchdog
    drvDB_WriteReg8Bit ( 0x3C, 0x60, 0x55 );
    drvDB_WriteReg8Bit ( 0x3C, 0x61, 0xAA );

    //cmd
    drvDB_WriteReg ( 0x3C, 0xE4, 0xDF4C );
    drvDB_WriteReg ( 0x1E, 0x04, 0x7d60 );
    // TP SW reset
    drvDB_WriteReg ( 0x1E, 0x04, 0x829F );

    //MCU run
    drvDB_WriteReg ( 0x0F, 0xE6, 0x0000 );

    //polling 0x3CE4
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x9432 );

    // Cal CRC Main from TP
    ret = drvDB_ReadReg ( 0x3C, 0x80 );
    ret = ( ret << 16 ) | drvDB_ReadReg ( 0x3C, 0x82 );

    tp_log_debug("%s %d:Current main crc32=0x%x\n", __func__, __LINE__, ret);
    return (ret);
}

static u32 _ReadBinConfig ( void )
{
    u8 i;
    u32 ret = 0;
    u8  dbbus_tx_data[5] = {0};
    u8  dbbus_rx_data[4] = {0};
    u16 reg_data = 0;

    _HalTscrHWReset();

    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 100 );

    //Stop MCU
    drvDB_WriteReg ( 0x0F, 0xE6, 0x0001 );

    // Stop Watchdog
    drvDB_WriteReg8Bit ( 0x3C, 0x60, 0x55 );
    drvDB_WriteReg8Bit ( 0x3C, 0x61, 0xAA );

    //cmd
    drvDB_WriteReg ( 0x3C, 0xE4, 0xA4AB );
    drvDB_WriteReg ( 0x1E, 0x04, 0x7d60 );

    // TP SW reset
    drvDB_WriteReg ( 0x1E, 0x04, 0x829F );

    //MCU run
    drvDB_WriteReg ( 0x0F, 0xE6, 0x0000 );

    //polling 0x3CE4
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x5B58 );

    dbbus_tx_data[0] = 0x72;
    dbbus_tx_data[1] = 0x7F;
    dbbus_tx_data[2] = 0xFC;
    dbbus_tx_data[3] = 0x00;
    dbbus_tx_data[4] = 0x04;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP , &dbbus_tx_data[0], 5 );

    // recive info data
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX_TP, &dbbus_rx_data[0], 4 );
    for (i = 0; i < 4; i++)
    {
        tp_log_debug("%s %d:Bin Config dbbus_rx_data[%d]=%x\n", 
            		__func__, __LINE__, i, dbbus_rx_data[i]);
    }

    ret = dbbus_rx_data[0];
    ret = (ret << 8) | dbbus_rx_data[1];
    ret = (ret << 8) | dbbus_rx_data[2];
    ret = (ret << 8) | dbbus_rx_data[3];

    tp_log_info("%s %d:crc32 from bin config=%x\n", __func__, __LINE__, ret);
    return (ret);
}

static u8 _CheckFwIntegrity(void)
{
    u8 ret = 0;
    u32 cal_crc32, bin_conf_crc32;

    cal_crc32 = _CalMainCRC32();
    bin_conf_crc32 = _ReadBinConfig();

    if ((cal_crc32 == bin_conf_crc32) && (curr_ic_major + curr_ic_minor) > 0)
    {
        ret = 1;
    }

    return (ret);
}
static void _LoadData(SWID_ENUM id)
{
    int i = 0;
    if (id == SWID_TRULY) //truly
    {
        for (i = 0; i < 33; i++)
        {
            firmware_data_store(NULL, NULL, &(MSG21XX_truly_update_bin[i * 1024]), 0);
        }
    }
    else if (id == SWID_OFILM) //ofilm
    {
        for (i = 0; i < 33; i++)
        {
            firmware_data_store(NULL, NULL, &(MSG21XX_ofilm_update_bin[i * 1024]), 0);
        }
    }
    update_flag = 1;
}

static void _CheckBinVersion(SWID_ENUM id)
{
    update_bin_major = 0;
    update_bin_minor = 0;

    if (sw_id == SWID_TRULY) //truly
    {
        update_bin_major = MSG21XX_truly_update_bin[0x7f4f] << 8 | MSG21XX_truly_update_bin[0x7f4e];
        update_bin_minor = MSG21XX_truly_update_bin[0x7f51] << 8 | MSG21XX_truly_update_bin[0x7f50];
    }
    else if (sw_id == SWID_OFILM)
    {
        //MSG21XX_ofilm_update_bin
        update_bin_major = MSG21XX_ofilm_update_bin[0x7f4f] << 8 | MSG21XX_ofilm_update_bin[0x7f4e];
        update_bin_minor = MSG21XX_ofilm_update_bin[0x7f51] << 8 | MSG21XX_ofilm_update_bin[0x7f50];
    }
    else
    {
        //NO Vendor ID
        sw_id = SWID_NULL;
        update_flag = 0;
        update_bin_major = 0;
        update_bin_minor = 0;
        tp_log_info("%s %d:MAIN VENDOR ID ERROR!!\n", __func__, __LINE__);
    }

}

static void _CheckFWVersionAndloadData(void)
{
    //get vendor id from main block.
    sw_id = _GetVendorID(EMEM_MAIN);

    _CheckBinVersion(sw_id);

    tp_log_info("%s %d:sw_id=%d, update_bin_major=%d, update_bin_minor=%d, curr_ic_major=%d, curr_ic_minor=%d\n",
                __func__, __LINE__, sw_id, update_bin_major, update_bin_minor, curr_ic_major, curr_ic_minor);

    //check upgrading
    if (update_bin_minor > curr_ic_minor && sw_id < SWID_NULL )
    {
        tp_log_info("%s %d:TP FW WILL UPGRADE TO V%x.%x\n", __func__, __LINE__,
                    update_bin_major, update_bin_minor);
        _LoadData(sw_id);
    }
    else  //normal boot up process
    {
        update_flag = 0;
        tp_log_info("%s %d:fw will not load: bin_minor = %d, curr_ic_minor = %d, sw_id = %d.\n",
                    __func__, __LINE__, update_bin_minor, curr_ic_minor, sw_id);
    }
}

static void _CheckSWIDAndLoadData(void)
{
    //get vendor id from info. block
    sw_id = _GetVendorID(EMEM_INFO);

    tp_log_debug("%s %d:SW_ID=%d\n", __func__, __LINE__, sw_id);

    //transfer data
    _LoadData(sw_id);
}

static void _Update_FW(void)
{
   firmware_update_store(NULL, NULL, NULL, 0);    
   mdelay(60);
   _GetMainFwVersion();
}
#endif

#ifdef __FIRMWARE_UPDATE__
static void _msg_create_file_for_fwUpdate(void)
{
    firmware_class = class_create(THIS_MODULE, "ms-touchscreen-msg20xx");
    if (IS_ERR(firmware_class))
    {
        tp_log_err("%s %d:Failed to create class(firmware)!\n", __func__, __LINE__);
    }
     
    firmware_cmd_dev = device_create(firmware_class, NULL, 0, NULL, "device");
    if (IS_ERR(firmware_cmd_dev))
    {
        tp_log_err("%s %d:Failed to create device(firmware_cmd_dev)!\n", __func__, __LINE__);
    }
    
    // version
    if (device_create_file(firmware_cmd_dev, &dev_attr_version) < 0)
    {
        tp_log_err("%s %d:Failed to create device file(%s)!\n", __func__, __LINE__,
            		dev_attr_version.attr.name);
	}
    
    // update
    if (device_create_file(firmware_cmd_dev, &dev_attr_update) < 0)
    {
        tp_log_err("%s %d:Failed to create device file(%s)!\n", __func__, __LINE__,
            		dev_attr_update.attr.name);
	}
    // data
    if (device_create_file(firmware_cmd_dev, &dev_attr_data) < 0)
    {
        tp_log_err("%s %d:Failed to create device file(%s)!\n", __func__, __LINE__,
            		dev_attr_data.attr.name);
	}

    if (device_create_file(firmware_cmd_dev, &dev_attr_clear) < 0)
    {
        tp_log_err("%s %d:Failed to create device file(%s)!\n", __func__, __LINE__, 
            		dev_attr_clear.attr.name);
	}

    // Psensor
#if CTP_PROXIMITY_FUN
    if (device_create_file(firmware_cmd_dev, &dev_attr_psensor) < 0)
    {
        tp_log_err("%s %d:Failed to create device file(%s)!\n", __func__, __LINE__, 
            		dev_attr_psensor.attr.name);
	}
#endif
	
    dev_set_drvdata(firmware_cmd_dev, NULL);
}
#endif

#if CTP_PROXIMITY_FUN
static void InitialProximityFunction(void)
{
    int ret = 0;
    if((ret = (alloc_chrdev_region(&msg2133_dev_number,0,3,MSTARALSPS_INPUT_NAME)))<0)
    {
        tp_log_err("%s %d:alloc_chrdev_region()failed in ltr558_init().\n", __func__, __LINE__);
        return(ret);
    }

    cdev_init(&cdev,&mstar2133_pls_fops);
    strcpy(msg2133_name,MSTARALSPS_INPUT_NAME);
    cdev.owner = THIS_MODULE;
    if((ret = (cdev_add(&cdev,msg2133_dev_number,1)))<0)
    {
        tp_log_err("%s %d:cdev_add fail in msg2133_init().\n", __func__, __LINE__);
        return(ret);
    }
    
    msg2133_class = class_create(THIS_MODULE,MSTARALSPS_INPUT_NAME);
    device_create(msg2133_class,NULL,MKDEV(MAJOR(msg2133_dev_number),0),&msg21xx_ts_driver,"proximity");
    input_dev = input_allocate_device();
    if (!input_dev) 
    {
        input_free_device(input_dev);
        tp_log_err("%s %d:input allocate device failed\n", __func__, __LINE__);
    }
    else
    {
        input_dev->name = MSTARALSPS_INPUT_NAME;
        input_dev->phys  = MSTARALSPS_INPUT_NAME;
        input_dev->id.bustype = BUS_I2C;
        input_dev->id.vendor = 0x0001;
        input_dev->id.product = 0x0001;
        input_dev->id.version = 0x0010;
        __set_bit(EV_ABS, input_dev->evbit);    
        input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);
        input_set_abs_params(input_dev, ABS_MISC, 0, 100001, 0, 0);
        ret = input_register_device(input_dev);
        if (ret < 0)
        {
            tp_log_err("%s %d:input device regist failed err = %d \n", __func__, __LINE__, ret);
        }

        wake_lock_init(&pls_delayed_work_wake_lock, WAKE_LOCK_SUSPEND, "msg2133-wake-lock");    //add by zg
    
        /* proximity hrtimer settings.  we poll for light values using a timer. */      
        hrtimer_init(&ps_timer,CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        proximity_poll_delay=ns_to_ktime(PROXIMITY_TIMER_VAL * NSEC_PER_MSEC);
        ps_timer.function = mstar2133_pls_timer_func;

    }

}
#endif
static void msg2138_get_module_name(SWID_ENUM module_id, char *module_name, int name_length)
{
	if (NULL == module_name)
    {
		tp_log_err("%s %d:Null parameter!", __func__, __LINE__);
        return;
    }

    switch(module_id)
    {
		case SWID_TRULY:
            strncpy(module_name, "Truly", name_length - 1);
            break;
        case SWID_OFILM:
            strncpy(module_name, "Ofilm", name_length - 1);
            break;
        default:
            strncpy(module_name, "Unknow", name_length - 1);
    }
}

static int __devinit msg21xx_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
    char module_name[APP_INFO_VALUE_LENTH] = {0};
    char touch_info[APP_INFO_VALUE_LENTH] = {0};
    struct msg21xx_ts_data *msg;
	msg21xx_i2c_client = client;
	
    i2c_client = client;

	if (!i2c_check_functionality(msg21xx_i2c_client->adapter, I2C_FUNC_I2C)) 
    {
        tp_log_err("%s %d:I2C check functionality failed.", __func__, __LINE__);
        return -ENODEV;
    }
    
	tp_log_info("%s %d:Probe start.\n", __func__, __LINE__);
	if (msg21xx_i2c_client->dev.of_node) 
    {
		msg = devm_kzalloc(&msg21xx_i2c_client->dev, sizeof(*msg), GFP_KERNEL);
		if (!msg) 
        {
			tp_log_err("%s %d:Failed to allocate memory.\n", __func__, __LINE__);
			return -ENOMEM;
		}
        
		memset(msg, 0, sizeof(*msg));
		ret = msg21xx_parse_dt(&msg21xx_i2c_client->dev, msg);
		if (ret)
		{
            tp_log_err("%s %d:msg21xx_parse_dt failed.\n", __func__, __LINE__);
			return ret;
		}
        
	} 
    else 
    {
        tp_log_err("%s %d:Can not find Device Tree node error.\n", __func__, __LINE__);
        return -ENODEV;
    }

	if (gpio_is_valid(msg->irq_gpio)) 
	{
		/* configure touchscreen irq gpio */
		ret = gpio_request(msg->irq_gpio, "msg21xx_irq_gpio");
		if (ret) 
		{
			tp_log_err("%s %d:unable to request gpio [%d].\n", __func__, __LINE__,
                         msg->irq_gpio);
			goto exit_irq_gpio;
		}
	}
    
	if (gpio_is_valid(msg->reset_gpio)) 
	{
		/* configure touchscreen reset out gpio */
		ret = gpio_request(msg->reset_gpio,"msg21xx_reset_gpio");
		if (ret) 
		{
			tp_log_err("%s %d:unable to request gpio [%d].\n", __func__, __LINE__,
                        msg->reset_gpio);
			goto exit_reset_gpio;
		}

        
        ret = gpio_tlmm_config(GPIO_CFG(msg->reset_gpio,0,
                    GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	}

	msg21xx_irq = gpio_to_irq(msg->irq_gpio);
	msg->vdd = regulator_get(&msg21xx_i2c_client->dev, "vdd");
	if (IS_ERR(msg->vdd)) 
	{
		tp_log_err("%s %d:regulator_get vdd fail.\n", __func__, __LINE__);
        ret = -ENODEV;
        goto exit_regulator_get;
	}
    
	ret=regulator_set_voltage(msg->vdd, MS_VDD_VTG_MIN_UV, MS_VDD_VTG_MAX_UV);
	if(ret)
    {
    	tp_log_err("%s %d:regulator_set_voltage vdd fail.\n", __func__, __LINE__); 
        ret = -ENODEV;
        goto exit_regulator_get;
	}

	msg->vcc_i2c = regulator_get(&msg21xx_i2c_client->dev, "vcc_i2c");
	if (IS_ERR(msg->vcc_i2c)) 
    {
		ret = PTR_ERR(msg->vcc_i2c);
		tp_log_err("%s %d:Regulator get failed vcc_i2c rc=%d\n", 
                    __func__, __LINE__, ret);
        ret = -ENODEV;
        goto exit_regulator_get;
	}

	if (regulator_count_voltages(msg->vcc_i2c) > 0) 
    {
		ret = regulator_set_voltage(msg->vcc_i2c, MS_I2C_VTG_MIN_UV, MS_I2C_VTG_MAX_UV);
		if (ret) 
        {
			tp_log_err("%s %d:Regulator set_vtg failed vcc_i2c rc=%d\n", 
                        __func__, __LINE__ ,ret);
            ret = -ENODEV;
            goto exit_regulator_get;
		}
	}

	ret = regulator_enable(msg->vdd);
	if (ret) 
    {
		tp_log_err("%s %d:Regulator vdd enable failed rc=%d\n", 
                    __func__, __LINE__, ret);
		ret = -ENODEV;
        goto exit_regulator_get;
	}

	ret = regulator_enable(msg->vcc_i2c);
	if (ret) 
    {
		tp_log_err("%s %d:Regulator vcc_i2c enable failed rc=%d\n", 
                    __func__, __LINE__, ret);
		ret = -ENODEV;
        goto exit_vcc_i2c_enable;
	}

	gpio_direction_input(MSG21XX_INT_GPIO);
	ret = Check_Device();
	if(ret < 0)
	{
		tp_log_err("%s %d:cannot read ic type.\n", __func__, __LINE__);
		goto exit_check_device;
	}

	INIT_WORK(&msg21xx_wq, msg21xx_do_work);

#ifdef CONFIG_MSG2138_SCAP_TEST
	ito_test_create_entry();
#endif
    msg21xx_init_input();
	ret = request_irq(msg21xx_irq, msg21xx_interrupt, 
							IRQF_TRIGGER_RISING, "ms-msg21xx", NULL);
	if (ret != 0) 
    {
		tp_log_err("%s %d:cannot register irq\n", __func__, __LINE__);
		goto exit_check_device;
	}
    
	disable_irq(msg21xx_irq); 
	_HalTscrHWReset();

#ifdef __FIRMWARE_UPDATE__
    _msg_create_file_for_fwUpdate();
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN+1;
	early_suspend.suspend = msg21xx_ts_early_suspend;
	early_suspend.resume = msg21xx_ts_late_resume;
	register_early_suspend(&early_suspend);
#endif
	
    #if CTP_PROXIMITY_FUN
	InitialProximityFunction();
    #endif

	curr_ic_type = CTP_ID_MSG21XXA;
    _GetMainFwVersion();

#ifdef	__AUTO_UPDATE__
    update_flag=0;
    if(_CheckFwIntegrity())
    {
        _CheckFWVersionAndloadData();
    }
    else
    {
        _CheckSWIDAndLoadData();
    }
    	
    if(update_flag == 1)
    {
        _Update_FW();
    }
#else
    sw_id=_GetVendorID(EMEM_INFO);
#endif

    _HalTscrHWReset();

	msg2138_get_module_name(sw_id, module_name, APP_INFO_VALUE_LENTH);
	sprintf(touch_info,"M-Star_Msg2138_%s_V%d.%d", module_name, curr_ic_major, curr_ic_minor);
	ret = app_info_set("touch_panel", touch_info);
	if (ret)
	{
        tp_log_err( "set_touch_chip_info error\n");
	}
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
    set_hw_dev_flag(DEV_I2C_TOUCH_PANEL);
#endif

	enable_irq(msg21xx_irq);
    tp_log_info("%s %d:Probe success.\n", __func__, __LINE__);
    
	return 0;
exit_check_device:
    regulator_disable(msg->vcc_i2c);
exit_vcc_i2c_enable:
    regulator_disable(msg->vdd);
exit_regulator_get:
    gpio_free(msg->reset_gpio);
exit_reset_gpio:
    gpio_free(msg->irq_gpio);
exit_irq_gpio:
	return ret;
}

static int __devexit msg21xx_ts_remove(struct i2c_client *client)
{
	return 0;
}

static int __init msg21xx_init(void)
{
	int err;

    mutex_init(&msg21xx_mutex);
	err = i2c_add_driver(&msg21xx_ts_driver);
	if (err) 
    {
		tp_log_err("%s %d:msg21xx  driver failed(errno = %d)\n", __func__, __LINE__, err);
	} 
    else 
	{
		tp_log_info("%s %d:Successfully added driver %s.\n", __func__, __LINE__, 
            		msg21xx_ts_driver.driver.name);
	}
    
	return err;
}

static void __exit msg21xx_cleanup(void)
{
	i2c_del_driver(&msg21xx_ts_driver);
}


module_init(msg21xx_init);
module_exit(msg21xx_cleanup);

MODULE_AUTHOR("wax.wang cellon");
MODULE_DESCRIPTION("Driver for msg21xx Touchscreen Controller");
MODULE_LICENSE("GPL");
