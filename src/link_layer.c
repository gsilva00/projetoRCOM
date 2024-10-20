// Link layer protocol implementation

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "link_layer.h"
#include "serial_port.h"

#include "frame_utils.h"


// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source


// ? For role distinction (for easier access, and MAINLY FOR llclose() -> why isn't it in the arguments???)
static LinkLayerRole currRole;
static int currRetransmissions;

// for stats (llclose())
static unsigned int frameCount = 0;
static unsigned int retransmissionCount = 0;
static unsigned int timeoutCount = 0;
static unsigned int errorCount = 0;

// for alarm (used by Tx)
static unsigned int alarmEnabled = FALSE;
static unsigned int alarmCount = 0;

static void alarmHandler(int signal)
{
  alarmEnabled = FALSE;
  alarmCount++;

  printf("alarmHandler() call #%d\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
	if (openSerialPort(connectionParameters.serialPort,
											connectionParameters.baudRate) < 0)
	{
    printf("Failed to open serial port %s\n", connectionParameters.serialPort);
		return -1;
	}

  // Save connection parameters for later use
  currRole = connectionParameters.role;
  currRetransmissions = connectionParameters.nRetransmissions;

  unsigned char openBuf[SU_BUF_SIZE] = {0};

  if (currRole == LlTx) {
    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);

    int uaReceived = FALSE;
    int readRet;
    while (alarmCount < connectionParameters.nRetransmissions && !uaReceived) {
      // Prepare and send SET frame
      prepSU(openBuf, SU_Addr_TX, SU_C_SET);
      if (writeSU(openBuf) == -1) {
        printf("%s: Tx write error!\n", __func__);
        return -1;
      }
      // Clear the buffer with the sent message
      // Will fill with received message
      memset(openBuf, 0, sizeof(openBuf));

      // ALARM FOR MAX TIME TO RECEIVE UA
      alarm(3);
      alarmEnabled = TRUE;
      printf("Alarme set for 3 seconds!\n");

      // Receive UA frame
      if ((readRet = readSU(&openBuf, SU_C_UA)) == -1) {
        printf("%s: Tx readSU error!\n", __func__);
        return -1;
      }
      else if (readRet == 0) {
        printf("%s: Tx readSU timeout!\n", __func__);
        continue;
      }
      else {
        uaReceived = TRUE;
        alarmCount = 0;
        alarmEnabled = FALSE;
        printf("%s: UA frame received!\n", __func__);
      }
    }

    if (!uaReceived) { // Exceeded retransmissions (maybe Rx is turned off)
      printf("%s: Maximum retransmissions reached, UA not received\n", __func__);
      return -1;
    }
  }
  else { // currRole == LlRx
    if (readSU(openBuf, SU_C_SET) == -1) {
      printf("%s: Rx readSU error!\n", __func__);
      return -1;
    }

    // Prepare and send UA frame
    prepSU(&openBuf, SU_Addr_TX, SU_C_UA);
    if (writeSU(openBuf) == -1) {
      printf("%s: Rx write error!\n", __func__);
      return -1;
    }
  }

	return 1; // Success
}


////////////////////////////////////////////////
// LLWRITE - For Transmitter (Tx) of Link Layer -> receives data from Application Layer (through the arguments) and sends data to Rx
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
  // TODO

  return 0;
}


////////////////////////////////////////////////
// LLREAD - For Receiver (Rx) of Link Layer -> receives data from Tx, and "sends" (returns through the argument) to application layer
// Ter atenção ao MAX_PAYLOAD_SIZE (macro)
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
  // TODO

  return 0;
}


////////////////////////////////////////////////
// LLCLOSE - For Transmitter (Tx) - no way of distinguishing Tx from Rx (no connectionParameters)
// Tx uses when he wants to close
// Rx reads values sent here by Tx in llwrite
// Rx invoke this when it wants to turn off
////////////////////////////////////////////////
int llclose(int showStatistics)
{
  unsigned char closeBuf[SU_BUF_SIZE] = {0};

  if (currRole == LlTx) {
    int discReceived = FALSE;
    int readRet;

    while (alarmCount < currRetransmissions && !discReceived) {
      // Prepare and send DISC frame
      prepSU(closeBuf, SU_Addr_TX, SU_C_DISC);
      if (writeSU(closeBuf) == -1) {
        printf("%s: Tx write error!\n", __func__);
        return -1;
      }


      // Clear the buffer with the sent message
      // Will fill with received message
      memset(closeBuf, 0, sizeof(closeBuf));

      // ALARM FOR MAX TIME TO RECEIVE DISC
      alarm(3);
      alarmEnabled = TRUE;
      printf("Alarme set for 3 seconds!\n");

      // Receive DISC frame
      if ((readRet = readSU(&closeBuf, SU_C_DISC)) == -1) {
        printf("%s: Tx readSU error!\n", __func__);
        return -1;
      }
      else if (readRet == 0) {
        printf("%s: Tx readSU timeout!\n", __func__);
        continue;
      }
      else {
        discReceived = TRUE;
        alarmCount = 0;
        alarmEnabled = FALSE;
        printf("%s: DISC frame received!\n", __func__);
      }


      // Prepare to send UA frame (LAST)
      prepSU(closeBuf, SU_Addr_TX, SU_C_UA);
      if (writeSU(closeBuf) == -1) {
        printf("%s: Tx write error!\n", __func__);
        return -1;
      }
    }

    if (!discReceived) { // Exceeded retransmissions (maybe Rx is turned off)
      printf("%s: Maximum retransmissions reached, disc not received\n", __func__);
      return -1;
    }
  }
  else { // currRole == LlRx
    // ?? Do nothing??
  }

  // Print stats
  if (showStatistics) {
    statAnalysis();
  }

  int clstat = closeSerialPort();
  printf("%s - Serial port of role: %s has been closed", __func__, (currRole == LlTx) ? "LlTx" : "LlRx");
  return clstat;
}

