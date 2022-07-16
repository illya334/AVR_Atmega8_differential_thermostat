/*
 * Atmega8_Termostat.c
 *
 * Created: 22.06.2022 14:18:58
 *  Author: illya
 */ 

#define F_CPU 4000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <math.h>

#define BUTT_PRESSED_CNT	5
#define BUTT_HELD_CNT		20
#define BUTT_HELD_DELAY		70

#define TCS 256 // should be ~10ms
/*
	Подобрать делитель можно по формуле: 256 * k / f = t (sec)
	k - делитель (8, 64, 256, 1024).
	f - частота (Hz)
	t - время (sec)

	   f   |   k   |    t
	 1 MHz |   64  |  ~0.016
	 2 MHz |  256  |  ~0.032
	 4 MHz |  256  |  ~0.016
	 6 MHz |  256  |  ~0.010
	 8 MHz | 1024  |  ~0.032
	 9 MHz |   64  |  ~0.018
	10 MHz | 1024  |  ~0.026
	12 MHz | 1024  |  ~0.021
	16 MHz | 1024  |  ~0.016
*/

#if (TCS == 8)
	#define _TCCR0_def 2;
#elif (TCS == 64)
	#define _TCCR0_def 3;
#elif (TCS == 256)
	#define _TCCR0_def 4;
#elif (TCS == 1024)
	#define _TCCR0_def 5;
#else
	#error Invalid TCS
#endif

#define reg register
#define ushort unsigned short
#define ubyte uint8_t
#define byte int8_t
#define bool ubyte
#define true 1
#define false 0

// for readADC
#define T1 1 // Терморезистор 1
#define T2 3 // Терморезистор 2
#define TNONE 100 // вывод тире

byte T2MAX = -9; // Максимальная температура бойллера

// https://www.gotronic.fr/pj2-mf52type-1554.pdf
const ushort ConstRes[] = { // сопротивление термистора (таблица), в омах
	181700, // -30
	133300, // -25
	 98880, // -20
	 74100, // -15
	 56060, // -10
	 42800, //  -5
	 98960, //   0
	 25580, //   5
	 20000, //  10
	 15760, //  15
	 12510, //  20
	 10000, //  25 (This)
	  8048, //  30
	  6518, //  35
	  5312, //  40
	  4354, //  45
	  3588, //  50
	  2974, //  55
	  2476, //  60
	  2072, //  65
	  1743, //  70
	  1473, //  75
	  1250, //  80
	  1065, //  85
	   911, //  90
	   782, //  95
	   674, // 100
	   583, // 105
	   506, // 110
};

struct stNowResTemp{
	byte	Temp1;
	ushort	R1;
	
	byte	Temp2;
	ushort	R2;
	
	byte	Temp3;
	ushort	R3;
}  NowResTemp;

// https://ru.wikipedia.org/wiki/%D0%A3%D1%80%D0%B0%D0%B2%D0%BD%D0%B5%D0%BD%D0%B8%D0%B5_%D0%A1%D1%82%D0%B5%D0%B9%D0%BD%D1%85%D0%B0%D1%80%D1%82%D0%B0_%E2%80%94_%D0%A5%D0%B0%D1%80%D1%82%D0%B0
// return Temterature
double ntcResult(double nowR, double Temp1, double R1, double Temp2, double R2, double Temp3, double R3){
	
	double 	L1 = log(R1),
			L2 = log(R2),
			L3 = log(R3);
			
	double 	Y1 = 1/Temp1,
			Y2 = 1/Temp2,
			Y3 = 1/Temp3;
			
	double 	h2 = (Y2 - Y1) / (L2 - L1),
			h3 = (Y3 - Y1) / (L3 - L1);
			
	double 	C = ((h3 - h2) / (L3 - L2)) * (1 / (L1 + L2 + L3)),
			B = h2 - C * (L1*L1 + L1*L2 + L2*L2),
			A = Y1 - (B + L1*L1*C) * L1;
	
	double tmp = log(nowR);
	
	return 1/(A + B * tmp + C * (tmp*tmp*tmp) );
}

ushort readADC( ubyte port ){
	// ONLY T1, T2 parameters
	
	ADMUX = port;
	ADCSRA = 0xC6;
	
	while( ADCSRA & ADSC );
	
	return ADC;
}

