#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>			// sprintf�� ����Ǿ� ���� 

#include "myDelay.h"
#include "lcdControl.h"
#include "myCANLib.h"

#include "myDelay.c"
#include "lcdControl.c"
#include "myCANLib.c"

#define FREQ_CLKIO		16000000	// clock IO 16 MHz
#define PRESCALE		64
#define RC_SERVO_PERIOD	25   		// RC servo ���� �ֱ� 25 msec
#define DC_PERIOD		25	// DC ���� �ֱ� 25 msec

#define BUTTON0_MASK 0x01
#define BUTTON1_MASK 0x02

unsigned int ADdata; 
unsigned int voltage_x, voltage_y;
unsigned int volume_x, volume_y;
unsigned char ADCFlag;

char strBuff[20]={0};

unsigned char engineFlag=0;



//=========================================================
// AD ��ȯ 
// PF0: ADC0 ���̽�ƽ y�� 
// PF1: ADC1 ���̽�ƽ x��
//=========================================================

void initAdc(void){
   	ADMUX =0x40;    // ��������; �ܺ� ĳ�۽��� ���� AVcc(AREF ��)
					// AD��ȯ ������ ����; ������ ���� 
					// AD��ȯ ä�� ����; PortF 0 ��	 

	DDRF  = 0xF0;	// PortF[3..0] �Է����� ����, PortF[7..4] ������� ���� 
	DIDR0 = 0x07;	// ������ �Է� �Ұ� PortF[3..0]

   	ADCSRA= 0xc8;  	// ADC �ο��̺�, ADC ��ȯ ����, ADC���ͷ�Ʈ �ο��̺� 
					// ADC Ŭ�� ����; XTAL��1/2(8MHz)
}

SIGNAL(ADC_vect)
{
	ADdata= ADC;	// AD��ȯ �����͸� ADdata �� ����
					//( Register�� �ִ°��� �������� ADdata�� �Ű� ����ϱ� ���� �̿��Ѵ�. )
   	ADCSRA= 0xC8;  	// ADC �ο��̺�, ADC ��ȯ ����, ADC���ͷ�Ʈ �ο��̺� 
					// ADC Ŭ�� ����; XTAL��1/2(8MHz)

}

void ADcLCD(void)
{
	if(ADCFlag == 0)
	{ 
		volume_y= ADdata; // �������� PortF0
		voltage_y= volume_y*50/1023;
		ADCFlag=1; 
		ADMUX = 0x40;	// PortF1 �Ƴ��α� �Է� ���� 
	} 
	else if(ADCFlag == 1)
	{ 
		volume_x= ADdata; // �µ����� PortF1
		voltage_x= volume_x*50/1023;
		ADCFlag=0; 
		ADMUX = 0x41;   // PortF2 �Ƴ��α� �Է� ���� 
	}
	else ADCFlag=0;
}

void initPort(void)
{
	DDRC  = 0xff;	// LCD ������ �� ��� 
	PORTC = 0xff;
	DDRG  = 0xff;	// LCD ���� ��� (RS, RW, E)
	DDRF  = 0xf0;	// �Ƴ��α� �Է� 
	DDRD  = 0x00;	// SW �Է¼���

	DDRE  = 0xff;	// Motor�� �̿��ϱ� ���� ��Ʈ. �ʿ��� �� ��� ����
}

// motor control ======================================================
void initMotor(void)
{
	TCCR3A=0xAA; 			// ���� 3������ 0x82�� ����
	TCCR3B=0x13;			
	ICR3 = FREQ_CLKIO/2/PRESCALE/1000*RC_SERVO_PERIOD;	
					// �ְ�(Top)3125, 40Hz(25msec) 

	OCR3C= 70;		// ������(output compare) PE5 pin output

}

// DC���� �ӵ� ����� ���� ȸ�� ���� ���� 
void CtrlDcMotor(unsigned int speed, unsigned int dir)
{
	unsigned int level=5;

	PORTE&=0xFC;

	// DC���� ȸ�� ���� ����
	if(dir==0)		// �ð� ���� ȸ��
	{
		// speed�� ���� �ӵ� ����
		OCR3A=(speed*FREQ_CLKIO/2/PRESCALE/1000*RC_SERVO_PERIOD)/level;
		PORTE|=0x01;	
	}
	else if(dir==1)	// �ݽð� ���� ȸ�� 
	{
		// speed�� ���� �ӵ� ����
		OCR3A=(speed*FREQ_CLKIO/2/PRESCALE/1000*RC_SERVO_PERIOD)/level;
		PORTE|=0x02;
	}
	else			// ���� ���� 
	{
		OCR3A=100;
		OCR3B=100;
	}
}

