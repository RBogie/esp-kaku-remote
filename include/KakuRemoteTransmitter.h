/*
 * KakuRemoteTransmitter.hpp
 *
 *  Created on: Nov 20, 2017
 *      Author: Rob Bogie
 */

#ifndef KAKUREMOTETRANSMITTER_HPP
#define KAKUREMOTETRANSMITTER_HPP

#include "driver/rmt.h"

#ifdef __cplusplus

//The clock divider that is used. The source clock is APB CLK (80MHZ)
#define RMT_CLK_DIVIDER      100
#define RMT_TICK_10_US    (80000000/RMT_CLK_DIVIDER/100000)   //Number of ticks needed for a 10 microseconds period

class KakuRemoteTransmitter {
public:

	/**
	 * Creates a new instance of a transmitter for the KAKU (KlikAanKlikUit) protocol on a 433mhz receiver using the specified configuration
	 *
	 * @param rmtChannel	The RMT channel to use for controlling the pin
	 * @param gpioNum		The io pin on which the 433 transmitter is attached
	 * @param periodUs		The duration of a single period in microseconds. Default this period is 260 microseconds long. The resolution is 1.25 microsecond
	 * @param repeats		The number of repeats of a single signal. This should be anywhere between 1 and 255. 2 or lower is highly unstable,
	 * 						with 8 or higher becoming more and more robust. The higher you go, the longer sending will take...
	 */
	KakuRemoteTransmitter(rmt_channel_t rmtChannel,	gpio_num_t gpioNum, uint16_t periodUs = 260, uint8_t repeats = 8);

	/**
	 * Send on/off command to the address group.
	 *
	 * @param address	The 26bit address to which the command should be sent.
	 * @param switchOn  Whether to send a switch on signal or not
	 */
	void sendGroup(uint32_t address, bool switchOn);

	/**
	 * Send on/off command to an unit on the current address.
	 *
	 * @param address	The 26bit address to which the command should be sent.
	 * @param unit      The unit to target (0-15)
	 * @param switchOn  Whether to send a switch on signal or not
	 */
	void sendUnit(uint32_t address, uint8_t unit, bool switchOn);

	/**
	 * Send dim value to an unit on the current address.
	 *
	 * @param address	The 26bit address to which the command should be sent.
	 * @param unit      The unit to target (0-15)
	 * @param dimLevel  The level to dim to (0-15)
	 */
	void sendDim(uint32_t address, uint8_t unit, uint8_t dimLevel);

private:

	rmt_channel_t rmtChannel;
	gpio_num_t gpioNum;
	uint16_t periodUs;
	uint16_t periodTick;
	uint8_t repeats;

	void initializeRmt();

	void sendStart(rmt_item32_t* items);
	void sendStop(rmt_item32_t* items);
	void sendAddress(rmt_item32_t* items, uint32_t address);
	void sendUnit(rmt_item32_t* items, uint8_t unit);
	void sendBit(rmt_item32_t* items, bool on);
	void sendDim(rmt_item32_t* items);
};

extern "C" {
#endif

typedef void* kaku_remote_tx;

/**
 * Allocates the needed structures needed for transmitting KAKU protocol on a 433mhz receiver using the specified configuration,
 * and returns a handle that can be used for sending data.
 *
 * This handle must be freed by calling kaku_remote_tx_free
 *
 * @param rmt_channel	The RMT channel to use for controlling the pin
 * @param ionum		The io pin on which the 433 transmitter is attached
 * @param period_us		The duration of a single period in microseconds. Default this period is 260 microseconds long
 * @param repeats		The number of repeats of a single signal. This should be anywhere between 1 and 255. 2 or lower is highly unstable,
 * 						with 8 or higher becoming more and more robust. The higher you go, the longer sending will take...
 */
kaku_remote_tx kaku_remote_tx_alloc(rmt_channel_t rmt_channel, gpio_num_t ionum, uint16_t period_us = 260, uint8_t repeats = 8);

/**
 * Used to free all structures behind the given handle. After this call, the handle cannot be used anymore.
 *
 * @param handle The handle to the KAKU transmitting structure
 */
void kaku_remote_tx_free(kaku_remote_tx handle);

/**
 * Send on/off command to the address group.
 *
 * @param handle The handle to the KAKU transmitting structure that should be used
 * @param address	The 26bit address to which the command should be sent.
 * @param switchon  Whether to send a switch on signal or not
 */
void kaku_remote_tx_send_group(kaku_remote_tx handle, uint32_t address, bool switchon);
/**
 * Send on/off command to an unit on the current address.
 *
 * @param handle The handle to the KAKU transmitting structure that should be used
 * @param address	The 26bit address to which the command should be sent.
 * @param unit      The unit to target (0-15)
 * @param switchon  Whether to send a switch on signal or not
 */
void kaku_remote_tx_send_unit(kaku_remote_tx handle, uint32_t address, uint8_t unit, bool switchon);
/**
 * Send dim value to an unit on the current address.
 *
 * @param handle The handle to the KAKU transmitting structure that should be used
 * @param address	The 26bit address to which the command should be sent.
 * @param unit      The unit to target (0-15)
 * @param dimlvl  The level to dim to (0-15)
 */
void kaku_remote_tx_send_dim(kaku_remote_tx handle, uint32_t address, uint8_t unit, uint8_t dimlvl);

#ifdef __cplusplus
}
#endif

#endif /* KAKUREMOTETRANSMITTER_HPP */
