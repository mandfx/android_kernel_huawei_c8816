#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/netdevice.h>
#include <linux/mount.h>
#include <linux/proc_fs.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/time.h>
#include "scap_test_lib.h"
#include "ini.h"

struct Test_ConfigParam_SCap{
	 int rawdata_max;
	 int rawdata_min;
	
	 int ChannelNum;
	 int KeyNum;
	
	 int CiMax;
	 int CiMin;
	 int DeltaCiMax;
	 
	 int DeltaCiDiffer;
	 int DeltaCiDifferS2;
	 int DeltaCiDifferS3;
	 int DeltaCiDifferS4;
	 int ChannelDeviation;
	 int ChannelDeviationS2;
	 int ChannelDeviationS3;
	 int ChannelDeviationS4;
	 int TwoSidesDeviation;
	 int TwoSidesDeviationS2;
	 int TwoSidesDeviationS3;
	 int TwoSidesDeviationS4;
	 int KeyDifferMax;
	 int CheckIncludeKeyTest;
	 int CheckChannelDeviationTest;//CHANNEL_DEVIATION_TEST = 1;
	 int CheckTwoSidesDeviationTest;//TWO_SIDES_DEVIATION_TEST = 1;
	 int CheckDeltaCiTest;//DELTA_CI_TEST = 1;

	 int DeltaCiBase[MAX_CHANNEL];
	 int DeltaCiSort[MAX_CHANNEL];
	 int CiMinArr[MAX_CHANNEL];
	 int CiMaxArr[MAX_CHANNEL]; 
};

/*test section*/
#define Section_TestItem 	"TestItem_SCap"
#define Section_BaseSet "BaseSet_SCap"
#define Section_SpecialSet "SpecialSet_SCAP"
/*test item*/
//	#define Item_Channel_Num "Channel_Num"
//	#define Item_Key_Num "Key_Num"
#define Check_ChannelDeviationTest "CHANNEL_DEVIATION_TEST"
#define Check_TwoSidesDeviationTest "TWO_SIDES_DEVIATION_TEST"
#define Check_DeltaCiTest "DELTA_CI_TEST"
#define Check_Include_Key_Test "Include_Key_Test"
#define Item_Key_Differ_Max "Key_Differ_Max"
#define Item_Chip_Ver "Chip_Ver"
#define Item_RawData_Min "RawData_Min"
#define Item_RawData_Max "RawData_Max"
#define Item_Ci_Min "Ci_Min"
#define Item_Ci_Max "Ci_Max"
//#define Item_Delta_Ci_Base "Delta_Ci_Base"
#define Item_Delta_Ci_Max "Delta_Ci_Max"
#define Special_Delta_Ci_Base "Delta_Ci_Base"
#define Item_Delta_Ci_Differ "Delta_Ci_Differ"
#define Item_Delta_Ci_Differ_S2 "Delta_Ci_Differ_S2"
#define Item_Delta_Ci_Differ_S3 "Delta_Ci_Differ_S3"
#define Item_Delta_Ci_Differ_S4 "Delta_Ci_Differ_S4"
#define Item_Channel_Deviation "Channel_Deviation"
#define Item_Channel_Deviation_S2 "Channel_Deviation_S2"
#define Item_Channel_Deviation_S3 "Channel_Deviation_S3"
#define Item_Channel_Deviation_S4 "Channel_Deviation_S4"
#define Item_TwoSides_Deviation "TwoSides_Deviation"
#define Item_TwoSides_Deviation_S2 "TwoSides_Deviation_S2"
#define Item_TwoSides_Deviation_S3 "TwoSides_Deviation_S3"
#define Item_TwoSides_Deviation_S4 "TwoSides_Deviation_S4"
#define Item_DeltaCi_Sort "DeltaCi_Sort"

FTS_I2c_Read_Function focal_I2C_Read;
FTS_I2c_Write_Function focal_I2C_write;

#define FOCAL_SCAP_DBG
#ifdef FOCAL_SCAP_DBG
#define FTS_SCAP_DBG(fmt, args...) printk("[Focal Scap Test]" fmt, ## args)
#else
#define FTS_SCAP_DBG(fmt, args...) do{}while(0)
#endif

boolean bRawdataTest = true;
	
int ChannelNum = 32;
int KeyNum = 4;
int AllChannelNum = MAX_CHANNEL;

int Proof_Normal = 0;
int Proof_Level0 = 1;
int Proof_NoWaterProof = 2;

int rawdata[MAX_CHANNEL];
int cidata[MAX_CHANNEL];
int cidata2[MAX_CHANNEL];
int deltaCiData[MAX_CHANNEL];
int deltaCiDataDiffer[MAX_CHANNEL];

#define EN_CS_NONE 	0    //不使用其他CS测试
#define EN_CS_BASE1	1   //使用BASE1测试
#define EN_CS_BASE2	2   //使用BASE2测试

struct Test_ConfigParam_SCap g_testparam;
char *g_testparamstring = NULL;

char g_testDetailInfo[512];

void TestTp(void);
void TestCi(void);
void GetTestParam(void);

static void focal_msleep(int ms)
{
	msleep(ms);
}
int SetParamData(char * TestParamData)
{
	g_testparamstring = TestParamData;
	GetTestParam();
	return 0;
}
void FreeTestParamData(void)
{
	if(g_testparamstring)
		kfree(g_testparamstring);

	g_testparamstring = NULL;
}

static int focal_abs(int value)
{
	int absvalue = 0;
	if(value > 0)
		absvalue = value;
	else
		absvalue = 0 - value;

	return absvalue;
}
int GetParamValue(char *section, char *ItemName, int defaultvalue) 
{
	int paramvalue = defaultvalue;
	char value[128];
	memset(value , 0, sizeof(value));
	if(ini_get_key(g_testparamstring, section, ItemName, value) < 0) {
		return paramvalue;
	} else {
		paramvalue = atoi(value);
	}
	
	return paramvalue;
}

int GetParamString(char *section, char *ItemName, char *defaultvalue) {
	char value[256];
	int len = 0;
	memset(value , 0, sizeof(value));
	if(ini_get_key(g_testparamstring, section, ItemName, value) < 0) {
		return 0;
	} else {
		len = sprintf(defaultvalue, "%s", value);
	}
	
	return len;
}

