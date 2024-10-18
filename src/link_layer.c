// Link layer protocol implementation

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "link_layer.h"
#include "serial_port.h"

#include "ll_macros.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source


// for Tx alarm
static int alarmEnabled = FALSE;
static int alarmCount = 0;

void alarmHandler(int signal)
{
  alarmEnabled = FALSE;
  alarmCount++;

  printf("alarmHandler #%d\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
	if (openSerialPort(connectionParameters.serialPort,
											connectionParameters.baudRate) < 0)
	{
		return -1;
	}

  unsigned char openBuf[SU_BUF_SIZE] = {0};


  if (connectionParameters.role == LlTx) {
    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);

    int uaReceived = FALSE;
    int readFeedback;
    while (alarmCount < connectionParameters.nRetransmissions && !uaReceived) {
      // Prepare and send SET frame
      prepSU(openBuf, SU_Addr_TX, SU_C_SET);
      if (writeSU(openBuf, connectionParameters.role) == -1) {
        printf("%s: Tx write error!", __func__);
        return -1;
      }
      // Clear the buffer with the sent message
      // Will fill with received message
      memset(openBuf, 0, sizeof(openBuf));

      // ALARM FOR MAX TIME TO RECEIVE UA
      alarm(3);
      alarmEnabled = TRUE;
      printf("Alarme ligado!\n");

      // Receive UA frame
      if ((readFeedback = readSU(&openBuf, connectionParameters.role)) == -1) {
        printf("%s: Tx readSU error!", __func__);
        return -1;
      }
      else if (readFeedback == 0) {
        printf("%s: Tx readSU timeout!", __func__);
        continue;
      }
      else {
        uaReceived = TRUE;
        alarmCount = 0;
        alarmEnabled = FALSE;
      }
    }
  }
  else { // connectionParameters.role == LlRx
    if (readSU(openBuf, connectionParameters.role) == -1) {
      printf("%s: Rx readSU error!", __func__);
      return -1;
    }

    // Prepare and send UA frame
    prepSU(&openBuf, SU_Addr_TX, SU_C_UA);
    if (writeSU(openBuf, connectionParameters.role) == -1) {
      printf("%s: Rx write error!", __func__);
      return -1;
    }
  }

	return 1; // Success
}

void prepSU(unsigned char *buf, unsigned char addr, unsigned char ctrl) {
  memset(buf, 0, SU_BUF_SIZE);
  buf[0] = SU_Flag;
  buf[1] = addr;
  buf[2] = ctrl;
  buf[3] = SU_BCC1(buf[1], buf[2]);
  buf[4] = SU_Flag;
}


// Send Supervision/Unnumbered Frames
int writeSU(const unsigned char *buf, LinkLayerRole currRole)
{
  if (writeBytesSerialPort(buf, 5) == -1) {
    return -1;
  }
  printf("Unnumbered (U) message written!\n");
  return 1;
}

int readSU(unsigned char *buf, LinkLayerRole currRole) // TALVEZ const unsigned char *bytes
{
  SU_State currState = START;
  unsigned char la_letra;
  // !! PASSAR ESTE WHILE PARA RECEBER UM A UM (É ISSO QUE É O PART LÁ EM BAIXO)
  while (currState != DONE && (!alarmEnabled && currRole == LlTx || currRole == LlRx)) {
    // Receiver stays here until it reads SET,
    // Transmitter has timeout if not received UA, to send SET again

    if (readByteSerialPort(&la_letra) == -1) { // Read error
      return -1;
    }

    printf("la_letra es= 0x%02x\n", la_letra);
    // !!
    /* if (buf[1] ^ buf[2] == buf[3]) { // Uses BCC to check if the message is correctly sent
      STOP = TRUE;
      printf("FLAG = 0x%02x;\nA = 0x%02x;\nC = 0x%02x;\nBCC = 0x%02x;\n", buf[0], buf[1], buf[2], buf[3]);
    }*/


    switch(currState) {
      case START:
        if (la_letra == SU_Flag) {
          currState = FLAG_STATE;
          buf[0] = la_letra;
          printf("el estado has mudado de START para FLAG_STATE\n");
        }
        else {
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          printf("el estado START no has mudado porque la letra no es una bandera, limpado el buffer.\n");
        }
        break;

      case FLAG_STATE:
        if (la_letra == SU_Addr_TX) {
          currState = A_STATE;
          printf("el estado has mudado de FLAG_STATE para A_STATE\n");
          buf[1] = la_letra;
        }
        else if (la_letra == SU_Flag) {
          printf("el mismo estado flag_state");
          continue;
        }
        else {
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          currState = START;
          printf("el estado FLAG_STATE has mudado para START porque la letra no es A, limpado el buffer.\n");
        }
        break;

      case A_STATE:
        if ((la_letra == SU_C_SET && currRole == LlRx) || (la_letra == SU_C_UA && currRole == LlTx)) {
          currState = C_STATE;
          buf[2] = la_letra;
          printf("el estado has mudado para C_STATE\n");
        }
        else if (la_letra == SU_Flag) {
          currState = FLAG_STATE;
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          buf[1] = SU_Flag;
          printf("el estado has mudado para FLAG_STATE\n");
        }
        else {
          currState = START;
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          printf("el estado FLAG_STATE has mudado para START porque la letra no es C, limpado el buffer.\n");
        }
        break;


      case C_STATE:
        if (la_letra == SU_BCC1(buf[1], buf[2])) { // Uses BCC to check if the message is correctly received
          currState = BCC_STATE;
          buf[3] = la_letra;
          printf("OK.el estado has mudado para BCC_STATE\n");
        }
        else if (la_letra == SU_Flag) {
          currState = FLAG_STATE;
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          buf[1] = SU_Flag;
          printf("el estado has mudado para FLAG_STATE\n");
        }
        else {
          currState = START;
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          printf("el estado has mudado para START porque la letra no es BCC, limpado el buffer.\n");
        }
        break;

      case BCC_STATE:
        if (la_letra == SU_Flag) {
          currState = DONE;
          buf[4] = la_letra;
          printf("el estado has mudado para DONE\n");
        }
        else {
          currState = START;
          memset(buf, 0, sizeof(SU_BUF_SIZE));
          printf("el estado has mudado para START porque la letra no es BCC, limpado el buffer.");
        }
        break;
    }
  }

  if (alarmEnabled) { // Only
    alarmEnabled == FALSE;
    return 0;
  }

  printf("SET message received!\n");
  return 1;
}

////////////////////////////////////////////////
// LLWRITE - For Receiver (Rx) of LL -> sends to Application Layer in the end
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
  // TODO

  return 0;
}

////////////////////////////////////////////////
// LLREAD - For Transmitter (Tx) of LL -> receives from Application Layer at the start
// Ter atenção ao MAX_PAYLOAD_SIZE (macro)
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
  // TODO

  return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
  // TODO







  int clstat = closeSerialPort();
  return clstat;
}
