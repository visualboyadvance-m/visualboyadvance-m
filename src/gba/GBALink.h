#ifndef GBA_GBALINK_H
#define GBA_GBALINK_H

/**
 * Link modes to be passed to InitLink
 */
enum LinkMode {
    LINK_DISCONNECTED,
    LINK_CABLE_IPC,
    LINK_CABLE_SOCKET,
    LINK_RFU_IPC,
    LINK_RFU_SOCKET,
    LINK_GAMECUBE_DOLPHIN,
    LINK_GAMEBOY_IPC,
    LINK_GAMEBOY_SOCKET
};

/**
 * State of the connection attempt
 */
enum ConnectionState { LINK_OK,
    LINK_ERROR,
    LINK_NEEDS_UPDATE,
    LINK_ABORT };

/**
 * Initialize GBA linking
 *
 * @param mode Device to emulate, plugged to the GBA link port.
 * @return success
 */
extern ConnectionState InitLink(LinkMode mode);

/**
 * Update a link connection request
 *
 * @param message Information message
 * @param size Maximum message size
 */
extern ConnectionState ConnectLinkUpdate(char* const message, size_t size);

/**
 * Get the currently enabled link mode
 *
 * @return link mode
 */
extern LinkMode GetLinkMode();

/**
 * Is this instance going to host a LAN link server?
 *
 * @param enabled Server mode
 * @param numSlaves Number of expected clients
 */
extern void EnableLinkServer(bool enable, int numSlaves);

/**
 * Should speed hacks be used?
 *
 * @param enabled Speed hacks
 */
extern void EnableSpeedHacks(bool enable);

/**
 * Set the host to connect to when in socket mode
 *
 * @return false if the address is invalid
 */
extern bool SetLinkServerHost(const char* host);

/**
 * Get the host relevant to context
 *
 * If in lan server mode, returns the external IP adress
 * If in lan client mode, returns the IP adress of the host to connect to
 * If in gamecube mode, returns the IP adress of the dolphin host
 *
 */
extern void GetLinkServerHost(char* const host, size_t size);

/**
 * Set the value in milliseconds of the timeout after which a connection is
 * deemed lost.
 *
 * @param value timeout
 */
extern void SetLinkTimeout(int value);

/**
 * Verify that the link between the emulators is still active
 */
extern void CheckLinkConnection();

/**
 * Set the current link mode to LINK_DISCONNECTED
 */
extern void CloseLink();

/**
 * Get the id of the player of this VBA instance
 *
 * @return id -1 means disconnected, 0 means master, > 0 means slave
 */
extern int GetLinkPlayerId();

/**
 * Start a link transfer
 *
 * @param siocnt the value of SIOCNT to be written
 */
extern void StartLink(uint16_t siocnt);

/**
 * Start a general purpose link transfer
 *
 * @param rcnt the value of RCNT to be written
 */
extern void StartGPLink(uint16_t rcnt);

/**
 * Emulate the linked device
 */
extern void LinkUpdate(int);

/**
 * Clean up IPC shared memory
 */
extern void CleanLocalLink();

/**
 * Append the current VBA ID to a filemane
 *
 * @param Input filename to complete
 * @return completed filename
 */

extern const char* MakeInstanceFilename(const char* Input);

// register definitions
#define COMM_SIODATA32_L 0x120 // Lower 16bit on Normal mode
#define COMM_SIODATA32_H 0x122 // Higher 16bit on Normal mode
#define COMM_SIOCNT 0x128
#define COMM_SIODATA8 0x12a // 8bit on Normal/UART mode, (up to 4x8bit with FIFO)
#define COMM_SIOMLT_SEND 0x12a // SIOMLT_SEND (16bit R/W) on MultiPlayer mode (local outgoing)
#define COMM_SIOMULTI0 0x120 // SIOMULTI0 (16bit) on MultiPlayer mode (Parent/Master)
#define COMM_SIOMULTI1 0x122 // SIOMULTI1 (16bit) on MultiPlayer mode (Child1/Slave1)
#define COMM_SIOMULTI2 0x124 // SIOMULTI2 (16bit) on MultiPlayer mode (Child2/Slave2)
#define COMM_SIOMULTI3 0x126 // SIOMULTI3 (16bit) on MultiPlayer mode (Child3/Slave3)
#define COMM_RCNT 0x134 // SIO Mode (4bit data) on GeneralPurpose mode
#define COMM_IR 0x136 // Infrared Register (16bit) 1bit data at a time(LED On/Off)?
#define COMM_JOYCNT 0x140
#define COMM_JOY_RECV_L 0x150 // Send/Receive 8bit Lower first then 8bit Higher
#define COMM_JOY_RECV_H 0x152
#define COMM_JOY_TRANS_L 0x154 // Send/Receive 8bit Lower first then 8bit Higher
#define COMM_JOY_TRANS_H 0x156
#define COMM_JOYSTAT 0x158 // Send/Receive 8bit lower only

#define JOYSTAT_RECV 2
#define JOYSTAT_SEND 8

#define JOYCNT_RESET 1
#define JOYCNT_RECV_COMPLETE 2
#define JOYCNT_SEND_COMPLETE 4
#define JOYCNT_INT_ENABLE 0x40

#define LINK_PARENTLOST 0x80

#define UNSUPPORTED -1
#define MULTIPLAYER 0
#define NORMAL8 1
#define NORMAL32 2 // wireless use normal32 also
#define UART 3
#define JOYBUS 4
#define GP 5
#define INFRARED 6 // Infrared Register at 4000136h

#define RFU_INIT 0
#define RFU_COMM 1
#define RFU_SEND 2
#define RFU_RECV 3

#define RF_RECVCMD \
    0x278 // Unknown, Seems to be related to Wireless Adapter(RF_RCNT or armMode/CPSR or CMD
// sent by the adapter when RF_SIOCNT=0x83 or when RCNT=0x80aX?)
#define RF_CNT 0x27a // Unknown, Seems to be related to Wireless Adapter(RF_SIOCNT?)

typedef struct {
    uint8_t len; // data len in 32bit words
    uint8_t gbaid; // source id
    uint32_t time; // linktime
    uint32_t data[255];
} rfu_datarec;

extern uint8_t gbSIO_SC;
extern bool LinkIsWaiting;
extern bool LinkFirstTime;
extern bool EmuReseted;
extern void gbInitLink();
extern uint8_t gbStartLink(uint8_t b);
extern uint16_t gbLinkUpdate(uint8_t b, int gbSerialOn);
extern void gbInitLinkIPC();
extern uint8_t gbStartLinkIPC(uint8_t b);
extern uint16_t gbLinkUpdateIPC(uint8_t b, int gbSerialOn);

extern void BootLink(int m_type, const char* host, int timeout, bool m_hacks, int m_numplayers);

#endif /* GBA_GBALINK_H */
