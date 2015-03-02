/**
 * emonhub interfacer
 *
 * - handles RF24 on one end
 * - provides data via a fifo-special-file
 *
 */

#include <time.h>
//#include <cstdlib>
//#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>

#include <errno.h>

#include <RF24/RF24.h>

#include "pidfile.c"
#include "oem_protocol.h"

#define PID_FILE "/var/run/oem.pid"

#define PIPE_SNS	1

using namespace std;

RF24 radio(RPI_V2_GPIO_P1_22, RPI_V2_GPIO_P1_24, BCM2835_SPI_SPEED_8MHZ);  

//--------------------------------------------------------------------------------

//
// Channel info
//
//const uint8_t num_channels = 128;
//const uint8_t num_channels = 120;
//uint8_t values[num_channels];

const uint8_t pipes[][6] = { "0emon", "1emon", "2emon", "3emon", "4emon", "5emon" };

//const int num_reps = 100;
//int reset_array=0;

sStatus oem;
//oem_packet rx_buffer, tx_buffer;

time_t aclock, cclock, e_ts, p_ts;

bool daemonize = false;
bool keep_running = true;

int ph;

//--------------------------------------------------------------------------------

void updateEnergy( uint8_t pipe, oem_energy *nrg) {
    char buffer[80];
    time_t now = time(0);
    
    if ( !daemonize) printf( "NRG[%i]: #[%010d]  P1[%10d]Wh  P2[%10d]Wh  P3[%10d]Wh    PA[%10d]Wh    TOTAL[%10d]kWh\n", pipe, nrg->timestamp+nrg->duration, nrg->wh_CT1, nrg->wh_CT2, nrg->wh_CT3, nrg->wh_CT4, ( nrg->wh_CT1+nrg->wh_CT2+nrg->wh_CT3+nrg->wh_CT4) / 1000);
    
    if ( pipe == PIPE_SNS && now - e_ts > 59) {
      e_ts = now;
      
      if ( ph <= 0) ph = open( "/var/run/emon", O_WRONLY | O_NONBLOCK);
      if ( ph > 0) {
        sprintf( buffer, "%i %d %d %d %d %d\n", 10, nrg->timestamp+nrg->duration, nrg->wh_CT1, nrg->wh_CT2, nrg->wh_CT3, nrg->wh_CT4);      
        if ( -1 == write( ph, buffer, strlen( buffer))) ph = -1;
      } else {
        syslog( LOG_NOTICE, "NRG: #[%010d]  P1[%10d]Wh  P2[%10d]Wh  P3[%10d]Wh    PA[%10d]Wh    TOTAL[%10d]kWh\n", nrg->timestamp+nrg->duration, nrg->wh_CT1, nrg->wh_CT2, nrg->wh_CT3, nrg->wh_CT4, ( nrg->wh_CT1+nrg->wh_CT2+nrg->wh_CT3+nrg->wh_CT4) / 1000);
      }
    }
}

void updatePower( uint8_t pipe, oem_power *pwr) {
    char buffer[80];
    time_t now = time(0);

    if ( !daemonize) printf( "POW[%i]:        [%3dV]  P1[%10d]W   P2[%10d]W   P3[%10d]W     PA[%10d]W     TOTAL[%10u]W\n", pipe, pwr->voltage, pwr->realPower_CT1, pwr->realPower_CT2, pwr->realPower_CT3, pwr->realPower_CT4, pwr->realPower_CT1+pwr->realPower_CT2+pwr->realPower_CT3+pwr->realPower_CT4);

    if ( pipe == PIPE_SNS && now - p_ts > 9) {
      p_ts = now;
      
      if ( ph <= 0) ph = open( "/var/run/emon", O_WRONLY | O_NONBLOCK);
      if ( ph > 0) {
          sprintf( buffer, "%i %d %d %d %d %d %d\n", 5, pwr->timestamp, pwr->voltage, pwr->realPower_CT1, pwr->realPower_CT2, pwr->realPower_CT3, pwr->realPower_CT4);
          if ( -1 == write( ph, buffer, strlen( buffer))) ph = -1;
      } else {
        syslog( LOG_NOTICE, "POW:        [%3dV]  P1[%10d]W   P2[%10d]W   P3[%10d]W     PA[%10d]W     TOTAL[%10u]W\n", pwr->voltage, pwr->realPower_CT1, pwr->realPower_CT2, pwr->realPower_CT3, pwr->realPower_CT4, pwr->realPower_CT1+pwr->realPower_CT2+pwr->realPower_CT3+pwr->realPower_CT4);
      }
    }
}

