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
 * Set the host to connect to when in socket mode
 */
extern void SetLinkServerHost(const char *host);

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
 * Verify that the link between the emulators is still active
 */
extern void CheckLinkConnection();

/**
 * Set the current link mode to LINK_DISCONNECTED
 */
extern void CloseLink();

// register definitions; these are always present

#define UNSUPPORTED -1
#define MULTIPLAYER 0
#define NORMAL8 1
#define NORMAL32 2
#define UART 3
#define JOYBUS 4
#define GP 5

#define RFU_INIT 0
#define RFU_COMM 1
#define RFU_SEND 2
#define RFU_RECV 3

#define COMM_SIODATA32_L	0x120
#define COMM_SIODATA32_H	0x122
#define COMM_SIOCNT			0x128
#define COMM_SIODATA8		0x12a
#define COMM_SIOMLT_SEND 0x12a
#define COMM_SIOMULTI0 0x120
#define COMM_SIOMULTI1 0x122
#define COMM_SIOMULTI2 0x124
#define COMM_SIOMULTI3 0x126
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

enum
{
	JOY_CMD_RESET	= 0xff,
	JOY_CMD_STATUS	= 0x00,
	JOY_CMD_READ	= 0x14,
	JOY_CMD_WRITE	= 0x15		
};

extern const char *MakeInstanceFilename(const char *Input);

#ifndef NO_LINK
// Link implementation
#include <SFML/Network.hpp>

typedef struct {
	sf::SocketTCP tcpsocket;
	int numslaves;
	int connectedSlaves;
	int type;
	bool server;
	bool speed;
} LANLINKDATA;

extern sf::IPAddress joybusHostAddr;
extern void JoyBusUpdate(int ticks);

extern void StartLink(u16);
extern void StartGPLink(u16);
extern void LinkUpdate(int);
extern void CleanLocalLink();
extern LANLINKDATA lanlink;
extern int vbaid;
extern int linktimeout;
extern int linkid;

#else

// stubs to keep #ifdef's out of mainline
inline void JoyBusUpdate(int) { }

inline bool InitLink() { return true; }
inline void CloseLink() { }
inline void StartLink(u16) { }
inline void StartGPLink(u16) { }
inline void LinkUpdate(int) { }
inline void CleanLocalLink() { }
#endif

#endif /* GBA_GBALINK_H */
