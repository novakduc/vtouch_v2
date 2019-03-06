#include "gemsecs.h"

extern struct V_data V;
extern header10 r_block;
extern struct header10 H10[];

/*
 * Checksum for message and header block after length byte
 */
uint16_t block_checksum(uint8_t *byte_block, uint16_t byte_count)
{
	uint16_t sum = 0, i;

	for (i = 0; i < byte_count; i++) {
		sum += byte_block[i];
	}
	return sum;
}

/*
 * Checksum for data stream
 */
uint16_t run_checksum(uint8_t byte_block, bool clear)
{
	static uint16_t sum = 0;

	if (clear)
		sum = 0;

	sum += byte_block;
	return sum;
}

LINK_STATES r_protocol(LINK_STATES *r_link)
{
	uint8_t rxData;
	static uint8_t rxData_l = 0;

	switch (*r_link) {
	case LINK_STATE_IDLE:
		if (UART1_is_rx_ready()) {
			rxData = UART1_Read();
			if (rxData == ENQ) {
				V.error = LINK_ERROR_NONE; // reset error status
				*r_link = LINK_STATE_ENQ;
			}
		}
		break;
	case LINK_STATE_ENQ:
		rxData_l = 0;
		UART1_Write(EOT);
		StartTimer(TMR_T2, T2);
		*r_link = LINK_STATE_EOT;
		break;
	case LINK_STATE_EOT:
		if (TimerDone(TMR_T2)) {
			V.error = LINK_ERROR_T2;
			*r_link = LINK_STATE_NAK;
		} else {
			if (UART1_is_rx_ready()) {
				rxData = UART1_Read();
				if (rxData_l == 0) { // start header reads
					r_block.length = rxData; // header+message bytes
					run_checksum(0, true);
					rxData_l++;
				} else {
					/*
					 * skip possible message data
					 */
					if (rxData_l <= 10) // save header only
						H10[1].block.b[r_block.length - rxData_l] = rxData;
					if (rxData_l <= r_block.length) // generate checksum from data stream
						V.r_checksum = run_checksum(rxData, false);

					if (rxData_l == r_block.length + 1) // checksum high byte
						H10[1].checksum = (uint16_t) rxData << 8;
					if (rxData_l == r_block.length + 2) // checksum low byte
						H10[1].checksum += rxData;

					rxData_l++;
					if (rxData_l > (r_block.length + 2)) { // end of total data stream
						if (V.r_checksum == H10[1].checksum) {
							*r_link = LINK_STATE_ACK;
						} else {
							V.error = LINK_ERROR_CHECKSUM;
							*r_link = LINK_STATE_NAK;
						}
					}
				}
			}
		}
		break;
	case LINK_STATE_ACK:
		V.stream = H10[1].block.block.stream;
		V.function = H10[1].block.block.function;
		UART1_Write(ACK);
		*r_link = LINK_STATE_DONE;
		break;
	case LINK_STATE_NAK:
		*r_link = LINK_STATE_ERROR;
		while (UART1_DataReady) { // dump the receive buffer
			UART1_Read();
		}
		break;
	case LINK_STATE_ERROR:
		break;
	case LINK_STATE_DONE: // auto move to idle to receive data from link
	default:
		*r_link = LINK_STATE_IDLE;
		break;
	}

	return *r_link;
}

LINK_STATES t_protocol(LINK_STATES * t_link)
{
	uint8_t rxData;
	static uint8_t txData_l = 0;

	switch (*t_link) {
	case LINK_STATE_IDLE:
		V.error = LINK_ERROR_NONE; // reset error status
		txData_l = 0;
		UART1_Write(ENQ);
		StartTimer(TMR_T2, T2);
		*t_link = LINK_STATE_ENQ;
		break;
	case LINK_STATE_ENQ:
		if (TimerDone(TMR_T2)) {
			V.error = LINK_ERROR_T2;
			*t_link = LINK_STATE_NAK;
		} else {
			if (UART1_is_rx_ready()) {
				rxData = UART1_Read();
				if (rxData == EOT) {
					StartTimer(TMR_T3, T3);
					*t_link = LINK_STATE_EOT;
				}
			}
		}
		break;
	case LINK_STATE_EOT: // transmit the secondary message response

		*t_link = LINK_STATE_ACK;
		break;
	case LINK_STATE_ACK:
		if (TimerDone(TMR_T3)) {
			V.error = LINK_ERROR_T3;
			*t_link = LINK_STATE_NAK;
		} else {
			if (UART1_is_rx_ready()) {
				rxData = UART1_Read();
				if (rxData == ACK) {
					*t_link = LINK_STATE_DONE;
				}
			}
		}
		break;
	case LINK_STATE_NAK:
		*t_link = LINK_STATE_ERROR;
		while (UART1_DataReady) { // dump the receive buffer
			UART1_Read();
		}
		break;
	case LINK_STATE_ERROR:
		break;
	case LINK_STATE_DONE: // stay in state until external state change trigger to idle
		break;
	default:
		*t_link = LINK_STATE_IDLE;
		break;
	}

	return *t_link;
}
