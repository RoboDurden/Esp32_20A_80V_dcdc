
  // mppt with the 20A 80V LT8705 step-up-down module and an ESP32 S2 Mini :-)
#define SERIALDEBUG Serial  // uncomment to disable Serial 115200 baud debug output

#include "include.h"

void setup() 
{
  pinMode(PIN_Uin,INPUT);
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER)
  {
    float fUin    = mapf(analogRead(PIN_Uin),   0.0, 5957.0 ,  0.0, 40.00 ,false);
    if (fUin < 5.0)
    {
      esp_sleep_enable_timer_wakeup(5 * uS_TO_S_FACTOR);
      OUT2N("Going to sleep now, U_in",oAdcLatest.fUin)
      esp_deep_sleep_start();
    }
  }

  
  #ifdef SERIALDEBUG
    SERIALDEBUG.begin(115200);
  #endif
  OUTN("mppt with the 20A 80V LT8705 step-up-down module and an ESP32 S2 Mini :-)");

  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(PIN_Uout,INPUT);
  pinMode(PIN_Iout,INPUT);

  //pinMode(PIN_DACin,INPUT);   // to not set an output hi or low level but analogOut
  
  dacDisable(PIN_DACin);
  //dacDisable(PIN_DACout); 

  _DacOutDisable(); // pinMode(PIN_DACout,INPUT);
  ledcSetup(CHANNEL_DacOut, 10000, 8);  // configure LED PWM functionalitites
  ledcAttachPin(PIN_DACout, CHANNEL_DacOut);  // attach the channel to the GPIO to be controlled


  pinMode(pin_Left, INPUT_PULLUP);
  pinMode(pin_Right, INPUT_PULLUP);
  pinMode(pin_Push, INPUT_PULLUP);

 
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  EEPROM.begin(512);
// load EEPROM data into RAM, see it
  EEPROM.get(0,oData);
  OUT2T("EEPROM version",oData.iVersion)  OUT2N("sSSID",oData.sSSID) 
  if (oData.iVersion != EEPROM_VERSION)
  {
    Data oNew;
    oData = oNew;
    EEPROM.put(0,oData);
    EEPROM.commit(); 
  }

  oAdcLogMax.fUin = 1.1 * oData.fUin;
  oAdcLogMax.fUout = 1.1 * oData.fUout;
  oAdcLogMax.fIout = 1;

  display.init();
  display.flipScreenVertically();

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Hello world");
  display.setFont(Dialog_plain_32); // ArialMT_Plain_10
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(60, 32,"Robo :-)" );
  display.display();

}

void PrintFloat(int x, int y, float f, int iDecimals=1, String sUnit="", int iMinWidth=3)
{
  char s[6];     //result string 5 positions + \0 at the end
  dtostrf(f, iMinWidth, iDecimals, s );
  if (sUnit == "")
    display.drawString(x, y,s );
  else
  {
    sUnit = String(s)+sUnit;
    display.drawString(x, y,sUnit );
  }
}


