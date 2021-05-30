/*
BMW i3 LIM  Controller Software. Alpha version.
Version 2 with extra commands to the LIM
0x3E9 is the most important message to send to the LIM!

Copyright 2021 
Damien Maguire

First attempt at DC!

//////////////////////////////////////////////////////////////
Signal that seem to be required for AC :
0x112 - BMS
0x12f - Wake up KL15/KL30
0x2FC
0x3E9 - LIM command
0x431 - possible battery status message?
0x560

//////////////////////////////////////////////////////////////
Signal that seem to be required for DC CCS :
0x3c
0x112 - BMS-this will need to reflect hv batt volts and amps
0x12f - Wake up KL15/KL30
0x2A0
0x2F1 - Lim command used only for DC.contains charge voltage limit and countdown timer to the desired SOC%
0x2FA - Lim command used only for DC.Seems to seed a state request to the FC. 0-9. also a countdown to departure timer.
Startup seq = 0 - 1 - 9(send chg pwr in 0x3e9 amd readiness=1) -2 -3 (send fc current command in 0x3e9).
0x2FC
0x328 - DLC6 seems to be 1 sec interval and counting up every interval. Time of day / date? Used by LIM for charge time perhaps?
0x330
0x397
0x3A0
0x3E8
0x3E9 - LIM command.The big guy.
0x3F9
0x431 - possible battery status message?
0x432 - another voltage and possible SOC in this msg.
0x510
0x512
0x51A
0x540
0x560
/////////////////////////////////////////////////////////////
*/
#include <Metro.h>
#include <due_can.h>  
#include <due_wire.h> 
#include <DueTimer.h>  
#include <Wire_EEPROM.h> 



#define SerialDEBUG SerialUSB
 template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; } //Allow streaming
 

#define OUT1 48
#define OUT2 49
#define IN1 6
#define IN2 7
#define led 13

Metro timer_diag = Metro(1100);
Metro timer_Frames600 = Metro(600);
Metro timer_Frames200 = Metro(200);
Metro timer_Frames10 = Metro(10);
Metro timer_Frames100 = Metro(100);


bool CHGState=false;
bool CHGreq=false;
bool PP=false;
bool Auto=false;
byte ACcur=0;
byte CABlim=0;
byte Pilot=0;
uint16_t Batt_Wh=21000; //battery energy capacity in watt hours. Values taken from 2014 and 2015 i3. Really need a 94ah or 120ah to verify ...
uint16_t CHG_Pwr=0; //calculated charge power. 12 bit value scale x25. Values based on 50kw DC fc and 1kw and 3kw ac logs. From bms???
int16_t FC_Cur=0; //10 bit signed int with the ccs dc current command.scale of 1.
byte  EOC_Time=0x00; //end of charge time in minutes.
byte CHG_Status=0;  //observed values 0 when not charging , 1 and transition to 2 when commanded to charge. only 4 bits used.
                    //seems to control led colour.
byte CHG_Req=0;  //observed values 0 when not charging , 1 when requested to charge. only 1 bit used in logs so far.
byte CHG_Ready=0;  //indicator to the LIM that we are ready to charge. observed values 0 when not charging , 1 when commanded to charge. only 2 bits used.
byte CONT_Ctrl=0;  //4 bits with DC ccs contactor command.


CAN_FRAME outFrame;  //A structured variable according to due_can library for transmitting CAN data.
CAN_FRAME inFrame;    //structure to keep inbound inFrames


#define Status_NotRdy 0x0 //no led
#define Status_Rdy 0x2  //pulsing blue led when on charge. 0x2. 0x1 = 1 red flash then off
#define Req_Charge 0x1
#define Req_EndCharge 0x0
#define Chg_Rdy 0x1
#define Chg_NotRdy 0x0



void setup() 
  {
  Can0.begin(CAN_BPS_500K);   // LIM CAN
  Can1.begin(CAN_BPS_500K);   // External CAN
  Can0.watchFor();
  Can1.watchFor();
  Serial.begin(115200);  //Initialize our USB port which will always be redefined as SerialUSB to use the Native USB port tied directly to the SAM3X processor.
  Serial2.begin(19200); //setup serial 2 for wifi access
  pinMode(OUT1,OUTPUT);
  pinMode(OUT2,OUTPUT);
  pinMode(led,OUTPUT);
  pinMode(IN1,INPUT);
  pinMode(IN2,INPUT);
  
  }

  
