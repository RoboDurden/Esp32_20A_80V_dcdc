//#include <driver/dac.h>

#include <EEPROM.h>
#include "net.h"

#define uS_TO_S_FACTOR 1000000  

unsigned long iNow = 0;

unsigned long iTimeDisplay = 0;
#define TIME_Display 200
unsigned long iTimeRead = 0;
#define TIME_ADC 50
unsigned long iTimeAdd = 0;


#define MAX_V 80.0
#define MIN_V 1.0

#define CONTROL_COUNT 5
#define CONTROL_NONE 0
#define CONTROL_Uout 1
#define CONTROL_Uin 2
#define CONTROL_mppt 3
#define CONTROL_mppt2 4


#define ADD_Control 1
#define ADD_Uin 2
#define ADD_Uout 3
#define ADD_Wifi 4
int iModeAdd = 0;

int iLogZoom = 1;

#define DISPLAY_NUMBERS 0
#define DISPLAY_LOG_1 1
#define DISPLAY_LOG_2 2
#define DISPLAY_LOG_3 3
#define DISPLAY_LOG_4 4

int iModeDisplay = DISPLAY_LOG_1; //DISPLAY_NUMBERS;

#define SETTING_WIFI 1  // wifi on or off

#define EEPROM_VERSION 1
typedef struct {
    uint32_t iVersion = EEPROM_VERSION;
    uint16_t  wSetting = 0; 
    char sSSID[31] = "";
    char sPwd[31] = "";
    char sApiKey[20] = "";
    float fUin = 30.0;
    float fUout = 29.4;
    uint8_t iControl = 0;
  } Data;
Data oData;

void SettingSave()
{
  EEPROM.put(0,oData);
  EEPROM.commit(); 
}

bool bWifiServer = false;


#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`
// custom fonts: http://oleddisplay.squix.ch
#include "Dialog_plain_32.h"

// Initialize the OLED display using Wire library
SSD1306Wire  display(0x3c, 13, 14); // I2C_Scanner: 0x3C

#define pin_Right 9
#define pin_Push 11
#define pin_Left 12

#define _ButtonDown 1
#define _ButtonUp 2
#define _ButtonPush 4

byte wButtons;

#define PIN_Uin 5
#define PIN_Uout 3
#define PIN_Iout 2


#define DAC_InMin 0
#define DAC_InMax 255
#define DAC_OutMin 0
#define DAC_OutMax 255

#define PIN_DACin 17
#define PIN_DACout 18
#define CHANNEL_DacOut 0

void _DacOutWrite(byte iOut)
{
  ledcWrite(CHANNEL_DacOut, iOut);
  //pinMode(PIN_DACout,OUTPUT);
}
void _DacOutDisable()
{
  pinMode(PIN_DACout,INPUT);
}

typedef struct
{
  boolean bIn = false;
  int iIn = 95;
  boolean bOut = true;
  int iOut = 95;
} dac_t;
dac_t oDac;

unsigned long iTimeAdc = 0;


typedef struct
{
  unsigned int iUin;
  unsigned int iUout;
  unsigned int iIout;
  float fOffsetI = 125;
} adc_analog_t;

adc_analog_t oAdcSum;
int iAdcSum = 0;

typedef struct
{
  float fUin;
  float fUout;
  float fIout;
  
} adc_float_t;

adc_float_t oAdc;
adc_float_t oAdcLatest;

#define ADC_LOG 128
adc_float_t aoAdcLog[ADC_LOG];
int iAdcLog = 0;
adc_float_t oAdcLogMax;



#define MPPT_ScanOffset 64
#define MPPT_ScanSize 128
#define MPPT_ScanSeconds 5 // 
#define MPPT_ProbeMillis 500
#define MPPT_WaitStablePV 1000 // millisesonds to have a stable pv input after iTimeNextScan that a next scan is initiated
typedef struct
{
  // byte iPosDo = 95;   // 95 = 1.2V = no influence on hardware ..
  int iScanWindow = MPPT_ScanSize;
  int iScanBegin = -10;
  int iScanPos = -1;  // no scan 
  float fPMax = 0;
  int iDacPMax = 0;
  float fPMin = 0;
  int iDacPMin = 0;
  boolean bOvervoltage = false;
  unsigned long iTimeNextScan = 0;
  float fPLastProbe = 0;
  int iProbeDirection = 1;
  
} mppt_t;

mppt_t oMpp;

String g_sMess = "";


#define CLAMP(v,vMin,vMax)  if (v > vMax)  v = vMax;  if (v < vMin)  v = vMin;

double mapf(double val, double in_min, double in_max, double out_min, double out_max, boolean bClamp=false) 
{
  double f = (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  if (bClamp)
  {
    if (f > out_max)  f = out_max;
    else if (f < out_min) f = out_min;
  }
  return f;
}

#ifdef SERIALDEBUG
  #define DEBUG(code) {code}
  #define OUT(s)  {SERIALDEBUG.print(s);}
  #define OUT2(s,i) {SERIALDEBUG.print(s);SERIALDEBUG.print(": ");SERIALDEBUG.print(i);}
  #define OUT2T(s,i)  {SERIALDEBUG.print(s);SERIALDEBUG.print(": ");SERIALDEBUG.print(i);SERIALDEBUG.print("  ");}
  #define OUT2N(s,i)  {SERIALDEBUG.print(s);SERIALDEBUG.print(": ");SERIALDEBUG.print(i);SERIALDEBUG.println();}
  #define OUTN(s) {SERIALDEBUG.println(s);}
  #define OUTI4(i){if (i<10) SERIALDEBUG.print("   ");  else if (i<100) SERIALDEBUG.print("  ");  else if (i<1000)  SERIALDEBUG.print(" "); SERIALDEBUG.print(i);}
#else
  #define DEBUG(code)
  #define OUT(s)
  #define OUT2(s,i)
  #define OUT2T(s,i)
  #define OUT2N(s,i)
  #define OUTN(s)
  #define OUTI4(i)
#endif

#define OUT_Log(s) {  OUT(s) OUT2T("\tP",oAdcLatest.fUout * oAdcLatest.fIout) OUT2T("fUout",oData.fUout) OUT2T("U_in",oAdcLatest.fUin) OUT2T("U_out",oAdcLatest.fUout) OUT2T("dac in",oDac.iIn) OUT2N("dac out",oDac.iOut)  }