void GetTestParam(void)
{
	char str_tmp[128], str_node[64], str_value[512];
	int j, index, valuelen = 0, i = 0;
	memset(str_tmp, 0, sizeof(str_tmp));
	memset(str_node, 0, sizeof(str_node));
	memset(str_value, 0, sizeof(str_value));

	g_testparam.CheckIncludeKeyTest = GetParamValue(Section_BaseSet, Check_Include_Key_Test, 1);
	g_testparam.CheckChannelDeviationTest = GetParamValue(Section_TestItem, Check_ChannelDeviationTest, 1);
	g_testparam.CheckTwoSidesDeviationTest = GetParamValue(Section_TestItem, Check_TwoSidesDeviationTest, 1);
	g_testparam.CheckDeltaCiTest = GetParamValue(Section_TestItem, Check_DeltaCiTest, 1);

	g_testparam.rawdata_min = GetParamValue(Section_BaseSet, Item_RawData_Min, 12500);
	g_testparam.rawdata_max = GetParamValue(Section_BaseSet, Item_RawData_Max, 16500);
	
	g_testparam.CiMin = GetParamValue(Section_BaseSet, Item_Ci_Min, 5);
	g_testparam.CiMax = GetParamValue(Section_BaseSet, Item_Ci_Max, 250);
	
	g_testparam.DeltaCiMax = GetParamValue(Section_BaseSet, Item_Delta_Ci_Max, 60);
	
	if (g_testparam.CheckIncludeKeyTest == 1) {
		g_testparam.KeyDifferMax = GetParamValue(Section_BaseSet, Item_Key_Differ_Max, 10);
	}
	
	g_testparam.DeltaCiDiffer = GetParamValue(Section_BaseSet, Item_Delta_Ci_Differ, 15);
	g_testparam.DeltaCiDifferS2 = GetParamValue(Section_BaseSet, Item_Delta_Ci_Differ_S2, 15);
	g_testparam.DeltaCiDifferS3 = GetParamValue(Section_BaseSet, Item_Delta_Ci_Differ_S3, 15);
	g_testparam.DeltaCiDifferS4 = GetParamValue(Section_BaseSet, Item_Delta_Ci_Differ_S4, 15);
	
	if (g_testparam.CheckChannelDeviationTest == 1) {
		g_testparam.ChannelDeviation = GetParamValue(Section_BaseSet,
				Item_Channel_Deviation, 10);
		g_testparam.ChannelDeviationS2 = GetParamValue(Section_BaseSet,
				Item_Channel_Deviation_S2, 10);
		g_testparam.ChannelDeviationS3 = GetParamValue(Section_BaseSet,
				Item_Channel_Deviation_S3, 10);
		g_testparam.ChannelDeviationS4 = GetParamValue(Section_BaseSet,
				Item_Channel_Deviation_S4, 10);
	}
	if (g_testparam.CheckTwoSidesDeviationTest == 1) {
		g_testparam.TwoSidesDeviation = GetParamValue(Section_BaseSet,
				Item_TwoSides_Deviation, 6);
		g_testparam.TwoSidesDeviationS2 = GetParamValue(Section_BaseSet,
				Item_TwoSides_Deviation_S2, 6);
		g_testparam.TwoSidesDeviationS3 = GetParamValue(Section_BaseSet,
				Item_TwoSides_Deviation_S3, 6);
		g_testparam.TwoSidesDeviationS4 = GetParamValue(Section_BaseSet,
				Item_TwoSides_Deviation_S4, 6);
	}
	valuelen = GetParamString(Section_BaseSet, Item_DeltaCi_Sort, str_value);
	if (valuelen > 0) {
		index = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for(j=0; j<valuelen; j++) {
			if(',' == str_value[j]) {
				g_testparam.DeltaCiSort[i] = atoi(str_tmp);
				index = 0;
				memset(str_tmp, 0, sizeof(str_tmp));
				i++;
			} else {
				if(' ' == str_value[j])
					continue;
				str_tmp[index] = str_value[j];
				index++;
			}
		}
	} else {
		for(j = 0; j < MAX_CHANNEL; j++) {
			g_testparam.DeltaCiSort[j] = 1;
		}
	}
	
	for(j=0; j<MAX_CHANNEL; j++) {
		sprintf(str_tmp, "%s[%d]", Special_Delta_Ci_Base ,(j+1));
		g_testparam.DeltaCiBase[j] = GetParamValue(Section_SpecialSet, str_tmp, 1);
	}
	for(j=0; j<MAX_CHANNEL; j++) {
		sprintf(str_tmp, "%s[%d]", Item_Ci_Min ,(j+1));
		g_testparam.CiMinArr[j] = GetParamValue(Section_SpecialSet, str_tmp, g_testparam.CiMin);
		
		sprintf(str_tmp, "%s[%d]", Item_Ci_Max ,(j+1));
		g_testparam.CiMaxArr[j] = GetParamValue(Section_SpecialSet, str_tmp, g_testparam.CiMax);
	}

#if 0
	FTS_SCAP_DBG("CheckIncludeKeyTest=%d\n", g_testparam.CheckIncludeKeyTest);
	FTS_SCAP_DBG("CheckTwoSidesDeviationTest=%d\n", g_testparam.CheckTwoSidesDeviationTest);
	FTS_SCAP_DBG("CheckChannelDeviationTest=%d\n", g_testparam.CheckChannelDeviationTest);
	FTS_SCAP_DBG("CheckDeltaCiTest=%d\n", g_testparam.CheckDeltaCiTest);
	FTS_SCAP_DBG("rawdata_min=%d\n", g_testparam.rawdata_min);
	FTS_SCAP_DBG("rawdata_max=%d\n", g_testparam.rawdata_max);
	FTS_SCAP_DBG("CiMin=%d\n", g_testparam.CiMin);
	FTS_SCAP_DBG("CiMax=%d\n", g_testparam.CiMax);
	FTS_SCAP_DBG("DeltaCiMax=%d\n", g_testparam.DeltaCiMax);
	FTS_SCAP_DBG("KeyDifferMax=%d\n", g_testparam.KeyDifferMax);
	FTS_SCAP_DBG("DeltaCiDiffer=%d\n", g_testparam.DeltaCiDiffer);
	FTS_SCAP_DBG("DeltaCiDifferS2=%d\n", g_testparam.DeltaCiDifferS2);
	FTS_SCAP_DBG("DeltaCiDifferS3=%d\n", g_testparam.DeltaCiDifferS3);
	FTS_SCAP_DBG("DeltaCiDifferS4=%d\n", g_testparam.DeltaCiDifferS4);
	FTS_SCAP_DBG("ChannelDeviation=%d\n", g_testparam.ChannelDeviation);
	FTS_SCAP_DBG("ChannelDeviationS2=%d\n", g_testparam.ChannelDeviationS2);
	FTS_SCAP_DBG("ChannelDeviationS3=%d\n", g_testparam.ChannelDeviationS3);
	FTS_SCAP_DBG("ChannelDeviationS4=%d\n", g_testparam.ChannelDeviationS4);
	FTS_SCAP_DBG("TwoSidesDeviation=%d\n", g_testparam.TwoSidesDeviation);
	FTS_SCAP_DBG("TwoSidesDeviationS2=%d\n", g_testparam.TwoSidesDeviationS2);
	FTS_SCAP_DBG("TwoSidesDeviationS3=%d\n", g_testparam.TwoSidesDeviationS3);
	FTS_SCAP_DBG("TwoSidesDeviationS4=%d\n", g_testparam.TwoSidesDeviationS4);
#endif
}

int Init_I2C_Read_Func(FTS_I2c_Read_Function fpI2C_Read)
{
	focal_I2C_Read = fpI2C_Read;
	return 0;
}

int Init_I2C_Write_Func(FTS_I2c_Write_Function fpI2C_Write)
{
	focal_I2C_write = fpI2C_Write;
	return 0;
}

int ReadReg(unsigned char RegAddr, unsigned char *RegData)
{
	return focal_I2C_Read(&RegAddr, 1, RegData, 1);
}

int WriteReg(unsigned char RegAddr, unsigned char RegData)
{
	unsigned char cmd[2] = {0};
	cmd[0] = RegAddr;
	cmd[1] = RegData;
	return focal_I2C_write(cmd, 2);
}

static int startscan(void)
{
	int err = 0, i = 0;
	unsigned char regvalue = 0x00;
	/*scan*/
	if(ReadReg(0x00, &regvalue)) {
		if(0x04 != ((regvalue>>4)&0x07)){
			if(WriteReg(0x00, 0x40) < 0) {
				FTS_SCAP_DBG("Enter factory failure\n");
			}
			focal_msleep(100);
		}
	} 
	
	err = ReadReg(0x08, &regvalue);
	if (err < 0) {
		return err;
	}
	else {
		regvalue = 0x01;
		err = WriteReg(0x08, regvalue);
		if (err < 0) {
			return err;
		}
		else {
			for(i=0; i<20; i++) {
				focal_msleep(8);
				err = ReadReg(0x08, &regvalue);
				if (err < 0) {
					return err;
				} else {
					if (0 == regvalue) {
						break;
					}
				}
			}
			if (i >= 20) {
				return -5;
			}
		}
	}
	return 0;
}	