void loop()
{ 
checkCAN();
  if(timer_diag.check())
  {
  handle_Wifi();
  }

 if(timer_Frames600.check())
  {
  Msgs600ms();
 }


  if(timer_Frames10.check())
  {
  Msgs10ms();
 }

  if(timer_Frames100.check())
  {
  Msgs100ms();
 }

  if(timer_Frames200.check())
  {
   Msgs200ms();

checkforinput();

}
}


void Msgs10ms()                       //10ms messages here
{
                                  
        outFrame.id = 0x112;    //BMS message required by LIM   
        outFrame.length = 8;            
        outFrame.extended = 0;          
        outFrame.rtr=1;                 
        outFrame.data.bytes[0]=0xf9;  //Battery current LSB. Scale 0.1 offset 819.2. 16 bit unsigned int
        outFrame.data.bytes[1]=0x1f;  //Battery current MSB. Scale 0.1 offset 819.2.  16 bit unsigned int
        outFrame.data.bytes[2]=0x8b;  //Battery voltage LSB. Scale 0.1. 16 bit unsigned int.
        outFrame.data.bytes[3]=0x0e;  //Battery voltage MSB. Scale 0.1. 16 bit unsigned int.
        outFrame.data.bytes[4]=0xa6;  //Battery SOC LSB. 12 bit unsigned int. Scale 0.1. 0-100%
        outFrame.data.bytes[5]=0x71;  //Battery SOC MSB. 12 bit unsigned int. Scale 0.1. 0-100%
        outFrame.data.bytes[6]=0x65;  //Low nibble battery status. Seem to need to be 0x5.
        outFrame.data.bytes[7]=0x5d;  //zwischenkreis. Battery voltage. Scale 4. 8 bit unsigned int. 
        Can0.sendFrame(outFrame); 

      

}
    
void Msgs100ms()                       //100ms messages here
{

        outFrame.id = 0x12f;       //Wake up message.
        outFrame.length = 8;            
        outFrame.extended = 0;          
        outFrame.rtr=1;                 
        outFrame.data.bytes[0]=0xf5;  
        outFrame.data.bytes[1]=0x28; 
        outFrame.data.bytes[2]=0x88;  
        outFrame.data.bytes[3]=0x1d; 
        outFrame.data.bytes[4]=0xf1;
        outFrame.data.bytes[5]=0x35;
        outFrame.data.bytes[6]=0x30;
        outFrame.data.bytes[7]=0x80;  
        Can0.sendFrame(outFrame);  

        outFrame.id = 0x2fc;       //central locking status message.
        outFrame.length = 8;            
        outFrame.extended = 0;          
        outFrame.rtr=1;                 
        outFrame.data.bytes[0]=0x81;  
        outFrame.data.bytes[1]=0x00; 
        outFrame.data.bytes[2]=0x04;  
        outFrame.data.bytes[3]=0xff; 
        outFrame.data.bytes[4]=0xff;
        outFrame.data.bytes[5]=0xff;
        outFrame.data.bytes[6]=0xff;
        outFrame.data.bytes[7]=0xff;  
        Can0.sendFrame(outFrame); 

        outFrame.id = 0x2f1;       //Lim command 2. Used in DC mode
        outFrame.length = 8;            
        outFrame.extended = 0;          
        outFrame.rtr=1;                 
        outFrame.data.bytes[0]=0xA2;  //Charge voltage limit LSB. 14 bit signed int.scale 0.1 0xfa2=4002*.1=400.2Volts
        outFrame.data.bytes[1]=0x0f;  //Charge voltage limit MSB. 14 bit signed int.scale 0.1
        outFrame.data.bytes[2]=0x00;  //Fast charge current limit. Not used in logs from 2014-15 vehicle so far. 8 bit unsigned int. scale 1.so max 254amps in theory...
        outFrame.data.bytes[3]=0x18;  //time remaining in seconds to hit soc target from byte 7 in AC mode. LSB. 16 bit unsigned int. scale 10.
        outFrame.data.bytes[4]=0x1B;  //time remaining in seconds to hit soc target from byte 7 in AC mode. MSB. 16 bit unsigned int. scale 10.
        outFrame.data.bytes[5]=0xFB;  //time remaining in seconds to hit soc target from byte 7 in ccs mode. LSB. 16 bit unsigned int. scale 10.
        outFrame.data.bytes[6]=0x06;  //time remaining in seconds to hit soc target from byte 7 in ccs mode. MSB. 16 bit unsigned int. scale 10.
        outFrame.data.bytes[7]=0xA0;  //Fast charge SOC target. 8 bit unsigned int. scale 0.5. 0xA0=160*0.5=80%
        Can0.sendFrame(outFrame); 

        outFrame.id = 0x2fa;       //Lim command 3. Used in DC mode
        outFrame.length = 8;            
        outFrame.extended = 0;          
        outFrame.rtr=1;                 
        outFrame.data.bytes[0]=0x84; //Time to go in minutes LSB. 16 bit unsigned int. scale 1. May be used for the ccs station display of charge remaining time... 
        outFrame.data.bytes[1]=0x04; //Time to go in minutes MSB. 16 bit unsigned int. scale 1. May be used for the ccs station display of charge remaining time... 
        outFrame.data.bytes[2]=0x00;  //upper nibble seems to be a mode command to the ccs station. 0 when off, 9 when in constant current phase of cycle.
                                      //more investigation needed here...
                                      //Lower nibble seems to be intended for two end charge commands each of 2 bits.
        outFrame.data.bytes[3]=0xff; 
        outFrame.data.bytes[4]=0xff;
        outFrame.data.bytes[5]=0xff;
        outFrame.data.bytes[6]=0xff;
        outFrame.data.bytes[7]=0xff;  
        Can0.sendFrame(outFrame); 


}


