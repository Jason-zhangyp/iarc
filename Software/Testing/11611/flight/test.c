#include <stdlib.h>
#include "../../drivers/avr_compiler.h"
#include "../../drivers/usart_driver.h"
#include "../../drivers/twi_master_driver.h"
#include "support.h"
#include <stdio.h>
#include "newsupport.h"

USART_data_t xbee;

enum states{running, stopped, offset} state = stopped;
volatile char readdata = 0;

TWI_Master_t imu;
volatile char input[9] = {0,0,0,0,0,0,0,0,0};
volatile char xbeecounter = 0;

int main(void){
	int i;
	int motorSpeed[4];

	//char joyaxis[] = {0,0,0,0};

	uint8_t accelstartbyte = 0x30;
	uint8_t gyrostartbyte = 0x1A;



	char gyrocache[3] = {0,0,0};
	int accelcache[3] = {0,0,0};
	int accelint[] = {0, 0, 0};
	int gyroint[] = {0, 0, 0};

	char rolhisx[50];
	char rolhisy[50];
	char rolhisz[50];
	int acchisx[50];
	int acchisy[50];
	int acchisz[50];

	char readyset = 0;

	int accelnorm[3] = {0,0,0};
	char gyronorm[3] = {0,0,0};


	char xbeebuffer[100];

	int dcmMatrix[9];
	int targetMatrix[9];
	dcmInit(dcmMatrix);
	dcmInit(targetMatrix);

	PORTD.DIR = 0x0F;
	TCD0.CTRLA = TC_CLKSEL_DIV1_gc;
	TCD0.CTRLB = TC_WGMODE_SS_gc | TC0_CCCEN_bm |  TC0_CCAEN_bm |TC0_CCBEN_bm | TC0_CCDEN_bm;
	TCD0.PER = 40000;

	TCC0.CTRLA = TC_CLKSEL_DIV1_gc;
	TCC0.CTRLB = TC_WGMODE_SS_gc;
	TCC0.PER = 40000;

	PORTE.DIR = 0x08;
	PORTF.DIR = 0x03;
	PORTC.DIR = 0b00001100;
	PORTC.OUT = 0b00001000;

	PMIC.CTRL |= PMIC_LOLVLEX_bm | PMIC_MEDLVLEX_bm | PMIC_HILVLEX_bm |
		PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;

		TCD0.CCA = 2000;
		TCD0.CCB = 2000;
		TCD0.CCC = 2000;
		TCD0.CCD = 2000;


	sei();
	uartInitiate(&xbee, &USARTE0);
	twiInitiate(&imu, &TWIC);
	sprintf(xbeebuffer, "starting\n\r");
	sendstring(&xbee, xbeebuffer);

	while(1){
		if(readdata){
			readdata = 0;
/*
			for(i = 0; i < 4; i ++){
				joyaxis[i] = input[3 + i] - 126;
			}

			if(input[7] == 3){
				state = stopped;
				sprintf(xbeebuffer, "stopped\n");
				sendstring(&xbee, xbeebuffer);
			}
			else if(input[7] == 4){
				state = running;
				sprintf(xbeebuffer, "running\n");
				sendstring(&xbee, xbeebuffer);
			}
			else if(input[7] == 5){
				state = offset;
				sprintf(xbeebuffer, "offsetting\n");
				sendstring(&xbee, xbeebuffer);
			}
			xbeecounter = 0;
*/
			if(input[0] == 'r'){
				state = running;
				sprintf(xbeebuffer, "running\n\r");
				sendstring(&xbee, xbeebuffer);
			}
			else if(input[0] == 's'){
				state = stopped;
				sprintf(xbeebuffer, "stopped\n\r");
				sendstring(&xbee, xbeebuffer);

			}
			else if(input[0] == 'o'){
				state = offset;
			}

		}

		switch(state){
			case stopped:
				TCD0.CCA = 2000;
				TCD0.CCB = 2000;
				TCD0.CCC = 2000;
				TCD0.CCD = 2000;
				break;
			case offset:
				updateoffset(&imu,
						accelnorm,
						gyronorm,
						rolhisx,
						rolhisy,
						rolhisz,
						acchisx,
						acchisy,
						acchisz,
						accelcache,
						gyrocache, 
						&readyset, 
						&gyrostartbyte, 
						&accelstartbyte);
				state = stopped;

				break;



			case running:
				while(!(TCC0.INTFLAGS & 0x01));

				TCC0.INTFLAGS = 0x01;


				getgyro(gyrocache, &imu, &gyrostartbyte);
				getaccel(accelcache, &imu, &accelstartbyte);
				for(i = 0; i < 3; i ++){
					gyrocache[i] -= gyronorm[i];
					accelcache[i] -= accelnorm[i];
				}

				for(i = 0; i < 3; i ++){
					accelint[i] = ((18 * accelint[i]) + (2 * accelcache[i]))/20;
					gyroint[i] = ((15 * gyroint[i]) + (5 * gyrocache[i]))/20;
				}

				updateMatrix(accelint, gyroint, dcmMatrix);
				sprintf(xbeebuffer, "%d %d %d\n\r", dcmMatrix[0], dcmMatrix[1], dcmMatrix[2]);
				sendstring(&xbee, xbeebuffer);
				updateMotor(dcmMatrix, targetMatrix, gyroint, motorSpeed);

				
				
				//sprintf(xbeebuffer, "1 - %6d\n\r", motorSpeed[0]);
				//sendstring(&xbee, xbeebuffer);

				break;
		}
	}
}


ISR(USARTE0_RXC_vect){
	USART_RXComplete(&xbee);
	input[xbeecounter] = USART_RXBuffer_GetByte(&xbee);
	readdata = 1;
/*

	if((input[0] == ' ') && (xbeecounter == 0)){
		xbeecounter ++;
	}
	else if((input[1] == 's') && (xbeecounter == 1)){
		xbeecounter ++;
	}
	else if((input[2] == 'a') && (xbeecounter == 2)){
		xbeecounter ++;
	}
	else if((xbeecounter >= 3) && (xbeecounter <= 7)){
		xbeecounter ++;
	}
	else if((input[8] == 'r') && (xbeecounter == 8)){
		readdata = 1;
		if(input[7] == 1){
			CCPWrite( &RST.CTRL, 1 );
		}
		xbeecounter ++;
	}
*/
}

ISR(USARTE0_DRE_vect){
	USART_DataRegEmpty(&xbee);
}
ISR(TWIC_TWIM_vect)
	TWI_MasterInterruptHandler(&imu);
}

