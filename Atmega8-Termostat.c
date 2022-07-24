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

#define Vcc		5		// Input voltage
#define R1		10000	// R1 in Ohms
#define Vref	Vcc		// Reference voltage for ADC

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

#define MENU_TIME 5 // delay in seconds

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

#define ADC0 0
#define ADC1 1

#define T1_ADC ADC0
#define T2_ADC ADC1

// for WriteDisplay
#define T1_Print	1	// Termoresistor 1 (Кател)
#define T2_Print	3	// Termoresistor 2 (Бойлер)
#define TNONE_Print 100 // print '-'

byte T2MAX = 60; // Максимальная температура бойллера

// https://www.gotronic.fr/pj2-mf52type-1554.pdf

// https://ru.wikipedia.org/wiki/%D0%A3%D1%80%D0%B0%D0%B2%D0%BD%D0%B5%D0%BD%D0%B8%D0%B5_%D0%A1%D1%82%D0%B5%D0%B9%D0%BD%D1%85%D0%B0%D1%80%D1%82%D0%B0_%E2%80%94_%D0%A5%D0%B0%D1%80%D1%82%D0%B0

const double	ntcA = 0.001120098911110, // For formula (GetTempByVolt)
				ntcB = 0.000236277875190,
				ntcC = 0.000000073729442;

byte GetTempByVolt( float Volt ){
	double ln = log( (R1*Volt)/(Vcc - Volt) );
	return (byte)( round(1/( ntcA + ntcB * ln + ntcC * (ln*ln*ln) ) - 273.15) );
}

float GetVoltByADC( ushort ADC_IN ){
	return (float)(ADC_IN) * Vref / 1024;
}

ushort readADC( ubyte port ){
	// ONLY T1, T2 parameters
	
	ADMUX = (1 << REFS0) | port;
	ADCSRA = (1 << ADEN) | (1 << ADSC);
	
	while( ADCSRA & (1 << ADSC) );
	
	return ADC;
}

// Not Use, this for WriteDisplay.
void _DisplayOn( ubyte type, ubyte value ){
	
	/*	  
		type = 1 - first 7-segment display
			( value < 10 ) if value > 9 is '-'
		type = 2 - second 7-segment display
		type = 3 - third 7-segment display
		type = 4 - fourth 7-segment display
	*/
	
	//PORTB = 0x3; // clear display
	//PORTD = 0x7F;
	
	//_delay_us(10);
	
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

// Print on display
void WriteDisplay( ubyte type, byte value ){
	/*	  
		type = 1 - first temperature (T1).
			value = temperature (if value > 99 is "--", or value < -9 is "--").
		type = 2 - second temperature (T2).
	*/
	if( value > 99 || value < -9 ){
		_DisplayOn( type, 10 );
		_DisplayOn( type+1, 10 );
	} else if( value < 0 ){
		value = -value;
		_DisplayOn( type, TNONE_Print );
		_DisplayOn( type+1, value );
	} else {
		_DisplayOn( type, value / 10 );
		_DisplayOn( type+1, value % 10 );
	}
}

byte TemperatureT1 = 0;
byte TemperatureT2 = 0;

bool T2MAX_MANU_ON = false;

bool RELEYON = false;

bool CanReleyOn = true;

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
		DDRB |= (1 << PB6);
		PORTB |= (1 << PB6);
		
		DDRD |= (1<<PD0);
		PORTD | (1<<PD0);
		
		if( T2MAX_MANU_ON == true ){
			
			WriteDisplay(T2_Print, T2MAX);
			
		} else {
			WriteDisplay(T1_Print, TemperatureT1);
			
			WriteDisplay(T2_Print, TemperatureT2);	
		}
		
		if( RELEYON == true ){
			
			// DISPLAY -> PLAY = ON
			DDRB |= (1 << PB6);
			PORTB |= (1 << PB6);
			
			DDRD |= (1 << PD0);
			PORTD &= ~(1 << PD0);
			
			// RELEY = ON
			DDRC |= (1 << PC5);
			PORTC |= (1 << PC5);
			
			_delay_us(100);
		} else {
			PORTC &= ~(1 << PC5);
		}
		
		if( (TemperatureT1 > TemperatureT2 + 10) && (CanReleyOn == true) && ( RELEYON == false ) )
			RELEYON = true;
			
		if( (TemperatureT1 < TemperatureT2 + 3) && (CanReleyOn == true) )
			RELEYON = false;
			
			
			
		if( (TemperatureT2 > T2MAX) && (CanReleyOn == true) ){
			RELEYON = false;
			CanReleyOn = false;
		}
		
		if( TemperatureT2 < T2MAX - 5 ) CanReleyOn = true;
		
	}
}


// Handle buttons and ADC, T2MAX_menu

ubyte BUTT_counter1_plus  = 0;
ubyte BUTT_counter2_minus = 0;

bool BUTT_WAS_PRESSED1 = false;
bool BUTT_WAS_HELD1	   = false;

bool BUTT_WAS_PRESSED2 = false;
bool BUTT_WAS_HELD2	   = false;

ubyte BUTT_counter1_held = 0;
ubyte BUTT_counter2_held = 0;

ubyte TADCREAD = 0;

ushort menu_counter = 0;

ISR(TIMER0_OVF_vect){
	DDRB  &= ~3;
	PORTB |= 3;
	
	// ======================== BUTTON 2 (+) =======================
	if( !(PINB & 1) ){ // Button + was pressed
		menu_counter = 0;
		
		if(BUTT_counter1_plus < 255)
			BUTT_counter1_plus++;
			
		if(BUTT_counter1_plus > BUTT_PRESSED_CNT && !BUTT_WAS_PRESSED1){ // MS_PRESSED
			BUTT_WAS_PRESSED1 = true;
			
			T2MAX_MANU_ON = true;
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
		BUTT_WAS_PRESSED1	= false;
		BUTT_WAS_HELD1		= false;
		BUTT_counter1_plus	= 0;
		BUTT_counter1_held	= 0;
	}
	
	// ======================== BUTTON 2 (-) =======================
	
	if( !(PINB & 2) ){ // Button - was pressed
		menu_counter = 0;
		
		if(BUTT_counter2_minus < 255)
			BUTT_counter2_minus++;
		
		if(BUTT_counter2_minus > BUTT_PRESSED_CNT && !BUTT_WAS_PRESSED2){ // MS_PRESSED
			BUTT_WAS_PRESSED2 = true;
			
			T2MAX_MANU_ON = true;
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
		BUTT_WAS_PRESSED2	= false;
		BUTT_WAS_HELD2		= false;
		BUTT_counter2_minus	= 0;
		BUTT_counter2_held	= 0;
	}
	
	// =================== ADC =================
	
	if( TADCREAD > 100 ){ // ~1 second
		TADCREAD = 0;
		
		TemperatureT1 = GetTempByVolt(GetVoltByADC(readADC(T1_ADC)));
		if( TemperatureT1 < 5 ) TemperatureT1 = TNONE_Print;
		
		TemperatureT2 = GetTempByVolt(GetVoltByADC(readADC(T2_ADC)));
		if( TemperatureT2 < 5 ) TemperatureT2 = TNONE_Print;
		
	} else TADCREAD++;
	
	// ======================== MENU =====================
	
	if( T2MAX_MANU_ON == true )
		menu_counter++;
		
	if( menu_counter > MENU_TIME*100 ){
		T2MAX_MANU_ON = false;
		menu_counter = 0;
	}
	
}