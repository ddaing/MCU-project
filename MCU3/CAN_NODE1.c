/*-------------------------------------------------------------------------------------------
            2015-2 Embedded system experiment

Project name : Project5-1
edit : wwChoi
---------------------------------------------------------------------------------------------
[ CAN송신 ]

Experiment1 -> : 조이스틱을 이용하여 X축, Y축값을 CAN을 이용하여 전송한다. ( LCD에 자신이 보내는 데이터를 표시한다. )

--------------------------------------------------------------------------------------------*/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>         // sprintf문 선언되어 있음 

#include "myDelay.h"
#include "lcdControl.h"
#include "myCANLib.h"

#include "myDelay.c"
#include "lcdControl.c"
#include "myCANLib.c"

unsigned int ADdata; 
unsigned int cds=0;

unsigned int temp_outside;
unsigned int thief_OX = 0;
unsigned char ADCFlag;
unsigned int cds_s, temp_s, temp, ptr, temp1, cds1;
unsigned int control;

struct MOb msg1;    
struct MOb msg2={0x02, 0, EXT, 8, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}};   
struct MOb msg3;

char strBuff[20]={0};





//=========================================================
// AD 변환 
// PF0: ADC0 조이스틱 y축 
// PF1: ADC1 조이스틱 x축
//=========================================================

void initAdc0(void)
{
   ADMUX = 0x40;   // 기준전압; 외부 캐퍼시터 가진 AVcc(AREF 핀)
            // AD변환 데이터 정렬; 오른쪽 정렬 
            // AD변환 채널 선택; PortF 0 핀    

   DDRF = 0xF0;   // PortF[3..0] 입력으로 설정, PortF[7..4] 출력으로 설정 
   DIDR0 = 0x0F;   // 디지털 입력 불가 PortF[3..0]

   ADCSRA = 0xC8;     // ADC 인에이블, ADC 변환 시작, ADC인터럽트 인에이블 
               // ADC 클럭 설정; XTAL의1/2(8MHz) //11001000        
}

void initAdc1(void)
{
   ADMUX = 0x41;   // 기준전압; 외부 캐퍼시터 가진 AVcc(AREF 핀)
            // AD변환 데이터 정렬; 오른쪽 정렬 
            // AD변환 채널 선택; PortF 0 핀    

   DDRF = 0xF0;   // PortF[3..0] 입력으로 설정, PortF[7..4] 출력으로 설정 
   DIDR0 = 0x0F;   // 디지털 입력 불가 PortF[3..0]

   ADCSRA = 0xC8;     // ADC 인에이블, ADC 변환 시작, ADC인터럽트 인에이블 
               // ADC 클럭 설정; XTAL의1/2(8MHz) //11001000        
}

SIGNAL(ADC_vect)
{
   ADdata= ADC;   // AD변환 데이터를 ADdata 에 저장
               //( Register에 있는값을 전역변수 ADdata에 옮겨 사용하기 쉽게 이용한다. )
   ADCSRA= 0xC8;     // ADC 인에이블, ADC 변환 시작, ADC인터럽트 인에이블 
               // ADC 클럭 설정; XTAL의1/2(8MHz)

}

void ADcLCD(void)
{
	if (ADCFlag == 0)
	{
		temp_s = ADdata; // 가변저항 PortF0
		temp = temp_s /20;
		ADCFlag = 1;
		ADMUX = 0x40;	// PortF1 아날로그 입력 선택 
	}
	else if (ADCFlag == 1)
	{
		cds_s = ADdata; // 온도센서 PortF1
		ptr = cds_s / 100;
		ADCFlag = 0;
		ADMUX = 0x41;   // PortF2 아날로그 입력 선택 
	}
	else ADCFlag = 0;
}


void initPort(void)
{
   DDRC  = 0xFF;   // LCD 데이터 및 명령 
   PORTC = 0xff;
   DDRG |= 0x07;   // LCD 제어 출력 (RS, RW, E)   
   DDRE  = 0xff;
}


int main(void)
{
   initPort();      // 입출력 포트 초기화
   LCD_init();     // LCD 초기화
   initAdc0(); 
   sei();         // INT 인에이블 
   can_init(b125k);   // CAN 보레이트를 원하는 값으로 세팅한다.

   can_rx_set(1, 0x01, EXT, 8, 0xFFFFFFFF, 0x00);    // CAN 수신기 초기화

   while(1)
   {
      ADcLCD();

      msg2.data[1] = control;
      can_tx(2, &msg2, 0); //(obj(mob num), msg내용, rtr(데이터 프레임) 
	 
      sprintf(strBuff, "%d",ptr);
      LCD_Write(0,0, strBuff);
	  ms_delay(1);

      sprintf(strBuff, "%d", temp);
      LCD_Write(5,0, strBuff);
      ms_delay(1);

	  sprintf(strBuff, "%d", temp_outside);
	  LCD_Write(0, 1, strBuff);
	  ms_delay(1);

	  sprintf(strBuff, "%d", thief_OX);
	  LCD_Write(5, 1, strBuff);
	  ms_delay(1);

      /********************************************
      실험1 소스코드
      ADcLCD()함수 이용
      msg1.data[]배열 이용 
       can_tx() 함수 이용
      ********************************************/	  
	  if(temp > temp_outside )
	  {
	  	control = 1;
	  }
      else if ((temp < temp_outside) && ptr<8)
	  {
	  	control = 2;
	  }
      else if ((temp > temp_outside) && ptr>=8) 
	  {
	  	control = 3;
	  }
	  else
	  {
		control = 0;
	  }

	  
	  can_rx(1, &msg1);
	  
      temp_outside=msg1.data[0];
      thief_OX=msg1.data[1];


   }
}

