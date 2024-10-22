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
// ? For tracking the maximum number of retransmissions during the protocol (IS IT NEEDED?)
static int currRetransmissions;

// for stats (llclose())
static unsigned int frameCount = 0;
static unsigned int retransmissionCount = 0;
static unsigned int timeoutCount = 0; // ?? Não terá a mesma contagem que retransmissionCount, já que de cada vez que ocorre um timeout de leitura de resposta, ocorre uma retransmission ??
static unsigned int errorCount = 0;   // ?? A que tipos de erros é que isto se refere ?? Maximum number of retransmissions é considerado erro?

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

  unsigned char sendBuf[SU_BUF_SIZE] = {0};   // Buffer with SU message
  unsigned char retBuf[SU_BUF_SIZE] = {0};    // Feedback buffer

  if (currRole == LlTx) {
    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);

    int uaReceived = FALSE;
    int readRet;
    while (alarmCount < connectionParameters.nRetransmissions && !uaReceived) {
      // Prepare and send SET frame
      prepSU(sendBuf, SU_Addr_TX, SU_C_SET);
      if (writeSU(sendBuf) == -1) {
        errorCount++;
        printf("%s: Tx write error!\n", __func__);
        alarmCount = 0;
        alarmEnabled = FALSE;
        return -1;
      }

      // ALARM FOR MAX TIME TO RECEIVE UA
      alarm(ALARM_INTV);
      alarmEnabled = TRUE;
      printf("Alarme set for %d seconds!\n", ALARM_INTV);

      // Receive UA frame
      if ((readRet = readSU_OC(retBuf, SU_C_UA)) == -1) {
        errorCount++;
        printf("%s: Tx readSU error!\n", __func__);
        alarmCount = 0;
        alarmEnabled = FALSE;
        return -1;
      }
      else if (readRet == 0) {
        retransmissionCount++;
        printf("%s: Tx readSU timeout!\n", __func__);
        continue;
      }
      else { // readRet == 1
        uaReceived = TRUE;
        printf("%s: Tx readSU success! UA frame received!\n", __func__);
      }
    }

    // Reset alarm variables
    alarmCount = 0;
    alarmEnabled = FALSE;

    if (!uaReceived) { // Exceeded retransmissions (maybe Rx is turned off)
      printf("%s: Maximum retransmissions reached, UA not received!\n", __func__);
      return -1;
    }
  }
  else { // currRole == LlRx
    if (readSU_OC(retBuf, SU_C_SET) == -1) {
      errorCount++;
      printf("%s: Rx readSU error!\n", __func__);
      return -1;
    }

    // Prepare and send UA frame
    prepSU(sendBuf, SU_Addr_TX, SU_C_UA);
    if (writeSU(sendBuf) == -1) {
      errorCount++;
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
  // ?? O bufSize não devia ser unsigned int (já que nunca poderá ser negativo)
  if (bufSize <= 0) {
    return -1; // Invalid buffer size
  }

  // ?? Why is this error happening (é pela macro ser dinâmica)
  unsigned char stuffBuf[I_BUF_SIZE] = {0}; // Array that will be filled with the frame (including stuffed bits)
  unsigned char retBuf[SU_BUF_SIZE] = {0};  // Feedback buffer

  // ?? O BUF VEM COM HEADER E TAILER??? ACHO QUE NÃO
  // while (i < 4) { // Header
  //   stuffBuf[i] = buf[i];
  //   i++;
  // }
  // Preparing Header
  stuffBuf[0] = I_Flag;
  stuffBuf[1] = I_Addr_TX;
  stuffBuf[2] = I_C(frameCount);
  stuffBuf[3] = I_BCC1(stuffBuf[1], stuffBuf[2]);

  // Byte stuffing the packet
  int i = 0, j = 4;
  while (i < bufSize) {
    if (buf[i] == I_Flag || buf[i] == STUFF_ESC) {
        stuffBuf[j++] = STUFF_ESC;
        stuffBuf[j++] = STUFF_MASK(buf[i++]);
    } else {
        stuffBuf[j++] = buf[i++];
    }
  }

  // Preparing Trailer
  stuffBuf[j++] = funcI_BCC2(buf, bufSize);
  stuffBuf[j++] = I_Flag;
  // ?? O BUF VEM COM HEADER E TAILER??? ACHO QUE NÃO
  // while (i < bufSize) { // Trailer
  //   stuffBuf[i] = buf[i];
  //   i++;
  // }

  int rrReceived = FALSE;
  int readRet;
  while (alarmCount < currRetransmissions && !rrReceived) {
    // ?? (void)signal(SIGALRM, alarmHandler); Already did it in llopen() probably don't need to do it again right ??

    if (writeBytesSerialPort(stuffBuf, j) != j) { // ?? FAÇO != j OU == -1 ??
      errorCount++;
      printf("%s: Tx write error!\n", __func__);
      return -1;
    }

    // ALARM FOR MAX TIME TO RECEIVE RR OR REJ
    alarm(ALARM_INTV);
    alarmEnabled = TRUE;
    printf("Alarme set for %d seconds!\n", ALARM_INTV);

    // Receive UA frame
    if ((readRet = readSU_RW(retBuf, frameCount)) == -1) {
      errorCount;
      printf("%s: Tx readSU error!\n", __func__);
      alarmCount = 0;
      alarmEnabled = FALSE;
      return -1;
    }
    else if (readRet == 0) {
      retransmissionCount++;
      printf("%s: Tx readSU timeout/Neg ACK received!\n", __func__);
      continue;
    }
    else {
      rrReceived = TRUE;
      printf("%s: Tx readSU success! UA frame received!\n", __func__);
    }
  }

  alarmCount = 0;
  alarmEnabled = FALSE;

  if (!rrReceived) { // Exceeded retransmissions (maybe Rx is turned off)
    printf("%s: Maximum retransmissions reached, UA not received!\n", __func__);
    return -1;
  }

  return j;
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
        errorCount++;
        printf("%s: Tx write error!\n", __func__);
        return -1;
      }


      // Clear the buffer with the sent message
      // Will fill with received message
      memset(closeBuf, 0, sizeof(closeBuf));

      // ALARM FOR MAX TIME TO RECEIVE DISC
      alarm(ALARM_INTV);
      alarmEnabled = TRUE;
      printf("Alarme set for %d seconds!\n", ALARM_INTV);

      // Receive DISC frame
      if ((readRet = readSU_default(&closeBuf, SU_C_DISC)) == -1) {
        errorCount++;
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
        errorCount++;
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
  if (writeBytesSerialPort(buf, 5) != 5) { // ?? FAÇO != 5 OU == -1 ??
    return -1;
  }
  printf("Unnumbered (U) message written!\n");
  return 1;
}


// For llopen and llclose (UA, DISC) - frameNum not needed for SU-frames feedback
inline int readSU_OC(unsigned char *buf, unsigned char ctrl)
{
  return readSU(buf, ctrl, -1);
}
// For llread and llwrite (RR0, RR1, REJ0, REJ1) - ctrl not needed for I-Frame feedback
inline int readSU_RW(unsigned char *buf, int frameNum)
{
  return readSU(buf, 0xFF, frameNum);
}
// Read Supervision/Unnumbered Frames
static int readSU(unsigned char *buf, unsigned char ctrl, int frameNum)
{
  SU_State currState = SU_START;
  unsigned char currByte;

  // !! PASSAR ESTE WHILE PARA RECEBER UM A UM (É ISSO QUE É O PART LÁ EM BAIXO) - o que é isto??
  while (currState != SU_DONE && ((!alarmEnabled && currRole == LlTx) || currRole == LlRx)) {
    // Receiver stays here until it reads SET,
    // Transmitter has timeout if not received UA, to send SET again

    if (readByteSerialPort(&currByte) == -1) { // Read error
      errorCount++;
      return -1;
    }

    printf("The received byte is: 0x%02x\n", currByte);


    switch(currState) {
      case SU_START:
        if (currByte == SU_Flag) {
          currState = SU_FLAG_STATE;
          buf[0] = currByte;
          printf("State has changed from SU_START to SU_FLAG_STATE\n");
        }
        else {
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          printf("State didn't change from SU_START because the byte isn't a Flag. Cleared the buffer\n");
        }
        break;

      case SU_FLAG_STATE:
        if (currByte == SU_Addr_TX) {
          currState = SU_A_STATE;
          printf("State has changed from SU_FLAG_STATE to SU_A_STATE\n");
          buf[1] = currByte;
        }
        else if (currByte == SU_Flag) {
          printf("State didn't change from SU_FLAG_STATE because the byte is a Flag, again.");
          continue;
        }
        else {
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          currState = SU_START;
          printf("State has changed from SU_FLAG_STATE to SU_START because the byte isn't an Address byte or a Flag. Cleared the buffer\n");
        }
        break;

      case SU_A_STATE:
        // !! For llopen and llclose
        if (ctrl != 0xFF) { // && frameNum == -1
          if (currByte == ctrl) {
            currState = SU_C_STATE;
            buf[2] = currByte;
            printf("State has changed from SU_A_STATE to SU_C_STATE.\n");
          }
          else if (currByte == SU_Flag) {
            currState = SU_FLAG_STATE;
            memset(buf, 0, sizeof(SU_BUF_SIZE));
            buf[1] = SU_Flag;
            printf("State has changed from SU_A_STATE to SU_FLAG_STATE because the byte is a Flag.\n");
          }
          else {
            currState = SU_START;
            memset(buf, 0, sizeof(SU_BUF_SIZE));
            printf("State has changed from SU_A_STATE to SU_START because the byte isn't a Control byte or a Flag. Cleared the buffer\n");
          }
        }
        // !! For llread and llwrite
        else { // ctrl == 0xFF (obviously && frameNum != -1)
          if (currByte == SU_C_RR(frameNum)) {
            currState = SU_C_STATE;
            buf[2] = currByte;
            printf("State has changed from SU_A_STATE to SU_C_STATE.\n");
          }
          else if (currByte == SU_C_REJ(frameNum)) {
            printf("Function will return 0 because REJ response was reached!\n");
            return 0;
          }
          else if (currByte == SU_Flag) {
            currState = SU_FLAG_STATE;
            memset(buf, 0, sizeof(SU_BUF_SIZE));
            buf[1] = SU_Flag;
            printf("State has changed from SU_A_STATE to SU_FLAG_STATE because the byte is a Flag.\n");
          }
          else {
            currState = SU_START;
            memset(buf, 0, sizeof(SU_BUF_SIZE));
            printf("State has changed from SU_A_STATE to SU_START because the byte isn't a Control byte or a Flag. Cleared the buffer\n");
          }
        }
        break;


      case SU_C_STATE:
        if (currByte == SU_BCC1(buf[1], buf[2])) { // Uses BCC to check if the message is correctly received
          currState = SU_BCC_STATE;
          buf[3] = currByte;
          printf("State has changed from SU_C_STATE to SU_BCC_STATE.\n");
        }
        else if (currByte == SU_Flag) {
          currState = SU_FLAG_STATE;
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          buf[1] = SU_Flag;
          printf("State has changed from SU_C_STATE to SU_FLAG_STATE because the byte is a Flag.\n");
        }
        else {
          currState = SU_START;
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          printf("State has changed from SU_C_STATE to SU_START because the byte isn't a BCC byte or a Flag. Cleared the buffer\n");
        }
        break;

      case SU_BCC_STATE:
        if (currByte == SU_Flag) {
          currState = SU_DONE;
          buf[4] = currByte;
          printf("State has changed from SU_BCC_STATE to SU_DONE\n");
        }
        else {
          currState = SU_START;
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          printf("State has changed from SU_C_STATE to SU_START because the byte isn't a BCC byte or a Flag. Cleared the buffer\n");
        }
        break;
    }
  }

  // Read timeout
  if (alarmEnabled) { // Only for LlTx
    alarmEnabled == FALSE;
    return 0;
  }

  // Read success
  printf("SET message received!\n");
  return 1;
}


// Read Information Frame
static int readSU(unsigned char *buf, unsigned char ctrl)
{

}