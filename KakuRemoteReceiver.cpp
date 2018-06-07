/*
 * KakuRemoteReceiver.cpp
 *
 *  Created on: Apr 5, 2018
 *      Author: Rob Bogie
 */

#include "include/KakuRemoteReceiver.h"

#include <sstream>
#include <cstring>

#include "esp_log.h"

static const char* TAG = "kakurx";

int KakuRemoteReceiver::nextInstanceId = 0;

KakuRemoteReceiver::KakuRemoteReceiver(gpio_num_t gpioNum)
: gpioNum(gpioNum) {

	this->queue = xQueueCreate(256, sizeof(KakuRemoteCode));

	assert(this->taskHandle == nullptr);

	std::ostringstream ss;
	ss << "task" << KakuRemoteReceiver::nextInstanceId++;
	this->taskName = ss.str();

	xTaskCreatePinnedToCore(&KakuRemoteReceiver::receiveBootstrap, taskName.c_str(), 6144, this, 15, &this->taskHandle, (portNUM_PROCESSORS - 1));
}

KakuRemoteReceiver::~KakuRemoteReceiver() {

}

void KakuRemoteReceiver::setEnabled(bool enabled) {
	this->enabled = enabled;
}

void KakuRemoteReceiver::addCallback(CallBack callback) {
	this->callbacks.push_back(callback);
}

void KakuRemoteReceiver::receive() {
	gpio_pad_select_gpio(this->gpioNum);
	gpio_set_direction(this->gpioNum, GPIO_MODE_INPUT);
	gpio_set_intr_type(this->gpioNum, GPIO_INTR_ANYEDGE);
	gpio_pulldown_dis(this->gpioNum);
	gpio_pullup_dis(this->gpioNum);
	ESP_LOGD(TAG, "Configured io %d", this->gpioNum);

	gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
	gpio_isr_handler_add(this->gpioNum, KakuRemoteReceiver::interruptBootstrap, this);

	while(true) {
		KakuRemoteCode event;
		xQueueReceive(this->queue, &event, portMAX_DELAY);

		ESP_LOGV(TAG, "Received event: address=%d, unit=%d, isGroup=%d, isDim=%d, isOn=%d, dimLevel=%d, repeat=%d", event.address, event.unit, event.isGroup, event.isDim, event.isOn, event.dimLevel, event.repeat);
		for(auto callback : this->callbacks) {
			callback(event);
		}
	}
}

