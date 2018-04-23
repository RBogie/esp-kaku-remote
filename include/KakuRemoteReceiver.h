/*
 * KakuRemoteTransmitter.hpp
 *
 *  Created on: Apr 5, 2018
 *      Author: Rob Bogie
 */

#ifndef KAKUREMOTERECEIVER_H
#define KAKUREMOTERECEIVER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/rmt.h"
#include <string>
#include <vector>

#ifdef __cplusplus

typedef struct {
	struct {
		uint32_t address : 26;
		uint32_t unit : 4;
	};
	struct {
		uint16_t isGroup : 1;
		uint16_t isDim : 1;
		uint16_t isOn : 1;
		uint16_t dimLevel : 4;
		uint16_t reserved : 1;
		uint16_t repeat : 8;
	};
	uint16_t period;
} KakuRemoteCode;

class KakuRemoteReceiver {
public:

	/**
	 * Creates a new instance of a receiver for the KAKU (KlikAanKlikUit) protocol on a 433mhz receiver using the specified configuration
	 *
	 * @param rmtChannel	The RMT channel to use for controlling the pin
	 * @param gpioNum		The io pin on which the 433 transmitter is attached
	 */
	KakuRemoteReceiver(gpio_num_t gpioNum);
	virtual ~KakuRemoteReceiver();

private:

	xQueueHandle queue;
	KakuRemoteCode lastCode = {};
	KakuRemoteCode currentCode = {};

	int16_t state = -1;
	uint64_t edgeTimeStamp[3] = {0, };
	uint16_t min1Period;
	uint16_t max1Period;
	uint16_t min5Period;
	uint16_t max5Period;
	uint8_t receivedBit;
	bool skipNextEdge = false;


	gpio_num_t gpioNum;
	xTaskHandle taskHandle = nullptr;
	std::string taskName;
	static int nextInstanceId;

	void receive();
	void onInterrupt();
	static void receiveBootstrap(void* instance);
	static void interruptBootstrap(void* instance);
};

extern "C" {
#endif

typedef void* kaku_remote_rx;

#ifdef __cplusplus
}
#endif

#endif /* KAKUREMOTERECEIVER_H */