void ReadInput()
{
  wButtons = 0;
  if (Serial.available())
  {
    String s = Serial.readStringUntil('\n');
    OUT2T("serial input",s)
    char c = s.charAt(0);
    s = s.substring(1);
    //char c = Serial.read();
    switch(c)
    {
    case 'a': wButtons = _ButtonDown; break;
    case 's': wButtons = _ButtonPush; break;
    case 'd': wButtons = _ButtonUp; break;
    case 'i': if (s.length()){oData.fUin = s.toFloat();} OUT2N("oData.fUin",oData.fUin) SettingSave(); return;
    case 'o': if (s.length()){oData.fUout = s.toFloat();} OUT2N("oData.fUout",oData.fUout) SettingSave(); return;
    case 'm': if (s.length())
      {
        c = s.charAt(0);
        switch(c)
        {
        case '0': ControlInit(CONTROL_NONE); break;
        case 'o': ControlInit(CONTROL_Uout); break;
        case 'i': ControlInit(CONTROL_Uin); break;
        case 'm': ControlInit(CONTROL_mppt); break;
        case 'n': ControlInit(CONTROL_mppt2); break;
        }
      }
      OUT2N("oData.iControl",oData.iControl) 
      SettingSave(); 
      return;
    default: OUT2N("no case for",c) return;
    }
    OUT2N("wButtons",wButtons)
  }
  else if (iNow < iTimeRead)  return;

  if (!digitalRead(pin_Left)) wButtons |= _ButtonDown;
  if (!digitalRead(pin_Right)) wButtons |= _ButtonUp;
  if (!digitalRead(pin_Push)) wButtons |= _ButtonPush;
  //OUT2N("wButtons",wButtons);

  if (g_sMess != "")
  {
    if (wButtons)
    {
      g_sMess = "";
      iTimeRead = iNow + 1000;
    }
    return;
  }
  
  if (wButtons & _ButtonPush)
  {
    iModeAdd++;
    if (iModeAdd > ADD_Wifi) iModeAdd = ADD_Control;
    iTimeAdd = iNow + 5000;
    iTimeRead = iNow + 500;
    iModeDisplay = DISPLAY_NUMBERS;
    return;
  }
  else if (iNow > iTimeAdd)
  {
    if (iModeAdd > 0)
    {
      OUT2N("EEPROM saved. fUin",oData.fUin)
      SettingSave();
    }
    iModeAdd = 0;
    if (wButtons & _ButtonUp)
    {
      if (iModeDisplay < DISPLAY_LOG_4) iModeDisplay++;
      iTimeRead = iNow + 500;
    }
    if (wButtons & _ButtonDown)
    {
      if (iModeDisplay > DISPLAY_NUMBERS) iModeDisplay--;
      iTimeRead = iNow + 500;
    }
    return;
  }
  
  int iAdd = 0;
  if (wButtons & _ButtonDown)
  {
     OUT("\tdown")
    iAdd = -1;
  }
  if (wButtons & _ButtonUp)
  {
     OUT("\tup")
     iAdd = +1;
  }

  if (iAdd!=0)
  {
    switch (iModeAdd)
    {
    case ADD_Control:
      oData.iControl += iAdd;
      CLAMP(oData.iControl,CONTROL_NONE,CONTROL_mppt2)
      ControlInit(oData.iControl);
      break;
    case ADD_Uin:
      oData.fUin += (float)iAdd/10.0;
      if (oData.fUin > MAX_V)  oData.fUin = MAX_V;
      if (oData.fUin < MIN_V)  oData.fUin = MIN_V;
      OUT2N(" change fUin to ",oData.fUin)
      break;
    case ADD_Uout:
      oData.fUout += (float)iAdd/10.0;
      if (oData.fUout > MAX_V)  oData.fUout = MAX_V;
      if (oData.fUout < MIN_V)  oData.fUout = MIN_V;
      OUT2N(" change fUout to ",oData.fUout)
      break;
    case ADD_Wifi:
     //oData.wSetting ^= 1UL << 1;
     //oData.wSetting ^= SETTING_WIFI;
     oData.wSetting = 0;
     if (iAdd>0)
     {
       oData.wSetting |= SETTING_WIFI;
      if (bWifiServer = SetupWifi())  display.println("no Wifi !");
     }
     else
     {
       oData.wSetting &= ~SETTING_WIFI;
       //WiFi.mode(WIFI_OFF);
     }
      OUT2N(" new wSetting",oData.wSetting)
      break;
    }
    iTimeAdd = iNow + 5000;
    iTimeDisplay = 0;
    iTimeRead = iNow + 250;
  }
}