void handle_signal(int signal) {
  const char *signal_name;
  sigset_t pending;
   
  // Find out which signal we're handling
  switch (signal) {
    case SIGPIPE:
    	syslog( LOG_INFO, "Caught SIGPIPE, connection to emonhub broke ...");
    break;
    case SIGHUP:
    	syslog( LOG_INFO, "Caught SIGHUP, reconfiguring ... not yet.");
    break;
    case SIGINT:
	keep_running = false;
    	syslog( LOG_INFO, "Caught SIGINT, exiting now");
    	break;
    case SIGTERM:
	keep_running = false;
    	syslog( LOG_INFO, "Caught SIGTERM, exiting now");
    	break;
    default:
    	syslog( LOG_INFO, "Caught wrong signal: %d\n", signal);
  }
}

int main(int argc, char** argv)
{
  struct sigaction sa;
  pid_t pid;

  openlog( argv[0], LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL7);

  syslog( LOG_NOTICE, "OpenEnergyMonitor RF24 monitor starting ...");
  printf( "OpenEnergyMonitor RF24 monitor");
  printf( "[ str#%i nrg%i pwr#%i ]\n", sizeof( oem_packet), sizeof( oem_energy), sizeof( oem_power));

  if ( argc > 1 && 0 == strncmp( "-d", argv[1], 2)) {
    daemonize = true;
    
    printf( "  daemonize, running in background ...\n");
    
    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
          exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
          exit(EXIT_SUCCESS);

      /* On success: The child process becomes session leader */
    if (setsid() < 0)
          exit(EXIT_FAILURE);
  } else {
    printf( "  interactive mode ...\n");
  }

  /* Catch, ignore and handle signals */
  //TODO: Implement a working signal handler */
  sa.sa_handler = &handle_signal;
  sa.sa_flags = SA_RESTART;
  sigfillset(&sa.sa_mask); 
  
   // Intercept SIGHUP and SIGINT
  if (sigaction(SIGHUP, &sa, NULL) == -1) {
	syslog( LOG_NOTICE, "Error: cannot handle SIGHUP"); // Should not happen
  }
   
  if (sigaction(SIGTERM, &sa, NULL) == -1) {
	syslog( LOG_NOTICE, "Error: cannot handle SIGTERM"); // Should not happen
  }
   
  if (sigaction(SIGINT, &sa, NULL) == -1) {
	syslog( LOG_NOTICE, "Error: cannot handle SIGINT"); // Should not happen
  }

  if (sigaction(SIGPIPE, &sa, NULL) == -1) {
	syslog( LOG_NOTICE, "Error: cannot handle SIGPIPE"); // Should not happen
  }

  if ( daemonize) {
    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
          exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
          exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);
    
    if ( check_pid( PID_FILE)) {
      syslog( LOG_NOTICE, "already running? locked!\n");
      exit(1);
    } else {
      write_pid( PID_FILE);
    }
  }
 
  //Serial.begin(57600);
  //printf_begin();
  
  while( keep_running) {
    //
    // Setup and configure rf radio
    //
    radio.begin();
    radio.setChannel( 32);
    radio.setPALevel( RF24_PA_MAX);
    radio.setDataRate( RF24_250KBPS);
    radio.setAutoAck(1);                     // Ensure autoACK is enabled
    radio.enableAckPayload();
    radio.enableDynamicPayloads();
    radio.setRetries( 2, 10); // Optionally, increase the delay between retries & # of retries
    radio.setCRCLength( RF24_CRC_16);

    radio.openWritingPipe( pipes[2]);

    radio.openReadingPipe( 0, pipes[0]);
    radio.openReadingPipe( 1, pipes[1]);
    radio.openReadingPipe( 2, pipes[2]);
    radio.openReadingPipe( 3, pipes[3]);
    radio.openReadingPipe( 4, pipes[4]);
    radio.openReadingPipe( 5, pipes[5]);
          
    // Get into standby mode
    radio.startListening();
      
    if ( !daemonize) {
      radio.printDetails();
      
      printf( "-------------------------------------------------\n\n");
  //    updatePower();
  //    updateEnergy();
    }
    
    int i = 0;
    char s[64];

    aclock = time( 0);
    aclock -= aclock % 10; 

    cclock = time( 0); 

    char rbuf[32];
    char tbuf[32];

    tbuf[0] = OEM_TIMESTAMP;
    memcpy( &tbuf[1], &cclock, sizeof( cclock));
    radio.writeAckPayload( 1, &tbuf, sizeof( cclock)+1);
      
    // forever loop
    while( keep_running) {
      uint8_t pipe;
  //    static char pbuf[32];

      cclock = time( 0); 
      
      if( radio.available( &pipe)){
      
        uint8_t psize = radio.getDynamicPayloadSize();

        
  //      if ( ! daemonize) printf( "pipe: [%04x] #%i\n", pipe, psize);
        
        if( psize < 1){
          // Corrupt payload has been flushed
  //        printf( ".");
          usleep( 100000);
          continue;
        }
        radio.read( &rbuf, psize);

        if ( pipe != PIPE_SNS) {
          if ( !daemonize) printf( "RX[%i] [%i #%i]\n", pipe, rbuf[0], psize);
//          continue;
        }
  /*      
        printf( "packet: [%02x] #%i\n", rx_buffer.packet_type, psize);

        for( char i=0; i < psize-1; i++) {
          printf( "%02x ", rx_buffer.data[i]);
        }
        printf( "\n");
  */
        switch( rbuf[0]) {
          case OEM_POWER:
            memcpy( &( oem.power), &rbuf[1], sizeof( oem_power));
            updatePower( pipe, &( oem.power));
          break;
          
          case OEM_ENERGY:
            memcpy( &( oem.energy), &rbuf[1], sizeof( oem_energy));
            updateEnergy( pipe, &( oem.energy));
          break;
          
          case OEM_TIMESTAMP:
            long ts;
            memcpy( &ts, &rbuf[1], sizeof( ts));
            syslog( LOG_INFO, "TS[%i]  #[ %ld ]\n", pipe, ts);
          break;
          
          case OEM_NOP:
            syslog( LOG_INFO, "NOP[%i]\n", pipe);
          break;
          
          default:
            syslog( LOG_INFO, "PCK[%i] [ %02x ] #[ %i ]\n", pipe, rbuf[0], psize);
        }

        tbuf[0] = OEM_TIMESTAMP;
        memcpy( &tbuf[1], &cclock, sizeof( cclock));
        radio.writeAckPayload( pipe, &tbuf, sizeof( cclock)+1);
        aclock = cclock;

        // echo out measurement data on display channel
        radio.stopListening();
        radio.writeFast( &rbuf, psize);
//        radio.txStandBy( 10);
        radio.startListening();

      } else {
        i++;
      }

      if ( cclock - 30 >= aclock) {
        strftime( s, 60, "---: %a, %e. %B %Y, %T %Z", localtime( &cclock));

        if ( daemonize) syslog( LOG_INFO, "%s [%ld] [%i]", s, cclock, i);
        else printf( "TIME: %s [%ld] [%i]\n", s, cclock, i);

        aclock = cclock;
        break;
      }

//      if ( !daemonize) printf( ".");
      
      usleep( 100000);
    }
  }
    
  syslog( LOG_NOTICE, "OpenEnergyMonitor RF24 stopping.");
  if ( !daemonize) printf( "OpenEnergyMonitor RF24 stopping.");
  closelog();
  
  if ( ph) close( ph);
	
  return 0;
}

