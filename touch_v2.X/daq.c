/*
 * 12-bit analog 64 sample average per channel on ports A and B
 * uses the compute burst average mode threshold interrupt to auto change
 * channels during interrupt after a repeat count
 */

#include "daq.h"

typedef struct D_data {
	uint8_t dac0 : 8;
	uint8_t dac1 : 4;
	uint8_t cont : 4;
} D_data;

union bytes2 {
	uint16_t ld;
	uint8_t bd[2];
};

union dac_buf_type {
	uint16_t ld;
	uint8_t bd[2];
	struct D_data map;
};

typedef struct R_data { // internal variables
	adc_result_t raw_adc[ADC_BUFFER_SIZE];
	adc_result_t raw_dac[DAC_BUFFER_SIZE];
	union dac_buf_type max5322_cmd;
	int16_t n_offset[NUM_C_SENSORS];
	float n_zero[NUM_C_SENSORS];
	uint8_t scan_index;
	uint16_t scan_select;
	bool done;
} R_data;

static volatile R_data R = {
	.done = false,
	.scan_index = 0,
	.n_offset[0] = N_OFFSET0,
	.n_offset[1] = N_OFFSET1,
	.n_zero[0] = 0.0,
	.n_zero[1] = 0.0,
	.raw_dac[0] = 0xfff,
	.raw_dac[1] = 0x777,

};

static void adc_int_handler(void);
static void adc_int_t_handler(void);

/*
 * start computed ADC results: 64 samples per average value per selected channel from
 * AN channel zero to LAST_ADC_CHAN
 * check_adc_scan returns true when sequence is complete
 */
bool start_adc_scan(void)
{
	if (R.done)
		return false;

	R.scan_index = 0;
	R.scan_select = (uint16_t) ((ANSELB << 8) + ANSELA) & ADC_SCAN_CHAN; // skip digital pins PORT A and B
	ADCC_SetADIInterruptHandler(adc_int_handler);
	ADCC_SetADTIInterruptHandler(adc_int_t_handler);
	ADCC_DischargeSampleCapacitor(); // short ADC sample cap before channel sampling
	ADCC_StartConversion(R.scan_index & 0xf);
#ifdef DEBUG_DAQ1
	DEBUG1_SetHigh();
#endif
#ifdef DEBUG_DAQ2
	DEBUG2_SetHigh();
#endif
	return true;
}

/*
 * check scan done flag
 */
bool check_adc_scan(void)
{
	return R.done;
}

/*
 * clear scan done flag
 */
void clear_adc_scan(void)
{
	R.done = false;
}

/*
 * update the raw adc values
 */
bool update_adc_result(void)
{
	if (R.done) {
		clear_adc_scan();
		start_adc_scan();
		StartTimer(TMR_ADC, ADC_SCAN_SPEED);
		while (!TimerDone(TMR_ADC) && !check_adc_scan());
		return true;
	} else {
		return false;
	}
}

/*
 * read average value of a channel after scan completion (done)
 */
adc_result_t get_raw_result(const adcc_channel_t index)
{
	return R.raw_adc[index];
}

/*
 * turn ADC values into standard program values
 */
float conv_raw_result(const adcc_channel_t chan, const adc_conv_t to_what)
{

	switch (to_what) {
	case CONV:
		if (!(ADC_SCAN_CHAN >> chan & 0x1))
			return NAN;

		if (ADC_C_CHAN >> chan & 0x1) { // current conversion
			if (ADC_C_CHAN_TYPE >> chan & 0x1) {
				return R.n_zero[0]+((float) ((int16_t) get_raw_result(chan)) - R.n_offset[0]) * C_A200;
			} else {
				return R.n_zero[1]+((float) ((int16_t) get_raw_result(chan)) - R.n_offset[1]) * C_A100;
			}
		} else {
			if (ADC_T_CHAN >> chan & 0x1) { // temp conversion
				return 25.0; // filler until sensor is selected
			} else { // voltage conversion
				return((float) get_raw_result(chan) * V_SCALE) / 1000.0;
			}
		}
		break;
	case O_CONV:
		if (ADC_C_CHAN >> chan & 0x1 || ADC_T_CHAN >> chan & 0x1)
			return((float) get_raw_result(chan) * C_SCALE) / 1000.0;

		return((float) get_raw_result(chan) * V_SCALE) / 1000.0;
		break;
	default:
		return 0.0;
		break;
	}
	return 0.0;
}

/*
 * ADC per conversion interrupt
 */