int GetRawData(int rawdata[])
{
	int ReCode = 0;
	unsigned char I2C_wBuffer[3];
	unsigned char rrawdata[MAX_CHANNEL*2];
	unsigned char j = 0, loop = 0, len = 0, i = 0;
	int mtk_readlen = 8;
	int ByteNum = AllChannelNum * 2;

	if((sizeof(rawdata)/sizeof(int)) > (MAX_CHANNEL) ) {
		FTS_SCAP_DBG("raw data buf is more than Max Channel\n");
		return -1;
	}

	if(startscan() < 0)
		return -1;

	I2C_wBuffer[0] = 0x01;
	I2C_wBuffer[1] = 0;
	ReCode = focal_I2C_write(I2C_wBuffer, 2);

	if (ReCode >= 0) {
		if(ByteNum % mtk_readlen == 0)
			loop = ByteNum / mtk_readlen;
		else
			loop = ByteNum / mtk_readlen + 1;
		for (j = 0; j < loop; j++) {
			len = ByteNum - j * mtk_readlen;
			if (len > mtk_readlen)
				len = mtk_readlen;
			
			I2C_wBuffer[0] = 0xB0 + j * mtk_readlen;
			// set begin address
			I2C_wBuffer[1] = len;

			ReCode = focal_I2C_Read(I2C_wBuffer, 2, rrawdata, len);
			
			if (ReCode >= 0) {
				for (i = 0; i < (len >> 1); i++) {
					rawdata[i+j*mtk_readlen/2] = (unsigned short)((unsigned short)(rrawdata[i << 1]) << 8) \
						+ (unsigned short)rrawdata[(i << 1) + 1];
				}
			}
			else {
				FTS_SCAP_DBG("Get Rawdata failure\n");
				break;
			}
		}
	}
#if 0
	for(i=0; i<AllChannelNum; i++)
		FTS_SCAP_DBG("rawdata[%d]=%d\n", i+1, rawdata[i]);
#endif
	return ReCode;
}

int GetCI(int ci[])
{
	int i = 0;
	unsigned char regdata = 0x00;
	if( startscan() < 0) {
		FTS_SCAP_DBG("scan failure in get ic...\n");
		return -1;
	}
	for (i = 0; i < MAX_CHANNEL; i++) {
		if (ReadReg(0x11 + i, &regdata) < 0) {
			FTS_SCAP_DBG("get ic %d failue...\n", i);
			return -1;
		}
		focal_msleep(10);
		ci[i] = regdata;
	}
			
	return 0;
}

boolean StartTestTP() {
	bRawdataTest = true;
	
	TestTp();
	
	return bRawdataTest;
}

int WaterProof(int ProofType) {
	int ret = 0, i = 0;
	
	ret = WriteReg(0xAe, ProofType);
	
	if(ret > 0)
		return ret;
	else {
		for(i=0; i<3; i++) {
			ret = startscan();
			if (ret < 0) {
				focal_msleep(50);
			} else
				break;
		}
	}
	return ret;
}
void SetCsOfTestMode1(void) {
	int cscValue, cs3Value, cs2Value;
	int cs4Value, cs5Value, cs6Value, CsType;
	if ( 1 == GetParamValue("BaseSet", "CS_TestMode1", 0)) {
		cscValue =  GetParamValue("BaseSet","CSC_Mode1", 0);
		cs2Value =  GetParamValue("BaseSet","CS2_Mode1", 0);
		cs3Value =  GetParamValue("BaseSet","CS3_Mode1", 0);
		cs4Value =  GetParamValue("BaseSet","CS4_Mode1", 0);
		cs5Value =  GetParamValue("BaseSet","CS5_Mode1", 0);
		cs6Value =  GetParamValue("BaseSet","CS6_Mode1", 0);
		
		CsType = GetParamValue("BaseSet","CS_Type", 0);
		if (CsType >= 0) {
			WriteReg(0x0D, cscValue);
			focal_msleep(50);
		} 
		if (CsType >= 1) {
			WriteReg(0x0E, cs2Value);
			focal_msleep(50);
		}
		if (CsType >= 2) {
			WriteReg(0xAC, cs3Value);
			focal_msleep(50);
		}
		if (CsType >= 3) {
			WriteReg(0xA1, cs4Value);
			focal_msleep(50);
			WriteReg(0xA2, cs5Value);
			focal_msleep(50);
			WriteReg(0xA3, cs6Value);
			focal_msleep(50);
		}
		startscan();
	}
}
void SetCsOfTestMode2(void) {
	int cscValue, cs3Value, cs2Value;
	int cs4Value, cs5Value, cs6Value, CsType;
	if ( 1 == GetParamValue("BaseSet", "CS_TestMode2", 0)) {
		cscValue =  GetParamValue("BaseSet","CSC_Mode2", 0);
		cs2Value =  GetParamValue("BaseSet","CS2_Mode2", 0);
		cs3Value =  GetParamValue("BaseSet","CS3_Mode2", 0);
		cs4Value =  GetParamValue("BaseSet","CS4_Mode2", 0);
		cs5Value =  GetParamValue("BaseSet","CS5_Mode2", 0);
		cs6Value =  GetParamValue("BaseSet","CS6_Mode2", 0);
		
		CsType = GetParamValue("BaseSet","CS_Type", 0);
		if (CsType >= 0) {
			WriteReg(0x0D, cscValue);
			focal_msleep(50);
		} 
		if (CsType >= 1) {
			WriteReg(0x0E, cs2Value);
			focal_msleep(50);
		}
		if (CsType >= 2) {
			WriteReg(0xAC, cs3Value);
			focal_msleep(50);
		}
		if (CsType >= 3) {
			WriteReg(0xA1, cs4Value);
			focal_msleep(50);
			WriteReg(0xA2, cs5Value);
			focal_msleep(50);
			WriteReg(0xA3, cs6Value);
			focal_msleep(50);
		}
		startscan();
	}
}

void GetCsValueByType(int csType, int mode, int csvalue[]) {
	if (EN_CS_BASE1 == csType) {
		if (1 == mode) {
			csvalue[0] =  GetParamValue("BaseSet","Another_CSC_Mode1", 0);
			csvalue[1] =  GetParamValue("BaseSet","Another_CS2_Mode1", 0);
			csvalue[2] =  GetParamValue("BaseSet","Another_CS3_Mode1", 0);
			csvalue[3] =  GetParamValue("BaseSet","Another_CS4_Mode1", 0);
			csvalue[4] =  GetParamValue("BaseSet","Another_CS5_Mode1", 0);
			csvalue[5] =  GetParamValue("BaseSet","Another_CS6_Mode1", 0);
		} else if (2 == mode) {
			csvalue[0] =  GetParamValue("BaseSet","Another_CSC_Mode2", 0);
			csvalue[1] =  GetParamValue("BaseSet","Another_CSC_Mode2", 0);
			csvalue[2] =  GetParamValue("BaseSet","Another_CSC_Mode2", 0);
			csvalue[3] =  GetParamValue("BaseSet","Another_CSC_Mode2", 0);
			csvalue[4] =  GetParamValue("BaseSet","Another_CSC_Mode2", 0);
			csvalue[5] =  GetParamValue("BaseSet","Another_CSC_Mode2", 0);
		}
	} else if (EN_CS_BASE2 == csType) {
		if (1 == mode) {
			csvalue[0] =  GetParamValue("BaseSet","Another_MODE1_CSA1", 0);
			csvalue[1] =  GetParamValue("BaseSet","Another_MODE1_CSA2", 0);
			csvalue[2] =  GetParamValue("BaseSet","Another_MODE1_CSA3", 0);
			csvalue[3] =  GetParamValue("BaseSet","Another_MODE1_CSA4", 0);
			csvalue[4] =  GetParamValue("BaseSet","Another_MODE1_CSA5", 0);
			csvalue[5] =  GetParamValue("BaseSet","Another_MODE1_CSA6", 0);
		} else if (2 == mode) {
			csvalue[0] =  GetParamValue("BaseSet","Another_MODE2_CSA1", 0);
			csvalue[1] =  GetParamValue("BaseSet","Another_MODE2_CSA2", 0);
			csvalue[2] =  GetParamValue("BaseSet","Another_MODE2_CSA3", 0);
			csvalue[3] =  GetParamValue("BaseSet","Another_MODE2_CSA4", 0);
			csvalue[4] =  GetParamValue("BaseSet","Another_MODE2_CSA5", 0);
			csvalue[5] =  GetParamValue("BaseSet","Another_MODE2_CSA6", 0);
		}
	}
}

