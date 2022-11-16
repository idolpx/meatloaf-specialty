

#include "pcf8575.h"

#include <hal/gpio_types.h>

#include "../../include/debug.h"


PCF8575::PCF8575() :
		_PIN(0), _oldPIN(0), _PORT(0), _DDR(0), _address(0)
{
}

void PCF8575::begin(uint8_t address, gpio_num_t sda, gpio_num_t scl, uint16_t speed) {

	/* Store the I2C address and init the Wire library */
	_address = address;
    myI2C.begin(sda, scl, (speed * 1000)); // 400Khz Default
    //myI2C.begin(GPIO_NUM_21, GPIO_NUM_22, 800000); // 800Khz Overclocked
    //myI2C.begin(GPIO_NUM_21, GPIO_NUM_22, 1000000); // 1Mhz Overclocked
    //myI2C.setTimeout(10);
    //myI2C.scanner();

	readGPIO();
}

void PCF8575::pinMode(uint8_t pin, uint8_t mode) {

	/* Switch according mode */
	if ( mode == GPIO_MODE_INPUT )
	{
		_DDR |= (1 << pin);
		_PORT &= ~(1 << pin);		
	}
	else if ( mode == (GPIO_MODE_INPUT | GPIO_PULLUP_ENABLE) ) 
	{
		_DDR &= (1 << pin);
		_PORT |= ~(1 << pin);		
	}
	else if ( mode == GPIO_MODE_OUTPUT ) {
		_DDR &= ~(1 << pin);
		_PORT &= ~(1 << pin);
	} 

	/* Update GPIO values */
	updateGPIO();
}

void PCF8575::portMode(port_t port, uint8_t mode) {

	if ( port == PCF8575_PORT0 )
	{
		// Modify P07-P00 (low byte)
		if ( mode == GPIO_MODE_INPUT )
		{
			_DDR = (_DDR & 0xFF00) | 0x00FF;  // set low byte to 0xFF
		}
		else if ( mode == (GPIO_MODE_INPUT | GPIO_PULLUP_ENABLE) ) 
		{
			_DDR = (_DDR & 0xFF00) | 0x00FF;  // set low byte to 0xFF
		}
		else if ( mode == GPIO_MODE_OUTPUT ) 
		{
			_DDR &= 0xFF00;           // set low byte to 0x00
		}
	}
	else if ( port == PCF8575_PORT1 )
	{
		// Modify P17-P10 (high byte)
		if ( mode == GPIO_MODE_INPUT )
		{
			_DDR = (_DDR & 0x00FF) | 0xFF00;  // set high byte to 0xFF
		}
		else if ( mode == (GPIO_MODE_INPUT | GPIO_PULLUP_ENABLE) ) 
		{
			_DDR = (_DDR & 0x00FF) | 0xFF00;  // set high byte to 0xFF
		}
		else if ( mode == GPIO_MODE_OUTPUT ) 
		{
			_DDR &= 0x00FF;           // set high byte to 0x00
		}
	}
	else if ( port == PCF8575_BOTH )
	{
		_DDR = 0xFFFF;
	}

	_PORT = ~_DDR;

	Debug_printv("port[%d] mode[%d] ddr[%d]", port, mode, _DDR);

	/* Update GPIO values */
	updateGPIO();
}

void PCF8575::digitalWrite(uint8_t pin, uint8_t value) {

	/* Set PORT bit value */
	if (value)
		_PORT |= (1 << pin);
	else
		_PORT &= ~(1 << pin);

	/* Update GPIO values */
	updateGPIO();
}

uint8_t PCF8575::digitalRead(uint8_t pin) {

	/* Read GPIO */
	readGPIO();

	/* Read and return the pin state */
	return (_PIN & (1 << pin));
}


void PCF8575::write(uint16_t value, port_t port) {
	/* Store pins values and apply */
	if ( port == PCF8575_PORT0)
		// low byte swap
		_PORT &= 0xFF00 | (value & 0x00FF);
	else if ( port == PCF8575_PORT1 )
		// hight byte swap
		_PORT &= 0x00FF | (value << 8 & 0xFF00);
	else
		_PORT = value;

	/* Update GPIO values */
	updateGPIO();
}

uint16_t PCF8575::read(port_t port) {
	/* Read GPIO */
	readGPIO();

	/* Return current pins values */
	if ( port == PCF8575_PORT0)
		return PORT0;
	else if ( port == PCF8575_PORT1)
		return PORT1;
	else
		return _PORT;
}

void PCF8575::clear(port_t port) {

	/* User friendly wrapper for write() */
	if ( port == PCF8575_BOTH )
		write(0x0000);
	else
		write(0x00, port);
}

void PCF8575::set(port_t port) {

	/* User friendly wrapper for write() */
	if ( port == PCF8575_BOTH )
		write(0xFFFF);
	else
		write(0xFF, port);
}

void PCF8575::toggle(uint8_t pin) {

	/* Toggle pin state */
	_PORT ^= (1 << pin);

	/* Update GPIO values */
	updateGPIO();
}


void PCF8575::readGPIO() {

	_oldPIN = _PIN;

	uint8_t buffer[2];

	// Read two bytes
	myI2C.readBytes(_address, 2, buffer);
	_PIN = buffer[0];                            // low byte
	_PIN = (_PIN & 0x00FF) | ( (buffer[1] << 8) & 0xFF00);  // high byte
	_PIN &= ~_DDR;

	this->PORT0 = buffer[0];
	this->PORT1 = buffer[1];
	Debug_printv("_address[%.2x] low[%d] high[%d] _pin[%d]", _address, PORT0, PORT1, _PIN);
}


void PCF8575::updateGPIO() {

	uint8_t buffer[2];

	/* Read current GPIO states */
	//readGPIO(); // Experimental

	/* Compute new GPIO states */
	//uint8_t value = ((_PIN & ~_DDR) & ~(~_DDR & _PORT)) | _PORT; // Experimental
	//Debug_printv("address[%.2x] in[%d] out[%d] ddr[%d]", _address, _PIN, _PORT, _DDR);
	uint16_t value = (_PIN & ~_DDR) | _PORT;
	//Debug_printv("address[%.2x] in[%d] out[%d] ddr[%d] value[%d]", _address, _PIN, _PORT, _DDR, value);

	// Write two bytes
	buffer[0] = value & 0x00FF;  // low byte
	buffer[1] = value >> 8;      // high byte
	//Debug_printv("low[%.2x] high[%.2x]", buffer[0], buffer[1]);
	myI2C.writeBytes(_address, 2, buffer);
	//Debug_printv("address[%.2x] in[%d] out[%d] ddr[%d] value[%d]", _address, _PIN, _PORT, _DDR, value);
}