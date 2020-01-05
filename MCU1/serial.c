/*-------------------------------------------------------------------------------------------
            2015-2 Embedded system experiment

Project name : Project4
edit : wwChoi
---------------------------------------------------------------------------------------------

 Experiment1 -> 초음파 센서를 이용하여 그 값을 CLCD에 표시
 Experiment2 -> 초음파 센서를 이용하여 그 값을 UART통신으로 Tera term에 표시
--------------------------------------------------------------------------------------------*/




#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <math.h>
#include "delay.h"
#include "lcd.h"
#include "myCANLib.h"
#include "myCANLib.c"

#define PE4_Clear	(PORTE &= 0xEF)
#define PE4_Set		(PORTE |= 0x10)
#define PE4_IN		(DDRE &= 0xEF)
#define PE4_OUT		(DDRE |= 0x10)




#define TEMPERATURE 25

int alarm = 0;//0이면 평상시 1이면 보안알림  
int alarmOn = 0; //0이면 보안 on 1이면 보안 off 
unsigned short tick=0, pulse_check=0, pulse_end=0;

struct MOb msg3={0x03, 0, EXT, 8, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}};
struct MOb msg4;



//========================================
// ultra sound 
void init_EXT_INT(void)
{
	EIMSK |= 0x70;		// INT4,5,6 Interrupt Set
	EICRB |= 0x3F;	//INT4,5,6 Rising Edge /INT Set
	//EIFR   = 0x10;		// Interrupt Flag
}

void init_TIMER0_COMPA(void)
{
	TCCR0A |= (1<<WGM01)|(1<<WGM00);	// CTC Mode
	TCCR0A |= (1<<CS01);	// clk/8 Prescaler
	TCNT0 = 0;
	OCR0A = 19;		// 1cycle -> 20us으로 세팅 1cycle = 1/(16Mhz/(2*8*( OCR0A+1)))
	TIMSK0 = 0x02;		//T/C0 Output Compare Match INT
	//TIFR0 = 
}

void initBuzzerLed(void)
{
	TCCR3A=0x89; //OC3A,C 사용 PWM,Phase Correct 모드 top =0xFF(0-255), 64분주   
	TCCR3B=0x03;
}

void ledBuzzer(void)
{
	OCR3A = 250;
	OCR3C = 100;
	ms_delay(1000);								 						
}


SIGNAL(TIMER0_COMP_vect)	
{
	tick++;
}

SIGNAL(INT4_vect)
{
	unsigned short pulse_tick;

	pulse_tick = tick;

	if(EICRB & 0x03)
	{
	   EICRB &= 0x00;	//  low state
	   tick = 0;	
	}
	else
	{
		EICRB |= (1<<ISC41)|(1<<ISC40);// INT4 Rising Edge / INT Set
		pulse_end = pulse_tick;	
	}
}

//======================================
// port init
void initPort(void)
{
	DDRC  = 0xFF;
	PORTC = 0xFF;
	DDRG  = 0xFF;
	DDRE  = 0x28;
	//PORTE = 0x28;
}

int main(void)
{
	unsigned char thousand, hundred, ten, one;
	float distance;
	
	initPort();
	init_EXT_INT();
	init_TIMER0_COMPA();
	initBuzzerLed();


	LCD_init();
	LCD_cmd(0x01);
	ms_delay(50);
	LCD_Write(0, 0, "AJOUUNIV Serial");
	LCD_Write(0, 1, "Distance:");
	LCD_Write(14, 1, "mm");
	ms_delay(200);

	/***************
	*AJOUUNIV Serial *
	*Distance: 0000mm*
	****************/
	
	can_init(b125k); 		// 초기화
	can_rx_set(4, 0x00, EXT, 8, 0x00000000, 0x00); 	// CAN 수신기 초기화 

	while(1)
	{
	/****************************************
	: 초음파 센서를 사용하기 위한 소스코드 제공
	****************************************/
		cli();				// clear interrupt
		PE4_Clear;
		PE4_OUT;			// PE4 pin is output
		us_delay(500);

		PE4_Set;			// output set during 5us
		us_delay(5);

		PE4_Clear;			// PE4 clear
		us_delay(100);

		PE4_IN;				// PE4 pin is input
		us_delay(100);

		sei();				// set interrupt


	/* distance = velocity * time */
	distance = (331.5+(0.6*TEMPERATURE))*(pulse_end*0.00001/2)*1000;
	if(alarmOn ==0 && distance < 15){
		alarm = 1;
			
	}
	if(alarm==1){
		//cli();
		ledBuzzer();
	}

		can_tx(3, &msg3, 0);
		can_rx(4, &msg4);
		alarmOn = msg4.data[0];//1을 받아오면 멈추기
		if(alarmOn == 1){
			alarm = 0;
		}
		msg3.data[0] = alarm; 


	/* distance digit display */
		thousand = distance/1000;
		distance = distance - (thousand * 1000);
	
		hundred = distance /100;
		distance = distance - (hundred * 100);

		ten = distance/10; 
		distance = distance - (ten * 10);

		one = distance;


		/*************************************
		실험1 소스코드
		LCD Display 함수를 이용하여 초음파 센서값 표시
		*************************************/
		LCD_Disp(10,1);	Write_Char(thousand+'0');
		LCD_Disp(11,1);	Write_Char(hundred+'0');
	   	LCD_Disp(12,1);	Write_Char(ten+'0');
		LCD_Disp(13,1);	Write_Char(one+'0');
		


		ms_delay(100);	
	}
	return 0;
}