void SetAnotherCsOfTestMode2(int csType) {
	int value = 0, CsType = 0;
	int cscValue, cs3Value, cs2Value, cs4Value, cs5Value, cs6Value;
	int csvalue[6];
	if(EN_CS_BASE1 == csType)
		value = GetParamValue("BaseSet", "Another_CS_TestMode2", 0);
	else if(EN_CS_BASE2 == csType)
        value = GetParamValue("BaseSet", "Another_CS2_TestMode2", 0);
	if(1 == value) {
		GetCsValueByType(csType, 2, csvalue);
		cscValue = csvalue[0];
		cs2Value = csvalue[1];
		cs3Value = csvalue[2];
		cs4Value = csvalue[3];
		cs5Value = csvalue[4];
		cs6Value = csvalue[5];
		
		CsType = GetParamValue("BaseSet", "CS_Type", 0);
		if (CsType >= 0) {
			WriteReg(0x0D, cscValue);
			focal_msleep(50);
		} 
		if (CsType >= 1) {
			WriteReg(0x0E, cs2Value);
			focal_msleep(50);
		}
		if (CsType >= 2) {
			WriteReg(0xAC, cs3Value);
			focal_msleep(50);
		}
		if (CsType >= 3) {
			WriteReg(0xA1, cs4Value);
			focal_msleep(50);
			WriteReg(0xA2, cs5Value);
			focal_msleep(50);
			WriteReg(0xA3, cs6Value);
			focal_msleep(50);
		}
		startscan();
	}
}
void SetAnotherCsOfTestMode1(int csType) {
	int value = 0, CsType = 0;
	int cscValue, cs3Value, cs2Value, cs4Value, cs5Value, cs6Value;
	int csvalue[6];
	if(EN_CS_BASE1 == csType)
		value = GetParamValue("BaseSet", "Another_CS_TestMode1", 0);
	else if(EN_CS_BASE2 == csType)
        value = GetParamValue("BaseSet", "Another_CS2_TestMode2", 0);
	if(1 == value) {
		GetCsValueByType(csType, 2, csvalue);
		cscValue = csvalue[0];
		cs2Value = csvalue[1];
		cs3Value = csvalue[2];
		cs4Value = csvalue[3];
		cs5Value = csvalue[4];
		cs6Value = csvalue[5];
		
		CsType = GetParamValue("BaseSet", "CS_Type", 0);
		if (CsType >= 0) {
			WriteReg(0x0D, cscValue);
			focal_msleep(150);
		} 
		if (CsType >= 1) {
			WriteReg(0x0E, cs2Value);
			focal_msleep(150);
		}
		if (CsType >= 2) {
			WriteReg(0xAC, cs3Value);
			focal_msleep(150);
		}
		if (CsType >= 3) {
			WriteReg(0xA1, cs4Value);
			focal_msleep(150);
			WriteReg(0xA2, cs5Value);
			focal_msleep(150);
			WriteReg(0xA3, cs6Value);
			focal_msleep(150);
		}
		startscan();
	}
}
void Test_AnotherCiTest(int csType) {
	WaterProof(Proof_NoWaterProof);
	SetAnotherCsOfTestMode2(csType);
	GetCI(cidata);
	
	WaterProof(Proof_Level0);
	SetAnotherCsOfTestMode1(csType);
	GetCI(cidata2);
}

void Test_ChannelsDeviation(void) {
	int i = 0;
	int Critical_Channel_S1 = 0;
	int Critical_Channel_S2 = 0;
	int Critical_Channel_S3 = 0;
	int Critical_Channel_S4 = 0;
	boolean bUseCriticalValue = false;
	int Sort1LastNum = 0, Sort2LastNum = 0, Sort3LastNum = 0, Sort4LastNum = 0;
	int GetDeviation;
	boolean bFirstUseSort1 = true;
	boolean bFirstUseSort2 = true;
	boolean bFirstUseSort3 = true;
	boolean bFirstUseSort4 = true;

	int MaxDev_AllS1= 0, MaxDev_AllS2= 0, MaxDev_AllS3= 0, MaxDev_AllS4= 0;
	
	if(1 == GetParamValue("TestItem_SCap","SET_CRITICAL_D", 0)) {
		bUseCriticalValue = true;
		Critical_Channel_S1 = GetParamValue("BaseSet_SCap","Critical_Channel_S1", 0);
		Critical_Channel_S2 = GetParamValue("BaseSet_SCap","Critical_Channel_S2", 0);
		Critical_Channel_S3 = GetParamValue("BaseSet_SCap","Critical_Channel_S3", 0);
		Critical_Channel_S4 = GetParamValue("BaseSet_SCap","Critical_Channel_S4", 0);
	}
	else
		bUseCriticalValue = false;
	
	for(i=0; i < ChannelNum; i++) {
		
		if(1 == g_testparam.DeltaCiSort[i]) {
			if(bFirstUseSort1) {
				bFirstUseSort1 = false;
			} else {
				if((Sort1LastNum + 1) == i) {//相邻通道 
					if((Sort1LastNum == (ChannelNum/2 - 1)) && (i == ChannelNum/2)) {
						//总通道分两部分，前半部分的结尾不与后半部分的开始相比较，
						//注意通道从0开始，前半部分的结尾是m_ChannelNum/2 - 1
					} else {
						if (deltaCiDataDiffer[i] > deltaCiDataDiffer[i-1])
							GetDeviation = deltaCiDataDiffer[i] - deltaCiDataDiffer[i-1];
						else
							GetDeviation = deltaCiDataDiffer[i-1] - deltaCiDataDiffer[i];
						if(GetDeviation >= g_testparam.ChannelDeviation) {
							if(bUseCriticalValue) {
								if(GetDeviation >= Critical_Channel_S1) {
									FTS_SCAP_DBG("GetDeviation >= Critical_Channel_S1\n");
									bRawdataTest = false;
								}
							} else {
								FTS_SCAP_DBG("GetDeviation >= g_testparam.ChannelDeviation!\n");
								bRawdataTest = false;
							}
						}
						if(MaxDev_AllS1 < GetDeviation)
							MaxDev_AllS1 = GetDeviation;
					}
				}
			}
			Sort1LastNum = i;				
		}
		if(2 == g_testparam.DeltaCiSort[i]) {
			if(bFirstUseSort2) {
				bFirstUseSort2 = false;
			} else {
				if((Sort2LastNum + 1) == i) {//相邻通道
					if (deltaCiDataDiffer[i] > deltaCiDataDiffer[i-1])
						GetDeviation = deltaCiDataDiffer[i] - deltaCiDataDiffer[i-1];
					else
						GetDeviation = deltaCiDataDiffer[i-1] - deltaCiDataDiffer[i];
						
					if(GetDeviation >= g_testparam.ChannelDeviationS2) {
						if(bUseCriticalValue) {
							if(GetDeviation >= Critical_Channel_S2) {
								FTS_SCAP_DBG("GetDeviation >= Critical_Channel_S2\n");
								bRawdataTest = false;
							}
						} else {
							FTS_SCAP_DBG("GetDeviation >= g_testparam.ChannelDeviationS2!\n");
							bRawdataTest = false;
						}
					} 
					if(MaxDev_AllS2 < GetDeviation)
						MaxDev_AllS2 = GetDeviation;
				}
			}
			Sort2LastNum = i;	
		}
		if(3 == g_testparam.DeltaCiSort[i]) {
			if(bFirstUseSort3) {
				bFirstUseSort3 = false;
			} else {
				if((Sort3LastNum + 1) == i) {//相邻通道
					if (deltaCiDataDiffer[i] > deltaCiDataDiffer[i-1])
						GetDeviation = deltaCiDataDiffer[i] - deltaCiDataDiffer[i-1];
					else
						GetDeviation = deltaCiDataDiffer[i-1] - deltaCiDataDiffer[i];
						
					if(GetDeviation >= g_testparam.ChannelDeviationS3) {
						if(bUseCriticalValue) {
							if(GetDeviation >= Critical_Channel_S3) {
								FTS_SCAP_DBG("GetDeviation >= Critical_Channel_S3!\n");
								bRawdataTest = false;
							}
						} else {
							FTS_SCAP_DBG("GetDeviation >= g_testparam.ChannelDeviationS3!\n");
							bRawdataTest = false;
						}
					} 
					if(MaxDev_AllS3 < GetDeviation)
						MaxDev_AllS3 = GetDeviation;
				}
			}
			Sort3LastNum = i;	
		}
		if(4 == g_testparam.DeltaCiSort[i]) {
			if(bFirstUseSort4) {
				bFirstUseSort4 = false;
			} else {
				if((Sort3LastNum + 1) == i) {//相邻通道
					if (deltaCiDataDiffer[i] > deltaCiDataDiffer[i-1])
						GetDeviation = deltaCiDataDiffer[i] - deltaCiDataDiffer[i-1];
					else
						GetDeviation = deltaCiDataDiffer[i-1] - deltaCiDataDiffer[i];
						
					if(GetDeviation >= g_testparam.ChannelDeviationS4) {
						if(bUseCriticalValue) {
							if(GetDeviation >= Critical_Channel_S4) {
								FTS_SCAP_DBG("GetDeviation >= Critical_Channel_S4!\n");
								bRawdataTest = false;
							}
						} else {
							FTS_SCAP_DBG("GetDeviation >= g_testparam.ChannelDeviationS4\n");
							bRawdataTest = false;
						}
					} 
					if(MaxDev_AllS4 < GetDeviation)
						MaxDev_AllS4 = GetDeviation;
				}
			}
			Sort4LastNum = i;	
		}
	}
}

