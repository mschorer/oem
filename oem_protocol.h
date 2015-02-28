/*
 * raspberry definitions for protocol
 *
 */
 
#ifndef __OEM_PROTOCOL_H__
#define __OEM_PROTOCOL_H__

enum packetTypes { OEM_ENERGY, OEM_POWER, OEM_TIMESTAMP, OEM_NOP};

typedef struct { 
  unsigned int timestamp; 
  unsigned int duration; 
  unsigned int wh_CT1;
  unsigned int wh_CT2;
  unsigned int wh_CT3;
  unsigned int wh_CT4;
} oem_energy;    // revised data for RF comms

typedef struct { 
  unsigned int timestamp; 
  short int realPower_CT1;
  short int realPower_CT2;
  short int realPower_CT3;
  short int realPower_CT4; 
  unsigned short int voltage;
  unsigned short int padding; 
} oem_power;    // revised data for RF comms

typedef struct { 
  uint32_t timestamp; 
} oem_timestamp;    // revised data for RF comms

// - All Atmega's shipped from OpenEnergyMonitor come with Arduino Uno bootloader

typedef struct {
  oem_timestamp ts;
  oem_power power;
  oem_energy energy;
} sStatus;    // revised data for RF comms

typedef struct {
  unsigned char packet_type;
  union {
    unsigned char data[];
    oem_energy energy;
    oem_power power;
    oem_timestamp timestamp;
  };
} oem_packet;

#endif
