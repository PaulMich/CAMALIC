/*
 * CAMALIC_v1_1.cpp
 *
 * Author : PaulMich
 * GITHUB: https://github.com/PaulMich/CAMALIC
 * LICENSE: https://github.com/PaulMich/CAMALIC/blob/master/LICENSE
 */ 

/************************
PIN DESCRIPTION

PORTB:
	PB0 -> not used (GND)
	PB1 -> PWM output (OC1A) for side mirrors lights
	PB2 -> PWM output (OC1B) for side edge lights
	PB3 -> not used (MOSI, OC2)
	PB4 -> not used (MISO)
	PB5 -> not used (SCK)
	PB6 -> digital output for RGB lights (HIGH when activated)
	PB7 -> digital output for front doors red warning lights (HIGH when activated)
	
PORTC:
	PC0 -> analog input from left photoresistor
	PC1 -> analog input from right photoresistor
	PC2 -> analog input from left photoresistor sensivity regulation pot
	PC3 -> analog input from right photoresistor sensivity regulation pot
	PC4 -> analog input from dimmer time regulation pot
	PC5 -> not used (GND)
	PC6 -> RESET
	
PORTD:
	PD0 -> digital input from reed sensor (HIGH when activated)
	PD1 -> digital input A from exterior lights mode toggle switch (LOW when activated)
	PD2 -> digital input B from exterior lights mode toggle switch (LOW when activated)
	PD3 -> digital input A from interior lights mode toggle switch (LOW when activated)
	PD4 -> digital input B from interior lights mode toggle switch (LOW when activated)
	PD5 -> not used (GND)
	PD6 -> digital input from reading lights (LOW when active)
	PD7 -> digital input from ignition (LOW when active)

*************************/

#define F_CPU 1000000L

#include <avr/io.h>
#include <avr/interrupt.h>

#include <util/delay.h>

long clockCounter = 0;


int f_TERMINATOR = 0;

int initPWM()
{
	DDRB |= _BV(1) | _BV(2);
	
	TCCR1A = _BV(WGM10);                   					//!< Fast PWM 8bit
	TCCR1B = _BV(WGM12);
	TCCR1A |= _BV(COM1A1) | _BV(COM1A0) | _BV(COM1B1) | _BV(COM1B0);	//!< Clear OC1A/OC1B on Compare Match, set OC1A/OC1B at BOTTOM
	TCCR1B |= _BV(CS11);							//!< Prescaler = 64  fpwm = 976,5Hz
	OCR1A = 0;								//!< side mirrors lights
	OCR1B = 0;
	
	return 0;
}

uint8_t initClock() //!< prescaler 8, 125tick, 131 start
{
	sei();
	
	TCCR0 |= _BV(CS01);
	TCNT0 = 131;
	TIMSK |= _BV(TOIE0);
	
	return 0;
}

ISR(TIMER0_OVF_vect)
{
	clockCounter++;
	TCNT0 = 0;
}

/*!
 * This functions makes measurements on ADC0, ADC1, ADC2, ADC3 and ADC4 inputs and saves them into 
 * provided variables.
 * \param adc0 - final result of ADC0 measurement
 * \param adc1 - final result of ADC1 measurement
 * \param adc2 - final result of ADC2 measurement
 * \param adc3 - final result of ADC3 measurement
 * \param adc4 - final result of ADC4 measurement
 */
int measureADC(int *adc0, int *adc1, int *adc2, int*adc3, int *adc4)
{
	int numberOfMeasurments = 20; 
	
	int sum_ADC0 = 0;
	int sum_ADC1 = 0;
	int sum_ADC2 = 0;
	int sum_ADC3 = 0;
	int sum_ADC4 = 0;

		
	ADCSRA = _BV(ADEN) | _BV(ADPS2); //!< enable ADC, prescaler equal to 8
	for(int measurmentsCounter = 0; measurmentsCounter < numberOfMeasurments; measurmentsCounter++)
	{
		ADMUX = _BV(REFS0);	//!< AVcc as Vref
							//!< left photoresistor connected to PC0
		ADCSRA |= _BV(ADSC); //!< start measuring
		while(ADCSRA & _BV(ADSC)); //!< wait until the end of measurement
		sum_ADC0 += ADC; //!< add the result to summarized value
	
		ADMUX = _BV(REFS0);	
		ADMUX |= _BV(MUX0);	//!< right photoresistor connected to PC1
		ADCSRA |= _BV(ADSC);
		while(ADCSRA & _BV(ADSC));
		sum_ADC1 += ADC;
	
		ADMUX = _BV(REFS0);
		ADMUX |= _BV(MUX1);	//!< left photoresistor sensivity regulation pot connected to PC2
		ADCSRA |= _BV(ADSC);
		while(ADCSRA & _BV(ADSC));
		sum_ADC2 += ADC;
		
		ADMUX = _BV(REFS0);
		ADMUX |= _BV(MUX0) | _BV(MUX1);	//!< right photoresistor sensivity regulation pot connected to PC3
		ADCSRA |= _BV(ADSC);
		while(ADCSRA & _BV(ADSC));
		sum_ADC3 += ADC;
		
		ADMUX = _BV(REFS0);
		ADMUX |= _BV(MUX2);	//!< dimmer time regulation pot connected to PC4
		ADCSRA |= _BV(ADSC);
		while(ADCSRA & _BV(ADSC));
		sum_ADC4 += ADC;
	}
	*adc0 = sum_ADC0 / numberOfMeasurments; //!< compute mean value of all samples and save as final result
	*adc1 = sum_ADC1 / numberOfMeasurments; 
	*adc2 = sum_ADC2 / numberOfMeasurments;
	*adc3 = sum_ADC3 / numberOfMeasurments;
	*adc4 = sum_ADC4 / numberOfMeasurments;

	
	return 0;
}