void Test_TwoSidesDeviation(void) {
	boolean bUseCriticalValue = false;
	int Critical_TwoSides_S1 = 0;
	int Critical_TwoSides_S2 = 0;
	int Critical_TwoSides_S3 = 0;
	int Critical_TwoSides_S4 = 0;
	int DevMax_AllS1 = 0, DevMax_AllS2 = 0, DevMax_AllS3 = 0, DevMax_AllS4 = 0;
	int GetDeviation = 0, i = 0;
	
	for (i = 0; i < ChannelNum / 2; i++) {
		if(deltaCiDataDiffer[i] > deltaCiDataDiffer[i + ChannelNum / 2])
			GetDeviation = deltaCiDataDiffer[i] - deltaCiDataDiffer[i + ChannelNum / 2];
		else
			GetDeviation = deltaCiDataDiffer[i + ChannelNum / 2] - deltaCiDataDiffer[i];
		if(1 == g_testparam.DeltaCiSort[i]) {
			if (GetDeviation >= g_testparam.TwoSidesDeviation) {
				if (bUseCriticalValue) {
					if (GetDeviation >= Critical_TwoSides_S1) {
						FTS_SCAP_DBG("GetDeviation >= Critical_TwoSides_S1!\n");
						bRawdataTest = false;
					}
				} else {
					FTS_SCAP_DBG("GetDeviation >= g_testparam.TwoSidesDeviation\n");
					bRawdataTest = false;
				}
			}
			if (DevMax_AllS1 < GetDeviation)
				DevMax_AllS1 = GetDeviation;
		}
		if(2 == g_testparam.DeltaCiSort[i]) {
			if (GetDeviation >= g_testparam.TwoSidesDeviationS2) {
				if (bUseCriticalValue) {
					if (GetDeviation >= Critical_TwoSides_S2) {
						FTS_SCAP_DBG("GetDeviation >= Critical_TwoSides_S2!\n");
						bRawdataTest = false;
					}
				} else {
					FTS_SCAP_DBG("GetDeviation >= g_testparam.TwoSidesDeviationS2!\n");
					bRawdataTest = false;
				}
			}
			if (DevMax_AllS2 < GetDeviation)
				DevMax_AllS2 = GetDeviation;
		}
		if(3 == g_testparam.DeltaCiSort[i]) {
			if (GetDeviation >= g_testparam.TwoSidesDeviationS3) {
				if (bUseCriticalValue) {
					if (GetDeviation >= Critical_TwoSides_S3) {
						FTS_SCAP_DBG("GetDeviation >= Critical_TwoSides_S3\n");
						bRawdataTest = false;
					}
				} else {
					FTS_SCAP_DBG("GetDeviation >= g_testparam.TwoSidesDeviationS3\n");
					bRawdataTest = false;
				}
			}
			if (DevMax_AllS3 < GetDeviation)
				DevMax_AllS3 = GetDeviation;
		}
		if(4 == g_testparam.DeltaCiSort[i]) {
			if (GetDeviation >= g_testparam.TwoSidesDeviationS4) {
				if (bUseCriticalValue) {
					if (GetDeviation >= Critical_TwoSides_S4) {
						FTS_SCAP_DBG("GetDeviation >= Critical_TwoSides_S4\n");
						bRawdataTest = false;
					}
				} else {
					FTS_SCAP_DBG("GetDeviation >= g_testparam.TwoSidesDeviationS4\n");
					bRawdataTest = false;
				}
			}
			if (DevMax_AllS4 < GetDeviation)
				DevMax_AllS4 = GetDeviation;
		}
	}
}
void TestCi(void) {
	int i = 0;
	int enUseAnotherCiTest = 0;
	
	int Sort1Min, Sort2Min, Sort3Min,Sort4Min;
	int Sort1Max, Sort2Max, Sort3Max,Sort4Max;
	boolean bUseSort1 = false;
	boolean bUseSort2 = false;
	boolean bUseSort3 = false;
	boolean bUseSort4 = false;
	int Critical_Delta_S1 = 0;
	int Critical_Delta_S2 = 0;
	int Critical_Delta_S3 = 0;
	int Critical_Delta_S4 = 0;
	int DeltaMin = 0, DeltaMax = 0;
	//int KeyDeltaMax = 0;
	boolean bUseCriticalValue = false;
	
	//test ci
	WaterProof(Proof_NoWaterProof);
	SetCsOfTestMode2();
	if ( GetCI(cidata) < 0) {
		bRawdataTest = false;
		return;
	}
	for (i=0; i<AllChannelNum; i++) {
		if(cidata[i] >= g_testparam.CiMax)
			enUseAnotherCiTest = EN_CS_BASE2;
		else if (cidata[i] <= g_testparam.CiMin)
			enUseAnotherCiTest = EN_CS_BASE1;
	}
	
	WaterProof(Proof_Level0);
	SetCsOfTestMode1();
	if ( GetCI(cidata2) < 0) {
		bRawdataTest = false;
		return;
	}
	for (i=0; i<AllChannelNum; i++) {
		if(cidata2[i] >= g_testparam.CiMax)
			enUseAnotherCiTest = EN_CS_BASE2;
		else if (cidata2[i] <= g_testparam.CiMin)
			enUseAnotherCiTest = EN_CS_BASE1;
	}
	
	if (1 == GetParamValue("BaseSet", "Set_Another_CS", 0)) {
		if(enUseAnotherCiTest != EN_CS_NONE)//如果读取到的Ci为0，采用另一套参数进行测试
			Test_AnotherCiTest( enUseAnotherCiTest );
	} else {
		enUseAnotherCiTest = EN_CS_NONE;
	}
	for (i=0; i<AllChannelNum; i++) {
		if (cidata2[i] > g_testparam.CiMaxArr[i] ||
				cidata2[i] < g_testparam.CiMinArr[i]) {
			FTS_SCAP_DBG("cidata2[i] > g_testparam.CiMaxArr[i] or cidata2[i] < g_testparam.CiMinArr[i]\n");
			bRawdataTest = false;
		}
	}
	if (1 == g_testparam.CheckDeltaCiTest) {
		for (i=0; i<AllChannelNum; i++) {
			deltaCiData[i] = cidata[i] - cidata2[i];
			deltaCiDataDiffer[i] = deltaCiData[i] - g_testparam.DeltaCiBase[i];
		}
	}
	//new test

	Sort1Min = Sort2Min = Sort3Min = Sort4Min =1000;
	Sort1Max = Sort2Max = Sort3Max = Sort4Max =-1000;
	for(i=0; i<ChannelNum; i++) {
		if(1 == g_testparam.DeltaCiSort[i]) {
			bUseSort1 = true;
			if(deltaCiDataDiffer[i] < Sort1Min) {
				Sort1Min = deltaCiDataDiffer[i];
			}
			if(deltaCiDataDiffer[i] > Sort1Max) {
				Sort1Max = deltaCiDataDiffer[i];
			}
		}
		if(2 == g_testparam.DeltaCiSort[i]) {
			bUseSort2 = true;
			if(deltaCiDataDiffer[i] < Sort2Min) {
				Sort2Min = deltaCiDataDiffer[i];
			}
			if(deltaCiDataDiffer[i] > Sort2Max) {
				Sort2Max = deltaCiDataDiffer[i];
			}
		}
		if(3 == g_testparam.DeltaCiSort[i]) {
			bUseSort3 = true;
			if(deltaCiDataDiffer[i] < Sort3Min) {
				Sort3Min = deltaCiDataDiffer[i];
			}
			if(deltaCiDataDiffer[i] > Sort3Max) {
				Sort3Max = deltaCiDataDiffer[i];
			}
		}
		if(4 == g_testparam.DeltaCiSort[i]) {
			bUseSort4 = true;
			if(deltaCiDataDiffer[i] < Sort4Min) {
				Sort4Min = deltaCiDataDiffer[i];
			}
			if(deltaCiDataDiffer[i] > Sort4Max) {
				Sort4Max = deltaCiDataDiffer[i];
			}
		}

	}	
	if(1 == GetParamValue("TestItem_SCap","SET_CRITICAL_D", 0)) {
		bUseCriticalValue = true;
		Critical_Delta_S1 = GetParamValue("BaseSet_SCap","Critical_Delta_S1", 0);
		Critical_Delta_S2 = GetParamValue("BaseSet_SCap","Critical_Delta_S2", 0);
		Critical_Delta_S3 = GetParamValue("BaseSet_SCap","Critical_Delta_S3", 0);
		Critical_Delta_S4 = GetParamValue("BaseSet_SCap","Critical_Delta_S4", 0);
		FTS_SCAP_DBG("S1=%d S2=%d S3=%d S4=%d \n", Critical_Delta_S1, Critical_Delta_S2, \
			Critical_Delta_S3, Critical_Delta_S4);
	
	}
	else
		bUseCriticalValue = false;

	FTS_SCAP_DBG("S1Max = %d S1Min = %d \n", Sort1Max, Sort1Min);
	FTS_SCAP_DBG("S2Max = %d S2Min = %d \n", Sort2Max, Sort2Min);
	FTS_SCAP_DBG("S3Max = %d S3Min = %d \n", Sort3Max, Sort3Min);
	FTS_SCAP_DBG("S4Max = %d S4Min = %d \n", Sort4Max, Sort4Min);
	if(bUseSort1) {
		if(g_testparam.DeltaCiDiffer <= (Sort1Max - Sort1Min)) {
			if(bUseCriticalValue) {
				if((Sort1Max - Sort1Min) >= Critical_Delta_S1) {
					FTS_SCAP_DBG("Sort1Max - Sort1Min >= Critical_Delta_S1!\n");
					bRawdataTest = false;
				}
			} else {
				FTS_SCAP_DBG("g_testparam.DeltaCiDiffer <= (Sort1Max - Sort1Min)!\n");
				bRawdataTest = false;
			}
		}
	}
	if(bUseSort2) {
		if(g_testparam.DeltaCiDifferS2 <= (Sort2Max - Sort2Min)) {
			if(bUseCriticalValue) {
				if((Sort2Max - Sort2Min) >= Critical_Delta_S2) {
					FTS_SCAP_DBG("Sort2Max - Sort2Min >= Critical_Delta_S2!\n");
					bRawdataTest = false;
				}
			} else {
				FTS_SCAP_DBG("g_testparam.DeltaCiDifferS2 <= (Sort2Max - Sort2Min)!\n");
				bRawdataTest = false;
			}
		}
	}
	if(bUseSort3) {
		if(g_testparam.DeltaCiDifferS3 <= (Sort3Max - Sort3Min)) {
			if(bUseCriticalValue) {
				if(Sort3Max - Sort3Min >= Critical_Delta_S3) {
					FTS_SCAP_DBG("Sort3Max - Sort3Min >= Critical_Delta_S3!\n");
					bRawdataTest = false;
				}
			} else {
				FTS_SCAP_DBG("g_testparam.DeltaCiDifferS3 <= (Sort3Max - Sort3Min)!\n");
				bRawdataTest = false;
			}
		}
	}
	if(bUseSort4) {
		if(g_testparam.DeltaCiDifferS4 <= (Sort4Max - Sort4Min)) {
			if(bUseCriticalValue) {
				if((Sort4Max - Sort4Min) >= Critical_Delta_S4) {
					FTS_SCAP_DBG("(Sort4Max - Sort4Min) >= Critical_Delta_S4!\n");
					bRawdataTest = false;
				}
			} else {
				FTS_SCAP_DBG("g_testparam.DeltaCiDifferS4 <= (Sort4Max - Sort4Min)!\n");
				bRawdataTest = false;
			}
		}
	}
	if(deltaCiDataDiffer[0] > 0) {
		DeltaMin = deltaCiDataDiffer[0];
		DeltaMax = deltaCiDataDiffer[0];
	} else {
		DeltaMin = 0 - deltaCiDataDiffer[0];
		DeltaMax = 0 - deltaCiDataDiffer[0];
	}

	for(i=1; i < ChannelNum; i++) {
		if(focal_abs(deltaCiDataDiffer[i]) < DeltaMin) {
			DeltaMin = focal_abs(deltaCiDataDiffer[i]);
		}
		if(focal_abs(deltaCiDataDiffer[i]) > DeltaMax) {
			DeltaMax = focal_abs(deltaCiDataDiffer[i]);
		}
	}
	if (DeltaMax > g_testparam.DeltaCiMax) {
		FTS_SCAP_DBG("DeltaMax > g_testparam.DeltaCiMax\n");
		bRawdataTest = false;
	}
	
	if (1 == g_testparam.CheckIncludeKeyTest) {
		int KeyDeltaMax = focal_abs(deltaCiDataDiffer[ChannelNum]);
		for(i=ChannelNum; i<AllChannelNum; i++) {
			if (focal_abs(deltaCiDataDiffer[i]) > KeyDeltaMax) {
				KeyDeltaMax = focal_abs(deltaCiDataDiffer[i]);
			}
		}
		
		if (g_testparam.KeyDifferMax <= KeyDeltaMax) {
			FTS_SCAP_DBG("g_testparam.KeyDifferMax <= KeyDeltaMax	g_testparam.KeyDifferMax=%d KeyDeltaMax=%d\n", 
				g_testparam.KeyDifferMax, KeyDeltaMax);
			bRawdataTest = false;
		}
	}
	
	if (1 == g_testparam.CheckChannelDeviationTest)
		Test_ChannelsDeviation();
	if (1 == g_testparam.CheckTwoSidesDeviationTest)
		Test_TwoSidesDeviation();
	
}

