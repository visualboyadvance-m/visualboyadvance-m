#ifndef GBA_GBALINK_H
#define GBA_GBALINK_H

/**
 * Link modes to be passed to InitLink
 */
enum LinkMode
{
	LINK_DISCONNECTED,
	LINK_CABLE_IPC,
	LINK_CABLE_SOCKET,
	LINK_RFU_IPC,
	LINK_GAMECUBE_DOLPHIN
};

/**
 * State of the connection attempt
 */
enum ConnectionState
{
	LINK_OK,
	LINK_ERROR,
	LINK_NEEDS_UPDATE,
	LINK_ABORT
};

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
extern ConnectionState ConnectLinkUpdate(char * const message, size_t size);

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
extern bool SetLinkServerHost(const char *host);

/**
 * Get the host relevant to context
 *
 * If in lan server mode, returns the external IP adress
 * If in lan client mode, returns the IP adress of the host to connect to
 * If in gamecube mode, returns the IP adress of the dolphin host
 *
 */
extern void GetLinkServerHost(char * const host, size_t size);

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
extern void StartLink(u16 siocnt);

/**
 * Start a general purpose link transfer
 *
 * @param rcnt the value of RCNT to be written
 */
extern void StartGPLink(u16 rcnt);

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
extern const char *MakeInstanceFilename(const char *Input);

// register definitions
#define COMM_SIODATA32_L	0x120
#define COMM_SIODATA32_H	0x122
#define COMM_SIOCNT			0x128
#define COMM_SIODATA8		0x12a
#define COMM_SIOMLT_SEND	0x12a
#define COMM_SIOMULTI0		0x120
#define COMM_SIOMULTI1		0x122
#define COMM_SIOMULTI2		0x124
#define COMM_SIOMULTI3		0x126
#define COMM_RCNT			0x134
#define COMM_JOYCNT			0x140
#define COMM_JOY_RECV_L		0x150
#define COMM_JOY_RECV_H		0x152
#define COMM_JOY_TRANS_L	0x154
#define COMM_JOY_TRANS_H	0x156
#define COMM_JOYSTAT		0x158

#define JOYSTAT_RECV		2
#define JOYSTAT_SEND		8

#define JOYCNT_RESET			1
#define JOYCNT_RECV_COMPLETE	2
#define JOYCNT_SEND_COMPLETE	4
#define JOYCNT_INT_ENABLE		0x40

#endif /* GBA_GBALINK_H */
