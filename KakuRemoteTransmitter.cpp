/*
 * KakuRemoteTransmitter.cpp
 *
 *  Created on: Nov 20, 2017
 *      Author: Rob Bogie
 */

#include "include/KakuRemoteTransmitter.h"

KakuRemoteTransmitter::KakuRemoteTransmitter(rmt_channel_t rmtChannel, gpio_num_t gpioNum, uint16_t periodUs, uint8_t repeats)
: rmtChannel(rmtChannel), gpioNum(gpioNum), periodUs(periodUs), repeats(repeats) {

	periodTick = periodUs*RMT_TICK_10_US/10;

	initializeRmt();
}

void KakuRemoteTransmitter::initializeRmt() {
	rmt_config_t config;
	config.channel = this->rmtChannel;
	config.clk_div = RMT_CLK_DIVIDER;
	config.gpio_num = this->gpioNum;
	config.mem_block_num = 1;
	config.rmt_mode = RMT_MODE_TX;
	config.tx_config.carrier_duty_percent = 50;
	config.tx_config.carrier_en = false;
	config.tx_config.carrier_freq_hz = 38000;
	config.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
	config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
	config.tx_config.idle_output_en = true;
	config.tx_config.loop_en = false;

	rmt_config(&config);
	rmt_driver_install(this->rmtChannel, 0, 0);
}

void KakuRemoteTransmitter::sendGroup(uint32_t address, bool switchOn) {
	int numItems = 66;
	rmt_item32_t items[numItems];
	rmt_item32_t* currentItem = items;

	this->sendStart(currentItem++);
	this->sendAddress(currentItem, address);
	currentItem += 52;
	this->sendBit(currentItem, true);
	currentItem += 2;
	this->sendBit(currentItem, switchOn);
	currentItem += 2;
	this->sendUnit(currentItem, 0);
	currentItem += 8;
	this->sendStop(currentItem);

	for(int i = 0; i < this->repeats; i++) {
		rmt_write_items(this->rmtChannel, items, numItems, true);
		rmt_wait_tx_done(this->rmtChannel, portMAX_DELAY);
	}
}

void KakuRemoteTransmitter::sendUnit(uint32_t address, uint8_t unit, bool switchOn) {
	int numItems = 66;
	rmt_item32_t items[numItems];
	rmt_item32_t* currentItem = items;

	this->sendStart(currentItem++);
	this->sendAddress(currentItem, address);
	currentItem += 52;
	this->sendBit(currentItem, false);
	currentItem += 2;
	this->sendBit(currentItem, switchOn);
	currentItem += 2;
	this->sendUnit(currentItem, unit);
	currentItem += 8;
	this->sendStop(currentItem);

	for(int i = 0; i < this->repeats; i++) {
		rmt_write_items(this->rmtChannel, items, numItems, true);
		rmt_wait_tx_done(this->rmtChannel, portMAX_DELAY);
	}
}

void KakuRemoteTransmitter::sendDim(uint32_t address, uint8_t unit, uint8_t dimLevel) {
	int numItems = 74;
	rmt_item32_t items[numItems];
	rmt_item32_t* currentItem = items;

	this->sendStart(currentItem++);
	this->sendAddress(currentItem, address);
	currentItem += 52;
	this->sendBit(currentItem, false);
	currentItem += 2;
	this->sendDim(currentItem);
	currentItem += 2;
	this->sendUnit(currentItem, unit);
	currentItem += 8;

	//Send dim information
	for (short j=3; j>=0; j--) {
		this->sendBit(currentItem, (dimLevel & 1<<j) != 0);
		currentItem += 2;
	}

	this->sendStop(currentItem);
	for(int i = 0; i < this->repeats; i++) {
		rmt_write_items(this->rmtChannel, items, numItems, true);
		rmt_wait_tx_done(this->rmtChannel, portMAX_DELAY);
	}
}

void KakuRemoteTransmitter::sendStart(rmt_item32_t* items) {
	//We send T high, 9 T low
	items[0].duration0 = this->periodTick;
	items[0].level0 = 1;
	items[0].duration1 = 10*this->periodTick + (this->periodTick>>1);
	items[0].level1 = 0;
}

void KakuRemoteTransmitter::sendStop(rmt_item32_t* items) {
	//We send T high, 40 T low
	items[0].duration0 = this->periodTick;
	items[0].level0 = 1;
	items[0].duration1 = 40*this->periodTick;
	items[0].level1 = 0;
}

void KakuRemoteTransmitter::sendAddress(rmt_item32_t* items, uint32_t address) {
	for (short i=25; i>=0; i--) {
	   this->sendBit(items, (address >> i) & 1);
	   items+=2;
	}
}

void KakuRemoteTransmitter::sendUnit(rmt_item32_t* items, uint8_t unit) {
	for (short i=3; i>=0; i--) {
	   this->sendBit(items, unit & 1<<i);
	   items+=2;
	}
}

void KakuRemoteTransmitter::sendBit(rmt_item32_t* items, bool on) {
	if(on) {
		// Send '1'
		items[0].duration0 = this->periodTick;
		items[0].level0 = 1;
		items[0].duration1 = 5*this->periodTick;
		items[0].level1 = 0;
		items[1].duration0 = this->periodTick;
		items[1].level0 = 1;
		items[1].duration1 = this->periodTick;
		items[1].level1 = 0;
	} else {
		// Send '0'
		items[0].duration0 = this->periodTick;
		items[0].level0 = 1;
		items[0].duration1 = this->periodTick;
		items[0].level1 = 0;
		items[1].duration0 = this->periodTick;
		items[1].level0 = 1;
		items[1].duration1 = 5*this->periodTick;
		items[1].level1 = 0;
	}
}

void KakuRemoteTransmitter::sendDim(rmt_item32_t* items) {
	items[0].duration0 = this->periodTick;
	items[0].level0 = 1;
	items[0].duration1 = this->periodTick;
	items[0].level1 = 0;
	items[1].duration0 = this->periodTick;
	items[1].level0 = 1;
	items[1].duration1 = this->periodTick;
	items[1].level1 = 0;
}

//C api
kaku_remote_tx kaku_remote_tx_alloc(rmt_channel_t rmt_channel, gpio_num_t ionum, uint16_t period_us, uint8_t repeats) {
	return (kaku_remote_tx)new KakuRemoteTransmitter(rmt_channel, ionum, period_us, repeats);
}

void kaku_remote_tx_free(kaku_remote_tx handle) {
	delete ((KakuRemoteTransmitter*)handle);
}

void kaku_remote_tx_send_group(kaku_remote_tx handle, uint32_t address, bool switchon) {
	((KakuRemoteTransmitter*)handle)->sendGroup(address, switchon);
}

void kaku_remote_tx_send_unit(kaku_remote_tx handle, uint32_t address, uint8_t unit, bool switchon) {
	((KakuRemoteTransmitter*)handle)->sendUnit(address, unit, switchon);
}

void kaku_remote_tx_send_dim(kaku_remote_tx handle, uint32_t address, uint8_t unit, uint8_t dimlvl) {
	((KakuRemoteTransmitter*)handle)->sendDim(address, unit, dimlvl);
}
