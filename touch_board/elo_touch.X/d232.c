#include "d232.h"

extern volatile A_data IO;

void Digital232_init(void)
{
	printf("%s", RST);
	WaitMs(5);
	printf("%s", DTEST);
	WaitMs(800);
	printf("%s", CNF);
	WaitMs(5);
	printf("%s", FMT);
	WaitMs(5);
	printf("%s", SRQ);
	WaitMs(5);
	IO.d232 = D232_INIT;
	/*
	 * empty receiver buffer
	 */
	if (UART2_is_rx_ready())
		UART2_Read();
	IO.io = IO_INIT;
}

bool Digital232_RW(void)
{
	uint8_t i = 0;
	/*
	 * empty receiver buffer
	 */
	if (UART2_is_rx_ready())
		UART2_Read();

	WaitMs(10);
	IO.outbytes[0] = 0;
	IO.outbytes[1]++;
	IO.outbytes[2] = 255;
	IO.outbytes[3] = 0;
	IO.outbytes[4]--;
	UART2_Write('D');
	UART2_Write(IO.outbytes[0]);
	UART2_Write(IO.outbytes[1]);
	UART2_Write(IO.outbytes[2]);
	UART2_Write(IO.outbytes[3]);
	UART2_Write(IO.outbytes[4]);
	UART2_Write('\r');
	WaitMs(5);
	printf("%s", DRD);
	IO.output_ok = true;
	IO.io = IO_OUT;
	/*
	 * wait for data
	 */
	StartTimer(TMR_RXTO, 250);
	while (!TimerDone(TMR_RXTO) && !UART2_is_rx_ready()) {
	}
	if (TimerDone(TMR_RXTO))
		return false;

	/*
	 * read data
	 */
	StartTimer(TMR_RXTO, 250);
	while (!TimerDone(TMR_RXTO) && (i < 6)) {
		if (UART2_is_rx_ready()) {
			IO.inbytes[i] = UART2_Read();
			i++;
		}
	}
	if (TimerDone(TMR_RXTO) || i < 6)
		return false;
	IO.input_ok = true;
	IO.io = IO_IN;
	IO.d232 = D232_OUT_IN;

	return true;
}
