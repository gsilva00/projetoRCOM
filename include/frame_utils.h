#ifndef GPU_MACROS_H
#define GPU_MACROS_H


#define SU_BUF_SIZE 5         // SU Frames have 5 bytes
#define I_BUF_SIZE(n) (2*n+6) // Byte Stuffing - I Frames have up to double the original size + header and tailer bytes

// Macros for the Supervision (S) and Unnumbered (U) Frames
// Byte 0,4 - F - Flag
#define SU_Flag 0x7E           // Flag - Synchronization - start or end of frame
// Byte 1 - A - Address field
#define SU_Addr_TX 0x03        // Address field - On COMMANDS SENT by Transmitter (TX) or REPLIES sent by Receiver (RX)
#define SU_Addr_RX 0x01        // Address field - On COMMANDS SENT by RX or REPLIES sent by TX
// Byte 2 - C - Control field to indicate the type of supervision frame/message
#define SU_C_SET 0x03          // Control field - Set up - Synchronization - sent by transmitter to initiate connection
#define SU_C_UA 0x07           // Control field - UA - unnumbered acknowledgement - confirmation to reception of a valid supervision frame
#define SU_C_RR0 0xAA          // Control field - Positive ACK - Receiver ready - receive information frame 0
#define SU_C_RR1 0xAB          // Control field - Positive ACK - Receiver ready - receive information frame 1
#define SU_C_REJ0 0x54         // Control field - Negative ACK - Receiver rejects - reject information in frame 0 (detected error)
#define SU_C_REJ1 0x55         // Control field - Negative ACK - Receiver rejects - reject information in frame 1 (detected error)
#define SU_C_DISC 0x0B         // Control field - DISC - disconnect - indicate the termination of connection
// Byte 3 - BCC1 - Block Check Character - Protection Field to detect the occurrence of errors in header
#define SU_BCC1(a,c) (a^c)     // Protection field - Field to detect occurences of errors in the header


// Macros for the Information (I) Frames
// Byte 0,Last - F - Flag
#define I_Flag 0x7E               // Flag - Synchronization - start or end of frame
// Byte 1 - A - Address field
#define I_Addr_TX 0x03         // Address field - On COMMANDS SENT by TX or REPLIES sent by RX
#define I_Addr_RX 0x01         // Address field - On COMMANDS SENT by RX or REPLIES sent by TX
// Byte 2 - C - Control Field to allow numbering information frames
#define I_C0 0x00              // Control field - Information frame 0
#define I_C1 0x80              // Control field - Information frame 1
#define I_C(n) (((n) % 2 == 0) ? I_C0 : I_C1) // Given the current frame count, get the control field

// Information Field here in the middle (packet generated by the Application) - no macros, just to see the layout of the payload

// Independent Protection Fields (Byte 3 - BCC1 - header, Byte Before Last - BCC2 - data)
#define I_BCC1(a,c) (a^c)                              // Protection field - Field to detect occurences of errors in the header
#define I_BCC2(arr, len) xor_all(arr, len) // Protection field - Field to detect the occurrence of errors in the data field
unsigned int xor_all(const unsigned char *arr, int len); // Helper function to XOR all bytes in an array


#define STUFF_ESC 0x7D                     // Escape octet to put before special data char
#define STUFF_MASK(byte) (byte^0x20)       // Octet to XOR with special data char


// Function to prepare Supervision and Unnumbered Frames
void prepSU(unsigned char *buf, unsigned char addr, unsigned char ctrl);



// State Machines
typedef enum {
  SU_START,
  SU_FLAG_STATE,
  SU_A_STATE,
  SU_C_STATE,
  SU_BCC_STATE,
  SU_DONE
} SU_State;

typedef enum {
  I_START,
  I_FLAG_STATE,
  I_A_STATE,
  I_C_STATE,
  I_BCC1_STATE,
  I_DATA_STATE,
  I_BCC2_STATE,
  I_DONE
} I_STATE;


#endif