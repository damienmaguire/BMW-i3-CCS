/*
BMW i3 LIM  Controller Software. Alpha version.


Copyright 2021 
Damien Maguire



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
byte ACcur=0;
byte CABlim=0;

CAN_FRAME outFrame;  //A structured variable according to due_can library for transmitting CAN data.
CAN_FRAME inFrame;    //structure to keep inbound inFrames






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
                                  
        outFrame.id = 0x112;       
        outFrame.length = 8;            
        outFrame.extended = 0;          
        outFrame.rtr=1;                 
        outFrame.data.bytes[0]=0xf9;  
        outFrame.data.bytes[1]=0x1f; 
        outFrame.data.bytes[2]=0x8b;  
        outFrame.data.bytes[3]=0x0e; 
        outFrame.data.bytes[4]=0xa6;
        outFrame.data.bytes[5]=0x71;
        outFrame.data.bytes[6]=0x65;
        outFrame.data.bytes[7]=0x5d;  
        Can0.sendFrame(outFrame); 

      

}
    
void Msgs100ms()                       //100ms messages here
{

        outFrame.id = 0x12f;       
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

}


void Msgs200ms()                      ////200ms messages here
{
if(CHGreq)
{
        outFrame.id = 0x3E9;       
        outFrame.length = 8;            
        outFrame.extended = 0;          
        outFrame.rtr=1;                 
        outFrame.data.bytes[0]=0x08;  
        outFrame.data.bytes[1]=0x52; 
        outFrame.data.bytes[2]=0x21;  
        outFrame.data.bytes[3]=0x81; 
        outFrame.data.bytes[4]=0x02;
        outFrame.data.bytes[5]=0x00;
        outFrame.data.bytes[6]=0x00;
        outFrame.data.bytes[7]=0xfe;  
        Can0.sendFrame(outFrame); 

        outFrame.id = 0x431;       
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
        
}

if(!CHGreq)
{
        outFrame.id = 0x3E9;       
        outFrame.length = 8;            
        outFrame.extended = 0;          
        outFrame.rtr=1;                 
        outFrame.data.bytes[0]=0x82;  
        outFrame.data.bytes[1]=0x52; 
        outFrame.data.bytes[2]=0x00;  
        outFrame.data.bytes[3]=0x00; 
        outFrame.data.bytes[4]=0x00;
        outFrame.data.bytes[5]=0x00;
        outFrame.data.bytes[6]=0x00;
        outFrame.data.bytes[7]=0x00;  
        Can0.sendFrame(outFrame); 

        outFrame.id = 0x431;       
        outFrame.length = 8;            
        outFrame.extended = 0;          
        outFrame.rtr=1;                 
        outFrame.data.bytes[0]=0xca;  
        outFrame.data.bytes[1]=0xff; 
        outFrame.data.bytes[2]=0xff;  
        outFrame.data.bytes[3]=0xff; 
        outFrame.data.bytes[4]=0xff;
        outFrame.data.bytes[5]=0x26;
        outFrame.data.bytes[6]=0xf3;
        outFrame.data.bytes[7]=0x4b;  
        Can0.sendFrame(outFrame);
        
}



       
digitalWrite(led,!digitalRead(led));//blink led every time we fire this interrrupt.

}

void Msgs600ms()                       //600ms messages here
{
                                  
        outFrame.id = 0x560;       
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
  ACcur = (inFrame.data.bytes[0]);
  CABlim = (inFrame.data.bytes[1]);
  }
  
}
}


void handle_Wifi(){

        outFrame.id = 0x2fc;       //this should be every 6 secs...
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
SerialDEBUG.println("Commands : 'c' engage charge. 'd' disengage charge.");

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
            break;


      }
}
}
