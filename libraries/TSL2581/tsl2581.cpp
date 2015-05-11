#include "tsl2581.h"

// lux equation approximation without floating point calculations
//////////////////////////////////////////////////////////////////////////////
// Routine: unsigned int CalculateLux(unsigned int ch0, unsigned int ch0)
//
// Description: Calculate the approximate illuminance (lux) given the raw
// channel values of the TSL2581. The equation if implemented
// as a piece−wise linear approximation.
//
// Arguments: unsigned int iGain − gain, where 0:1X, 1:8X, 2:16X, 3:128X
// unsigned int tIntCycles − INTEG_CYCLES defined in Timing Register
// unsigned int ch0 − raw channel value from channel 0 of TSL2581
// unsigned int ch1 − raw channel value from channel 1 of TSL2581
//
// Return: unsigned int − the approximate illuminance (lux)
//
//////////////////////////////////////////////////////////////////////////////
unsigned int tsl2581::CalculateLux(unsigned int iGain, unsigned int tIntCycles, unsigned int ch0, unsigned int ch1)
{
	//−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−
	// first, scale the channel values depending on the gain and integration time
	// 1X, 400ms is nominal setting
	unsigned long chScale0;
	unsigned long chScale1;
	unsigned long channel1;
	unsigned long channel0;
	
	// No scaling if nominal integration (148 cycles or 400 ms) is used
	if (tIntCycles == NOM_INTEG_CYCLE)
		chScale0 = (1UL << CH_SCALE);
	else
		chScale0 = (NOM_INTEG_CYCLE << CH_SCALE) / tIntCycles;
				
	switch (iGain)
	{
		case 0: // 1x gain
			chScale1 = chScale0; // No scale. Nominal setting
		break;
		case 1: // 8x gain
			chScale0 = chScale0 >> 3; // Scale/multiply value by 1/8
			chScale1 = chScale0;
		break;
		case 2: // 16x gain
			chScale0 = chScale0 >> 4; // Scale/multiply value by 1/16
			chScale1 = chScale0;
		break;
		case 3: // 128x gain
			chScale1 = chScale0 / CH1GAIN128X; //Ch1 gain correction factor applied
			chScale0 = chScale0 / CH0GAIN128X; //Ch0 gain correction factor applied
		break;
	}
	
	// scale the channel values
	channel0 = (ch0 * chScale0) >> CH_SCALE;
	channel1 = (ch1 * chScale1) >> CH_SCALE;
	
	//−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−
	// find the ratio of the channel values (Channel1/Channel0)
	// protect against divide by zero
	unsigned long ratio1 = 0;
	if (channel0 != 0) ratio1 = (channel1 << (RATIO_SCALE+1)) / channel0;
	
	// round the ratio value
	unsigned long ratio = (ratio1 + 1) >> 1;
	
	// is ratio <= eachBreak?
	unsigned int b, m;
	
	if ((ratio >= 0) && (ratio <= K1C))
		{b=B1C; m=M1C;}
	else if (ratio <= K2C)
		{b=B2C; m=M2C;}
	else if (ratio <= K3C)
		{b=B3C; m=M3C;}
	else if (ratio <= K4C)
		{b=B4C; m=M4C;}
	else if (ratio > K5C)
		{b=B5C; m=M5C;}
	
	unsigned long temp;
	unsigned long lux;
	
	temp = ((channel0 * b) - (channel1 * m));
	// round lsb (2^(LUX_SCALE−1))
	temp += (1UL << (LUX_SCALE-1));
	// strip off fractional portion
	lux = temp >> LUX_SCALE;
	return(lux);
}