void Display()
{
  if (iNow < iTimeDisplay)  return;
  iTimeDisplay = iNow + TIME_Display;

  
  display.clear();
  display.setColor(WHITE);

  display.setFont(ArialMT_Plain_16); // Dialog_plain_32 ArialMT_Plain_10
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  PrintFloat(40,0,oAdc.fUin,1,"V");
  PrintFloat(82,0,oAdc.fIout,1,"A");
  PrintFloat(128,0,oAdc.fUout,1,"V");

  if (g_sMess != "")
  {
    display.setFont(ArialMT_Plain_10); // Dialog_plain_32 ArialMT_Plain_10
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawStringMaxWidth(0, 16, 128,g_sMess);
    display.display();
    return;
  }
  
  display.setFont(ArialMT_Plain_10); // Dialog_plain_32 ArialMT_Plain_10
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  if (oData.iControl)
  {
    display.drawString(64, 35,String(oDac.iIn) );
    display.drawString(64, 45,String(oDac.iOut) );
  }
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  
  switch (iModeDisplay)
  {
  case DISPLAY_NUMBERS:
    display.setFont(ArialMT_Plain_24); // Dialog_plain_32 ArialMT_Plain_10
    display.drawString(64, 40,String(iNow%3000<1000 ? oAdc.fUin : oAdc.fUout) );
    display.drawString(128, 40,String(oAdc.fIout) );
    switch (iModeAdd)
    {
    case ADD_Control:
    {
      //display.drawLine(0,39,60,39);
      display.drawString(60, 16,"mode" );
      String s;
      switch(oData.iControl)
      {
        case CONTROL_NONE: s = "none";  break;
        case CONTROL_Uout: s = "U_out";  break;
        case CONTROL_Uin: s = "U_in";  break;
        case CONTROL_mppt: s = "mppt";  break;
        case CONTROL_mppt2: s = "mppt2";  break;
      }
      display.drawString(128, 16,s );
    }
      break;
    case ADD_Uin:
      //display.drawLine(0,39,60,39);
      display.drawString(60, 16,"U_in" );
      display.drawString(128, 16,String(oData.fUin) );
      break;
    case ADD_Uout:
      //display.drawLine(0,39,60,39);
      display.drawString(60, 16,"U_out" );
      display.drawString(128, 16,String(oData.fUout) );
      break;
    case ADD_Wifi:
      display.drawString(60, 16,"wifi" );
      display.drawString(128, 16, oData.wSetting&SETTING_WIFI == SETTING_WIFI ? "yes" : "no" );
      break;
     default:
      display.drawString(60, 16,String(oData.fUin) );
      display.drawString(128, 16,String(oData.fUout) );
    }
    break;
  case DISPLAY_LOG_1:
  case DISPLAY_LOG_2:
  case DISPLAY_LOG_3:
  case DISPLAY_LOG_4:

    #define GRAPHS 4

    int x0;
    int aY0[GRAPHS];
    float fUmax = oAdcLogMax.fUout > oAdcLogMax.fUin ? oAdcLogMax.fUout : oAdcLogMax.fUin;
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    PrintFloat(0,16,fUmax,0,"V");
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    PrintFloat(128,16,oAdcLogMax.fIout,0,"A");
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    PrintFloat(64,16,fUmax*oAdcLogMax.fIout,0,"W");
    
    for (int i=0; i<ADC_LOG; i++)
    {
      adc_float_t o = aoAdcLog[(iAdcLog+i)%ADC_LOG];

      int x = 128-i;  // 128-
      int aY[GRAPHS];
      aY[0] = 64 - 48 * o.fUout / fUmax;
      aY[1] = 64 - 48 * o.fUin / fUmax;
      aY[2] = 64 - 48 * o.fIout / oAdcLogMax.fIout;
      aY[3] = 64 - 48 * (o.fUout*o.fIout) / (fUmax*oAdcLogMax.fIout);

      if (i)
      {
        for (int j=0; j<GRAPHS; j++)
        {
          display.drawLine(x0,aY0[j],x,aY[j]);
        }
      }
      x0 = x;
     for (int j=0; j<GRAPHS; j++)  aY0[j] = aY[j];
    }
    
  }
  display.display();
}