// Not Use, this for WriteDisplay.
void _DisplayOn( ubyte type, ubyte value ){
	
	/*
		type = 0 - DOP menu (play, pause, USB, SD, MHz, MP3, Points).
			value = 0 - play.
				  = 1 - pause.
				  = 2 - USB.
				  = 3 - SD.
				  = 4 - MHz.
				  = 5 - MP3.
				  = 6 - Points.
				  
		type = 1 - first 7-segment display
			( value < 10 ) if value > 9 is '-'
		type = 2 - second 7-segment display
		type = 3 - third 7-segment display
		type = 4 - fourth 7-segment display
	*/
	
	//PORTB = 0x3; // clear display
	//PORTD = 0x7F;
	
	//_delay_us(10);

	if( type == 0 ){
		
		switch(value){
			case 0:
				PORTD = ~0x1;
			break;
			case 1:
				PORTD = ~0x2;
			break;
			case 2:
				PORTD = ~0x4;
			break;
			case 3:
				PORTD = ~0x8;
			break;
			case 4:
				PORTD = ~0x20;
			break;
			case 5:
				PORTD = ~0x40;
			break;
			case 6:
				PORTD = ~0x10;
			break;
		}
		
		return;
	}
	
	switch(type){
		case 1:
			PORTB = 0x7;
		break;
		case 2:
			PORTB = 0xB;
		break;
		case 3:
			PORTB = 0x13;
		break;
		case 4:
			PORTB = 0x23;
		break;
	}
	
	
	
	// Не правильно компилица - default работает всегда(
	if(value > 9){ // вместо default
		PORTD = ~0x40;
		_delay_us(100);
	} else {
		switch(value){
			case 0:
				//PORTD = ~0x3F;
				PORTD = ~1;
				_delay_us(100);
				PORTD = ~2;
				_delay_us(100);
				PORTD = ~4;
				_delay_us(100);
				PORTD = ~8;
				_delay_us(100);
				PORTD = ~16;
				_delay_us(100);
				PORTD = ~32;
				_delay_us(100);
			break;
			case 1:
				//PORTD = ~0x6;
				PORTD = ~2;
				_delay_us(100);
				PORTD = ~4;
				_delay_us(100);
			break;
			case 2:
				//PORTD = ~0x5B;
				PORTD = ~1;
				_delay_us(100);
				PORTD = ~2;
				_delay_us(100);
				PORTD = ~8;
				_delay_us(100);
				PORTD = ~16;
				_delay_us(100);
				PORTD = ~64;
				_delay_us(100);
			break;
			case 3:
				//PORTD = ~0x4F;
				PORTD = ~1;
				_delay_us(100);
				PORTD = ~2;
				_delay_us(100);
				PORTD = ~4;
				_delay_us(100);
				PORTD = ~8;
				_delay_us(100);
				PORTD = ~64;
				_delay_us(100);
			break;
			case 4:
				//PORTD = ~0x66;
				PORTD = ~2;
				_delay_us(100);
				PORTD = ~4;
				_delay_us(100);
				PORTD = ~32;
				_delay_us(100);
				PORTD = ~64;
				_delay_us(100);
			break;
			case 5:
				//PORTD = ~0x6D;
				PORTD = ~1;
				_delay_us(100);
				PORTD = ~4;
				_delay_us(100);
				PORTD = ~8;
				_delay_us(100);
				PORTD = ~32;
				_delay_us(100);
				PORTD = ~64;
				_delay_us(100);
			break;
			case 6:
				//PORTD = ~0x7D;
				PORTD = ~1;
				_delay_us(100);
				PORTD = ~4;
				_delay_us(100);
				PORTD = ~8;
				_delay_us(100);
				PORTD = ~16;
				_delay_us(100);
				PORTD = ~32;
				_delay_us(100);
				PORTD = ~64;
				_delay_us(100);
			break;
			case 7:
				//PORTD = ~0x7;
				PORTD = ~1;
				_delay_us(100);
				PORTD = ~2;
				_delay_us(100);
				PORTD = ~4;
				_delay_us(100);
			break;
			case 8:
				//PORTD = ~0x7F;
				PORTD = ~1;
				_delay_us(100);
				PORTD = ~2;
				_delay_us(100);
				PORTD = ~4;
				_delay_us(100);
				PORTD = ~8;
				_delay_us(100);
				PORTD = ~16;
				_delay_us(100);
				PORTD = ~32;
				_delay_us(100);
				PORTD = ~64;
				_delay_us(100);
			break;
			case 9:
				//PORTD = ~0x6F;
				PORTD = ~1;
				_delay_us(100);
				PORTD = ~2;
				_delay_us(100);
				PORTD = ~4;
				_delay_us(100);
				PORTD = ~8;
				_delay_us(100);
				PORTD = ~32;
				_delay_us(100);
				PORTD = ~64;
				_delay_us(100);
			break;
		}
	}
	PORTD = 0x7F; // clear display
}