int checkDoorClosed()	//1-closed, 0-open
{
	if(PINB & _BV(0)) return 1;
	else return 0;				
}

int checkIgnition() //0-off, 1-on
{
	if(PIND & _BV(7)) return 0;
	else return 1;
}


int checkReadingLight() //0-off, 1-on
{
	if(f_TERMINATOR) return 0;
	if(PIND & _BV(6)) return 1;
 	else return 0;
}
/*
uint8_t checkReadingLight() //0-off, 1-on
{
	if(PIND & _BV(6))
	{
		clockCounter = 0;
		while(clockCounter < 90)
		{
			if( !(PIND & _BV(6)) ) return 0;
		}
		return 1;
	}
	else return 0;
}*/

#define EX_MODE1 1
#define EX_MODE2 2
#define EX_DISABLED 0 
int checkExteriorLightsMode()
{
	if( (PIND & _BV(1)) && !(PIND & _BV(2))) return EX_MODE1;
	else if( (PIND & _BV(2)) && !(PIND & _BV(1))) return EX_MODE2;
	else return EX_DISABLED;
}

#define IN_MODE1 1
#define IN_MODE2 2
#define IN_DISABLED 0
int checkInteriorLightsMode()
{
	if( (PIND & _BV(3)) && !(PIND & _BV(4))) return IN_MODE1;
	else if( (PIND & _BV(4)) && !(PIND & _BV(3))) return IN_MODE2;
	else return IN_DISABLED;
}

int enableRGB() 
{
	PORTB |= _BV(6);
	return 0; 
}

int disableRGB()
{
	PORTB &= ~_BV(6);
	return 0;
}

int enableRedLights()
{
	PORTB |= _BV(7);
	return 0;
}

int disableRedLights()
{
	PORTB &= ~_BV(7);
	return 0;
}

int enableSideMirrorLights()
{
	OCR1A = 0;
	return 0;
}

int disableSideMirrorLights()
{
	OCR1A = 255;
	return 0;
}

int enableSideEdgeLights()
{
	//PORTB |= _BV(2);
	OCR1B = 100;
	return 0;
}

int disableSideEdgeLights()
{
	//PORTB &= ~_BV(2);
	OCR1B = 255; 
	return 0;
}

int dimmMirrorLights(int duration_ms)
{
	for(int i=0; i<255; i++)
	{
		OCR1A = i;
		
		for(int k = 0; k < duration_ms; k++)
		{
			_delay_ms(1);
		}
	}
	return 0;
}

int brightMirrorLights(int duration_ms)
{
	for(int i=255; i>0; i--)
	{
		OCR1A = i;
		
		for(int k = 0; k < duration_ms; k++)
		{
			_delay_ms(1);
		}
	}
	return 0;
}

int init()
{
	//!DIGITAL INPUTS
	DDRD = 0x00;
	
	//!ANALOG IPUTS
	DDRC &= ~_BV(0);
	DDRC &= ~_BV(1);
	DDRC &= ~_BV(2);
	DDRC &= ~_BV(3);
	DDRC &= ~_BV(4);
	
	//!DIGITAL OUTPUTS
	DDRB |= _BV(6) | _BV(7);
	
	//!PWM OUTPUTS
	initPWM();
	disableRGB();
	disableRedLights();
	disableSideEdgeLights();
	disableSideMirrorLights();
	
	return 0;
}

