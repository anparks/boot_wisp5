/**
 * @file       usr.c
 * @brief      WISP application-specific code set
 * @details    The WISP application developer's implementation goes here.
 *
 * @author     Aaron Parks, UW Sensor Systems Lab
 *
 */

#include "wisp-base.h"

#define SIZE_ADDR        0x1910
#define ADDRESS_ADDR     0x1912
#define BSL_PASSWD       0x1920

WISP_dataStructInterface_t wispData;

/**
 * This function is called by WISP FW after a successful ACK reply
 *
 */
void my_ackCallback (void) {
	asm(" NOP");
}

/**
 * This function is called by WISP FW after a successful read command
 *  reception
 *
 */
void my_readCallback (void) {
	asm(" NOP");
}

/**
 * This function is called by WISP FW after a successful write command
 *  reception
 *
 */
void my_writeCallback (void) {
	// Get data descriptor.
	uint8_t hi = (wispData.writeBufPtr[0] >> 8)  & 0xFF;
	uint8_t lo = (wispData.writeBufPtr[0])  & 0xFF;

	// Check whether we got a command to enter FRAM write mode.
	if (hi == 0xB1 && lo == 0x05) {
		// Acknowledge the message.
		wispData.epcBuf[0]  = (wispData.writeBufPtr[0] >> 8)  & 0xFF;
		wispData.epcBuf[1] = (wispData.writeBufPtr[0])  & 0xFF;
		// Otherwise, enter application directly.
	} else {
		wispData.epcBuf[0]   = 0XB0;
		wispData.epcBuf[1]  = 0X07;
	}
}

/**
 * This function is hidden within the process time of the wisp BEFORE responding to the reader!
 * While the maximum delayed response time is 20 ms, the WISP lives for ~14 ms per power cycle.
 * Furthermore, the bigger this function is, the smaller the time window becomes (i.e. less bwr/sec).
 */
void my_blockWriteCallback  (void) {
	uint8_t word_count = (wispData.blockWriteBufPtr[0] >> 8)  & 0xFF;
	uint8_t size       = ((wispData.blockWriteBufPtr[0])  & 0xFF);
	uint16_t address   = (wispData.blockWriteBufPtr[1]);
	uint8_t checksum   = (wispData.blockWriteBufPtr[word_count] >> 8) & 0xFF;
	uint8_t offset     = 0x00;
	uint8_t calcsum    = 0x00;

	// Calculate checksum.
	for (offset = word_count; offset > 0; offset--) {
		calcsum += (wispData.blockWriteBufPtr[offset-1] >> 8) & 0xff;
		calcsum += wispData.blockWriteBufPtr[offset-1] & 0xff;
	}

	// Only do stuff if checksum matches.
	if (calcsum == checksum) {
		checksum = word_count + size + ((address >> 8) & 0xFF) + (address & 0xFF);
		for (offset = 0x00; offset < size; offset += 0x02) {
			(* (uint16_t *) (address + offset)) = ((wispData.blockWriteBufPtr[2 + (offset >> 1)] & 0xff) << 8) | ((wispData.blockWriteBufPtr[2 + (offset >> 1)] & 0xff00) >> 8);
			checksum += (* (uint8_t *) (address + offset));
			checksum += (* (uint8_t *) (address + offset + 0x01));
		}

		// Send ACK.
		wispData.epcBuf[0]  = word_count;
		wispData.epcBuf[1]  = size;
		wispData.epcBuf[2]  = (address >> 8)  & 0xFF;
		wispData.epcBuf[3]  = (address)  & 0xFF;
		wispData.epcBuf[4]  = checksum;
	}
}


/**
 * This implements the user application and should never return
 *
 * Must call WISP_init() in the first line of main()
 * Must call WISP_doRFID() at some point to start interacting with a reader
 */
void main(void) {

	WISP_init();

	// Register callback functions with WISP comm routines
	WISP_registerCallback_ACK(&my_ackCallback);
	WISP_registerCallback_READ(&my_readCallback);
	WISP_registerCallback_WRITE(&my_writeCallback);
	WISP_registerCallback_BLOCKWRITE(&my_blockWriteCallback);

	// Initialize BlockWrite buffer.
	uint16_t bwr_array[32] = {0};
	RWData.bwrBufPtr = bwr_array;

	// Get access to EPC, READ, and WRITE data buffers
	WISP_getDataBuffers(&wispData);

	// Set up operating parameters for WISP comm routines
	WISP_setMode( MODE_READ | MODE_WRITE | MODE_USES_SEL);
	WISP_setAbortConditions(CMD_ID_READ | CMD_ID_WRITE /*| CMD_ID_BLOCKWRITE*/ );

	// Set up EPC
	wispData.epcBuf[0] = 0x00; // WISP version
	wispData.epcBuf[1] = 0x00;
	wispData.epcBuf[2] = 0x00;
	wispData.epcBuf[3] = 0x00;
	wispData.epcBuf[4] = 0x00;
	wispData.epcBuf[5] = 0x00;
	wispData.epcBuf[6] = 0x00;
	wispData.epcBuf[7] = 0x00;
	wispData.epcBuf[8] = 0x00;
	wispData.epcBuf[9] = 0x00; // RFID Status/Control
	wispData.epcBuf[10]= 0x00; // RFID Status/Control
	wispData.epcBuf[11]= 0x00;

	// Talk to the RFID reader.
	while (FOREVER) {
		// If command is given, jump to  next application.
		if (wispData.epcBuf[0] == 0xB0 && wispData.epcBuf[1] == 0x07) {
			(*((void (*)(void))(*(unsigned int *)0xFDFE)))();
		}
		WISP_doRFID();
	}
}