void Msgs200ms()                      ////200ms messages here
{
        
        outFrame.id = 0x3E9;   //Main LIM control message    
        outFrame.length = 8;            
        outFrame.extended = 0;          
        outFrame.rtr=1;                 
        outFrame.data.bytes[0]=lowByte(Batt_Wh);  
        outFrame.data.bytes[1]=highByte(Batt_Wh); 
        outFrame.data.bytes[2]=((CHG_Status<<4)|(CHG_Req));  //charge status in bits 4-7.goes to 1 then 2.8 secs later to 2. Plug locking???. Charge request in lower nibble. 1 when charging. 0 when not charging.
        outFrame.data.bytes[3]=((lowByte(CHG_Pwr)<<4)|CHG_Ready); //charge readiness in bits 0 and 1. 1 = ready to charge.upper nibble is LSB of charge power.
        outFrame.data.bytes[4]=lowByte(CHG_Pwr)>>4;  //MSB of charge power.in this case 0x28 = 40x25 = 1000W. Probably net DC power into the Batt.
        outFrame.data.bytes[5]=0x00;  //LSB of the DC ccs current command
        outFrame.data.bytes[6]=0x00;  //bits 0 and 1 MSB of the DC ccs current command.Upper nibble is DC ccs contactor control. Observed in DC fc logs only.
                                      //transitions from 0 to 2 and start of charge but 2 to 1 to 0 at end. Status and Ready operate the same as in AC logs.
        outFrame.data.bytes[7]=EOC_Time;// end of charge timer.  
        Can0.sendFrame(outFrame); 

        outFrame.id = 0x431;       //LIM needs to see this but doesnt control anything...
        outFrame.length = 8;            
        outFrame.extended = 0;          
        outFrame.rtr=1;                 
        outFrame.data.bytes[0]=0xca;  
        outFrame.data.bytes[1]=0xff; 
        outFrame.data.bytes[2]=0x0b;  
        outFrame.data.bytes[3]=0x02; 
        outFrame.data.bytes[4]=0x69;
        outFrame.data.bytes[5]=0x26;
        outFrame.data.bytes[6]=0xf3;
        outFrame.data.bytes[7]=0x4b;  
        Can0.sendFrame(outFrame);
        
   
digitalWrite(led,!digitalRead(led));//blink led every time we fire this interrrupt.

}