void ControlInit(byte iControlNew)
{
  oData.iControl = iControlNew;
  OUT2N(" change iControl to ",oData.iControl)
  switch(oData.iControl)
  {
    case CONTROL_NONE: break;
    case CONTROL_Uout: break;
    case CONTROL_Uin: break;
    case CONTROL_mppt: oDac.iIn = oMpp.iDacPMax;  break;
    case CONTROL_mppt2: oDac.iIn = oMpp.iDacPMax;  break;
  }
}

boolean _MpptCheckOut(unsigned long iMillisNext)
{
  //_DacOutDisable();
  _DacOutWrite(oDac.iOut);
  
  if (oAdcLatest.fUout > oData.fUout)
  {
    oMpp.bOvervoltage = true;
    oMpp.iScanPos = -1; // abort a possible running scan
    oMpp.fPLastProbe = 0; // abort a possibe probe (mppt2)
    
    if (oDac.iOut < DAC_OutMax) oDac.iOut++;
    OUT2T("overvoltage",oAdcLatest.fUout) OUT2T("U_in",oAdcLatest.fUin) OUT2N("dac out",oDac.iOut)
    _DacOutWrite(oDac.iOut);
    return true;
  }

  oDac.iOut -= 10;    // do not limit charging voltage by dac_out without overvoltage 
  CLAMP(oDac.iOut,DAC_OutMin,DAC_OutMax)
  
  if (oMpp.bOvervoltage)
  {
    if (oDac.iOut > DAC_OutMin)
    {
      OUT2T("overvoltage recovery",oAdcLatest.fUout) OUT2T("U_in",oAdcLatest.fUin) OUT2N("dac out",oDac.iOut)
      _DacOutWrite(oDac.iOut);
      return true;
    }
    oMpp.bOvervoltage = false;
    oMpp.iTimeNextScan = iNow + iMillisNext;  // might only be a cloud
  }
  return false;
}

boolean _MpptCheckNextScan()
{   
  if (iNow < oMpp.iTimeNextScan)
    return true;

  // try new mpp scan
  adc_float_t o = aoAdcLog[iAdcLog];
  float fPMin = o.fUout * o.fIout;
  float fPMax = fPMin;
  int iTest = MPPT_WaitStablePV / TIME_ADC;
  for (int i=0; i<iTest; i++)
  {
    adc_float_t o = aoAdcLog[(iAdcLog+i)%ADC_LOG];
    float fP = o.fUout * o.fIout;
    if (fPMin > fP) fPMin = fP;
    else if (fPMax < fP) fPMax = fP;
  }
  if (fPMax - fPMin > (0.1 * oAdcLogMax.fUout *oAdcLogMax.fIout)  ) // only start new mpp scan when power has not changed for more then 10% of max power
  {
    OUT2N("solar power not stable for mpp scan",fPMax - fPMin)
    return true;
  }
  if (fPMax  < (0.03 * oAdcLogMax.fUout *oAdcLogMax.fIout)  ) // no new mpp scan when max power was less then 3% of overall max power
  {
    OUT2N("solar power to small for mpp scan",fPMax)
    return true;
  }
  return false;
}