// ���� ���� ȸ�� ����(degree : ����) 
void CtrlRcServoMotor(unsigned int degree)
{
	OCR3C= FREQ_CLKIO/2/PRESCALE/1000*(1.6/180*degree+(0.15/180*degree+0.6));								 						
}

// ���̽�ƽ Y���� AD ��ȭ���� ���� ���� �ӵ� ���� 
unsigned int DcMotorSpeed(unsigned int volt_y)
{
	unsigned int speed=0;

	if (volt_y<10) speed=5;
	else if (volt_y<14) speed=4;
	else if (volt_y<18) speed=3;
	else if (volt_y<22) speed=2;
	else if (volt_y<26) speed=1;
	else if (volt_y<28) speed=1;
	else if (volt_y<32) speed=2;
	else if (volt_y<36) speed=3;
	else if (volt_y<40) speed=4;
	else if (volt_y<55) speed=5;

	return speed;
}	

// ���̽�ƽ Y���� AD ��ȭ���� ���� DC���� ȸ�� ���� ���� 	
unsigned int DcMotorDir(unsigned int volt_y)
{
	unsigned int dir=0;

	if(volt_y<25) dir=0;		// �ð� ���� 
	else if(volt_y>28) dir=1;	// �ݽð� ���� 
	else dir=2;					// ���� 

	return dir;
}

// ���̽�ƽ X���� AD ��ȭ���� ���� ���� ���� ȸ���� ���� 
unsigned int RcServoMotorAngle(unsigned int volt_x)
{
	unsigned int angle;
	if(volt_x>25 && volt_x<29) angle=90;
	else angle=(volt_x)*180/50;
	
	return angle;
}

unsigned char chattering(unsigned char SW)
{
	unsigned char result = 0;
	unsigned char input_state;
	
	if(SW == 2)
	{
		input_state = PIND & BUTTON0_MASK;
		if(input_state <= 0)
		{
			ms_delay(10);
			
			input_state = PIND & BUTTON0_MASK;
			if(input_state <= 0)
			result = 1;
		}
	}
	else if(SW == 3)
	{
		input_state = PIND & BUTTON1_MASK;
		if(input_state <= 0)
		{
			ms_delay(10);
			
			input_state = PIND & BUTTON1_MASK;
			if(input_state <= 0)
			result = 1;
		}
	}
	
	return result;
}

unsigned char* ButtonInput(void)
{
	unsigned char WARNING = 0;
	unsigned char Switch = 0;
	unsigned char Button_Info;

	WARNING = chattering(2);
	Switch = chattering(3);	

	if(WARNING==1 && Switch==0) Button_Info= "STP";	// S2��ư ���� ���
	else if(WARNING==0 && Switch==1) Button_Info = "CHN";	// S3��ư ���� ���
	
	return Button_Info;
}

int main(void)
{
	unsigned int angle, dir, speed;
	unsigned int AutoManual = 0;
	unsigned char status;
	unsigned int mode;

	initPort();		// ����� ��Ʈ �ʱ�ȭ
    	LCD_init();     // LCD �ʱ�ȭ
	initMotor();	// ���ܸ��� ��� ���� Ÿ�̸�/ī���� �ʱ�ȭ 
	initAdc();		// AD ��ȯ �ʱ�ȭ
	sei();			// INT �ο��̺�

	while(1)
	{
		status = ButtonInput();

		if(!strcmp(status, "STP"))	// status�� "STP" ��ġ Ȯ��
		{
			mode = 1;
		}
		if(!strcmp(status, "CHN"))	// status�� "CHN" ��ġ Ȯ��
		{
			mode = 0;
		}

			if((mode == 0))	// ���� ���¶��
			{				
				speed=DcMotorSpeed(voltage_y);
				dir=DcMotorDir(voltage_y);
				CtrlDcMotor(speed, dir);
				angle=RcServoMotorAngle(voltage_x);
				CtrlRcServoMotor(angle); 
		sprintf(strBuff,"angle: %d", angle);
		LCD_Write(0,0,strBuff);
		sprintf(strBuff,"speed: %d", speed);
		LCD_Write(0,1,strBuff);
			}
			else if ((mode == 1)) // �ڵ� ���¶��
			{
				continue;/* Case�з� �� �ڵ� Settting */
			}	
		}
	}
