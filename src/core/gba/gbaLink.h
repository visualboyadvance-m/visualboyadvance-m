#ifndef VBAM_CORE_GBA_GBALINK_H_
#define VBAM_CORE_GBA_GBALINK_H_

#include <cstdint>
#include <string>

#if defined(NO_LINK)
#error "This file should not be included with NO_LINK."
#endif  // defined(NO_LINK)

extern uint16_t IP_LINK_PORT;

extern std::string IP_LINK_BIND_ADDRESS;

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

#endif // VBAM_CORE_GBA_GBALINK_H_
