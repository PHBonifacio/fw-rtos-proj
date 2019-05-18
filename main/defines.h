#ifndef __DEFINES_H__
#define __DEFINES_H__

#define GPIO_LED    GPIO_NUM_2
#define GPIO_INPUT  GPIO_NUM_17

#define WAIT_FIST_INPUT     BIT0
#define WAIT_TO_CONNECT     BIT1
#define WIFI_CONNECTED      BIT2
#define MQTT_CONNECTED      BIT3
#define GPIO_EVENT          BIT4
#define LED_EVENT           BIT5

#define TOPIC_INPUT         "/input/17"
#define TOPIC_LED           "/led"
#endif