void TestTp(void) {
	int i = 0, min = 0, max = 0;
	unsigned char regvalue = 0x00;

	memset(rawdata, 0, MAX_CHANNEL);
	
	bRawdataTest = true;

	if(WriteReg(0x00, 0x40) < 0) {
		FTS_SCAP_DBG("Enter factory failure\n");
		bRawdataTest = false;
		goto Enter_WorkMode;
	}
	focal_msleep(200);
	if(ReadReg(0x0a, &regvalue) >= 0)
		ChannelNum = regvalue;
	else {
		FTS_SCAP_DBG("Get Channel Number failure\n");
		bRawdataTest = false;
		goto Enter_WorkMode;
	}
	if(ReadReg(0x0b, &regvalue) >= 0)
		KeyNum = regvalue;
	else {
		FTS_SCAP_DBG("Get Key Number failure\n");
		bRawdataTest = false;
		goto Enter_WorkMode;
	}
	
	AllChannelNum = ChannelNum + KeyNum;
	FTS_SCAP_DBG("AllChannelNum=%d\n", AllChannelNum);
	//test rawdata
	for(i=0; i<3; i++) {
		if( GetRawData(rawdata) < 0) {
			break;
		}
	}

    /* test capacitance value */
	min = g_testparam.rawdata_min; max = g_testparam.rawdata_max;
	for (i = 0; i < AllChannelNum; i++) {
		if (rawdata[i] < min || rawdata[i] > max) {
			bRawdataTest = false;
			FTS_SCAP_DBG("rawdata test failure. min=%d max=%d rawdata[%d]=%d\n", \
				min, max, i+1, rawdata[i]);
			goto Enter_WorkMode;
		}
	}
	
	TestCi();
Enter_WorkMode:	
	//the end, return work mode
	for (i = 0; i < 3; i++) {
		if (WriteReg(0x00, 0x00) >=0)
			break;
		else {
			focal_msleep(200);
		}
	}
}

