#include "ads12xx.h"

volatile int DRDY_state = HIGH;

// Waits until DRDY Pin is falling (see Interrupt setup). 
// Some commands like WREG, RREG need the DRDY to be low.
void waitforDRDY() {
	while(DRDY_state) {		
	}
	//continue;
	noInterrupts();
	DRDY_state = HIGH;
	interrupts();
}

//Interrupt function
void DRDY_Interuppt() {
	DRDY_state = LOW;
}


// ads12xx setup
ads12xx::ads12xx() {}

void ads12xx::begin(int CS, int START, int DRDY) {
	pinMode(CS, OUTPUT);              // set the slaveSelectPin as an output:
	digitalWrite(CS, HIGH); // CS HIGH = nothing selected

#ifdef ADS1248
#define	spimodeX SPI_MODE1
#endif
#ifdef ADS1256
#define	spimodeX SPI_MODE1
#endif
#ifdef ADS1258
#define	spimodeX SPI_MODE3
#endif

	SPI.begin();

#ifdef ENERGIA
	SPI.setBitOrder(MSBFIRST);
	SPI.setDataMode(spimodeX);
	SPI.setClockDivider(SPI_CLOCK_DIV8); //For 25 MHz F5529, (8) does not work either
#endif

	pinMode(DRDY, INPUT);             // DRDY read
	pinMode(START, OUTPUT);
	digitalWrite(START, LOW);        // HIGH = Start Convert Continuously
	delay(500);
	digitalWrite(START, HIGH);
	_CS = CS;
	_DRDY = DRDY;
	delay(500);
//	SPI.begin();
//	attachInterrupt(0, DRDY_Interuppt, FALLING); //Interrupt setup for DRDY detection ARDUINO
	attachInterrupt(_DRDY, DRDY_Interuppt, FALLING); //Interrupt setup for DRDY detection ENERGIA
    DRDY_state = digitalRead(_DRDY);
	delay(500);

	// interal VREF einschalten
}


// function to get a 3byte conversion result from the ADS1248 or ADS1256 adc
long ads12xx::GetConversion() {
	long regData;
	uint8_t byte0;
	uint8_t byte1;
	uint8_t byte2;
	waitforDRDY(); // Wait until DRDY is LOW
#ifndef ENERGIA
	SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, spimodeX)); 
#endif
	digitalWrite(_CS, LOW); //Pull SS Low to Enable Communications with ADS1247
	delayMicroseconds(10); // RD: Wait 25ns for ADC12xx to get ready
	SPI.transfer(RDATA); //Issue RDATA

	SPI.transfer(NOP);
	byte0=SPI.transfer(NOP); 
	byte1=SPI.transfer(NOP);
	byte2=SPI.transfer(NOP);
	regData = ((((long)byte0<<24) | ((long)byte1<<16) | ((long)byte2<<8)) >> 8); //as data from ADC comes in twos complement

	digitalWrite(_CS, HIGH);
#ifndef ENERGIA
	SPI.endTransaction();
#endif
	

	return regData;
}

// function to get a 3byte conversion result and the status byte from the ADS1258 ADC
	void ads12xx::GetConversion1258(uint8_t *statusByte, int32_t *regData) {
	uint8_t byte0;
	uint8_t byte1;
	uint8_t byte2;
    waitforDRDY(); // Wait until DRDY is LOW
#ifndef ENERGIA
	SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, spimodeX)); 
#endif
    digitalWrite(_CS, LOW); //Pull SS Low to Enable Communications with ADS1247
	SPI.transfer(RDATA); //Issue RDATA

 	*statusByte=SPI.transfer(NOP);
	byte0=SPI.transfer(NOP); 
	byte1=SPI.transfer(NOP);
	byte2=SPI.transfer(NOP);
	//singleConv->
	*regData = ((((long)byte0<<24) | ((long)byte1<<16) | ((long)byte2<<8)) >> 8); //as data from ADC comes in twos complement

	digitalWrite(_CS, HIGH);
#ifndef ENERGIA
	SPI.endTransaction();
