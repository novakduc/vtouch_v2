/**
  TMR5 Generated Driver File

  @Company
    Microchip Technology Inc.

  @File Name
    tmr5.c

  @Summary
    This is the generated driver implementation file for the TMR5 driver using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  @Description
    This source file provides APIs for TMR5.
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.77
	Device            :  PIC18F57K42
	Driver Version    :  2.11
    The generated drivers are tested against the following:
        Compiler          :  XC8 2.05 and above
        MPLAB 	          :  MPLAB X 5.20
 */

/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
    may accompany Microchip software.
    
    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER 
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY 
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS 
    FOR A PARTICULAR PURPOSE.
    
    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP 
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO 
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL 
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT 
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS 
    SOFTWARE.
 */

/**
  Section: Included Files
 */

#include <xc.h>
#include "tmr5.h"
#include "interrupt_manager.h"
#include "../vconfig.h"
#include "../mbmc.h"

/**
  Section: Global Variables Definitions
 */
volatile uint16_t timer5ReloadVal;
void (*TMR5_InterruptHandler)(void);
extern V_data V;
extern volatile C_data C;

/**
  Section: TMR5 APIs
 */

void TMR5_Initialize(void)
{
	//Set the Timer to the options selected in the GUI

	//T5GE disabled; T5GTM disabled; T5GPOL low; T5GGO done; T5GSPM disabled; 
	T5GCON = 0x00;

	//GSS T5G_pin; 
	T5GATE = 0x00;

	//CS FOSC/4; 
	T5CLK = 0x01;

	//TMR5H 99; 
	TMR5H = 0x63;

	//TMR5L 192; 
	TMR5L = 0xC0;

	// Load the TMR value to reload variable
	timer5ReloadVal = (uint16_t) ((TMR5H << 8) | TMR5L);

	// Clearing IF flag before enabling the interrupt.
	PIR8bits.TMR5IF = 0;

	// Enabling TMR5 interrupt.
	PIE8bits.TMR5IE = 1;

	// Set Default Interrupt Handler
	TMR5_SetInterruptHandler(TMR5_DefaultInterruptHandler);

	// CKPS 1:2; NOT_SYNC synchronize; TMR5ON enabled; T5RD16 disabled; 
	T5CON = 0x11;
}

void TMR5_StartTimer(void)
{
	// Start the Timer by writing to TMRxON bit
	T5CONbits.TMR5ON = 1;
}

void TMR5_StopTimer(void)
{
	// Stop the Timer by writing to TMRxON bit
	T5CONbits.TMR5ON = 0;
}

uint16_t TMR5_ReadTimer(void)
{
	uint16_t readVal;
	uint8_t readValHigh;
	uint8_t readValLow;

	T5CONbits.T5RD16 = 1;

	readValLow = TMR5L;
	readValHigh = TMR5H;

	readVal = ((uint16_t) readValHigh << 8) | readValLow;

	return readVal;
}

void TMR5_WriteTimer(uint16_t timerVal)
{
	if (T5CONbits.NOT_SYNC == 1) {
		// Stop the Timer by writing to TMRxON bit
		T5CONbits.TMR5ON = 0;

		// Write to the Timer5 register
		TMR5H = (timerVal >> 8);
		TMR5L = timerVal;

		// Start the Timer after writing to the register
		T5CONbits.TMR5ON = 1;
	} else {
		// Write to the Timer5 register
		TMR5H = (timerVal >> 8);
		TMR5L = timerVal;
	}
}

void TMR5_Reload(void)
{
	TMR5_WriteTimer(timer5ReloadVal);
}

void TMR5_StartSinglePulseAcquisition(void)
{
	T5GCONbits.T5GGO = 1;
}

uint8_t TMR5_CheckGateValueStatus(void)
{
	return(T5GCONbits.T5GVAL);
}

void __interrupt(irq(TMR5), base(8), low_priority) TMR5_ISR()
{
	static volatile unsigned int CountCallBack = 0;

	// Clear the TMR5 interrupt flag
	PIR8bits.TMR5IF = 0;
	TMR5_WriteTimer(timer5ReloadVal);

	// callback function - called every 100th pass
	if (++CountCallBack >= (TMR5_INTERRUPT_TICKER_FACTOR + V.ticker)) {
		// ticker function call
		TMR5_CallBack();

		// reset ticker counter
		CountCallBack = 0;
	}
}

void TMR5_CallBack(void)
{
	// Add your custom callback code here
#ifdef DEBUG_TRM5
	DEBUG2_Toggle();
#endif
	if (TMR5_InterruptHandler) {
		TMR5_InterruptHandler();
	}
}

void TMR5_SetInterruptHandler(void (* InterruptHandler)(void))
{
	TMR5_InterruptHandler = InterruptHandler;
}

void TMR5_DefaultInterruptHandler(void)
{
	// add your TMR5 interrupt custom code
	// or set custom function using TMR5_SetInterruptHandler()
	SLED = (uint8_t) ~SLED;
	V.timerint_count++;
	V.ticks++;
	check_day_time();
	V.ticker = 0;

	if (RELAYL1_PORT || RELAYL2_PORT) {
		if (++V.wdt_ticks > MAX_LOAD_TIME) {
			while (1) { // shutdown load relays and lockup for WDT to reboot
				RELAYL1_SetLow();
				DEBUG2_Toggle();
				RELAYL2_SetLow();
				DEBUG1_Toggle();
				SLED = (uint8_t) ~SLED;
			};
		}
	} else {
		V.wdt_ticks = 0;
	}
}

/**
  End of File
 */