void Msgs600ms()                       //600ms messages here
{
                                  
        outFrame.id = 0x560;       //Network managment msg perhaps ...
        outFrame.length = 8;            
        outFrame.extended = 0;          
        outFrame.rtr=1;                 
        outFrame.data.bytes[0]=0x00;  
        outFrame.data.bytes[1]=0x00; 
        outFrame.data.bytes[2]=0x00;  
        outFrame.data.bytes[3]=0x00; 
        outFrame.data.bytes[4]=0xfe;
        outFrame.data.bytes[5]=0x00;
        outFrame.data.bytes[6]=0x00;
        outFrame.data.bytes[7]=0x60;  
        Can0.sendFrame(outFrame); 

      

}


void checkCAN()
{

  if(Can0.available())
  {
    Can0.read(inFrame);
  if(inFrame.id == 0x3B4)
  {
  ACcur = inFrame.data.bytes[0];
  CABlim = inFrame.data.bytes[1];
  PP = (inFrame.data.bytes[2]&0x1);
  Pilot = (inFrame.data.bytes[4]&0xF);
  }
  
}
}


void handle_Wifi(){


if((Auto)&&(PP)) CHGreq=true;

if(CHGreq)
{
  EOC_Time=0xFE;
  CHG_Status=Status_Rdy;
  CHG_Req=Req_Charge;
  CHG_Ready=Chg_Rdy;
  CHG_Pwr=0x2c;//just a holding value of 1kw for now.
  
}

if(!CHGreq)
{
  EOC_Time=0x00;
  CHG_Status=Status_NotRdy;
  CHG_Req=Req_EndCharge;
  CHG_Ready=Chg_NotRdy;
  CHG_Pwr=0x00;
  
}


  
/*
 * 
 * Routine to send data to wifi on serial 2
The information will be provided over serial to the esp8266 at 19200 baud 8n1 in the form :
vxxx,ixxx,pxxx,mxxxx,nxxxx,oxxx,rxxx,qxxx* where :

v=pack voltage (0-700Volts)
i=current (0-1000Amps)
p=power (0-300kw)
m=half pack voltage (0-500volts)
n=Amp Hours (0-300Ah)
o=KiloWatt Hours (0-150kWh)
r=HV Box Temp (0-100C)
q=third pack Volts (0-500Volts)
*=end of string
xxx=three digit integer for each parameter eg p100 = 100kw.
updates will be every 1000ms approx.

v100,i200,p35,m3000,n4000,o20,r100,q50*
*/
  
Serial2.print("v100,i200,p35,m3000,n4000,o20,r30,q50*"); //test string

//digitalWrite(13,!digitalRead(13));//blink led every time we fire this interrrupt.
SerialDEBUG.println("//////////////////EVBMW LIM CONTROLLER V1//////////////////////////////////////////////////////////");
SerialDEBUG.print("Pilot Current=");                                                                             
SerialDEBUG.print(ACcur);
SerialDEBUG.println("Amps");
SerialDEBUG.print("Cable Limit=");
SerialDEBUG.print(CABlim);
SerialDEBUG.println("Amps");
if(PP) SerialDEBUG.println("Plug Inserted");
if(!PP) SerialDEBUG.println("Unplugged");
SerialDEBUG.print("CP Status=");
if(Pilot==0x8) SerialDEBUG.println("None");
if(Pilot==0x9) SerialDEBUG.println("STD AC");
if(Pilot==0xA) SerialDEBUG.println("AC Charging");
if(Pilot==0xC) SerialDEBUG.println("5% DC");
if(Pilot==0xD) SerialDEBUG.println("CCS Comms");
if(Auto) SerialDEBUG.println("Auto Chg Active");
if(!Auto) SerialDEBUG.println("Auto Chg Deactivated");
SerialDEBUG.println("Commands : 'c' engage charge. 'd' disengage charge. 'a' toggle auto charge");

SerialDEBUG.println("//////////////////////////////////////////////////////////////////////////////////////////");
}

void checkforinput(){ 
  //Checks for keyboard input from Native port 
   if (SerialDEBUG.available()) 
     {
      int inByte = SerialDEBUG.read();
      switch (inByte)
         {
          case 'c':            
            CHGreq=true;
            break;
  
          case 'd':    
            CHGreq=false;
            Auto=false;
            break;

          case 'a':    
            Auto=!Auto;
            break;

      }
}
}