void KakuRemoteReceiver::onInterrupt() {
	if(!this->enabled)
		return;

	// Filter out too short pulses. This method works as a low pass filter.
	edgeTimeStamp[1] = edgeTimeStamp[2];
	edgeTimeStamp[2] = esp_timer_get_time();

	if (skipNextEdge) {
		skipNextEdge = false;
		return;
	}

	if (state >= 0 && edgeTimeStamp[2]-edgeTimeStamp[1] < min1Period) {
		// Last edge was too short.
		// Skip this edge, and the next too.
		skipNextEdge = true;
		return;
	}

	uint16_t duration = edgeTimeStamp[1] - edgeTimeStamp[0];
	edgeTimeStamp[0] = edgeTimeStamp[1];

	// Note that if state>=0, duration is always >= 1 period.
	if (state == -1) {
		// wait for the long low part of a stop bit.
		// Stopbit: 1T high, 40T low
		// By default 1T is 260µs, but for maximum compatibility go as low as 120µs
		if (duration > 4800) { // =40*120µs, minimal time between two edges before decoding starts.
			// Sync signal received.. Preparing for decoding
			currentCode.repeat = 0;

			currentCode.period = duration / 40; // Measured signal is 40T, so 1T (period) is measured signal / 40.

			// Allow for large error-margin. ElCheapo-hardware :(
			min1Period = currentCode.period * 3 / 10; // Lower limit for 1 period is 0.3 times measured period; high signals can "linger" a bit sometimes, making low signals quite short.
			max1Period = currentCode.period * 3; // Upper limit for 1 period is 3 times measured period
			min5Period = currentCode.period * 3; // Lower limit for 5 periods is 3 times measured period
			max5Period = currentCode.period * 8; // Upper limit for 5 periods is 8 times measured period
		} else {
			return;
		}
	} else if (state == 0) { // Verify start bit part 1 of 2
		// Duration must be ~1T
		if (duration > max1Period) {
			state = -1;
			return;
		}

		// Start-bit passed. Do some clean-up.
		currentCode.address = 0;
		currentCode.unit = 0;
		currentCode.dimLevel = 0;
	} else if (state == 1) { // Verify start bit part 2 of 2
		// Duration must be ~10.44T
		if (duration < 7 * currentCode.period || duration > 15 * currentCode.period) {
			state = -1;
			return;
		}
	}else if (state < 148) { // state 146 is first edge of stop-sequence. All bits before that adhere to default protocol, with exception of dim-bit
		receivedBit <<= 1;

		// One bit consists out of 4 bit parts.
		// bit part durations can ONLY be 1 or 5 periods.
		if (duration <= max1Period) {
			receivedBit &= 0b1110; // Clear LSB of receivedBit
		} else if (duration >= min5Period && duration <= max5Period) {
			receivedBit |= 0b1; // Set LSB of receivedBit
		} else if (
			// Check if duration matches the second part of stopbit (duration must be ~40T), and ...
			(duration >= 20 * currentCode.period && duration <= 80 * currentCode.period) &&
			// if first part op stopbit was a short signal (short signal yielded a 0 as second bit in receivedBit), and ...
			((receivedBit & 0b10) == 0b00) &&
			// we are in a state in which a stopbit is actually valid, only then ...
			(state == 147 || state == 131) ) {
				// If a dim-level was present...
				if (state == 147) {
					// mark received switch signal as signal-with-dim
					currentCode.isDim = true;
				}

				// a valid signal was found!
				if (
						currentCode.address != lastCode.address ||
						currentCode.unit != lastCode.unit ||
						currentCode.isDim != lastCode.isDim ||
						currentCode.dimLevel != lastCode.dimLevel ||
						currentCode.isGroup != lastCode.isGroup
					) { // memcmp isn't deemed safe
					currentCode.repeat = 0;
					lastCode = currentCode;
				}

				xQueueSendToBackFromISR(this->queue, &currentCode, nullptr);

				currentCode.repeat++;

				// Reset for next round
				state=0; // no need to wait for another sync-bit!
				return;
		}
		else { // Otherwise the entire sequence is invalid
			state = -1;
			return;
		}

		if (state % 4 == 1) { // Last bit part? Note: this is the short version of "if ( (_state-2) % 4 == 3 )"
			// There are 3 valid options for receivedBit:
			// 0, indicated by short short short long == B0001.
			// 1, short long shot short == B0100.
			// dim, short shot short shot == B0000.
			// Everything else: inconsistent data, trash the whole sequence.


			if (state < 106) {
				// States 2 - 105 are address bit states

				currentCode.address <<= 1;

				// Decode bit. Only 4 LSB's of receivedBit are used; trim the rest.
				switch (receivedBit & 0b1111) {
					case 0b0001: // Bit "0" received.
						// receivedCode.address |= 0; But let's not do that, as it is wasteful.
						break;
					case 0b0100: // Bit "1" received.
						currentCode.address |= 1;
						break;
					default: // Bit was invalid. Abort.
						state = -1;
						return;
				}
			} else if (state < 110) {
				// States 106 - 109 are group bit states.
				switch (receivedBit & 0b1111) {
					case 0b0001: // Bit "0" received.
						currentCode.isGroup = false;
						break;
					case 0b0100: // Bit "1" received.
						currentCode.isGroup = true;
						break;
					default: // Bit was invalid. Abort.
						state = -1;
						return;
				}
			} else if (state < 114) {
				// States 110 - 113 are switch bit states.
				switch (receivedBit & 0b1111) {
					case 0b0001: // Bit "0" received.
						currentCode.isOn = false;
						break;
					case 0b0100: // Bit "1" received. Note: this might turn out to be a on_with_dim signal.
						currentCode.isOn = true;
						break;
					case 0b0000: // Bit "dim" received.
						currentCode.isDim = true;
						break;
					default: // Bit was invalid. Abort.
						state = -1;
						return;
				}
			} else if (state < 130){
				// States 114 - 129 are unit bit states.
				currentCode.unit <<= 1;

				// Decode bit.
				switch (receivedBit & 0b1111) {
					case 0b0001: // Bit "0" received.
						// receivedCode.unit |= 0; But let's not do that, as it is wasteful.
						break;
					case 0b0100: // Bit "1" received.
						currentCode.unit |= 1;
						break;
					default: // Bit was invalid. Abort.
						state = -1;
						return;
				}

			} else if (state < 146) {
				// States 130 - 145 are dim bit states.
                // Depending on hardware, these bits can be present, even if switchType is NewRemoteCode::on or NewRemoteCode::off

				currentCode.isDim = true;
				currentCode.dimLevel <<= 1;

				// Decode bit.
				switch (receivedBit & 0b1111) {
					case 0b0001: // Bit "0" received.
						// receivedCode.dimLevel |= 0; But let's not do that, as it is wasteful.
						break;
					case 0b0100: // Bit "1" received.
						currentCode.dimLevel |= 1;
						break;
					default: // Bit was invalid. Abort.
						state = -1;
						return;
				}
			}
		}
	}

	state++;
}

void KakuRemoteReceiver::receiveBootstrap(void* instance) {
	((KakuRemoteReceiver*)instance)->receive();
}

void KakuRemoteReceiver::interruptBootstrap(void* instance) {

	((KakuRemoteReceiver*)instance)->onInterrupt();
}

//C Api
bool kaku_remote_code_is_equal(KakuRemoteCode code1, KakuRemoteCode code2) {
	return code1.address == code2.address &&
			code1.dimLevel == code2.dimLevel &&
			code1.isDim == code2.isDim &&
			code1.isGroup == code2.isGroup &&
			code1.isOn == code2.isOn &&
			code1.unit == code2.unit &&
			code1.period == code2.period;
}
