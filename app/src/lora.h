/******************** LoRaWAN Configuration Parameters ************************/
/* Enable or disable LoRaWAN for testing purposes */
#define ENABLE_LORAWAN

/* Switch between using ABP or OTAA parameters */
#define USE_ABP

#define DEFAULT_RADIO_NODE DT_ALIAS(lora0)
BUILD_ASSERT(DT_NODE_HAS_STATUS(DEFAULT_RADIO_NODE, okay),
	     "No default LoRa radio specified in DT");
#define DEFAULT_RADIO DT_LABEL(DEFAULT_RADIO_NODE)

//#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL

#ifndef USE_ABP
/* OTAA Parameters */
#define LORAWAN_APP_EUI			{ 0x2C, 0xF7, 0xF1, 0x20, 0x32, 0x30,\
					  0x4D, 0xE8 }
#define LORAWAN_JOIN_EUI		{ 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,\
					  0x00, 0x06 }
#define LORAWAN_APP_KEY			{ 0xB4, 0x4E, 0x83, 0x6D, 0x99, 0x06,\
					  0x79, 0xC8, 0x78, 0x03, 0x1A, 0x32,\
					  0x60, 0x51, 0x11, 0xC8 }

#else

/* ABP Parameters */
#define LORAWAN_DEV_ADDR 0x260CFF4F;
#define LORAWAN_JOIN_EUI		{ 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,\
					  0x00, 0x06 }
#define LORAWAN_DEV_EUI			{ 0x2C, 0xF7, 0xF1, 0x20, 0x32, 0x30,\
					  0x4D, 0xE8}					  
#define LORAWAN_APP_EUI			{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
					  0x00, 0x00}
#define LORAWAN_APP_SKEY		{ 0x88, 0x03, 0x0F, 0x9D, 0x09, 0x0D,\
					  0x09, 0xC0, 0x44, 0x63, 0x20, 0x4F, 0xC0, 0xE8,\
					  0x2C, 0xF5}
#define LORAWAN_NWK_SKEY		{ 0x06, 0xB1, 0x13, 0xEE, 0x65, 0x09,\
					  0xAA, 0xFB, 0x91, 0x75, 0xC2, 0x6E, 0xF2, 0x67,\
					  0x91, 0x92}
#endif
