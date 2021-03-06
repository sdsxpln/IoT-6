/*
 * wifi.c
 *
 *  Created on: Dec 30, 2014
 *      Author: Minh
 */
#include "wifi.h"
#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "mem.h"
#include "user_config.h"
#include "debug.h"
#include "global_var.h"

static int LOSTconn = 0;
static ETSTimer WiFiLinker;
WifiCallback wifiCb = NULL;
static uint8_t wifiStatus = STATION_IDLE, lastWifiStatus = STATION_IDLE;
static void ICACHE_FLASH_ATTR wifi_check_ip(void *arg)
{
	struct ip_info ipConfig;

	os_timer_disarm(&WiFiLinker);
	wifi_get_ip_info(STATION_IF, &ipConfig);
	wifiStatus = wifi_station_get_connect_status();
	if (wifiStatus == STATION_GOT_IP && ipConfig.ip.addr != 0)
	{
		os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
		os_timer_arm(&WiFiLinker, 2000, 0);
	}
	else
	{
		if(wifi_station_get_connect_status() == STATION_WRONG_PASSWORD)
		{
			INFO("STATION_WRONG_PASSWORD\r\n");
			wifi_station_connect();
		}
		else if(wifi_station_get_connect_status() == STATION_NO_AP_FOUND)
		{
			INFO("STATION_NO_AP_FOUND:%d\r\n", LOSTconn);
			LOSTconn++;
			if( LOSTconn >= 5 )
			{
				LOSTconn = 0;
				uint32_t input =DOWN_FLAG;
				spi_flash_erase_sector(EEPROM_SECTION);
				spi_flash_write((EEPROM_SECTION)*SECTION_SIZE, (uint32 *) &input, 4);
				/* Restart the system */
				system_restart();
			}
			
			wifi_station_connect();
		}
		else if(wifi_station_get_connect_status() == STATION_CONNECT_FAIL)
		{
			INFO("STATION_CONNECT_FAIL\r\n");
			wifi_station_connect();
		}
		else
		{
			INFO("STATION_IDLE\r\n");
		}

		INFO("STATION IP: %d\tnetmask: %d\r\n", ipConfig.ip,ipConfig.netmask);
		os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
		os_timer_arm(&WiFiLinker, 500, 0);
	}
	if(wifiStatus != lastWifiStatus){
		lastWifiStatus = wifiStatus;
		if(wifiCb)
			wifiCb(wifiStatus);
	}
}

uint32_t ICACHE_FLASH_ATTR
WIFI_Connect(uint8_t* ssid, uint8_t* pwd, WifiCallback cb)
{
	struct station_config stationConf;
	os_memset(&stationConf, 0, sizeof(struct station_config));

	os_sprintf(stationConf.ssid, "%s", ssid);
	os_sprintf(stationConf.password, "%s", pwd);

	wifi_station_set_auto_connect(FALSE);
	wifi_set_opmode(STATION_MODE);

	wifiCb = cb;
	wifi_station_set_config(&stationConf);
	wifi_station_disconnect();
	wifi_station_connect();

	os_timer_disarm(&WiFiLinker);
	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
	os_timer_arm(&WiFiLinker, 1000, 0);

	wifi_station_set_auto_connect(TRUE);
	return 0;
}

/* @brief	Set up an the SSID and password when ESP is at AP mode 
 * @param	(PACKET_CMD) command package
 * @retval	 0: Success
 * @error	-1: Failure
 * */
uint32_t ICACHE_FLASH_ATTR
WIFI_AP_Config(uint8_t* ssid, uint8_t* pwd)
{
	struct softap_config apConfig;

	/* Set up the memory space of the configuration file */
	os_bzero(&apConfig, sizeof(struct softap_config));
	wifi_set_opmode(SOFTAP_MODE); /* set the ESP into an AP mode */
	wifi_softap_get_config(&apConfig); /* Configure the ESP at AP mode */

	/* Copy the content into the corresponding vairables of configuration file */
	os_sprintf(apConfig.ssid, "%s", ssid);
	os_sprintf(apConfig.password, "%s", pwd);

	/* TODO: For this moment, hardcode the rest of configuration. Later, implemented if needed */
	apConfig.authmode = AUTH_WPA_WPA2_PSK;
	//apConfig.ssid_hidden = 0;
	apConfig.channel = 5;
	apConfig.max_connection = 4;

	wifi_softap_set_config(&apConfig);

	return 0;
}