void Control()
{
    oAdcLatest = aoAdcLog[iAdcLog]; // lated adc
    float fP = oAdcLatest.fUout * oAdcLatest.fIout;

    if (oAdcLatest.fUin < 5.0)
    {
      display.clear();
      display.display();
      esp_sleep_enable_timer_wakeup(5 * uS_TO_S_FACTOR);
      OUT2N("Going to sleep now, U_in",oAdcLatest.fUin)
      delay(1000);
      Serial.flush(); 
      esp_deep_sleep_start();
    }
    
    switch(oData.iControl)
    {
    case CONTROL_NONE:
      _DacOutDisable();
      dacDisable(PIN_DACin);
      break;

    case CONTROL_Uout:
    {
      //if (++oDac.iOut > DAC_OutMax) oDac.iOut = DAC_OutMin;
      if (oDac.iIn < DAC_InMax) oDac.iIn++;    // do not limit input voltage 

      float fUDiff = oAdcLatest.fUout - oData.fUout;
      oDac.iOut += fUDiff / 4 + (fUDiff > 0 ? 1 : -1);
      //oDac.iOut += (oAdcLatest.fUout - oData.fUout) / 2;
      CLAMP(oDac.iOut,DAC_OutMin,DAC_OutMax)

      OUT_Log("CONTROL_Uout")
      dacWrite(PIN_DACin,oDac.iIn);
      _DacOutWrite(oDac.iOut);
      break;
    }  
    case CONTROL_Uin:
    {
      oDac.iOut -= 10;    // do not limit charging voltage by dac_out without overvoltage 
      CLAMP(oDac.iOut,DAC_OutMin,DAC_OutMax)
      float fUDiff = oAdcLatest.fUin - oData.fUin;
      oDac.iIn += fUDiff / 4 + (fUDiff > 0 ? 1 : -1);
      CLAMP(oDac.iIn,DAC_InMin,DAC_InMax)
      dacWrite(PIN_DACin,oDac.iIn);
      _DacOutWrite(oDac.iOut);
      break;
    }  
    case CONTROL_mppt2:
    {
      if (_MpptCheckOut(MPPT_ProbeMillis))
        break;
      if (oMpp.fPLastProbe)
      {
        if (fP > oMpp.fPLastProbe)
        {
          oDac.iIn = oMpp.iDacPMax = oMpp.iScanPos;
        }
        else
        {
          oMpp.iProbeDirection *= -1;
        }
        oMpp.fPLastProbe = 0;
        oMpp.iTimeNextScan = iNow + MPPT_ProbeMillis;
        break;
      }

      if (_MpptCheckNextScan())
        break;

      //OUT_Log(oMpp.iProbeDirection)
      oMpp.iScanPos = oDac.iIn + oMpp.iProbeDirection;
      dacWrite(PIN_DACin,oMpp.iScanPos);
      oMpp.fPLastProbe = fP;
      break;
    }
    case CONTROL_mppt:
    {
      if (_MpptCheckOut(MPPT_ScanSeconds * 1000))
        break;
        
      if (oMpp.iScanPos >= 0)  // doing mpp scan
      {
        OUT2N(oMpp.iScanPos,fP)
        if (oMpp.fPMax < fP) 
        {
          oMpp.fPMax = fP;
          oMpp.iDacPMax = oMpp.iScanPos;
        }
        if (oMpp.iScanPos < oDac.iIn)
        {
          if (oMpp.fPMin > fP) 
          {
            oMpp.fPMin = fP;
            oMpp.iDacPMin = oMpp.iScanPos;
          }
          
        }
        oMpp.iScanPos++;
        if (  (oMpp.iScanPos < DAC_InMax) && (oMpp.iScanPos-oMpp.iDacPMax < 10) )    // if no new mpp found there will be no new anyway
        {
          dacWrite(PIN_DACin,oMpp.iScanPos);
          break;
        }
        
        // scan finished
        oMpp.iScanPos = -1;
        
        OUT2T("max power",oMpp.fPMax) OUT2N("at dac",oMpp.iDacPMax)

        //oDac.iIn = oMpp.iDacPMax;
        oDac.iIn = (50 * oDac.iIn + 50 * oMpp.iDacPMax)/100;  // low pass
        OUT2T("new iIn",oDac.iIn)

        OUT2N(oMpp.fPMax - oMpp.fPMin,0.1 * oMpp.fPMax)
        if (oMpp.fPMax - oMpp.fPMin < 0.1 * oMpp.fPMax) // if power drop to low at begin of scan, do start earlier
        {
          if (oDac.iIn+oMpp.iScanBegin > DAC_InMin ) oMpp.iScanBegin--;
        }
        else
        {
          if (oMpp.iScanBegin < -5) oMpp.iScanBegin++;
        }
        OUT2T("min left power",oMpp.fPMin) OUT2N("iScanBegin",oMpp.iScanBegin)
        
        dacWrite(PIN_DACin,oDac.iIn);
        oMpp.iTimeNextScan = iNow + MPPT_ScanSeconds * 1000;
        break;
      }

      dacWrite(PIN_DACin,oDac.iIn);

      if (_MpptCheckNextScan())
        break;
        
      oMpp.iScanPos = oDac.iIn + oMpp.iScanBegin;
      if (oMpp.iScanPos < DAC_InMin) oMpp.iScanPos = DAC_InMin;

      OUT2T(oMpp.iScanPos,DAC_InMax) 
      OUT_Log("new mpp scan")
      
      oMpp.fPMax = 0;
      oMpp.fPMin = 9999;
      
      dacWrite(PIN_DACin,oMpp.iScanPos);
      break;
    }
            
    }
}

