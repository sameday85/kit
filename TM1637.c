//  Author:Fred.Chu
//  Date:9 April,2013
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
//  Modified record:
//
/*******************************************************************************/
#include "TM1637.h"

#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

uint8_t _PointFlag;     //_PointFlag=1:the clock point on
uint8_t Clkpin;
uint8_t Datapin;


#define num 200

static int8_t TubeTab[] = {0x3f,0x06,0x5b,0x4f,
                           0x66,0x6d,0x7d,0x07,
                           0x7f,0x6f,0x77,0x7c,
                           0x39,0x5e,0x79,0x71};//0~9,A,b,C,d,E,F      
                  
void TM1637_init(uint8_t Clk, uint8_t Data)
{
  Clkpin = Clk;
  Datapin = Data;
  pinMode(Clkpin,OUTPUT);
  pinMode(Datapin,OUTPUT);
  digitalWrite(Clkpin,HIGH);
  digitalWrite(Datapin,HIGH);
  TM1637_clearDisplay();
}

void TM1637_ack()
{
  uint count=0;   
  pinMode(Datapin,INPUT);
  digitalWrite(Clkpin,LOW); 
  delayMicroseconds(num); 
  while(digitalRead(Datapin))
  {
    count +=1;
    delayMicroseconds(num);
    if(count==100)
    {  
      pinMode(Datapin,OUTPUT);
      break;
    }
  }
  digitalWrite(Datapin,LOW); 
  digitalWrite(Clkpin,HIGH);  
  delayMicroseconds(num); 
  digitalWrite(Clkpin,LOW); 
  pinMode(Datapin,OUTPUT);
}

void TM1637_writeByte(int8_t wr_data)
{
  uint8_t i=0;

  for(i=0;i<8;i++)        //sent 8bit data
  {
    digitalWrite(Clkpin,LOW);      
    if((wr_data&0x01)==0x01) digitalWrite(Datapin,HIGH);//LSB first
    else                     digitalWrite(Datapin,LOW);
    wr_data >>= 1;     
    delayMicroseconds(num); 
    digitalWrite(Clkpin,HIGH);
    delayMicroseconds(num); 
  }
  TM1637_ack();  
}


//send start signal to TM1637
void TM1637_start(void)
{
  digitalWrite(Clkpin,HIGH);//send start signal to TM1637
  digitalWrite(Datapin,HIGH);
  delayMicroseconds(num); 
  digitalWrite(Datapin,LOW); 
  delayMicroseconds(num);
  digitalWrite(Clkpin,LOW);
  delayMicroseconds(num); 
} 

//End of transmission
void TM1637_stop(void)
{
  digitalWrite(Clkpin,LOW);
  delayMicroseconds(num); 
  digitalWrite(Datapin,LOW);
  delayMicroseconds(num); 
  digitalWrite(Clkpin,HIGH);
  delayMicroseconds(num);
  digitalWrite(Datapin,HIGH); 
  
}

//display function.Write to full-screen.
void TM1637_display_str(int8_t DispData[])
{
  int8_t SegData[4];
  uint8_t i;
  for(i = 0;i < 4;i ++)
  {
    SegData[i] = DispData[i];
  }
  TM1637_coding_str(SegData);
  TM1637_start();          //start signal sent to TM1637 from MCU
  TM1637_writeByte(ADDR_AUTO);//
  TM1637_stop();           //
  TM1637_start();          //
  TM1637_writeByte(Cmd_SetAddr);//
  for(i=0;i < 4;i ++)
  {
    TM1637_writeByte(SegData[i]);        //
  }
  TM1637_stop();           //
  TM1637_start();          //
  TM1637_writeByte(Cmd_DispCtrl);//
  TM1637_stop();           //
}

//******************************************
void TM1637_display(uint8_t BitAddr,int8_t DispData)
{
  int8_t SegData;
  SegData = TM1637_coding(DispData);
  TM1637_start();          //start signal sent to TM1637 from MCU
  TM1637_writeByte(ADDR_FIXED);//
  TM1637_stop();           //
  TM1637_start();          //
  TM1637_writeByte(BitAddr|0xc0);//
  TM1637_writeByte(SegData);//
  TM1637_stop();            //
  TM1637_start();          //
  TM1637_writeByte(Cmd_DispCtrl);//
  TM1637_stop();           //
}

void TM1637_clearDisplay(void)
{
  TM1637_display(0x00,0x7f);
  TM1637_display(0x01,0x7f);
  TM1637_display(0x02,0x7f);
  TM1637_display(0x03,0x7f);  
}

//To take effect the next time it displays.
void TM1637_set(uint8_t brightness,uint8_t SetData,uint8_t SetAddr)
{
  Cmd_SetData = SetData;
  Cmd_SetAddr = SetAddr;
  Cmd_DispCtrl = 0x88 + brightness;//Set the brightness and it takes effect the next time it displays.
}

//Whether to light the clock point ":".
//To take effect the next time it displays.
void TM1637_point(uint8_t PointFlag)
{
  _PointFlag = PointFlag;
}

void TM1637_coding_str(int8_t DispData[])
{
  uint8_t i;
  uint8_t PointData;
  if(_PointFlag == POINT_ON)PointData = 0x80;
  else PointData = 0; 
  for( i = 0;i < 4;i ++)
  {
    if(DispData[i] == 0x7f)DispData[i] = 0x00;
    else DispData[i] = TubeTab[DispData[i]] + PointData;
  }
}

int8_t TM1637_coding(int8_t DispData)
{
  uint8_t PointData;
  if(_PointFlag == POINT_ON)PointData = 0x80;
  else PointData = 0; 
  if(DispData == 0x7f) DispData = 0x00 + PointData;//The bit digital tube off
  else DispData = TubeTab[DispData] + PointData;
  return DispData;
}
