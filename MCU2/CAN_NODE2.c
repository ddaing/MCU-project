#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>			// sprintf문 선언되어 있음 

#include "myDelay.h"
#include "lcdControl.h"
#include "myCANLib.h"

#include "myDelay.c"
#include "lcdControl.c"
#include "myCANLib.c"

#define FREQ_CLKIO		16000000	// clock IO 16 MHz
#define PRESCALE		64
#define RC_SERVO_PERIOD	25   		// RC servo 모터 주기 25 msec
#define DC_PERIOD		25	// DC 모터 주기 25 msec

#define BUTTON0_MASK 0x01
#define BUTTON1_MASK 0x02

unsigned int ADdata; 
unsigned int voltage_x, voltage_y;
unsigned int volume_x, volume_y;
unsigned char ADCFlag;

char strBuff[20]={0};

unsigned char engineFlag=0;



//=========================================================
// AD 변환 
// PF0: ADC0 조이스틱 y축 
// PF1: ADC1 조이스틱 x축
//=========================================================

void initAdc(void){
   	ADMUX =0x40;    // 기준전압; 외부 캐퍼시터 가진 AVcc(AREF 핀)
					// AD변환 데이터 정렬; 오른쪽 정렬 
					// AD변환 채널 선택; PortF 0 핀	 

	DDRF  = 0xF0;	// PortF[3..0] 입력으로 설정, PortF[7..4] 출력으로 설정 
	DIDR0 = 0x07;	// 디지털 입력 불가 PortF[3..0]

   	ADCSRA= 0xc8;  	// ADC 인에이블, ADC 변환 시작, ADC인터럽트 인에이블 
					// ADC 클럭 설정; XTAL의1/2(8MHz)
}

SIGNAL(ADC_vect)
{
	ADdata= ADC;	// AD변환 데이터를 ADdata 에 저장
					//( Register에 있는값을 전역변수 ADdata에 옮겨 사용하기 쉽게 이용한다. )
   	ADCSRA= 0xC8;  	// ADC 인에이블, ADC 변환 시작, ADC인터럽트 인에이블 
					// ADC 클럭 설정; XTAL의1/2(8MHz)

}

void ADcLCD(void)
{
	if(ADCFlag == 0)
	{ 
		volume_y= ADdata; // 가변저항 PortF0
		voltage_y= volume_y*50/1023;
		ADCFlag=1; 
		ADMUX = 0x40;	// PortF1 아날로그 입력 선택 
	} 
	else if(ADCFlag == 1)
	{ 
		volume_x= ADdata; // 온도센서 PortF1
		voltage_x= volume_x*50/1023;
		ADCFlag=0; 
		ADMUX = 0x41;   // PortF2 아날로그 입력 선택 
	}
	else ADCFlag=0;
}

void initPort(void)
{
	DDRC  = 0xff;	// LCD 데이터 및 명령 
	PORTC = 0xff;
	DDRG  = 0xff;	// LCD 제어 출력 (RS, RW, E)
	DDRF  = 0xf0;	// 아날로그 입력 
	DDRD  = 0x00;	// SW 입력설정

	DDRE  = 0xff;	// Motor를 이용하기 위한 포트. 필요한 핀 출력 설정
}

// motor control ======================================================
void initMotor(void)
{
	TCCR3A=0xAA; 			// 실험 3에서는 0x82를 썼음
	TCCR3B=0x13;			
	ICR3 = FREQ_CLKIO/2/PRESCALE/1000*RC_SERVO_PERIOD;	
					// 최고값(Top)3125, 40Hz(25msec) 

	OCR3C= 70;		// 최저값(output compare) PE5 pin output

}

// DC모터 속도 제어와 모터 회전 방향 제어 
void CtrlDcMotor(unsigned int speed, unsigned int dir)
{
	unsigned int level=5;

	PORTE&=0xFC;

	// DC모터 회전 방향 결정
	if(dir==0)		// 시계 방향 회전
	{
		// speed에 따른 속도 제어
		OCR3A=(speed*FREQ_CLKIO/2/PRESCALE/1000*RC_SERVO_PERIOD)/level;
		PORTE|=0x01;	
	}
	else if(dir==1)	// 반시계 방향 회전 
	{
		// speed에 따른 속도 제어
		OCR3A=(speed*FREQ_CLKIO/2/PRESCALE/1000*RC_SERVO_PERIOD)/level;
		PORTE|=0x02;
	}
	else			// 모터 정지 
	{
		OCR3A=100;
		OCR3B=100;
	}
}

// 서보 모터 회전 제어(degree : 각도) 
void CtrlRcServoMotor(unsigned int degree)
{
	OCR3C= FREQ_CLKIO/2/PRESCALE/1000*(1.6/180*degree+(0.15/180*degree+0.6));								 						
}

// 조이스틱 Y축의 AD 변화값에 따른 모터 속도 설정 
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

// 조이스틱 Y축의 AD 변화값에 따른 DC모터 회전 방향 결정 	
unsigned int DcMotorDir(unsigned int volt_y)
{
	unsigned int dir=0;

	if(volt_y<25) dir=0;		// 시계 방향 
	else if(volt_y>28) dir=1;	// 반시계 방향 
	else dir=2;					// 정지 

	return dir;
}

// 조이스틱 X축의 AD 변화값에 따른 서보 모터 회전각 결정 
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

	if(WARNING==1 && Switch==0) Button_Info= "STP";	// S2버튼 누른 경우
	else if(WARNING==0 && Switch==1) Button_Info = "CHN";	// S3버튼 누른 경우
	
	return Button_Info;
}

int main(void)
{
	unsigned int angle, dir, speed;
	unsigned int AutoManual = 0;
	unsigned char status;
	unsigned int mode;

	initPort();		// 입출력 포트 초기화
    	LCD_init();     // LCD 초기화
	initMotor();	// 스텝모터 제어를 위한 타이머/카운터 초기화 
	initAdc();		// AD 변환 초기화
	sei();			// INT 인에이블

	while(1)
	{
		status = ButtonInput();

		if(!strcmp(status, "STP"))	// status과 "STP" 일치 확인
		{
			mode = 1;
		}
		if(!strcmp(status, "CHN"))	// status과 "CHN" 일치 확인
		{
			mode = 0;
		}

			if((mode == 0))	// 수동 상태라면
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
			else if ((mode == 1)) // 자동 상태라면
			{
				continue;/* Case분류 및 자동 Settting */
			}	
		}
	}
