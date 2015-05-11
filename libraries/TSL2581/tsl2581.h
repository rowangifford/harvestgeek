#ifndef tsl2581_h
#define tsl2581_h

#if ARDUINO < 100
#include <WProgram.h>
#else
#include <Arduino.h>
#endif

//****************************************************************************
//
// Copyright 2004−2008 TAOS, Inc.
//
// THIS CODE AND INFORMATION IS PROVIDED ”AS IS” WITHOUT WARRANTY OF ANY
// KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
// PURPOSE.
//
// Module Name:
// tsl2581.cpp
//
//****************************************************************************
#define LUX_SCALE 16 // scale by 2^16
#define RATIO_SCALE 9 // scale ratio by 2^9
//−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−
// Integration time scaling factors
//−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−
#define CH_SCALE 16 // scale channel values by 2^16
#define NOM_INTEG_CYCLE 148 // Nominal 400 ms integration. See Timing Register
//−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−
// Gain scaling factors
//−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−
#define CH0GAIN128X 107 // 128X gain scalar for Ch0
#define CH1GAIN128X 115 // 128X gain scalar for Ch1
//−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−
// FN Package coefficients
//−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−
// For Ch1/Ch0=0.00 to 0.30:
// Lux=0.130*Ch0−0.240*Ch1
//
// For Ch1/Ch0=0.30 to 0.38:
// Lux=0.1649*Ch0−0.3562*Ch1
//
// For Ch1/Ch0=0.38 to 0.45:
// Lux=0.0974*Ch0−0.1786*Ch1
//
// For Ch1/Ch0=0.45 to 0.54:
// Lux=0.062*Ch0−0.10*Ch1
//
// For Ch1/Ch0>0.54:
// Lux/Ch0=0
//
//−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−
#define K1C 0x009A // 0.30 * 2^RATIO_SCALE
#define B1C 0x2148 // 0.130 * 2^LUX_SCALE
#define M1C 0x3d71 // 0.240 * 2^LUX_SCALE
#define K2C 0x00c3 // 0.38 * 2^RATIO_SCALE
#define B2C 0x2a37 // 0.1649 * 2^LUX_SCALE
#define M2C 0x5b30 // 0.3562 * 2^LUX_SCALE
#define K3C 0x00e6 // 0.45 * 2^RATIO_SCALE
#define B3C 0x18ef // 0.0974 * 2^LUX_SCALE
#define M3C 0x2db9 // 0.1786 * 2^LUX_SCALE
#define K4C 0x0114 // 0.54 * 2^RATIO_SCALE
#define B4C 0x0fdf // 0.062 * 2^LUX_SCALE
#define M4C 0x199a // 0.10 * 2^LUX_SCALE
#define K5C 0x0114 // 0.54 * 2^RATIO_SCALE
#define B5C 0x0000 // 0.00000 * 2^LUX_SCALE
#define M5C 0x0000 // 0.00000 * 2^LUX_SCALE

class tsl2581
{
public:
    unsigned int CalculateLux(unsigned int iGain, unsigned int tIntCycles, unsigned int ch0, unsigned int ch1);
	
};
#endif