// Вывод на дисплей
void WriteDisplay( ubyte type, byte value ){
	/*
		type = 0 - DOP menu (play, pause, USB, SD, MHz, MP3, Points).
			value = 0 - play.
				  = 1 - pause.
				  = 2 - USB.
				  = 3 - SD.
				  = 4 - MHz.
				  = 5 - MP3.
				  = 6 - Points.
				  
		type = 1 - first temperature (T1).
			value = temperature (if value > 99 is "--", or value < -9 is "--").
		type = 2 - second temperature (T2).
	*/
	
	if( type != 0 ){
		
		if( value > 99 || value < -9 ){
			_DisplayOn( type, 10 );
			_DisplayOn( type+1, 10 );
		} else if( value < 0 ){
			value = -value;
			_DisplayOn( type, TNONE );
			_DisplayOn( type+1, value );
		} else {
			_DisplayOn( type, value / 10 );
			_DisplayOn( type+1, value % 10 );
		}
	} else
		_DisplayOn( type, value );
}

void Get3Res(ushort nowR){
	
}

byte GetT1(){
	
	
	
	return (byte) round( ntcResult(0, 0, 0, 0, 0, 0, 0) );
	
}

int main(void){
	
	// Init Timer/Counter0
	
	TIMSK = 1;
	TCCR0 =	_TCCR0_def;
	sei();
	
	// Init Pins
	
	DDRB  = 0x7C;
	PORTB = 0x7F;
	
	DDRC |= (1<<PC5);
	
	DDRD = 0x7F;
	
	// ====== CODE ======
	
    while(1){
		WriteDisplay(T1, T2MAX);
		WriteDisplay(T2, TNONE);
	}
}


ubyte BUTT_counter1_plus  = 0;
ubyte BUTT_counter2_minus = 0;

bool BUTT_WAS_PRESSED1 = false;
bool BUTT_WAS_HELD1	   = false;

bool BUTT_WAS_PRESSED2 = false;
bool BUTT_WAS_HELD2	   = false;

ubyte BUTT_counter1_held = 0;
ubyte BUTT_counter2_held = 0;

ISR(TIMER0_OVF_vect){
	DDRB  &= ~3;
	PORTB |= 3;
	
	if( !(PINB & 1) ){ // Button + was pressed
		if(BUTT_counter1_plus < 255)
			BUTT_counter1_plus++;
			
		if(BUTT_counter1_plus > BUTT_PRESSED_CNT && !BUTT_WAS_PRESSED1){ // MS_PRESSED
			BUTT_WAS_PRESSED1 = true;
			
			
		}
		
		if( BUTT_counter1_plus > BUTT_HELD_CNT ){ // MS_HELD
			BUTT_WAS_HELD1 = true;
			BUTT_counter1_held++;
			
			if( BUTT_counter1_held > BUTT_HELD_DELAY ){
				T2MAX += 10;
				BUTT_counter1_held = 0;
			}
			
		}
		
	} else { // Button + was released
		if( (BUTT_WAS_PRESSED1 == true) && (!BUTT_WAS_HELD1)  ){ // MS_RELEASED without held
			
			T2MAX++;
			
		}
		BUTT_WAS_PRESSED1 = false;
		BUTT_WAS_HELD1 = false;
		BUTT_counter1_plus = 0;
		BUTT_counter1_held = 0;
	}
	
	// =============== BUTTON 2 (-) =======================
	
	if( !(PINB & 2) ){ // Button - was pressed
		
		if(BUTT_counter2_minus < 255)
			BUTT_counter2_minus++;
		
		if(BUTT_counter2_minus > BUTT_PRESSED_CNT && !BUTT_WAS_PRESSED2){ // MS_PRESSED
			BUTT_WAS_PRESSED2 = true;
			
			
		}
		
		if( BUTT_counter2_minus > BUTT_HELD_CNT ){ // MS_HELD
			BUTT_WAS_HELD2 = true;
			BUTT_counter2_held++;
			
			if( BUTT_counter2_held > BUTT_HELD_DELAY ){
				T2MAX -= 10;
				BUTT_counter2_held = 0;
			}
		}
		
	} else { // Button - was released
		if( (BUTT_WAS_PRESSED2 == true) && (!BUTT_WAS_HELD2) ){ // MS_RELEASED
			
			T2MAX--;
			
		}
		BUTT_WAS_PRESSED2 = false;
		BUTT_WAS_HELD2 = false;
		BUTT_counter2_minus = 0;
		BUTT_counter2_held = 0;
	}
}