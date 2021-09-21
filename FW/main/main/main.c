/*
 * main.c
 *
 * Created: 8/30/2021 5:03:13 PM
 * Author : Orest
 */ 


#define F_CPU					9600000UL / 8UL	// CPU frequency 9.6 MHz

#include <avr/io.h>
#include <stdbool.h>
#include <util/delay.h>
#include <avr/eeprom.h> 

#define POWER_MAX	255
#define POWER_HIGH	130
#define POWER_LOW	50
#define POWER_LIMIT	130

#define PIN_BUTTON_PLUS			PINB1
#define PIN_BUTTON_MINUS		PINB2
#define PIN_PARKLIGHT_SENSE		PINB4

#define BLINK_TIME_MS			0
#define COUNTER_PERIOD_MS		10		// Program counter period
#define OH_PROTECTION_TIME		1000	// Overheat protection time in counter periods
#define OHP_DEC_PERIOD_MS		500		// Overheat protection power decrease period

#define BUTTON_PLUS				!(PINB & (1 << PIN_BUTTON_PLUS))
#define BUTTON_MINUS			!(PINB & (1 << PIN_BUTTON_MINUS))
#define POWER_SET_STEP			5
#define BUTTON_DELAY_MS			200
#define ANTI_GLITCH_DELAY_MS	20

bool parklight;
bool settings_enabled = false;
unsigned char power_high;
unsigned char power_low;
unsigned char power;
unsigned int counter = 0;
uint8_t *eeprom_addr_power;

// EEPROM
#define EEPROM_ADDR_POWER_HIGH		0U
#define EEPROM_ADDR_POWER_LOW		1U

bool button_plus();

int main(void)
{
    DDRB |= 1 << PINB0;					// Configure PINB0 as output
	PORTB |= 1 << PIN_PARKLIGHT_SENSE;	// Connect pull-up to PINB4
	PORTB |= 1 << PIN_BUTTON_PLUS;		// Connect pull-up to PINB1
	PORTB |= 1 << PIN_BUTTON_MINUS;		// Connect pull-up to PINB2
		
	// Configure PWM
	TCCR0B |= 1 << CS00;					// Set Timer 0 prescaler to clock/1 (No prescaling)
	TCCR0A |= (1 << WGM01) | (1 << WGM00);	// Set to 'Fast PWM' mode	
	TCCR0A |= (1 << COM0A1);				// Clear OC0A output on compare match, upwards counting.
	
	// Max power blink on the start
	OCR0A = POWER_MAX;
	_delay_ms(BLINK_TIME_MS);
	
	// Load and validate settings from EEPROM
	power_high = eeprom_read_byte((uint8_t*)EEPROM_ADDR_POWER_HIGH);
	power_low = eeprom_read_byte((uint8_t*)EEPROM_ADDR_POWER_LOW);	
	if((power_high == 0) | (power_high > POWER_LIMIT)) {power_high = POWER_HIGH;}
	if((power_low == 0) | (power_low > POWER_LIMIT)) {power_low = POWER_LOW;}
		
	parklight = !(PINB & (1 << PIN_PARKLIGHT_SENSE));	// Get parklight status. Pin low if parklight active.
	if (parklight)
	{
		power = power_low;
		eeprom_addr_power = (uint8_t*)EEPROM_ADDR_POWER_LOW;
	}
	else
	{
		power = power_high;
		eeprom_addr_power = (uint8_t*)EEPROM_ADDR_POWER_HIGH;
	}
	OCR0A = power;
	
    while (1) 
    {
		if(BUTTON_PLUS & BUTTON_MINUS & (counter < OH_PROTECTION_TIME)) // Enable settings (only if OHP not active)
		{
			settings_enabled = true;
			_delay_ms(BUTTON_DELAY_MS);
		}	
		if((BUTTON_PLUS | BUTTON_MINUS) & settings_enabled)
		{
			_delay_ms(ANTI_GLITCH_DELAY_MS);
			if(BUTTON_PLUS)
			{
				power += POWER_SET_STEP;
				_delay_ms(BUTTON_DELAY_MS);
			}
			if(BUTTON_MINUS)
			{
				if(power > POWER_SET_STEP) {power -= POWER_SET_STEP;}
				_delay_ms(BUTTON_DELAY_MS);
			}
			if(power > POWER_LIMIT) {power = POWER_LIMIT;}		// Ensure power does not exceed limitations
			eeprom_write_byte(eeprom_addr_power, power);		// Save new settings to EEPROM
		}
		OCR0A = power;
		
		// Overheat protection
		if(!settings_enabled)	// Enable OHP only if settings disabled
		{	
			if(counter < OH_PROTECTION_TIME)
			{
				counter++;
				_delay_ms(COUNTER_PERIOD_MS);
			}
			else
			{
				if(power > POWER_LOW) {power--;}
				_delay_ms(OHP_DEC_PERIOD_MS);
			}
		}
	}
}


