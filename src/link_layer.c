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


int alarmEnabled = FALSE;   // from alarm
int alarmCount = 0;

void alarmHandler(int signal)
{
  alarmEnabled = FALSE;
  alarmCount++;

  printf("Alarm #%d\n", alarmCount);
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

  if (connectionParameters.role == LlTx)
  {
    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);

    int msgReceived = FALSE;
    while (alarmCount < connectionParameters.nRetransmissions && !msgReceived) 
    {
      unsigned char buf[BUF_SIZE] = {0};
      buf[0] = SU_FRM_F;
      buf[1] = SU_FRM_A_TX;
      buf[2] = SU_FRM_C_SET;
      buf[3] = SU_FRM_BCC1(buf[1], buf[2]);
      buf[4] = SU_FRM_F;

      if (sendSupervision(buf))
      {
        return -1;
      }


    }
  }
  else // connectionParameters.role == LlRx
  {

  }



	return 1;
}

// Send Supervision/Unnumbered Frames
int sendSU(const unsigned char *bytes) {
  if (alarmEnabled == FALSE)
  {
    if (writeBytesSerialPort(bytes, 5) == -1)
    {
      return -1;
    }
    printf("SET message written!\n");

    alarm(3);
    alarmEnabled = TRUE;
    printf("Alarme ligado!\n");

    return -1;
  }
}

int readSupervision(const unsigned char *bytes) {


  // Wait for UA response from the receiver
  // Returns after 5 chars have been input or 0 if error
  int bytesReceived = readByteSerialPort();

  if (bytesReceived != 0) {
    buf[bytesReceived] = '\0'; // Set end of string to '\0', so we can printf

    printf(":%s:%d\n", buf, bytesReceived);
    if (buf[1] ^ buf[2] == buf[3]) {
      // Uses BCC to check if the message is correctly sent
      msgReceived = TRUE;
    }
  }
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
  // TODO

  return 0;
}

////////////////////////////////////////////////
// LLREAD
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