void GetTestDetail(char * TestDetailInfo)
{
	TestDetailInfo = g_testDetailInfo;
}
static int GetCiRawDataMinMaxAvg(int *pMin, int *pMax, int *pAvg, int buf[])
{
	int i, temp, iMax, iMin;
	int iCount = 0, iTotal = 0;

	iMax = 0;
	iMin = 15000;
	for(i = 0; i < AllChannelNum; i++) {
		temp =  buf[i];
		iTotal += temp;
		iCount++;
		if(iMax < temp){
			iMax = temp;
		}
		if(iMin > temp){
			iMin = temp;
		}
	}
	if(iCount > 0)
		*pAvg = iTotal/iCount;
	*pMax = iMax;
	*pMin = iMin;
	return 0;
}
int GetSortInfo(char sortinfo[])
{
	int i, infolen = 0;
	int Sort1Min, Sort2Min, Sort3Min,Sort4Min;
	int Sort1Max, Sort2Max, Sort3Max,Sort4Max;
	boolean bUseSort1, bUseSort2, bUseSort3, bUseSort4;
	char tmpbuf[128];
	
	Sort1Min = Sort2Min = Sort3Min = Sort4Min =1000;
	Sort1Max = Sort2Max = Sort3Max = Sort4Max =-1000;
	for(i=0; i<ChannelNum; i++) {
		if(1 == g_testparam.DeltaCiSort[i]) {
			bUseSort1 = true;
			if(deltaCiDataDiffer[i] < Sort1Min) {
				Sort1Min = deltaCiDataDiffer[i];
			}
			if(deltaCiDataDiffer[i] > Sort1Max) {
				Sort1Max = deltaCiDataDiffer[i];
			}
		}
		if(2 == g_testparam.DeltaCiSort[i]) {
			bUseSort2 = true;
			if(deltaCiDataDiffer[i] < Sort2Min) {
				Sort2Min = deltaCiDataDiffer[i];
			}
			if(deltaCiDataDiffer[i] > Sort2Max) {
				Sort2Max = deltaCiDataDiffer[i];
			}
		}
		if(3 == g_testparam.DeltaCiSort[i]) {
			bUseSort3 = true;
			if(deltaCiDataDiffer[i] < Sort3Min) {
				Sort3Min = deltaCiDataDiffer[i];
			}
			if(deltaCiDataDiffer[i] > Sort3Max) {
				Sort3Max = deltaCiDataDiffer[i];
			}
		}
		if(4 == g_testparam.DeltaCiSort[i]) {
			bUseSort4 = true;
			if(deltaCiDataDiffer[i] < Sort4Min) {
				Sort4Min = deltaCiDataDiffer[i];
			}
			if(deltaCiDataDiffer[i] > Sort4Max) {
				Sort4Max = deltaCiDataDiffer[i];
			}
		}
	}	
	if(bUseSort1) {
		infolen += sprintf(tmpbuf, "S1_Max_DeltaCi_Differ:, %d, S1_Min_DeltaCi_Differ:, %d, S1_Deviation_DeltaCi_Differ:, %d,",
					Sort1Max, Sort1Min, Sort1Max - Sort1Min);
		strcat(sortinfo, tmpbuf);
	}
	if(bUseSort2) {
		infolen += sprintf(tmpbuf, "S2_Max_DeltaCi_Differ:, %d, S2_Min_DeltaCi_Differ:, %d, S2_Deviation_DeltaCi_Differ:, %d,",
					Sort2Max, Sort2Min, Sort2Max - Sort2Min);
		strcat(sortinfo, tmpbuf);
	}
	if(bUseSort3) {
		infolen += sprintf(tmpbuf, "S3_Max_DeltaCi_Differ:, %d, S3_Min_DeltaCi_Differ:, %d, S3_Deviation_DeltaCi_Differ:, %d,",
					Sort3Max, Sort3Min, Sort3Max - Sort3Min);
		strcat(sortinfo, tmpbuf);
	}
	if(bUseSort4) {
		infolen += sprintf(tmpbuf, "S4_Max_DeltaCi_Differ:, %d, S4_Min_DeltaCi_Differ:, %d, S4_Deviation_DeltaCi_Differ:, %d,",
					Sort4Max, Sort4Min, Sort4Max - Sort4Min);
		strcat(sortinfo, tmpbuf);
	}
	strcat(sortinfo, "\r\n");
	return infolen;
}
#if 0
#define FOCAL_SCAP_SAMPLE_DATA_FILEPATH "/mnt/sdcard/"
int focal_save_scap_sample(char *sample_name)
{
	struct file *pfile = NULL;
	char filepath[128];
	char databuf[1024];
	char tmpbuf[16];
	loff_t pos;
	off_t fwritelen = 0;
	mm_segment_t old_fs;
	int minvalue=0, maxvalue=0, avgvalue=0, i=0;

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s%s", FOCAL_SCAP_SAMPLE_DATA_FILEPATH, sample_name);
	if (NULL == pfile)
		pfile = filp_open(filepath, O_CREAT | O_RDWR, 0);
	if (IS_ERR(pfile)) {
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	fwritelen = sprintf(databuf, "Mode:,SCAP, Channels:, %d, Keys:, %d ,"\
						"Ci test:, 1, Delta Ci test:, %d, RawData Test:, 1, K1 Differ Test:, 0, \r\n\r\n",
						ChannelNum, KeyNum, g_testparam.CheckDeltaCiTest);
	vfs_write(pfile, databuf, fwritelen, &pos);
	/*write into file*/
	GetCiRawDataMinMaxAvg(&minvalue, &maxvalue, &avgvalue, cidata);
	fwritelen = sprintf(databuf, "Max Ci Value:, %d, Min Ci Value:, %d, Average Value:, %d,",
						minvalue, maxvalue, avgvalue);
	vfs_write(pfile, databuf, fwritelen, &pos);
	GetCiRawDataMinMaxAvg(&minvalue, &maxvalue, &avgvalue, rawdata);
	fwritelen = sprintf(databuf, "Max Raw Data:, %d, Min Raw Data:, %d, Average Value:, %d,\r\n",
						minvalue, maxvalue, avgvalue);
	vfs_write(pfile, databuf, fwritelen, &pos);
	
	memset(databuf, 0, sizeof(databuf));
	fwritelen = GetSortInfo(databuf);
	vfs_write(pfile, databuf, fwritelen, &pos);
	
	fwritelen = sprintf(databuf, "\r\n\r\n\r\n\r\n\r\n\r\n");
	vfs_write(pfile, databuf, fwritelen, &pos);
	
	fwritelen = 0;
	memset(databuf, 0, sizeof(databuf));
	for(i=0; i<AllChannelNum; i++) {
		fwritelen += sprintf(tmpbuf, "%d,", cidata[i]);
		strcat(databuf, tmpbuf);
		if((i+1)*2 == ChannelNum || (i+1)==ChannelNum) {
			strcat(databuf, "\r\n");
			fwritelen += 2;
		}
	}
	if(0 == KeyNum){
		strcat(databuf, "\r\n");
		fwritelen += 2;
	}
	vfs_write(pfile, databuf, fwritelen, &pos);
	
	fwritelen = 0;
	memset(databuf, 0, sizeof(databuf));
	for(i=0; i<AllChannelNum; i++) {
		fwritelen += sprintf(tmpbuf, "%d,", deltaCiData[i]);
		strcat(databuf, tmpbuf);
		if((i+1)*2 == ChannelNum || (i+1)==ChannelNum) {
			strcat(databuf, "\r\n");
			fwritelen += 2;
		}
	}
	if(0 == KeyNum){
		strcat(databuf, "\r\n");
		fwritelen += 2;
	}
	vfs_write(pfile, databuf, fwritelen, &pos);

	fwritelen = 0;
	memset(databuf, 0, sizeof(databuf));
	for(i=0; i<AllChannelNum; i++) {
		fwritelen += sprintf(tmpbuf, "%d,", rawdata[i]);
		strcat(databuf, tmpbuf);
		if((i+1)*2 == ChannelNum || (i+1)==ChannelNum) {
			strcat(databuf, "\r\n");
			fwritelen += 2;
		}
	}
	if(0 == KeyNum){
		strcat(databuf, "\r\n");
		fwritelen += 2;
	}
	vfs_write(pfile, databuf, fwritelen, &pos);

	fwritelen = 0;
	memset(databuf, 0, sizeof(databuf));
	for(i=0; i<AllChannelNum; i++) {
		fwritelen += sprintf(tmpbuf, "%d,", deltaCiDataDiffer[i]);
		strcat(databuf, tmpbuf);
		if((i+1)*2 == ChannelNum || (i+1)==ChannelNum) {
			strcat(databuf, "\r\n");
			fwritelen += 2;
		}
	}
	if(0 == KeyNum){
		strcat(databuf, "\r\n");
		fwritelen += 2;
	}
	vfs_write(pfile, databuf, fwritelen, &pos);
	
	filp_close(pfile, NULL);
	set_fs(old_fs);

	return 0;
}
#else
int focal_save_scap_sample(char *samplebuf)
{
	char databuf[512];
	char tmpbuf[16];
	int datalen = 0, fwritelen = 0;
	int minvalue=0, maxvalue=0, avgvalue=0, i=0;

	memset(databuf, 0, sizeof(databuf));
	memset(tmpbuf, 0,sizeof(tmpbuf));
	
	fwritelen = sprintf(databuf, "Mode:,SCAP, Channels:, %d, Keys:, %d ,"\
						"Ci test:, 1, Delta Ci test:, %d, RawData Test:, 1, K1 Differ Test:, 0, \r\n\r\n",
						ChannelNum, KeyNum, g_testparam.CheckDeltaCiTest);
	strcat(samplebuf, databuf);
	datalen += fwritelen;
	/*write into file*/
	GetCiRawDataMinMaxAvg(&minvalue, &maxvalue, &avgvalue, cidata);
	fwritelen = sprintf(databuf, "Max Ci Value:, %d, Min Ci Value:, %d, Average Value:, %d,",
						maxvalue, minvalue, avgvalue);
	strcat(samplebuf, databuf);
	datalen += fwritelen;
	
	GetCiRawDataMinMaxAvg(&minvalue, &maxvalue, &avgvalue, rawdata);
	fwritelen = sprintf(databuf, "Max Raw Data:, %d, Min Raw Data:, %d, Average Value:, %d,\r\n",
						maxvalue, minvalue, avgvalue);
	strcat(samplebuf, databuf);
	datalen += fwritelen;
	
	memset(databuf, 0, sizeof(databuf));
	fwritelen = GetSortInfo(databuf);
	strcat(samplebuf, databuf);
	datalen += fwritelen;
	
	fwritelen = sprintf(databuf, "\r\n\r\n\r\n\r\n\r\n\r\n");
	strcat(samplebuf, databuf);
	datalen += fwritelen;
	
	fwritelen = 0;
	memset(databuf, 0, sizeof(databuf));
	for(i=0; i<AllChannelNum; i++) {
		fwritelen += sprintf(tmpbuf, "%d,", cidata[i]);
		strcat(databuf, tmpbuf);
		if((i+1)*2 == ChannelNum || (i+1)==ChannelNum) {
			strcat(databuf, "\r\n");
			fwritelen += 2;
		}
	}
	strcat(databuf, "\r\n");
	fwritelen += 2;
	
	strcat(samplebuf, databuf);
	datalen += fwritelen;
	
	fwritelen = 0;
	memset(databuf, 0, sizeof(databuf));
	for(i=0; i<AllChannelNum; i++) {
		fwritelen += sprintf(tmpbuf, "%d,", deltaCiData[i]);
		strcat(databuf, tmpbuf);
		if((i+1)*2 == ChannelNum || (i+1)==ChannelNum) {
			strcat(databuf, "\r\n");
			fwritelen += 2;
		}
	}
	strcat(databuf, "\r\n");
	fwritelen += 2;

	strcat(samplebuf, databuf);
	datalen += fwritelen;

	fwritelen = 0;
	memset(databuf, 0, sizeof(databuf));
	for(i=0; i<AllChannelNum; i++) {
		fwritelen += sprintf(tmpbuf, "%d,", rawdata[i]);
		strcat(databuf, tmpbuf);
		if((i+1)*2 == ChannelNum || (i+1)==ChannelNum) {
			strcat(databuf, "\r\n");
			fwritelen += 2;
		}
	}
	strcat(databuf, "\r\n");
	fwritelen += 2;
	
	strcat(samplebuf, databuf);
	datalen += fwritelen;

	fwritelen = 0;
	memset(databuf, 0, sizeof(databuf));
	for(i=0; i<AllChannelNum; i++) {
		fwritelen += sprintf(tmpbuf, "%d,", deltaCiDataDiffer[i]);
		strcat(databuf, tmpbuf);
		if((i+1)*2 == ChannelNum || (i+1)==ChannelNum) {
			strcat(databuf, "\r\n");
			fwritelen += 2;
		}
	}
	strcat(databuf, "\r\n\r\n");
	fwritelen += 4;
	
	strcat(samplebuf, databuf);
	datalen += fwritelen;

	return datalen;
}
#endif