#endif
//	noInterrupts();
//	delayMicroseconds(10);
//	DRDY_state = HIGH;
//	interrupts();
}


// function to write a register value to the adc
// argumen: adress for the register to write into, value to write
void ads12xx::SetRegisterValue(uint8_t regAdress, uint8_t regValue) {
#ifdef ADS1248
	if (regAdress == IDAC0) {
		regValue = regValue | IDAC0_ID;	  // add non 0 non-write register value IDAC0_ID
	}
#endif
	uint8_t regValuePre = ads12xx::GetRegisterValue(regAdress);
	if (regValue != regValuePre) {
		//digitalWrite(_START, HIGH);
		delayMicroseconds(10);
		waitforDRDY();
#ifndef ENERGIA
		SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, spimodeX)); // initialize SPI with SPI_SPEED, MSB first, SPI Mode1
#endif
		digitalWrite(_CS, LOW);
		delayMicroseconds(10);
		SPI.transfer(WREG | regAdress); // send 1st command byte, address of the register
#ifndef ADS1258
		SPI.transfer(0x00);		// send 2nd command byte, write only one register
#endif
		SPI.transfer(regValue);         // write data (1 Byte) for the register
		delayMicroseconds(10);
		digitalWrite(_CS, HIGH);
		//digitalWrite(_START, LOW);
		if (regValue != ads12xx::GetRegisterValue(regAdress)) {   //Check if write was succesfull
			Serial.print("Write to Register 0x");
			Serial.print(regAdress, HEX);
			Serial.println(" failed!");
		}
#ifndef ENERGIA
		SPI.endTransaction();
#endif
		
	}

}


//function to read a register value from the adc
//argument: adress for the register to read
//To do: implement multiple register read for ADS1258
unsigned long ads12xx::GetRegisterValue(uint8_t regAdress) {
	//digitalWrite(_START, HIGH);
	waitforDRDY();
#ifndef ENERGIA
	SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, spimodeX)); // initialize SPI with 4Mhz clock, MSB first, SPI Mode0
#endif
	uint8_t bufr;
	digitalWrite(_CS, LOW);
	delayMicroseconds(10);
	SPI.transfer(RREG | regAdress); // send 1st command byte, address of the register
#ifndef ADS1258
//	SPI.transfer(0x00);			// send 2nd command byte, read only one register
#endif
	delayMicroseconds(10);
	bufr = SPI.transfer(NOP);	// read data of the register
	delayMicroseconds(10);
	digitalWrite(_CS, HIGH);
#ifndef ENERGIA
	SPI.endTransaction();
#endif
	return bufr;
}

/*
Sends a Command to the ADC
Like SELFCAL, GAIN, SYNC, WAKEUP
*/
void ads12xx::SendCMD(uint8_t cmd) {
	waitforDRDY();
#ifndef ENERGIA
	SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, spimodeX)); // initialize SPI with 4Mhz clock, MSB first, SPI Mode0
#endif
	digitalWrite(_CS, LOW);
	delayMicroseconds(10);
	SPI.transfer(cmd);
	delayMicroseconds(10);
	digitalWrite(_CS, HIGH);
#ifndef ENERGIA
	SPI.endTransaction();
#endif
}


// function to reset the adc
void ads12xx::Reset() {

	//Pulse reset line.
/*	pinMode(RESET_PIN, OUTPUT);
	digitalWrite(RESET_PIN, HIGH);
	digitalWrite(RESET_PIN, LOW);
	delayMicroseconds(100);
	digitalWrite(RESET_PIN, HIGH); //Reset line must be high before continue
	delay(1); //RESET high to SPI communication start*/
#ifndef ENERGIA
	SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, spimodeX)); // initialize SPI with  clock, MSB first, SPI Mode1
#endif
	digitalWrite(_CS, LOW);
	delayMicroseconds(10);
	SPI.transfer(RESET); //Reset
	delay(2); //Minimum 0.6ms required for Reset to finish.
	SPI.transfer(SDATAC); //Issue SDATAC
	delayMicroseconds(100);
	digitalWrite(_CS, HIGH);
#ifndef ENERGIA
	SPI.endTransaction();
#endif
}

