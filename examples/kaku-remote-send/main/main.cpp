#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "KakuRemoteTransmitter.h"

static const char* TAG = "main";

KakuRemoteTransmitter transmitter(RMT_CHANNEL_0, GPIO_NUM_33, 260, 1);

extern "C"
void app_main(void)
{
	while(true) {
		ESP_LOGI(TAG, "Sending code...");
		transmitter.sendGroup(31337, true);
		vTaskDelay(5000/portTICK_PERIOD_MS);
	}
}
