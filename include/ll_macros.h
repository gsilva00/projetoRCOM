#ifndef GPU_MACROS_H
#define GPU_MACROS_H

#include <stdio.h>


#define BUF_SIZE 256

// F - Flag
#define SU_FRM_F 0x7E           // Synchronization - start or end of frame
// A - Address field
#define SU_FRM_A_TX 0x03        // On COMMANDS SENT by Transmitter (TX) or REPLIES sent by Receiver (RX) 
#define SU_FRM_A_RX 0x01        // On COMMANDS SENT by RX or REPLIES sent by TX
// C - Control field to indicate the type of supervision frame/message
#define SU_FRM_C_SET 0x03       // Set up - Synchronization - sent by transmitter to initiate connection
#define SU_FRM_C_UA 0x07        // UA - unnumbered acknowledgement - confirmation to reception of a valid supervision frame
#define SU_FRM_C_RR0 0xAA       // Positive ACK - Receiver ready - receive information frame 0
#define SU_FRM_C_RR1 0xAB       // Positive ACK - Receiver ready - receive information frame 1
#define SU_FRM_C_REJ0 0x54      // Negative ACK - Receiver rejects - reject information in frame 0 (detected error)
#define SU_FRM_C_REJ1 0x55      // Negative ACK - Receiver rejects - reject information in frame 1 (detected error)
#define SU_FRM_C_DISC 0x0B      // DISC - disconnect - indicate the termination of connection
// BCC1 - Block Check Character - Protection Field to detect the occurrence of errors in header
#define SU_FRM_BCC1(a,c) (a^c)  // Field to detect occurences of errors in the header

// F - Flag
#define I_FRM_F 0x7E            // Synchronization - start or end of frame
// A - Address field
#define I_FRM_A_TX 0x03         // On COMMANDS SENT by TX or REPLIES sent by RX
#define I_FRM_A_RX 0x01         // On COMMANDS SENT by RX or REPLIES sent by TX
// C - Control Field to allow numbering information frames
#define I_FRM_C0 0x00           // Information frame 0
#define I_FRM_C1 0x80           // Information frame 1

// Information Field (packet generated by the Application)

// Independent Protection Fields (1 – header, 2 – data)
#define I_FRM_BCC1(a,c) (a^c)   // Field to detect occurences of errors in the header
#define I_FRM_BCC2(count, ...) xor_all(count, __VA_ARGS__) // Field to detect the occurrence of errors in the data field

// Helper function to XOR a large number of arguments
unsigned int xor_all(int count, ...) {
  unsigned int res = 0;
  unsigned int current;
  va_list args;

  va_start(args, count);
  for (int i = 0; i < count; i++) {
    current = va_arg(args, unsigned int);
    res ^= current;
  }
  va_end(args);

  return res;
}




#endif