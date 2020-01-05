/*-------------------------------------------------------------------------------------------
            2015-2 Embedded system experiment

Project name : Project5-1
edit : wwChoi
---------------------------------------------------------------------------------------------
[ CAN�۽� ]

Experiment1 -> : ���̽�ƽ�� �̿��Ͽ� X��, Y�ప�� CAN�� �̿��Ͽ� �����Ѵ�. ( LCD�� �ڽ��� ������ �����͸� ǥ���Ѵ�. )

--------------------------------------------------------------------------------------------*/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>         // sprintf�� ����Ǿ� ���� 

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
// AD ��ȯ 
// PF0: ADC0 ���̽�ƽ y�� 
// PF1: ADC1 ���̽�ƽ x��
//=========================================================

void initAdc0(void)
{
   ADMUX = 0x40;   // ��������; �ܺ� ĳ�۽��� ���� AVcc(AREF ��)
            // AD��ȯ ������ ����; ������ ���� 
            // AD��ȯ ä�� ����; PortF 0 ��    

   DDRF = 0xF0;   // PortF[3..0] �Է����� ����, PortF[7..4] ������� ���� 
   DIDR0 = 0x0F;   // ������ �Է� �Ұ� PortF[3..0]

   ADCSRA = 0xC8;     // ADC �ο��̺�, ADC ��ȯ ����, ADC���ͷ�Ʈ �ο��̺� 
               // ADC Ŭ�� ����; XTAL��1/2(8MHz) //11001000        
}

void initAdc1(void)
{
   ADMUX = 0x41;   // ��������; �ܺ� ĳ�۽��� ���� AVcc(AREF ��)
            // AD��ȯ ������ ����; ������ ���� 
            // AD��ȯ ä�� ����; PortF 0 ��    

   DDRF = 0xF0;   // PortF[3..0] �Է����� ����, PortF[7..4] ������� ���� 
   DIDR0 = 0x0F;   // ������ �Է� �Ұ� PortF[3..0]

   ADCSRA = 0xC8;     // ADC �ο��̺�, ADC ��ȯ ����, ADC���ͷ�Ʈ �ο��̺� 
               // ADC Ŭ�� ����; XTAL��1/2(8MHz) //11001000        
}

SIGNAL(ADC_vect)
{
   ADdata= ADC;   // AD��ȯ �����͸� ADdata �� ����
               //( Register�� �ִ°��� �������� ADdata�� �Ű� ����ϱ� ���� �̿��Ѵ�. )
   ADCSRA= 0xC8;     // ADC �ο��̺�, ADC ��ȯ ����, ADC���ͷ�Ʈ �ο��̺� 
               // ADC Ŭ�� ����; XTAL��1/2(8MHz)

}

void ADcLCD(void)
{
	if (ADCFlag == 0)
	{
		temp_s = ADdata; // �������� PortF0
		temp = temp_s /20;
		ADCFlag = 1;
		ADMUX = 0x40;	// PortF1 �Ƴ��α� �Է� ���� 
	}
	else if (ADCFlag == 1)
	{
		cds_s = ADdata; // �µ����� PortF1
		ptr = cds_s / 100;
		ADCFlag = 0;
		ADMUX = 0x41;   // PortF2 �Ƴ��α� �Է� ���� 
	}
	else ADCFlag = 0;
}


void initPort(void)
{
   DDRC  = 0xFF;   // LCD ������ �� ��� 
   PORTC = 0xff;
   DDRG |= 0x07;   // LCD ���� ��� (RS, RW, E)   
   DDRE  = 0xff;
}


int main(void)
{
   initPort();      // ����� ��Ʈ �ʱ�ȭ
   LCD_init();     // LCD �ʱ�ȭ
   initAdc0(); 
   sei();         // INT �ο��̺� 
   can_init(b125k);   // CAN ������Ʈ�� ���ϴ� ������ �����Ѵ�.

   can_rx_set(1, 0x01, EXT, 8, 0xFFFFFFFF, 0x00);    // CAN ���ű� �ʱ�ȭ

   while(1)
   {
      ADcLCD();

      msg2.data[1] = control;
      can_tx(2, &msg2, 0); //(obj(mob num), msg����, rtr(������ ������) 
	 
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
      ����1 �ҽ��ڵ�
      ADcLCD()�Լ� �̿�
      msg1.data[]�迭 �̿� 
       can_tx() �Լ� �̿�
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