boolean ReadAnalog()
{
  oAdcSum.iUin += analogRead(PIN_Uin);
  oAdcSum.iUout += analogRead(PIN_Uout);
  oAdcSum.iIout += analogRead(PIN_Iout);
  iAdcSum++;

  if (iNow < iTimeAdc)
    return false;
  iTimeAdc = iNow + TIME_ADC;

  oAdc.fUin = (float)oAdcSum.iUin / iAdcSum;
  oAdc.fUout = (float)oAdcSum.iUout / iAdcSum;
  oAdc.fIout = (float)oAdcSum.iIout / iAdcSum;

  //OUT2T(analogRead(PIN_Uin),analogRead(PIN_Uout))
  oAdc.fUin    = mapf(oAdc.fUin,   0.0, 5957.0 ,  0.0, 40.00 ,false);
  oAdc.fUout    = mapf(oAdc.fUout, 0.0, 3964.5 ,  0.0, 26.92 ,false);
  if (oAdc.fIout > oAdcSum.fOffsetI)
    oAdc.fIout -= oAdcSum.fOffsetI;
  else
  {
    oAdcSum.fOffsetI = oAdc.fIout;
    oAdc.fIout = 0;
    OUT2N("new fOffsetI",oAdcSum.fOffsetI)
  }
  
  //OUT2T("-",oAdc.fIout)
  oAdc.fIout    = mapf(oAdc.fIout,    0.0,1720.0,  0.0,4.0,false);

  //OUT2T("iUin",oAdcSum.iUin/iAdcSum)OUT2T("=",oAdc.fUin) OUT2T("iUout",oAdcSum.iUout/iAdcSum) OUT2T("=",oAdc.fUout) OUT2T("fIout",oAdc.fIout) OUT2T("=",oAdc.fIout) OUTN(iAdcSum);
  
  iAdcSum = oAdcSum.iUin = oAdcSum.iUout = oAdcSum.iIout = 0;

  iAdcLog = (iAdcLog +ADC_LOG - 1) % ADC_LOG;
  memcpy(&aoAdcLog[iAdcLog],&oAdc,sizeof(oAdc));

  if (oAdcLogMax.fUin < oAdc.fUin)  oAdcLogMax.fUin = 1 + (int)oAdc.fUin;
  if (oAdcLogMax.fUout < oAdc.fUout)  oAdcLogMax.fUout = 1 + (int)oAdc.fUout;
  if (oAdcLogMax.fIout < oAdc.fIout)  oAdcLogMax.fIout = 1 + (int)oAdc.fIout;
  return true;
}

unsigned long iTimeBlink = 0;
boolean bBlink = false;

// the loop function runs over and over again forever
void loop() 
{
  iNow = millis();

  //digitalWrite(LED_BUILTIN, iNow%1000 < 200);
  digitalWrite(LED_BUILTIN, bBlink);
  if (iNow > iTimeBlink)
  {
    iTimeBlink = iNow + 4*oDac.iIn-DAC_OutMin+50;
    bBlink = !bBlink;
  }

 
  ReadInput();

  if (ReadAnalog())
  {
    Control();
  }
    
  Display();
  
}