int main(void)
{	
	int time_ms = 0;
	int timerOverflowCount = 0;
	int t_max = 0;
	
	int photoresistor_left = 0;
	int photoresistor_right = 0;
	int photoresistor_reg_left = 0;
	int photoresistor_reg_right = 0;
	int dimmer_time_reg = 0;
	
	int dimm_time = 0;
	int photoresitors_sensivity_lvl_left = 0;	
	int photoresitors_sensivity_lvl_right = 0;
	
	int f_isDark = 0;
	int f_isMirrorLightsOn = 0;
	int f_isEdgeLightsOn = 0;
	int f_enableMirrorLights = 0;
	int f_enableEdgeLights = 0;
	int f_isTimerEnabled = 0;
	int f_isRedLightsOn = 0;
	int f_isRGBon = 0;
	
	int f_wasDoorClosed = 1;
	
	init();
	
	
    while (1) 
    {
		//enableRGB();
		//enableRedLights();
		//enableSideEdgeLights();
		//enableSideMirrorLights();
		
		//if(checkReadingLight()) enableSideMirrorLights();
		//else dimmMirrorLights(5);
		
		measureADC(&photoresistor_left, &photoresistor_right, &photoresistor_reg_left,
					&photoresistor_reg_right, &dimmer_time_reg);
					
		dimm_time = dimmer_time_reg / 50;
		t_max = (dimm_time*1000) / ( ((1024*1000)/F_CPU)*255 ); // <------ ????
		
		photoresitors_sensivity_lvl_left = photoresistor_reg_left / 4 * 3;		//0-3.75[V]
		photoresitors_sensivity_lvl_right = photoresistor_reg_right / 4 * 3;	//
		
		/*
		if(photoresistor_left <= photoresitors_sensivity_lvl_left && photoresistor_right <= photoresitors_sensivity_lvl_right)
		{
			f_isDark = 1;		
		}
		else 
		{
			f_isDark = 0;
		}*/
		
		if(f_wasDoorClosed)
		{
			if(!f_isTimerEnabled)
			{
				initClock();
				f_isTimerEnabled = 1;
			}
			if(clockCounter > 58823) //po upływie 2[min]
			{
				f_isTimerEnabled = 0;
				f_TERMINATOR = 1;
			}
		}

		f_isDark = 1;
		
		switch(checkInteriorLightsMode())
		{
			case IN_MODE1:
				if(checkIgnition())
				{
					if(!f_isRGBon)
					{
						enableRGB();
						f_isRGBon = 1;
					}
				}
				if(!checkIgnition())
				{
					if(f_isRGBon)
					{
						disableRGB();
						f_isRGBon = 0;
					}
				}
				break;
				
			case IN_MODE2:	
				if(checkReadingLight() && !f_isRGBon)
				{
					enableRGB();
					f_isRGBon = 1;
					_delay_ms(800);
				}
				
				if(!checkReadingLight() && f_isRGBon)
				{
					disableRGB();
					f_isRGBon = 0;
					_delay_ms(800);
				}
				break;
				
			case IN_DISABLED:
				disableRGB();
				f_isRGBon = 0;
				break;
				
			default:
				disableRGB();
				f_isRGBon = 0;
				break;
		}
		
		switch(checkExteriorLightsMode())
		{
			case EX_MODE1:
				if(checkReadingLight() && !f_isRedLightsOn)
				{
					enableRedLights();
					f_isRedLightsOn = 1;
				}
				else if(!checkReadingLight() && f_isRedLightsOn)
				{
					disableRedLights();
					f_isRedLightsOn = 0;
				}
				if(!checkIgnition())
				{
					if(!checkDoorClosed())
					{
						if(f_wasDoorClosed && !f_isMirrorLightsOn)
						{
							brightMirrorLights(5);
							enableSideMirrorLights();
							f_isMirrorLightsOn = 1;
							f_wasDoorClosed = 0;
							f_TERMINATOR = 0;
						}
						
						if(checkReadingLight())
						{
							
							if(!f_isEdgeLightsOn)
							{
								enableSideEdgeLights();
								f_isEdgeLightsOn = 1;
							}
							if(!f_isMirrorLightsOn)
							{
								brightMirrorLights(5);
								enableSideMirrorLights();
								f_isMirrorLightsOn = 1;
							}
						}
						else
						{
							if(f_isEdgeLightsOn)
							{
								disableSideEdgeLights();
								f_isEdgeLightsOn = 0;
							}
							if(f_isMirrorLightsOn)
							{
								dimmMirrorLights(5);
								disableSideMirrorLights();
								f_isMirrorLightsOn = 0;
							}
						}
					}
					else
					{
						if(f_isEdgeLightsOn)
						{
							disableSideEdgeLights();
							f_isEdgeLightsOn = 0;
						}
						if(f_isMirrorLightsOn)
						{
							_delay_ms(30000);
							dimmMirrorLights(5);
							disableSideMirrorLights();
							f_isMirrorLightsOn = 0;
						}
						f_wasDoorClosed = 1;
					}
				}
				else
				{
					if(f_isMirrorLightsOn)
					{
						disableSideMirrorLights();
						f_isMirrorLightsOn = 0;
					}
				}
				break;
				
			case EX_MODE2:
				if(checkReadingLight())
				{
					if(!f_isRedLightsOn)
					{
						enableRedLights();
						f_isRedLightsOn = 1;
					}
				}
				else 
				{
					if(f_isRedLightsOn)
					{
						disableRedLights();
						_delay_ms(1500);
						f_isRedLightsOn = 0;
					}
				}
				break;
			
			case EX_DISABLED:
				disableSideMirrorLights();
				disableRedLights();
				disableSideEdgeLights();
				f_isMirrorLightsOn = 0;
				f_isRedLightsOn = 0;
				f_isEdgeLightsOn = 0;				
				break;
				
			default:
				disableSideMirrorLights();
				disableRedLights();
				disableSideEdgeLights();
				f_isMirrorLightsOn = 0;
				f_isRedLightsOn = 0;
				f_isEdgeLightsOn = 0;
				break;	
		}
		
    }
}

