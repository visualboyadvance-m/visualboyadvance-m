#pragma once

#include <SFML/Network.hpp>

#define LINK_PARENTLOST 0x80
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

#ifdef _MSC_VER
typedef struct {
	u16 linkdata[4];
	u16 linkcmd[4];
	u16 numtransfers;
	int lastlinktime;
	u8 numgbas;
	u8 linkflags;
	int rfu_q[4];
	u8 rfu_request[4];
	int rfu_linktime[4];
	u32 rfu_bdata[4][7];
	u32 rfu_data[4][32];
} LINKDATA;

class lserver{
	int numbytes;
	fd_set fdset;
	timeval wsocktimeout;
	//timeval udptimeout;
	char inbuffer[256], outbuffer[256];
	int *intinbuffer;
	u16 *u16inbuffer;
	int *intoutbuffer;
	u16 *u16outbuffer;
	int counter;
	int done;
public:
	int howmanytimes;
	SOCKET tcpsocket[4];
	SOCKADDR_IN udpaddr[4];
	lserver(void);
	int Init(void*);
	void Send(void);
	void Recv(void);
};

class lclient{
	fd_set fdset;
	timeval wsocktimeout;
	char inbuffer[256], outbuffer[256];
	int *intinbuffer;
	u16 *u16inbuffer;
	int *intoutbuffer;
	u16 *u16outbuffer;
	int numbytes;
public:
	bool oncesend;
	SOCKADDR_IN serverinfo;
	SOCKET noblock;
	int numtransfers;
	lclient(void);
	int Init(LPHOSTENT, void*);
	void Send(void);
	void Recv(void);
	void CheckConn(void);
};

typedef struct {
	SOCKET tcpsocket;
	//SOCKET udpsocket;
	int numgbas;
	HANDLE thread;
	u8 type;
	u8 server;
	bool terminate;
	bool connected;
	bool speed;
	bool active;
} LANLINKDATA;
#endif

extern bool gba_joybus_enabled;
extern sf::IPAddress joybusHostAddr;
extern void JoyBusConnect();
extern void JoyBusShutdown();
extern void JoyBusUpdate(int ticks);

extern bool gba_link_enabled;

#ifdef _MSC_VER
extern void StartLink(u16);
extern void StartGPLink(u16);
extern void LinkSSend(u16);
extern void LinkUpdate(int);
extern void LinkChildStop();
extern void LinkChildSend(u16);
extern void CloseLanLink();
extern char *MakeInstanceFilename(const char *Input);
extern LANLINKDATA lanlink;
extern int vbaid;
extern bool rfu_enabled;
extern int linktimeout;
extern lclient lc;
extern int linkid;
#else // These are stubbed for now
inline void StartLink(u16){}
inline void StartGPLink(u16){}
inline void LinkSSend(u16){}
inline void LinkUpdate(int){}
#endif