static void adc_int_handler(void)
{
#ifdef DEBUG_DAQ2
	DEBUG2_Toggle();
#endif
}

/*
 * ADC per channel average interrupt
 */
static void adc_int_t_handler(void)
{
	/*
	 * use the filter result buffer for raw adc data
	 */
	R.raw_adc[R.scan_index] = ((adc_result_t) ((ADFLTRH << 8) + ADFLTRL));
	do {
		if (++R.scan_index > LAST_ADC_CHAN) {
			R.done = true;
#ifdef DEBUG_DAQ1
			DEBUG1_SetLow();
#endif
			return;
		}
	} while (!((R.scan_select >> R.scan_index) &0x1)); // check for analog port bit
	ADCC_DischargeSampleCapacitor(); // short ADC sample cap before next channel sampling
	ADCC_StartConversion(R.scan_index & 0xf);
#ifdef DEBUG_DAQ1
	DEBUG1_Toggle();
#endif
}

/*
 * set == true, set dac spi params
 * set == false restore default spi params
 */
void dac_spi_control(bool set)
{
	static bool init = false;
	static uint8_t S0, S1, S2, SC, SB; // SPI device status backup

	if (set) {
		SPI1CON0bits.EN = 0;
		if (!init) {
			init = true;
			S0 = SPI1CON0;
			S1 = SPI1CON1;
			S2 = SPI1CON2;
			SC = SPI1CLK;
			SB = SPI1BAUD;
		}
		/*
		 * set DAC SPI mode
		 */
		// mode 0
		SPI1CON1 = 0x00;
		// SSET disabled; RXR suspended if the RxFIFO is full; TXR required for a transfer; 
		SPI1CON2 = 0x03;
		// BAUD 0; 
		SPI1BAUD = 0x0f; // 2MHz SCK
		// CLKSEL FOSC; 
		SPI1CLK = 0x00;
		// BMODE every byte; LSBF MSb first; EN enabled; MST bus master; 
		SPI1CON0 = 0x83;
		SPI1CON0bits.EN = 1;
	} else {
		if (init) {
			/*
			 * restore default SPI mode
			 */
			SPI1CON0bits.EN = 0;
			SPI1CON1 = S1;
			SPI1CON2 = S2;
			SPI1CLK = SC;
			SPI1BAUD = SB;
			SPI1CON0 = S0;
			SPI1CON0bits.EN = 1;
		}
	}
}

void set_dac(void)
{
	while (!SPI1STATUSbits.TXBE); // wait until TX buffer is empty
	CSB_SetHigh();
	CS_SDCARD_SetHigh();
	dac_spi_control(true);
	R.max5322_cmd.map.dac0 = R.raw_dac[DCHAN_A]&0xff;
	R.max5322_cmd.map.dac1 = (R.raw_dac[DCHAN_A] > 8) &0xf;
	R.max5322_cmd.map.cont = DAC_LOAD_A; // update DAC A @ registers
	DAC_CS0_SetLow();
	SPI1_Exchange8bit(R.max5322_cmd.bd[1]);
	SPI1_Exchange8bit(R.max5322_cmd.bd[0]);
	DAC_CS0_SetHigh();
	R.max5322_cmd.map.dac0 = R.raw_dac[DCHAN_B]&0xff;
	R.max5322_cmd.map.dac1 = (R.raw_dac[DCHAN_B] > 8) &0xf;
	R.max5322_cmd.map.cont = DAC_LOAD_B; // update DAC B @ registers
	DAC_CS0_SetLow();
	SPI1_Exchange8bit(R.max5322_cmd.bd[1]);
	SPI1_Exchange8bit(R.max5322_cmd.bd[0]);
	DAC_CS0_SetHigh();
	dac_spi_control(false);
}

static uint16_t convert_dac_raw(float voltage)
{
	/*
	 * check limits
	 */
	if (voltage < 0.001)
		voltage = 0.001;
	if (voltage > 10.0)
		voltage = 10.0;
	/*
	 * scale to DAC units
	 */
	return(uint16_t) (voltage / DAC_SCALE);
}

/*
 * 0.0 to 10.0 volts
 */
uint16_t set_dac_a(const float voltage)
{
	R.raw_dac[DCHAN_A] = convert_dac_raw(voltage);
	return R.raw_dac[DCHAN_A];
}

/*
 * 0.0 to 10.0 volts
 */
uint16_t set_dac_b(const float voltage)
{
	R.raw_dac[DCHAN_B] = convert_dac_raw(voltage);
	return R.raw_dac[DCHAN_B];
}
