#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "KakuRemoteReceiver.h"
#include "KakuRemoteTransmitter.h"

static const char* TAG = "main";


KakuRemoteTransmitter transmitter(RMT_CHANNEL_0, GPIO_NUM_33, 260, 8);
KakuRemoteReceiver receiver(GPIO_NUM_32);

extern "C"
void app_main(void)
{
	int i = 0;
	while(true) {
		ESP_LOGW(TAG, "Sending code...");
		transmitter.sendGroup(i++, true);
		vTaskDelay(5000/portTICK_PERIOD_MS);
	}
}