static void statAnalysis() {
  printf("Number of frames: %d\n", frameCount);
  printf("Number of retransmissions: %d\n", retransmissionCount);
  printf("Number of timeouts: %d\n", timeoutCount);
  printf("Number of errors: %d\n", errorCount);

  // !! provavelmente há mais, muito mais
}


// Send Supervision/Unnumbered Frames
static int writeSU(const unsigned char *buf)
{
  if (writeBytesSerialPort(buf, 5) != 5) { // ?? FAÇO != 5 OU == -1
    return -1;
  }
  printf("Unnumbered (U) message written!\n");
  return 1;
}

// Read Supervision/Unnumbered Frames
static int readSU(unsigned char *buf, unsigned char ctrl)
{
  SU_State currState = SU_START;
  unsigned char currByte;
  // !! PASSAR ESTE WHILE PARA RECEBER UM A UM (É ISSO QUE É O PART LÁ EM BAIXO)
  while (currState != SU_DONE && (!alarmEnabled && currRole == LlTx || currRole == LlRx)) {
    // Receiver stays here until it reads SET,
    // Transmitter has timeout if not received UA, to send SET again

    if (readByteSerialPort(&currByte) == -1) { // Read error
      return -1;
    }

    printf("The received byte is: 0x%02x\n", currByte);


    switch(currState) {
      case SU_START:
        if (currByte == SU_Flag) {
          currState = SU_FLAG_STATE;
          buf[0] = currByte;
          printf("State has changed from START to FLAG_STATE\n");
        }
        else {
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          printf("State didn't change from START because the byte isn't a Flag. Cleared the buffer\n");
        }
        break;

      case SU_FLAG_STATE:
        if (currByte == SU_Addr_TX) {
          currState = SU_A_STATE;
          printf("State has changed from FLAG_STATE to A_STATE\n");
          buf[1] = currByte;
        }
        else if (currByte == SU_Flag) {
          printf("State didn't change from FLAG_STATE because the byte is a Flag, again.");
          continue;
        }
        else {
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          currState = SU_START;
          printf("State has changed from FLAG_STATE to START because the byte isn't an Address byte or a Flag. Cleared the buffer\n");
        }
        break;

      case SU_A_STATE:
        if (currByte == ctrl) {
          currState = SU_C_STATE;
          buf[2] = currByte;
          printf("State has changed from A_STATE to C_STATE.\n");
        }
        else if (currByte == SU_Flag) {
          currState = SU_FLAG_STATE;
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          buf[1] = SU_Flag;
          printf("State has changed from A_STATE to FLAG_STATE because the byte is a Flag.\n");
        }
        else {
          currState = SU_START;
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          printf("State has changed from A_STATE to START because the byte isn't a Control byte or a Flag. Cleared the buffer\n");
        }
        break;


      case SU_C_STATE:
        if (currByte == SU_BCC1(buf[1], buf[2])) { // Uses BCC to check if the message is correctly received
          currState = SU_BCC_STATE;
          buf[3] = currByte;
          printf("State has changed from C_STATE to BCC_STATE.\n");
        }
        else if (currByte == SU_Flag) {
          currState = SU_FLAG_STATE;
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          buf[1] = SU_Flag;
          printf("State has changed from C_STATE to FLAG_STATE because the byte is a Flag.\n");
        }
        else {
          currState = SU_START;
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          printf("State has changed from C_STATE to START because the byte isn't a BCC byte or a Flag. Cleared the buffer\n");
        }
        break;

      case SU_BCC_STATE:
        if (currByte == SU_Flag) {
          currState = SU_DONE;
          buf[4] = currByte;
          printf("State has changed from BCC_STATE to DONE\n");
        }
        else {
          currState = SU_START;
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          printf("State has changed from C_STATE to START because the byte isn't a BCC byte or a Flag. Cleared the buffer\n");
        }
        break;
    }
  }

  if (alarmEnabled) { // Only for LlTx
    alarmEnabled == FALSE;
    return 0;
  }

  printf("SET message received!\n");
  return 1;
}
