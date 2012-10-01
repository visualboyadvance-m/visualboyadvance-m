// This file was written by denopqrihg

// Joybus
bool gba_joybus_enabled = false;

// If disabled, gba core won't call any (non-joybus) link functions
bool gba_link_enabled = false;

#ifndef NO_LINK
#ifdef _WIN32
#include "../win32/stdafx.h"
#include "../win32/VBA.h"
#include "../win32/MainWnd.h"
#include "../win32/LinkOptions.h"
#include "../win32/Reg.h"
#endif

#include <stdio.h>
#include "../common/Port.h"
#include "GBA.h"
#include "GBALink.h"
#include "GBASockClient.h"
#include "../gb/gbGlobals.h"

#define UPDATE_REG(address, value) WRITE16LE(((u16 *)&ioMem[address]),value)

int linktime = 0;
int tmpctr = 0;

GBASockClient* dol = NULL;
sf::IPAddress joybusHostAddr = sf::IPAddress::LocalHost;

#ifdef _MSC_VER
// Hodgepodge
u8 tspeed = 3;
u8 transfer = 0;
LINKDATA *linkmem = NULL;
LINKDATA rfudata;
int linkid = 0, vbaid = 0;
HANDLE linksync[5];
int savedlinktime = 0;
HANDLE mmf = NULL;
char linkevent[] = "VBA link event  ";
static int i, j;
int linktimeout = 1000; //
LANLINKDATA lanlink;
u16 linkdata[4];
bool linkdatarecvd[4];
u8 gbSIO_SC = 0;
lserver ls;
lclient lc;
bool oncewait = false, after = false;
bool LinkIsWaiting = false;
bool LinkFirstTime = true;
bool EmuReseted = true;
int EmuCtr = 0;
WinHelper::CCriticalSection c_s; //AdamN: critical section object to lock shared resource on multithread as CWnd is not thread-safe
int LinkCommand = 0;
int LinkParam1 = 0;
int LinkParam2 = 0;
int LinkParam4 = 0;
int LinkParam8 = 0;
extern bool LinkHandlerActive = false;
CPtrList LinkCmdList;
CString LogStr = _T("");

// RFU crap (except for numtransfers note...should probably check that out)
bool rfu_enabled = false;
bool rfu_initialized = false;
u8 rfu_cmd, rfu_qsend, rfu_qrecv, rfu_lastcmd;
u16 rfu_id, rfu_thisid;
int rfu_state, rfu_polarity, linktime2, rfu_counter, rfu_masterq;//, rfu_cacheq;
// numtransfers seems to be used interchangeably with linkmem->numtransfers
// probably a bug?
int rfu_transfer_end, numtransfers = 0;
u32 rfu_masterdata[5*35];//, rfu_cachedata[35]; //for 0x24-0x26, temp buffer before data actually sent to other gbas or cache after read

// ???
int trtimedata[4][4] = {
	{34080, 8520, 5680, 2840},
	{65536, 16384, 10923, 5461},
	{99609, 24903, 16602, 8301},
	{133692, 33423, 22282, 11141}
};

int trtimeend[3][4] = {
	{72527, 18132, 12088, 6044},
	{106608, 26652, 17768, 8884},
	{133692, 33423, 22282, 11141}
};

int gbtime = 1024;

int GetSIOMode(u16, u16);

DWORD WINAPI LinkClientThread(void *);
DWORD WINAPI LinkServerThread(void *);
DWORD WINAPI LinkHandlerThread(void *);

int StartServer(void);

u16 StartRFU(u16);
u16 StartRFU2(u16 value);
u16 StartRFU3(u16 value);
BOOL LinkSendData(const char *buf, int size, int nretry = 0);
BOOL LinkIsDataReady(void);
BOOL LinkWaitForData(int ms);

void LogStrPush(const char *str) {
	c_s.Lock();
	LogStr.AppendFormat(_T("%s(%08x)>"),str,GetTickCount());
	c_s.Unlock();
	return;
}

void LogStrPop(int len) {
	c_s.Lock();
	LogStr.Delete(LogStr.GetLength()-(len+11),(len+11));
	c_s.Unlock();
	return;
}

void LinkCmdQueue(u16 Cmd, u16 Prm) {
	LINKCMDPRM cmdprm;
	ULONG tmp, tmp2;
	//POSITION pos;
	cmdprm.Command = Cmd;
	cmdprm.Param = Prm;
	tmp = Prm;
	tmp = tmp << 16;
	tmp |= Cmd;
	tmp2 = 0;
	c_s.Lock();
	if((lanlink.connected /*|| !lanlink.active*/) && LinkCmdList.GetCount()<255) { //as Client GetCount usually no more than 4, as Server it can be alot more than 256 (flooded with LinkUpdate)
		if(LinkCmdList.GetCount()>0)
		tmp2 = (ULONG)*&LinkCmdList.GetTail(); //AdamN: check last command that hasn't been processed yet
		if((Cmd==8) && ((tmp2 & 0xffff)==Cmd)) { //AdamN: LinkUpdate could flood the command queue
			LinkCmdList.SetAt(LinkCmdList.GetTailPosition(),(void*)tmp); //AdamN: replace the value, doesn't need to delete the old value isn't? since it's not really a pointer
			//log("Add: %04X %04X\n",cmdprm.Command,cmdprm.Param);
		} else LinkCmdList.AddTail((void*)tmp);
		//if(LinkCmdList.GetCount()>1) log("Count: %d\n",LinkCmdList.GetCount());
	}
	c_s.Unlock();
	return;
}

char *MakeInstanceFilename(const char *Input)
{
	if (vbaid == 0)
		return (char *)Input;

	static char *result=NULL;
	if (result!=NULL)
		free(result);

	result = (char *)malloc(strlen(Input)+3);
	char *p = strrchr((char *)Input, '.');
	sprintf(result, "%.*s-%d.%s", (int)(p-Input), Input, vbaid+1, p+1);
	return result;
}

u8 gbStartLink(u8 b)
{
  u8 dat = 0xff; //master (w/ internal clock) will gets 0xff if slave is turned off (or not ready yet also?)
  //if(linkid) return 0xff; //b; //Slave shouldn't be sending from here
  BOOL sent = false;
  if(gba_link_enabled && lanlink.connected) {
	LogStrPush("gbStartLink");
	//Send Data (Master/Slave)
	if(linkid) { //Client
		lc.outbuffer[0] = b;
		sent = lc.SendData(1, 1);
	} else //Server
	{
		ls.outbuffer[0] = b;
		sent = ls.SendData(1, 1);
	}
	//if(linkid) return b;
	//Receive Data (Master)
	if(sent)
	//if(gbMemory[0xff02] & 1)
	if(linkid) { //Client
		if(lc.WaitForData(linktimeout)) { //-1 might not be getting infinity as expected :(
			if(lc.RecvData(1))
			dat = lc.inbuffer[0];
			//dat = b;
		}
	} else //Server
	{
		if(ls.WaitForData(linktimeout)) { //-1 might not be getting infinity as expected :(
			if(ls.RecvData(1, 1))
			dat = ls.inbuffer[0];
			//dat = b;
		}
	}
	#ifdef GBA_LOGGING
		if(systemVerbose & VERBOSE_SIO) {
			log("sSIO : %02X  %02X->%02X  %d\n", gbSIO_SC, b, dat, GetTickCount() ); //register_LY
		}
	#endif
	LinkFirstTime = true;
	if(dat==0xff/*||b==0x00||dat==0x00*/) { //dat==0xff
		//LinkFirstTime = true;
		//if(dat==0xff)
		if(linkid) lc.DiscardData();
		else ls.DiscardData(1);
	} else //
	//if(!(gbMemory[0xff02] & 2)) //((gbMemory[0xff02] & 3) == 1) 
		LinkFirstTime = false; //it seems internal clocks can send 1(w/ speed=0) following data w/ external clock, does the speed declare how many followers that can be send?
    //if( /*(gbMemory[0xff02] & 2) ||*/ !(gbMemory[0xff02] & 1) ) LinkFirstTime = true;
	LinkIsWaiting = false;
	LogStrPop(11);
  }
  return dat;
}

u16 gbLinkUpdate(u8 b)
{
  u8 dat = b; //0xff; //slave (w/ external clocks) won't be getting 0xff if master turned off
  BOOL recvd = false;
  int gbSerialOn = (gbMemory[0xff02] & 0x80);
  if(gbSerialOn)
  if(gba_link_enabled && lanlink.connected) {
	//if(!gbSerialOn && !linkid) return (b<<8);
	LogStrPush("gbLinkUpd");
	//Receive Data (Slave)
	//if(!(gbMemory[0xff02] & 1))
	if(linkid) { //Client
		if((/*LinkFirstTime &&*/ lc.IsDataReady()) || ( !lanlink.speed && !LinkFirstTime && lc.WaitForData(linktimeout)) ) {
			recvd = lc.RecvData(1);
			if(recvd /*&& gbSerialOn*/) { //don't update if not ready?
				dat = lc.inbuffer[0];
				//LinkFirstTime = false;
				//LinkFirstTime = true;
			} else LinkIsWaiting = true;
		} else LinkIsWaiting = true;
	} else //Server
	{
		if((/*LinkFirstTime &&*/ ls.IsDataReady()) || ( !lanlink.speed && !LinkFirstTime && ls.WaitForData(linktimeout)) ) {
			recvd = ls.RecvData(1, 1);
			if(recvd /*&& gbSerialOn*/) { //don't update if not ready?
				dat = ls.inbuffer[0];
				//LinkFirstTime = false;
				//LinkFirstTime = true;
			} else LinkIsWaiting = true;
		} else LinkIsWaiting = true;
	}
	/*if(!linkid) //Master shouldn't be initiate a transfer from here
	{
		LogStrPop(9);
		return b; //returning b seems to make it as Player1
	}*/
	//Send Data (Master/Slave), slave should replies 0xff if it's not ready yet?
	//if(!LinkFirstTime) LinkFirstTime = true; else
	if(recvd)
	if(linkid) { //Client
		if(gbSerialOn)
		lc.outbuffer[0] = b; else lc.outbuffer[0] = (u8)0xff;
		if(gbSerialOn) //don't reply if not ready?
		lc.SendData(1, 1);
		LinkIsWaiting = false;
		//LinkFirstTime = false;
	} else //Server
	{
		if(gbSerialOn)
		ls.outbuffer[0] = b; else ls.outbuffer[0] = (u8)0xff;
		if(gbSerialOn) //don't reply if not ready?
		ls.SendData(1, 1);
		LinkIsWaiting = false;
		//LinkFirstTime = false;
	}
	#ifdef GBA_LOGGING
		if(recvd && gbSerialOn)
		if(systemVerbose & VERBOSE_SIO) {
			log("cSIO : %02X  %02X->%02X  %d\n", gbSIO_SC, b, dat, GetTickCount() ); //register_LY
		}
	#endif
	if(dat==0xff||dat==0x00||b==0x00) { //dat==0xff||dat==0x00
	  LinkFirstTime = true;
	  if(dat==0xff)
	  if(linkid) lc.DiscardData();
	  else ls.DiscardData(1);
	}
	LogStrPop(9);
  }
  return ((dat << 8) | (recvd & (u8)0xff));
}

void StartLink2(u16 value) //Called when COMM_SIOCNT written
{
	char inbuffer[256], outbuffer[256];
	u16 *u16inbuffer = (u16*)inbuffer;
	u16 *u16outbuffer = (u16*)outbuffer;
	BOOL disable = true;
	BOOL sent = false;
	unsigned long notblock = 1; //AdamN: 0=blocking, non-zero=non-blocking
	//unsigned long arg = 0;
	//fd_set fdset;
	//timeval wsocktimeout;
	
	if (ioMem == NULL)
		return;

	/*if (rfu_enabled) {
		UPDATE_REG(COMM_SIOCNT, StartRFU(value));
		return;
	}*/
	u16 rcnt = READ16LE(&ioMem[COMM_RCNT]);
	u16 siocnt = READ16LE(&ioMem[COMM_SIOCNT]);
	int commmode = GetSIOMode(value, rcnt);
	if(!linkid && (((siocnt&3) != (value&3)) || (GetSIOMode(siocnt, rcnt) != commmode))) linkdatarecvd[/*0*/linkid]=false; //AdamN: reset if clock/mode changed from the last time
	switch (commmode) {
	case MULTIPLAYER: 
		if (value & 0x08) { //AdamN: SD Bit.3=1 (All GBAs Ready)
			if(lanlink.active && lanlink.connected)
			if (/*!(value & 0x04)*/ !linkid) { //Parent/Master, AdamN: SI Bit.2=0 (0=Parent/Master/Server,1=Child/Slave/Client)
				if (value & 0x80) //AdamN: Start/Busy Bit.7=1 (Active/Transfering)
				if(!linkdatarecvd[0]) { //AdamN: cycle not started yet
					UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) | 0x0080); //AdamN: Activate bit.7 (Start/Busy) since value hasn't been updated yet at this point
					UPDATE_REG(COMM_RCNT, READ16LE(&ioMem[COMM_RCNT]) & 0xfffe); //AdamN: LOWering bit.0 (SC)
					linkdata[0] = READ16LE(&ioMem[COMM_SIOMLT_SEND]);
					linkdata[1] = 0xffff;
					WRITE32LE(&linkdata[2], 0xffffffff);
					WRITE32LE(&ioMem[COMM_SIOMULTI0], 0xffffffff); //AdamN: data from SIOMULTI0 & SIOMULTI1
					WRITE32LE(&ioMem[COMM_SIOMULTI2], 0xffffffff); //AdamN: data from SIOMULTI2 & SIOMULTI3					
					outbuffer[0] = 3; //AdamN: size of packet
					outbuffer[1] = linkid; //AdamN: Sender ID (0)
					u16outbuffer[1] = linkdata[0]; //AdamN: u16outbuffer[1] points to outbuffer[2]
					for(int i=1; i<=lanlink.numgbas; i++) {
						notblock = 0; //1;
						ioctlsocket(ls.tcpsocket[i], FIONBIO, &notblock); //AdamN: temporarily use non-blocking for sending multiple data at once
						int ern=WSAGetLastError();
						if(ern!=0) log("IOCTL1 Error: %d\n",ern);
						#ifdef GBA_LOGGING
							if(systemVerbose & VERBOSE_LINK) {
								log("%sSSend to : %d  Size : %d  %s\n", (LPCTSTR)LogStr, i, 4, (LPCTSTR)DataHex(outbuffer,4));
							}
						#endif
						/*outbuffer[0] = 0;
						for(int c=0;c<8;c++) //AdamN: sending 8 bytes dummies
							send(ls.tcpsocket[i], outbuffer, 1, 0); //send a dummies, it seems the first packet won't arrive on the other side for some reason, like need to fill the internal buffer before it actually get sent
						outbuffer[0] = 3;*/
						setsockopt(ls.tcpsocket[i], IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
						sent|=(send(ls.tcpsocket[i], outbuffer, 4, 0)>=0);
						ern=WSAGetLastError();
						if(ern!=0) log("Send%d Error: %d\n",i,ern);
					}
					/*notblock = 0;
					ioctlsocket(ls.tcpsocket[i], FIONBIO, &notblock); //AdamN: back to blocking, might not be needed
					ern=WSAGetLastError();
					if(ern!=0) log("IOCTL2 Error: %d\n",ern);*/
					if(sent) {
						UPDATE_REG(COMM_SIOMULTI0, linkdata[0]); //AdamN: SIOMULTI0 (16bit data received from master/server)
						UPDATE_REG(COMM_RCNT, READ16LE(&ioMem[COMM_RCNT]) & 0xfff7); //AdamN: LOWering SO bit.3 (RCNT or SIOCNT?)
						for(int i=1; i<=lanlink.numgbas; i++) linkdatarecvd[i] = false; //AdamN: new cycle begins
						linkdatarecvd[0] = true;
						//value &= 0xff7f; //AdamN: Deactivate bit.7 (Start/Busy), Busy bit should be Deactivated when all SIOMULTI has been filled with data from active GBAs
						//value |= (sent != 0) << 7; 
					}
				}
			} else { //Slave/Client?
				//todo: ignoring ReadOnly bits on Slaves, is this needed?
				if (value & 0x80) log("Slave wants to Send");
			}
			/*value &= 0xff8b; //AdamN: bit.2(SI),4-5(ID),6(ERR) masked out
			//if(sent) 
				value |= (linkid ? 0xc : 8); //AdamN: master=0x08 bit.3(SD), slave=0x0c bit.2-3(SI,SD)
			value |= linkid << 4; //AdamN: setting bit.4-5 (ID) after a successful transfer*/
		}
		value &= 0xff8b; //AdamN: bit.2(SI),4-5(ID),6(ERR) masked out
		value |= 8; //AdamN: need to be set as soon as possible for Mario Kart
		/*if(sent)*/ {
			value |= (linkid ? 0xc : 8); //AdamN: master=0x08 bit.3(SD), slave=0x0c bit.2-3(SI,SD)
			value |= linkid << 4; //AdamN: setting bit.4-5 (ID) after a successful transfer, need to be set as soon as possible otherwise Client may think it's a Server(when getting in-game timeout) and tried to initiate a transfer on next retry
		}
		UPDATE_REG(COMM_SIOCNT, value);
		if(linkid && (value & 0x3f)==0x0f) lc.WaitForData(-1); else
		if(sent) {
			ls.WaitForData(-1);
			//MSG msg;
			//int ready = 0;
			//do { //Waiting for incomming data before continuing CPU execution
			//	SleepEx(0,true); //SleepEx(0,true); //to give time for incoming data
			//	if(PeekMessage(&msg, 0/*theApp.GetMainWnd()->m_hWnd*/,  0, 0, PM_NOREMOVE))
			/*		theApp.PumpMessage(); //seems to be processing message only if it has message otherwise it halt the program
				
				//fdset.fd_count = lanlink.numgbas;
				notblock = 0;
				for(int i=1; i<=lanlink.numgbas; i++) {
					//fdset.fd_array[i-1] = ls.tcpsocket[i];
					ioctlsocket(ls.tcpsocket[i], FIONBIO, &notblock); //AdamN: temporarily use blocking for reading
					int ern=WSAGetLastError();
					if(ern!=0) {
						log("slIOCTL Error: %d\n",ern);
						//if(ern==10054 || ern==10053 || ern==10057 || ern==10051 || ern==10050 || ern==10065) lanlink.connected = false;
					}
					arg = 1; //0;
					if(ioctlsocket(ls.tcpsocket[i], FIONREAD, &arg)!=0) { //AdamN: Alternative(Faster) to Select(Slower)
						int ern=WSAGetLastError(); 
						if(ern!=0) {
							log("%sSC Error: %d\n",(LPCTSTR)LogStr,ern);
							char message[40];
							lanlink.connected = false;
							sprintf(message, _T("Player %d disconnected."), i+1);
							MessageBox(NULL, message, _T("Link"), MB_OK);
							break;
						}
					}
					if(arg>0) ready++;
				}
				//wsocktimeout.tv_sec = linktimeout / 1000;
				//wsocktimeout.tv_usec = linktimeout % 1000; //0; //AdamN: remainder should be set also isn't?
				//ready = select(0, &fdset, NULL, NULL, &wsocktimeout);
				//int ern=WSAGetLastError();
				//if(ern!=0) {
				//	log("slCC Error: %d\n",ern);
				//	if(ern==10054 || ern==10053 || ern==10057 || ern==10051 || ern==10050 || ern==10065) lanlink.connected = false;
				//}
			} while (lanlink.connected && ready==0);*/
		}
		
		//AdamN: doesn't seems to be needed here
		if (linkid) //Slave
			UPDATE_REG(COMM_RCNT, 7); //AdamN: Bit.0-2 (SC,SD,SI) as for GP
		else //Master
			UPDATE_REG(COMM_RCNT, 3); //AdamN: Bit.0-1 (SC,SD) as for GP //not needed
		if(sent || (linkid && (value & 0x3f)==0x0f))
			LinkUpdate2(0,0);
		break;
	case NORMAL8:
	case NORMAL32: //AdamN: Wireless mode also use NORMAL32 mode for transreceiving the data
	case UART:
	default:
		UPDATE_REG(COMM_SIOCNT, value);
		break;
	}
}

void StartLink(u16 value) //Called when COMM_SIOCNT written
{
	if (ioMem == NULL)
		return;

	if (rfu_enabled) {
		//if(lanlink.connected)
			UPDATE_REG(COMM_SIOCNT, StartRFU3(value)); //RF use NORMAL32 mode to communicate
		//else UPDATE_REG(COMM_SIOCNT, StartRFU(value)); //RF use NORMAL32 mode to communicate

		return;
	}

	BOOL sent = false;
	//u16 prvSIOCNT = READ16LE(&ioMem[COMM_SIOCNT]);

	switch (GetSIOMode(value, READ16LE(&ioMem[COMM_RCNT]))) {
	case MULTIPLAYER: 
		if (value & 0x80) { //AdamN: Start Bit=Start/Active (transfer initiated)
			if (!linkid) { //master/server?
				if (!transfer) { //not transfered yet?
					if (lanlink.active) //On Networks
					{
						if (lanlink.connected)
						{
							linkdata[0] = READ16LE(&ioMem[COMM_SIODATA8]); //AdamN: SIOMLT_SEND(16bit data to be sent) on MultiPlayer mode
							savedlinktime = linktime;
							tspeed = value & 3;
							LogStrPush("StartLink1");
							sent = ls.Send(); //AdamN: Server need to send this often
							LogStrPop(10);
							if(sent) {
							transfer = 1;
							linktime = 0;
							UPDATE_REG(COMM_SIODATA32_L, linkdata[0]); //AdamN: SIOMULTI0 (16bit data received from master/server)
							UPDATE_REG(COMM_SIODATA32_H, 0xffff); //AdamN: SIOMULTI1 (16bit data received from slave1, reset to FFFFh upon transfer start)
							WRITE32LE(&ioMem[0x124], 0xffffffff); //AdamN: data from SIOMULTI2 & SIOMULTI3
							if (lanlink.speed && oncewait == false) //lanlink.speed = speedhack
								ls.howmanytimes++;
							after = false;
							}
						}
					}
					else if (linkmem->numgbas > 1) //On Single Computer
					{
						ResetEvent(linksync[0]);
						linkmem->linkcmd[0] = ('M' << 8) + (value & 3);
						linkmem->linkdata[0] = READ16LE(&ioMem[COMM_SIODATA8]);

						if (linkmem->numtransfers != 0)
							linkmem->lastlinktime = linktime;
						else
							linkmem->lastlinktime = 0;

						if ((++linkmem->numtransfers) == 0)
							linkmem->numtransfers = 2;
						transfer = 1;
						linktime = 0;
						tspeed = value & 3;
						WRITE32LE(&ioMem[COMM_SIODATA32_L], 0xffffffff);
						WRITE32LE(&ioMem[0x124], 0xffffffff); //COMM_SIOMULTI2
						sent = true;
					}
				}
			}
			if(sent) {
			value &= 0xff7f;
			value |= (transfer != 0) << 7;
			}
		}
		/*if(sent)*/ { //AdamN: no need to check for sent here
		value &= 0xff8b;
		value |= (linkid ? 0xc : 8);
		value |= linkid << 4;
		
		//AdamN: doesn't seems to be needed
		if (linkid)
			UPDATE_REG(COMM_RCNT, 7);
		else
			UPDATE_REG(COMM_RCNT, 3);
		}
		UPDATE_REG(COMM_SIOCNT, value);
		if(sent || (linkid && (value & 0x3f)==0x0f))
			LinkUpdate(0);
		break;

	case NORMAL8:
	case NORMAL32:
	case UART:
	default:
		UPDATE_REG(COMM_SIOCNT, value);
		break;
	}
}

void StartGPLink(u16 value) //Called when COMM_RCNT written
{
	UPDATE_REG(COMM_RCNT, value);

	if (!value)
		return; //if value=0(bit.15=1 & bit.14=0 for GP) then it's not possible for GP mode

	switch (GetSIOMode(READ16LE(&ioMem[COMM_SIOCNT]), value)) { //bit.15=0 & bit.14=any for MP/Normal/UART
	case MULTIPLAYER: 
		value &= 0xc0f0;
		value |= 3;
		if (linkid)
			value |= 4;
		UPDATE_REG(COMM_SIOCNT, ((READ16LE(&ioMem[COMM_SIOCNT])&0xff8b)|(linkid ? 0xc : 8)|(linkid<<4)));
		break;
	
	case GP: //Only bit.0-3&14-15 of RCNT used for GP, JoyBus use bit.15=1 & bit.14=1
	//default: //Normal mode, may cause problem w/ RF if default also reset the RF
		if (rfu_enabled) {
			rfu_state = RFU_INIT; //reset wireless
			rfu_polarity = 0;
			#ifdef GBA_LOGGING
				if(systemVerbose & (VERBOSE_SIO | VERBOSE_LINK)) {
					log("RFU Reset : %04X  %04X  %d\n", READ16LE(&ioMem[COMM_RCNT]), READ16LE(&ioMem[COMM_SIOCNT]), GetTickCount() );
				}
			#endif
		}
		break;
	}
}

#endif // _MSC_VER

void JoyBusConnect()
{
	delete dol;
	dol = NULL;

	dol = new GBASockClient(joybusHostAddr);
}

void JoyBusShutdown()
{
	delete dol;
	dol = NULL;
}

void JoyBusUpdate(int ticks)
{
	linktime += ticks;
	static int lastjoybusupdate = 0;

	// Kinda ugly hack to update joybus stuff intermittently
	if (linktime > lastjoybusupdate + 0x3000)
	{
		lastjoybusupdate = linktime;

		char data[5] = {0x10, 0, 0, 0, 0}; // init with invalid cmd
		std::vector<char> resp;

		if (!dol)
			JoyBusConnect();

		u8 cmd = dol->ReceiveCmd(data);
		switch (cmd) {
		case JOY_CMD_RESET:
			UPDATE_REG(COMM_JOYCNT, READ16LE(&ioMem[COMM_JOYCNT]) | JOYCNT_RESET);

		case JOY_CMD_STATUS:
			resp.push_back(0x00); // GBA device ID
			resp.push_back(0x04);
			break;
		
		case JOY_CMD_READ:
			resp.push_back((u8)(READ16LE(&ioMem[COMM_JOY_TRANS_L]) & 0xff));
			resp.push_back((u8)(READ16LE(&ioMem[COMM_JOY_TRANS_L]) >> 8));
			resp.push_back((u8)(READ16LE(&ioMem[COMM_JOY_TRANS_H]) & 0xff));
			resp.push_back((u8)(READ16LE(&ioMem[COMM_JOY_TRANS_H]) >> 8));
			UPDATE_REG(COMM_JOYSTAT, READ16LE(&ioMem[COMM_JOYSTAT]) & ~JOYSTAT_SEND);
			UPDATE_REG(COMM_JOYCNT, READ16LE(&ioMem[COMM_JOYCNT]) | JOYCNT_SEND_COMPLETE);
			break;

		case JOY_CMD_WRITE:
			UPDATE_REG(COMM_JOY_RECV_L, (u16)((u16)data[2] << 8) | (u8)data[1]);
			UPDATE_REG(COMM_JOY_RECV_H, (u16)((u16)data[4] << 8) | (u8)data[3]);
			UPDATE_REG(COMM_JOYSTAT, READ16LE(&ioMem[COMM_JOYSTAT]) | JOYSTAT_RECV);
			UPDATE_REG(COMM_JOYCNT, READ16LE(&ioMem[COMM_JOYCNT]) | JOYCNT_RECV_COMPLETE);
			break;

		default:
			return; // ignore
		}

		resp.push_back((u8)READ16LE(&ioMem[COMM_JOYSTAT]));
		dol->Send(resp);

		// Generate SIO interrupt if we can
		if ( ((cmd == JOY_CMD_RESET) || (cmd == JOY_CMD_READ) || (cmd == JOY_CMD_WRITE))
			&& (READ16LE(&ioMem[COMM_JOYCNT]) & JOYCNT_INT_ENABLE) )
		{
			IF |= 0x80;
			UPDATE_REG(0x202, IF);
		}
	}
}

#ifdef _MSC_VER

void LinkUpdate2(int ticks, int FrameCnt) //It seems Frameskipping on Client side causes instability, Client may need to execute the CPU slower than Server to maintain stability for some games (such as Mario Kart)
{
	char inbuffer[256], outbuffer[256];
	u16 *u16inbuffer = (u16*)inbuffer;
	u16 *u16outbuffer = (u16*)outbuffer;
	unsigned long notblock = 0; //AdamN: 0=blocking, non-zero=non-blocking
	BOOL disable = true;
	unsigned long arg = 0;
	BOOL recvd = false;
	BOOL sent = false;
	fd_set fdset;
	timeval wsocktimeout;
	
	if (ioMem == NULL)
		return;

	linktime += ticks;

	int missed = 0;
	int stacked = 0;
	static int misctr = 0;

	//if(ticks!=0 && linktime<1008) return;

	/*if (rfu_enabled)
	{
		linktime2 += ticks; // linktime2 is unused!
		rfu_transfer_end -= ticks;
		if (transfer && rfu_transfer_end <= 0) 
		{
			transfer = 0;
			if (READ16LE(&ioMem[COMM_SIOCNT]) & 0x4000) //IRQ Enable
			{
				IF |= 0x80; //Serial Communication
				UPDATE_REG(0x202, IF); //Interrupt Request Flags / IRQ Acknowledge
			}
			UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) & 0xff7f);
		}
		return;
	}*/

	if (lanlink.active) //On Networks
	{
		if (lanlink.connected)
		{
			int numbytes = 0;
			u16 siocnt = READ16LE(&ioMem[COMM_SIOCNT]);
			u16 rcnt = READ16LE(&ioMem[COMM_RCNT]);
			if(GetSIOMode(siocnt, rcnt)!=MULTIPLAYER) {/*UPDATE_REG(0x06, VCOUNT);*/ return;} //AdamN: skip if it's not MP mode, default mode at startup is Normal8bit
			if (linkid) { //Slave/Client?
				UPDATE_REG(COMM_SIOCNT, siocnt | 0x000c); //AdamN: set Bit.2(SI) and Bit.3(SD) to mark it as Ready
				//if(linktime<159/*1008*/) return; //AdamN: checking scanlines? giving delays to prevent too much slowdown, could cause instability?
				//linktime=0;
				//if(VCOUNT>160) UPDATE_REG(0x06, 160); //AdamN: forcing VCOUNT value to trick games that expect data exchange when VCOUNT=160
				//UPDATE_REG(COMM_SIOCNT, siocnt | 0x000c);
				//if(GetSIOMode(siocnt, rcnt)!=MULTIPLAYER) return; //AdamN: skip if it's not MP mode
				/*u16outbuffer[0] = 0;
				u16outbuffer[1] = 0;
				notblock = 1;
				ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //AdamN: temporarily use non-blocking for sending
				setsockopt(lanlink.tcpsocket, IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
				sent=(send(lanlink.tcpsocket, outbuffer, 4, 0)>=0);*/ //AdamN: sending dummies packet to prevent timeout when server tried to read socket w/ an empty buffer
				notblock = 0;
				ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //AdamN: temporarily use blocking for reading
				int ern=WSAGetLastError();
				if(ern!=0) log("IOCTL1 Error: %d\n",ern);
				if(linkdatarecvd[0] /*|| VCOUNT>=159 || !LinkFirstTime*/) {
					bool last = true;
					for(int i=1;i<=lanlink.numgbas;i++) last&=linkdatarecvd[i];
					if(!last) {
						/*fdset.fd_count = 1;
						fdset.fd_array[0] = lanlink.tcpsocket;
						wsocktimeout.tv_sec = linktimeout / 1000;
						wsocktimeout.tv_usec = linktimeout % 1000; //0; //AdamN: remainder should be set also isn't?
						if(select(0, &fdset, NULL, NULL, &wsocktimeout)<=0) { //AdamN: this will blocks executions but allows Server to be paused w/o causing disconnection on Client
							int ern=WSAGetLastError();
							if(ern!=0) {
								lanlink.connected = false;
								log("%sCR[%d]\n",(LPCTSTR)LogStr,ern);
							}
							return;
						}*/
						if(!lc.WaitForData(-1)) return;
					}
				}
				do {
				arg = lc.IsDataReady(); //0;
				/*if(ioctlsocket(lanlink.tcpsocket, FIONREAD, &arg)!=0) { //AdamN: Alternative(Faster) to Select(Slower)
					int ern=WSAGetLastError(); 
					if(ern!=0) {
						log("%sCC Error: %d\n",(LPCTSTR)LogStr,ern);
						lanlink.connected = false;
						MessageBox(NULL, _T("Player 1 disconnected."), _T("Link"), MB_OK);
					}
				}*/
				if(arg>0) {
					stacked++;
					//UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) | 0x000c); //AdamN: set Bit.2(SI) and Bit.3(SD) to mark it as Ready
					numbytes = 0;
					int sz=recv(lanlink.tcpsocket, inbuffer, 1, 0); //AdamN: read the size of packet
					int ern=WSAGetLastError();
					if(ern!=0) {
						if(ern==10054 || ern==10053 || ern==10057 || ern==10051 || ern==10050 || ern==10065) lanlink.connected = false;
						#ifdef GBA_LOGGING
							if(ern!=10060)
							if(systemVerbose & VERBOSE_LINK) {
								log("%sCRecv1 Error: %d\n",(LPCTSTR)LogStr,ern); //oftenly gets access violation if App closed down, could be due to LogStr=""
							}
						#endif
					}
					if(sz > 0) {
						sz = inbuffer[0];
						/*if(sz==0) 
							tmpctr++;*/
						UPDATE_REG(COMM_RCNT, READ16LE(&ioMem[COMM_RCNT]) & 0xfffe); //AdamN: LOWering bit.0 (SC)
						UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) | 0x0080); //AdamN: Activate bit.7 (Start/Busy)
					}
					numbytes++;
					while(numbytes<=sz) {
						int cnt=recv(lanlink.tcpsocket, inbuffer+numbytes, (sz-numbytes)+1, 0); 
						int ern=WSAGetLastError(); //AdamN: it seems WSAGetLastError can still gets 0 even when server no longer existed
						if(ern!=0 || cnt<=0) {
							log("%sCRecv2 Error: %d\n",(LPCTSTR)LogStr,ern);
							if(ern==10054 || ern==10053 || ern==10057 || ern==10051 || ern==10050 || ern==10065 || cnt<=0) lanlink.connected = false;
							break;
						}
						numbytes+=cnt;
					}
					recvd=(numbytes>1);
					//done: check sender id and update linkdata+register
					if(recvd) {
						#ifdef GBA_LOGGING
							if(systemVerbose & VERBOSE_LINK) {
								log("%sCRecv2 Size : %d  %s\n", (LPCTSTR)LogStr, numbytes, (LPCTSTR)DataHex(inbuffer,numbytes));
							}
						#endif
						int sid = inbuffer[1]; //AdamN: sender ID
						if(!sid) { //AdamN: if sender is parent then it's new cycle
							for(int i=1; i<=lanlink.numgbas; i++) linkdatarecvd[i] = false;
							WRITE32LE(&linkdata[0], 0xffffffff);
							WRITE32LE(&linkdata[2], 0xffffffff);
							WRITE32LE(&ioMem[COMM_SIOMULTI0], 0xffffffff); //AdamN: data from SIOMULTI0 & SIOMULTI1
							WRITE32LE(&ioMem[COMM_SIOMULTI2], 0xffffffff); //AdamN: data from SIOMULTI2 & SIOMULTI3
						}
						linkdata[sid] = u16inbuffer[1]; //AdamN: received data, u16inbuffer[1] points to inbuffer[2]
						linkdatarecvd[sid] = true;
						LinkFirstTime = false;
						//UPDATE_REG(0x06, VCOUNT); //AdamN: restore to the real VCOUNT, may be it shouldn't be restored until no longer in MP mode?
						UPDATE_REG((sid << 1)+COMM_SIOMULTI0, linkdata[sid]); //AdamN: SIOMULTI0 (16bit data received from master/server)
						UPDATE_REG(COMM_SIOCNT, (READ16LE(&ioMem[COMM_SIOCNT]) & 0xffcf) | (linkid << 4)); //AdamN: Set ID bit.4-5
						//UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) & 0xff7f); //AdamN: Deactivate bit.7 (Start/Busy), should be done after a cycle finished
						//value &= 0xff7f; 
						//value |= (sent != 0) << 7;
						//done: if (sid<linkid)and(linkdata[linkid]/COMM_SIOMLT_SEND!=COMM_SIOMULTI[linkid] or COMM_SIOMULTI[linkid]==FFFF) then pass linkdata[linkid] to server to broadcast to the other GBAs
						sent = false;
						if(sid<linkid) { //AdamN: todo:need to check if client's program initiate transfer by through StartLink so this section doesn't needed
							linkdata[linkid] = READ16LE(&ioMem[COMM_SIOMLT_SEND]);
							if(/*linkdata[linkid] != READ16LE(&ioMem[(linkid << 1)+COMM_SIOMULTI0])*/!linkdatarecvd[linkid]) { //AdamN: if it's already send it's data on this cycle then don't send again
								outbuffer[0] = 3; //AdamN: size of packet
								outbuffer[1] = linkid; //AdamN: Sender ID
								u16outbuffer[1] = linkdata[linkid];
								UPDATE_REG(COMM_RCNT, READ16LE(&ioMem[COMM_RCNT]) & 0xfffb); //AdamN: LOWering SI bit.2 (RCNT or SIOCNT?)
								notblock = 0; //1
								ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //AdamN: temporarily use non-blocking for sending
								int ern=WSAGetLastError();
								if(ern!=0) log("IOCTL11 Error: %d\n",ern);
								#ifdef GBA_LOGGING
									if(systemVerbose & VERBOSE_LINK) {
										log("%sCSend-0 Size : %d  %s\n", (LPCTSTR)LogStr, 4, (LPCTSTR)DataHex(outbuffer,4));
									}
								#endif
								setsockopt(lanlink.tcpsocket, IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
								sent=(send(lanlink.tcpsocket, outbuffer, 4, 0)>=0);
								ern=WSAGetLastError();
								if(ern!=0) {
									log("Send-0 Error: %d\n",ern); //AdamN: might be getting err 10035(would block) when used on non-blocking socket
									if(ern==10054 || ern==10053 || ern==10057 || ern==10051 || ern==10050 || ern==10065) lanlink.connected = false;
								}
								if(sent) {
									UPDATE_REG((linkid << 1)+COMM_SIOMULTI0, linkdata[linkid]);
									//UPDATE_REG(COMM_RCNT, READ16LE(&ioMem[COMM_RCNT]) & 0xfff7); //AdamN: LOWering SO bit.3 (RCNT or SIOCNT?)
									linkdatarecvd[linkid] = true;
								}
							}
						}
						bool last = true;
						for(int i=0; i<=lanlink.numgbas; i++) last &= linkdatarecvd[i];
						if(last) { //AdamN: cycle ends
							//UPDATE_REG(COMM_RCNT, READ16LE(&ioMem[COMM_RCNT]) | 0x0001); //AdamN: HIGHering bit.0 (SC), only needed for Master/Server?
							//UPDATE_REG(COMM_RCNT, READ16LE(&ioMem[COMM_RCNT]) | 0x0008); //AdamN: HIGHering SO bit.3 (RCNT or SIOCNT?)
							UPDATE_REG(COMM_RCNT, READ16LE(&ioMem[COMM_RCNT]) | 0x0007/*f*/); //AdamN: HIGHering SC,SD,SI,SO (apparently SD and SI need to be High also for Slave), Slave might be checking SC when not using IRQ
							UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) & 0xff7f); //AdamN: Deactivate bit.7 (Start/Busy)
							//AdamN: Trigger Interrupt if Enabled
							if (READ16LE(&ioMem[COMM_SIOCNT]) & 0x4000) { //IRQ Enable
								IF |= 0x80; //Serial Communication
								UPDATE_REG(0x202, IF); //Interrupt Request Flags / IRQ Acknowledge
							}
							for(int i=0; i<=lanlink.numgbas; i++) linkdatarecvd[i]=false; //AdamN: does it need to reset these?
							#ifdef GBA_LOGGING
								if(systemVerbose & VERBOSE_SIO) {
									//if(((READ16LE(&ioMem[COMM_SIOCNT]) >> 4) & 3) != linkid)
									//if(READ16LE(&ioMem[COMM_SIOCNT]) & 0x4000)
									log("SIO : %04X %04X  %04X %04X %04X %04X  (VCOUNT = %d) %d\n", READ16LE(&ioMem[COMM_RCNT]), READ16LE(&ioMem[COMM_SIOCNT]), READ16LE(&ioMem[COMM_SIOMULTI0]), READ16LE(&ioMem[COMM_SIOMULTI1]), READ16LE(&ioMem[COMM_SIOMULTI2]), READ16LE(&ioMem[COMM_SIOMULTI3]), VCOUNT, GetTickCount() );
								}
							#endif
							break; //to exit while(arg>0 && lanlink.connected); if it's last
						} else if(sent) UPDATE_REG(COMM_RCNT, READ16LE(&ioMem[COMM_RCNT]) & 0xfff7); //AdamN: LOWering SO bit.3 (RCNT or SIOCNT?)
						//if(sent) UPDATE_REG(COMM_RCNT, 7); //AdamN: Bit.0-2 (SC,SD,SI) as for GP
					}
				} else missed++;
				} while(arg>0 && lanlink.connected);
				UPDATE_REG(COMM_SIOCNT, (READ16LE(&ioMem[COMM_SIOCNT]) & 0xffcf) | (linkid << 4)); //AdamN: Set ID bit.4-5, needed for Mario Kart
				//UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) & 0xff77); //AdamN: making it not ready so slave doesn't start counting the in-game timeout counter
				//if(!LinkFirstTime) {
				//lc.WaitForData(1);
				//if(misctr++<400)
				//	log("Frame:%d Tick:%d Missed:%d Stacked:%d\n",FrameCnt,/*GetTickCount()*/ticks,missed,stacked);
				//}
				/*notblock = 1;
				ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //AdamN: back to non-blocking, might not be needed
				int ern=WSAGetLastError();
				if(ern!=0) log("IOCTL2 Error: %d\n",ern);*/

			} else { //Server/Master?
				UPDATE_REG(COMM_SIOCNT, siocnt | 0x0008); //AdamN: set Bit.2(SI) and Bit.3(SD) to mark it as Ready
				//if(linktime<2) return; //AdamN: giving delays to prevent too much slowdown, but could cause instability
				//linktime=0;
				//if(GetSIOMode(siocnt, rcnt)!=MULTIPLAYER) return; //AdamN: skip if it's not MP mode
				/*sent = false;
				u16outbuffer[0] = 0;
				u16outbuffer[1] = 0;
				notblock = 1;
				for(int i=1;i<=lanlink.numgbas;i++) {
					ioctlsocket(ls.tcpsocket[i], FIONBIO, &notblock); //AdamN: temporarily use non-blocking for sending multiple data at once
					setsockopt(ls.tcpsocket[i], IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
					sent|=(send(ls.tcpsocket[i], outbuffer, 1, 0)>=0); //AdamN: sending dummies packet to prevent timeout when client tried to read socket w/ an empty buffer
				}*/
				if(!linkdatarecvd[0]) return; //AdamN: don't read if it's not starting a cycle yet
				bool last = true;
				int ready = 0;
				for(int i=1;i<=lanlink.numgbas;i++) last&=linkdatarecvd[i];
				if(!last) {
				fdset.fd_count = lanlink.numgbas;
				notblock = 0;
				for(int i=1; i<=lanlink.numgbas; i++) {
					fdset.fd_array[i-1] = ls.tcpsocket[i];
					ioctlsocket(ls.tcpsocket[i], FIONBIO, &notblock); //AdamN: temporarily use blocking for reading
					int ern=WSAGetLastError();
					if(ern!=0) log("IOCTL30 Error: %d\n",ern);
				}
				wsocktimeout.tv_sec = linktimeout / 1000;
				wsocktimeout.tv_usec = linktimeout % 1000; //0; //AdamN: remainder should be set also isn't?
				ready = select(0, &fdset, NULL, NULL, &wsocktimeout); //AdamN: this will blocks executions but allows Client to be paused w/o causing disconnection on Server
				if(ready<=0){ //AdamN: may cause noticible delay, result can also be SOCKET_ERROR, Select seems to be needed to maintain stability
					int ern=WSAGetLastError();
					if(ern!=0) {
						lanlink.connected = false;
						log("%sCR[%d]\n",(LPCTSTR)LogStr,ern);
					}
					return;
				}
				}
				if(ready>0)
				do {
				for(int i=1; i<=lanlink.numgbas; i++) {
					notblock = 0;
					ioctlsocket(ls.tcpsocket[i], FIONBIO, &notblock); //AdamN: temporarily use blocking for reading
					int ern=WSAGetLastError();
					if(ern!=0) log("IOCTL3 Error: %d\n",ern);
					/*fdset.fd_count = 1;
					fdset.fd_array[0] = ls.tcpsocket[i];
					wsocktimeout.tv_sec = linktimeout / 1000;
					wsocktimeout.tv_usec = linktimeout % 1000; //0; //AdamN: remainder should be set also isn't?
					if(select(0, &fdset, NULL, NULL, &wsocktimeout)<=0){ //AdamN: may cause noticible delay, result can also be SOCKET_ERROR, Select seems to be needed to maintain stability
						int ern=WSAGetLastError();
						if(ern!=0) {
							lanlink.connected = false;
							log("%sCR%d[%d]\n",(LPCTSTR)LogStr,i,ern);
						}
						return;
					}*/
					arg = 0;
					if(ioctlsocket(ls.tcpsocket[i], FIONREAD, &arg)!=0) {
						int ern=WSAGetLastError(); //AdamN: this seems to get ern=10038(invalid socket handle) often
						if(ern!=0) {
							log("%sSR-%d[%d]\n",(LPCTSTR)LogStr,i,ern);
							char message[40];
							lanlink.connected = false;
							sprintf(message, _T("Player %d disconnected."), i+1);
							MessageBox(NULL, message, _T("Link"), MB_OK);
							break;
						}
						continue; //break;
					}
					if(arg>0) {
						numbytes = 0;
						int sz=recv(ls.tcpsocket[i], inbuffer, 1, 0); //AdamN: read the size of packet
						int ern=WSAGetLastError();
						if(ern!=0) {
							if(ern==10054 || ern==10053 || ern==10057 || ern==10051 || ern==10050 || ern==10065) lanlink.connected = false;
							#ifdef GBA_LOGGING
								if(ern!=10060)
								if(systemVerbose & VERBOSE_LINK) { 
									log("%sSRecv1-%d Error: %d\n",(LPCTSTR)LogStr,i,ern);
								}
							#endif
						}
						if(sz > 0) {
							sz = inbuffer[0];
							UPDATE_REG(COMM_RCNT, READ16LE(&ioMem[COMM_RCNT]) & 0xfffe); //AdamN: LOWering bit.0 (SC), not really needed here
							UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) | 0x0080); //AdamN: Activate bit.7 (Start/Busy), not really needed here
						}
						numbytes++;
						while(numbytes<=sz) {
							int cnt=recv(ls.tcpsocket[i], inbuffer+numbytes, (sz-numbytes)+1, 0);
							int ern=WSAGetLastError();
							if(ern!=0 || cnt<=0) {
								log("%sSRecv2-%d Error: %d\n",(LPCTSTR)LogStr,i,ern);
								if(ern==10054 || ern==10053 || ern==10057 || ern==10051 || ern==10050 || ern==10065 || cnt<=0) lanlink.connected = false;
								break;
							}
							numbytes+=cnt;
						}
						recvd=(numbytes>1);
						//done: check sender id and update linkdata+register
						if(recvd) {
							#ifdef GBA_LOGGING
								if(systemVerbose & VERBOSE_LINK) {
									log("%sSRecv2 Size : %d  %s\n", (LPCTSTR)LogStr, numbytes, (LPCTSTR)DataHex(inbuffer,numbytes));
								}
							#endif
							int sid = inbuffer[1]; //AdamN: sender ID
							linkdata[sid] = u16inbuffer[1]; //AdamN: received data, u16inbuffer[1] points to inbuffer[2]
							//if(!sid) //AdamN: if sender is parent then it's new cycle, not possible here
							//	for(int i=0; i<=lanlink.numgbas; i++) linkdatarecvd[i] = false;
							linkdatarecvd[sid] = true;
							UPDATE_REG((sid << 1)+COMM_SIOMULTI0, linkdata[sid]); //AdamN: SIOMULTI0 (16bit data received from master/server)
							//UPDATE_REG(COMM_SIOCNT, (READ16LE(&ioMem[COMM_SIOCNT]) & 0xffcf) | (linkid << 4)); //AdamN: Set ID bit.4-5 not needed for server as it's already set in StartLink
							//UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) & 0xff7f); //AdamN: Deactivate bit.7 (Start/Busy), should be done after a cycle finished
							//value &= 0xff7f; 
							//value |= (sent != 0) << 7;
							//
							sent = false;
							//todo: Master: broadcast received data to other GBAs
							outbuffer[0] = 3; //AdamN: size of packet
							outbuffer[1] = sid; //AdamN: Sender ID
							u16outbuffer[1] = linkdata[sid];
							for(int j=1; j<=lanlink.numgbas; j++)
							if(j!=i && j!=sid) {
								notblock = 0; //1
								ioctlsocket(ls.tcpsocket[j], FIONBIO, &notblock); //AdamN: temporarily use non-blocking for sending multiple data at once
								int ern=WSAGetLastError();
								if(ern!=0) log("IOCTL31 Error: %d\n",ern);
								#ifdef GBA_LOGGING
									if(systemVerbose & VERBOSE_LINK) {
										log("%sSSend1 to : %d  Size : %d  %s\n", (LPCTSTR)LogStr, j, 4, (LPCTSTR)DataHex(outbuffer,4));
									}
								#endif
								setsockopt(ls.tcpsocket[j], IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
								sent|=(send(ls.tcpsocket[j], outbuffer, 4, 0)>=0);
								ern=WSAGetLastError();
								if(ern!=0) {
									log("Send-%d Error: %d\n",j,ern);
									if(ern==10054 || ern==10053 || ern==10057 || ern==10051 || ern==10050 || ern==10065) lanlink.connected = false;
									if(!lanlink.connected) break;
								}
								if(sent) {
									//UPDATE_REG((linkid << 1)+COMM_SIOMULTI0, linkdata[linkid]);
									//UPDATE_REG(COMM_RCNT, READ16LE(&ioMem[COMM_RCNT]) & 0xfff7); //AdamN: LOWering SO bit.3 (RCNT or SIOCNT?)
									linkdatarecvd[j] = true;
								}
							}
						}
					}
				}
				last = true;
				for(int i=0; i<=lanlink.numgbas; i++) last &= linkdatarecvd[i];
				if(last) { //AdamN: cycle ends
					//UPDATE_REG(COMM_RCNT, READ16LE(&ioMem[COMM_RCNT]) | 0x0001); //AdamN: HIGHering bit.0 (SC), only needed for Master/Server?
					//UPDATE_REG(COMM_RCNT, READ16LE(&ioMem[COMM_RCNT]) | 0x0009 /*8*/); //AdamN: HIGHering SO bit.3 (RCNT or SIOCNT?)
					UPDATE_REG(COMM_RCNT, READ16LE(&ioMem[COMM_RCNT]) | 0x0003/*b*/); //AdamN: HIGHering SC,SD,SO (apparently SD need to be High also for Master), Does Master checking SC when not using IRQ also?
					UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) & 0xff7f); //AdamN: Deactivate bit.7 (Start/Busy)
					//AdamN: Trigger Interrupt if Enabled
					if (READ16LE(&ioMem[COMM_SIOCNT]) & 0x4000) { //IRQ Enable
						IF |= 0x80; //Serial Communication
						UPDATE_REG(0x202, IF); //Interrupt Request Flags / IRQ Acknowledge
					}
					for(int i=0; i<=lanlink.numgbas; i++) linkdatarecvd[i]=false; //AdamN: does it need to reset these?
					#ifdef GBA_LOGGING
						if(systemVerbose & VERBOSE_SIO) {
							//if(READ16LE(&ioMem[COMM_SIOCNT]) & 0x4000)
							log("SIO : %04X %04X  %04X %04X %04X %04X  (VCOUNT = %d) %d\n", READ16LE(&ioMem[COMM_RCNT]), READ16LE(&ioMem[COMM_SIOCNT]), READ16LE(&ioMem[COMM_SIOMULTI0]), READ16LE(&ioMem[COMM_SIOMULTI1]), READ16LE(&ioMem[COMM_SIOMULTI2]), READ16LE(&ioMem[COMM_SIOMULTI3]), VCOUNT, GetTickCount() );
						}
					#endif
				} else if(sent) UPDATE_REG(COMM_RCNT, READ16LE(&ioMem[COMM_RCNT]) & 0xfff7); //AdamN: LOWering SO bit.3 (RCNT or SIOCNT?)	
				} while(!last && lanlink.connected);
				UPDATE_REG(COMM_SIOCNT, (READ16LE(&ioMem[COMM_SIOCNT]) & 0xffcf) | (linkid << 4)); //AdamN: Set ID bit.4-5, needed for Mario Kart
			}
		}
		return;
	} 
	return;
}

void LinkRFUUpdate()
{
 if(!lanlink.active) { //Single Comp
	if ((rfu_cmd==0xa5||rfu_cmd==0xa7||rfu_cmd==0x28) && (READ16LE(&ioMem[COMM_SIOCNT]) & 0x80) && !transfer) {
		bool ok = false;
		if(rfu_cmd==0xa5 && rfu_transfer_end <= 0) ok=true; //AdamN: workaround for single comp, to prevent the game getting timeout due to both side waiting for incoming data (as incoming data aren't queued)
		else
		for(int i=0; i<linkmem->numgbas; i++)
			if(i!=vbaid) {
				WaitForSingleObject(linksync[i], linktimeout);
				ResetEvent(linksync[i]);
				if(linkmem->rfu_q[i]>0 && ((linkmem->rfu_qid[i]&(1<<vbaid))==0||(linkmem->rfu_qid[i]==(vbaid<<3)+0x61f1))) {ok=true;SetEvent(linksync[i]);break;}
				SetEvent(linksync[i]);
			}
			
		if(ok) {
			rfu_cmd = 0x28;
			transfer = 1;
			rfu_qrecv = 0;
			rfu_transfer_end = 0;
			/*u16 value = READ16LE(&ioMem[COMM_SIOCNT]);
			if (value & 8) //Transfer Enable Flag Send (bit.3, 1=Disable Transfer/Not Ready)
				value &= 0xfffb; //Transfer enable flag receive (0=Enable Transfer/Ready, bit.2=bit.3 of otherside)	// A kind of acknowledge procedure
			else //(Bit.3, 0=Enable Transfer/Ready)
				value |= 4; //bit.2=1 (otherside is Not Ready)
			UPDATE_REG(COMM_SIOCNT, value);*/
			UPDATE_REG(COMM_SIODATA32_H, 0x9966);
			UPDATE_REG(COMM_SIODATA32_L, (rfu_qrecv<<8) | rfu_cmd);
		}
	}
 } else //Network
 if(lanlink.connected) {
	 
 }
}

// Windows threading is within!
void LinkUpdate(int ticks)
{
	BOOL recvd = false;
	BOOL sent = false;
	linktime += ticks;

	if (rfu_enabled)
	{
		linktime2 += ticks; // linktime2 is unused!
		rfu_transfer_end -= ticks;
		
		LinkRFUUpdate();

		if (transfer && rfu_transfer_end <= 0) 
		{
			transfer = 0;
			if (READ16LE(&ioMem[COMM_SIOCNT]) & 0x4000)
			{
				IF |= 0x80;
				UPDATE_REG(0x202, IF);
			}
			UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) & 0xff7f); //Start bit.7 reset
			#ifdef GBA_LOGGING
				if(systemVerbose & VERBOSE_SIO) {
					log("SIOn32 : %04X %04X  %08X  (VCOUNT = %d) %d %d\n", READ16LE(&ioMem[COMM_RCNT]), READ16LE(&ioMem[COMM_SIOCNT]), READ32LE(&ioMem[COMM_SIODATA32_L]), VCOUNT, GetTickCount(), linktime2 );
				}
			#endif
		}
		return;
	}

	if (lanlink.active) //On Networks
	{
		if (lanlink.connected)
		{
			if (after)
			{
				if (linkid && linktime > 6044) { 
					LogStrPush("LinkUpd1");
					recvd = lc.Recv(); //AdamN: not sure who need this, never reach here?
					LogStrPop(8);
					oncewait = true;
				}
				else
					return;
			}

			if (linkid && !transfer && lc.numtransfers > 0 && linktime >= savedlinktime)
			{
				linkdata[linkid] = READ16LE(&ioMem[COMM_SIODATA8]);

				if (!lc.oncesend) {
					LogStrPush("LinkUpd2");
					sent = lc.Send(); //AdamN: Client need to send this often
					LogStrPop(8);
				}
				lc.oncesend = false;
				if(sent) { //AdamN: no need to check sent here ?
				UPDATE_REG(COMM_SIODATA32_L, linkdata[0]); //AdamN: linkdata[0] instead of linkdata[linkid]?
				UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) | 0x80);
				transfer = 1;
				if (lc.numtransfers==1)
					linktime = 0;
				else
					linktime -= savedlinktime;
				}
			}

			if (transfer && linktime >= trtimeend[lanlink.numgbas-1][tspeed])
			{
				if (READ16LE(&ioMem[COMM_SIOCNT]) & 0x4000) //IRQ Enable
				{
					IF |= 0x80; //Serial Communication
					UPDATE_REG(0x202, IF); //Interrupt Request Flags / IRQ Acknowledge
				}

				UPDATE_REG(COMM_SIOCNT, (READ16LE(&ioMem[COMM_SIOCNT]) & 0xff0f) | (linkid << 4));
				transfer = 0;
				linktime -= trtimeend[lanlink.numgbas-1][tspeed];
				oncewait = false;

				if (!lanlink.speed) //lanlink.speed = speedhack
				{
					if (linkid) { //Slave/Client
						LogStrPush("LinkUpd3");
						recvd = lc.Recv(); //AdamN: Client need to read this often
						LogStrPop(8);
					} else { //Master/Server
						LogStrPush("LinkUpd4");
						recvd = ls.Recv(); // WTF is the point of this?
					               //AdamN: This is often Needed for Server/Master to works properly, This seems also to be the cause of stop responding on server when a client closed down after connection established, but the infinite loop was occuring inside ls.Recv(); (fixed tho) however, timeouts inside ls.Recv() can still cause lag
						LogStrPop(8);
					}
					if(recvd) {
					UPDATE_REG(COMM_SIODATA32_H, linkdata[1]);
					UPDATE_REG(0x124, linkdata[2]);
					UPDATE_REG(0x126, linkdata[3]);
					oncewait = true;
					#ifdef GBA_LOGGING
						if(systemVerbose & VERBOSE_SIO) {
							//if(((READ16LE(&ioMem[COMM_SIOCNT]) >> 4) & 3) != linkid)
							//if(READ16LE(&ioMem[COMM_SIOCNT]) & 0x4000)
							log("SIOm16 : %04X %04X  %04X %04X %04X %04X  (VCOUNT = %d) %d %d\n", READ16LE(&ioMem[COMM_RCNT]), READ16LE(&ioMem[COMM_SIOCNT]), READ16LE(&ioMem[COMM_SIOMULTI0]), READ16LE(&ioMem[COMM_SIOMULTI1]), READ16LE(&ioMem[COMM_SIOMULTI2]), READ16LE(&ioMem[COMM_SIOMULTI3]), VCOUNT, GetTickCount(), savedlinktime );
						}
					#endif
					}

				} else {
					if(recvd) { //AdamN: should this be checked for linkdata consistency?
					after = true;
					if (lanlink.numgbas == 1) //AdamN: only for 2 players?
					{
						UPDATE_REG(COMM_SIODATA32_H, linkdata[1]);
						UPDATE_REG(0x124, linkdata[2]);
						UPDATE_REG(0x126, linkdata[3]);
						#ifdef GBA_LOGGING
						if(systemVerbose & VERBOSE_SIO) {
							//if(((READ16LE(&ioMem[COMM_SIOCNT]) >> 4) & 3) != linkid)
							//if(READ16LE(&ioMem[COMM_SIOCNT]) & 0x4000)
							log("SIOm16 : %04X %04X  %04X %04X %04X %04X  (VCOUNT = %d) %d %d\n", READ16LE(&ioMem[COMM_RCNT]), READ16LE(&ioMem[COMM_SIOCNT]), READ16LE(&ioMem[COMM_SIOMULTI0]), READ16LE(&ioMem[COMM_SIOMULTI1]), READ16LE(&ioMem[COMM_SIOMULTI2]), READ16LE(&ioMem[COMM_SIOMULTI3]), VCOUNT, GetTickCount(), savedlinktime );
						}
						#endif
					}
					}
				}
			}
		}
		return;
	} 

	// ** CRASH ** linkmem is NULL, todo investigate why, added null check
	if (linkid && !transfer && linkmem && linktime >= linkmem->lastlinktime && linkmem->numtransfers) //On Single Computer (link.active=false)
	{
		linkmem->linkdata[linkid] = READ16LE(&ioMem[COMM_SIODATA8]);

		if (linkmem->numtransfers == 1)
		{
			linktime = 0;
			if (WaitForSingleObject(linksync[linkid], linktimeout) == WAIT_TIMEOUT)
				linkmem->numtransfers = 0;
		}
		else
			linktime -= linkmem->lastlinktime;

		switch ((linkmem->linkcmd[0]) >> 8)
		{
		case 'M':
			tspeed = (linkmem->linkcmd[0]) & 3;
			transfer = 1;
			WRITE32LE(&ioMem[COMM_SIODATA32_L], 0xffffffff);
			WRITE32LE(&ioMem[0x124], 0xffffffff); //COMM_SIOMULTI2
			UPDATE_REG(COMM_SIOCNT, READ16LE(&ioMem[COMM_SIOCNT]) | 0x80);
			break;
		}
	}

	if (!transfer)
		return;

	if (transfer && linktime >= trtimedata[transfer-1][tspeed] && transfer <= linkmem->numgbas)
	{
		if (transfer-linkid == 2)
		{
			SetEvent(linksync[linkid+1]);
			if (WaitForSingleObject(linksync[linkid], linktimeout) == WAIT_TIMEOUT)
				linkmem->numtransfers = 0;
			ResetEvent(linksync[linkid]);
		}

		UPDATE_REG(0x11e + (transfer<<1), linkmem->linkdata[transfer-1]);
		transfer++;
	}

	if (transfer && linktime >= trtimeend[linkmem->numgbas-2][tspeed])
	{
		if (linkid == linkmem->numgbas-1)
		{
			SetEvent(linksync[0]);
			if (WaitForSingleObject(linksync[linkid], linktimeout) == WAIT_TIMEOUT)
				linkmem->numtransfers = 0;

			ResetEvent(linksync[linkid]);
			
		}

		transfer = 0;
		linktime -= trtimeend[0][tspeed];
		if (READ16LE(&ioMem[COMM_SIOCNT]) & 0x4000)
		{
			IF |= 0x80;
			UPDATE_REG(0x202, IF);
		}
		UPDATE_REG(COMM_SIOCNT, (READ16LE(&ioMem[COMM_SIOCNT]) & 0xff0f) | (linkid << 4));
		linkmem->linkdata[linkid] = 0xffff;
	}

	return;
}

inline int GetSIOMode(u16 siocnt, u16 rcnt)
{
	if (!(rcnt & 0x8000))
	{
		switch (siocnt & 0x3000) {
		case 0x0000: return NORMAL8;
		case 0x1000: return NORMAL32;
		case 0x2000: return MULTIPLAYER; //AdamN: 16bit
		case 0x3000: return UART; //AdamN: 8bit
		}
	}else //AdamN: added ELSE to make sure bit.15 is 1 as there is no default handler above

	if (rcnt & 0x4000) //AdamN: Joybus is (rcnt & 0xC000)==0xC000
		return JOYBUS;

	return GP; //AdamN: GeneralPurpose is (rcnt & 0xC000)==0x8000
}

u16 StartRFU2(u16 value)
{
	static char inbuffer[1028], outbuffer[1028];
	u16 *u16inbuffer = (u16*)inbuffer;
	u16 *u16outbuffer = (u16*)outbuffer;
	u32 *u32inbuffer = (u32*)inbuffer;
	u32 *u32outbuffer = (u32*)outbuffer;
	static int outsize, insize;
	static int gbaid = 0;
	int initid = 0;
	int ngbas = 0;
	BOOL recvd = false;
	bool ok = false;

	switch (GetSIOMode(value, READ16LE(&ioMem[COMM_RCNT]))) {
	case NORMAL8:
		rfu_polarity = 0;
		return value;
		break;

	case NORMAL32:
		/*if (value & 8) //Transfer Enable Flag Send (bit.3, 1=Disable Transfer/Not Ready)
			value &= 0xfffb; //Transfer enable flag receive (0=Enable Transfer/Ready, bit.2=bit.3 of otherside)	// A kind of acknowledge procedure
		else //(Bit.3, 0=Enable Transfer/Ready)
			value |= 4; //bit.2=1 (otherside is Not Ready)*/

		if (value & 0x80) //start/busy bit
		{
			value |= 0x02; //wireless always use 2Mhz speed right?

			if ((value & 3) == 1) //internal clock w/ 256KHz speed
				rfu_transfer_end = 2048;
			else //external clock or any clock w/ 2MHz speed
				rfu_transfer_end = 0; //256; //0

			u16 a = READ16LE(&ioMem[COMM_SIODATA32_H]);

			switch (rfu_state) {
			case RFU_INIT:
				if (READ32LE(&ioMem[COMM_SIODATA32_L]) == 0xb0bb8001) {
					rfu_state = RFU_COMM;	// end of startup
					//WaitForSingleObject(linksync[vbaid], linktimeout);
					//ResetEvent(linksync[vbaid]);
					rfudata.rfu_q[vbaid] = 0;
					rfudata.rfu_qid[vbaid] = 0;
					rfudata.rfu_request[vbaid] = 0;
					rfudata.rfu_reqid[vbaid] = 0;
					rfudata.rfu_bdata[vbaid][0] = 0;
					numtransfers = 0;
					rfu_masterq = 0;
					//SetEvent(linksync[vbaid]); //vbaid
				}
				UPDATE_REG(COMM_SIODATA32_H, READ16LE(&ioMem[COMM_SIODATA32_L]));
				UPDATE_REG(COMM_SIODATA32_L, a);
				break;

			case RFU_COMM:
				if (a == 0x9966) //initialize command
				{
					rfu_cmd = ioMem[COMM_SIODATA32_L];
					if ((rfu_qsend=ioMem[COMM_SIODATA32_L+1]) != 0) { //COMM_SIODATA32_L+1, following word size
						rfu_state = RFU_SEND;
						rfu_counter = 0;
					}
					if (rfu_cmd == 0x25 || rfu_cmd == 0x24) { //send data
						rfu_masterq = rfu_qsend;
					} else 
					if(rfu_cmd == 0xa8) {
						rfu_id = (gbaid<<3)+0x61f1; 
					}
					#ifdef GBA_LOGGING
						if(systemVerbose & VERBOSE_LINK) {
							log("CMD1 : %08X  %d\n", READ32LE(&ioMem[COMM_SIODATA32_L]), GetTickCount());
						}
					#endif
					UPDATE_REG(COMM_SIODATA32_L, 0);
					UPDATE_REG(COMM_SIODATA32_H, 0x8000);
				}
				else if (a == 0x8000) //finalize command
				{
					switch (rfu_cmd) {
					case 0x1a:	// check if someone joined
						rfu_qrecv = 0;
						ok = false;
						initid = gbaid;
						do {
							gbaid++;
							gbaid %= linkmem->numgbas;
							WaitForSingleObject(linksync[gbaid], linktimeout); //wait until this gba allowed to move
							ResetEvent(linksync[gbaid]);
							ok = (gbaid!=initid && (gbaid==vbaid || (linkmem->rfu_request[gbaid]==0 || linkmem->rfu_reqid[gbaid]!=(vbaid<<3)+0x61f1)));
							SetEvent(linksync[gbaid]);
						} while(ok); //only include connected gbas
						//rfu_id = (gbaid<<3)+0x61f1;
						WaitForSingleObject(linksync[gbaid], linktimeout); //wait until this gba allowed to move
						ResetEvent(linksync[gbaid]);
						if(linkmem->rfu_reqid[gbaid]==(vbaid<<3)+0x61f1 && linkmem->rfu_request[gbaid]) {
							rfu_masterdata[rfu_qrecv] = (gbaid<<3)+0x61f1;
							rfu_qrecv++;
							//linkmem->rfu_request[gbaid] = 0; //to prevent receiving the same request, it seems the same request need to be received more than once
						}
						SetEvent(linksync[gbaid]);

						if(rfu_qrecv!=0) {
							rfu_state = RFU_RECV;
							rfu_counter = 0;

							rfu_id = rfu_masterdata[rfu_qrecv-1];
							gbaid = (rfu_id-0x61f1)>>3;
						}
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x1e:	// receive broadcast data
					case 0x1d:	// no visible difference
						rfu_polarity = 0;
						rfu_state = RFU_RECV;
						rfu_counter = 0;
						rfu_qrecv = 0; //7*(linkmem->numgbas-1);
						//int ctr = 0;
						for(int i=0; i<linkmem->numgbas; i++) {
							WaitForSingleObject(linksync[i], linktimeout); //wait until this gba allowed to move
							ResetEvent(linksync[i]); //don't allow this gba to move (send data)
							if(i!=vbaid && linkmem->rfu_bdata[i][0]!=0) 
							//if(linkmem->rfu_gdata[i]==linkmem->rfu_gdata[vbaid]) //only matching game id will be shown
							{
								memcpy(&rfu_masterdata[rfu_qrecv],linkmem->rfu_bdata[i],sizeof(linkmem->rfu_bdata[i]));
								rfu_qrecv+=sizeof(linkmem->rfu_bdata[i]);
							}
							SetEvent(linksync[i]);
						}
						rfu_qrecv = rfu_qrecv >> 2;
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x11:	// ? always receives 0xff - I suspect it's something for 3+ players
						rfu_qrecv = 0;
						if(rfu_qrecv==0) { //not the host
							rfu_qrecv++;
						}
						if(rfu_qrecv!=0)
							rfu_state = RFU_RECV;
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x13:	// unknown
					case 0x14:	// unknown, seems to have something to do with 0x11
					case 0x20:	// this has something to do with 0x1f
					case 0x21:	// this too
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						rfu_polarity = 0;
						rfu_state = RFU_RECV;
						rfu_qrecv = 1;
						break;

					case 0x24:	// send data (broadcast to all connected adapters?)
						if((numtransfers++)==0) linktime = 1; //numtransfers doesn't seems to be used?
						WaitForSingleObject(linksync[vbaid], linktimeout); //wait until the last sent data has been received/read
						ResetEvent(linksync[vbaid]); //mark it as unread/unreceived data
						linkmem->rfu_linktime[vbaid] = linktime;
						linkmem->rfu_q[vbaid] = rfu_masterq; //linkmem->rfu_q[vbaid];
						memcpy(linkmem->rfu_data[vbaid],rfu_masterdata,rfu_masterq<<2);
						linkmem->rfu_qid[vbaid] = 0; //rfu_id; //mark to whom the data for, 0=to all connected gbas
						SetEvent(linksync[vbaid]);
						//log("Send%02X[%d] : %02X  %0s\n", rfu_cmd, GetTickCount(), rfu_masterq*4, (LPCTSTR)DataHex((char*)rfu_masterdata,rfu_masterq*4) );
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						linktime = 0;
						break;

					case 0x25:	// send (to currently connected adapter) & wait for data reply
						if((numtransfers++)==0) linktime = 1; //numtransfers doesn't seems to be used?
						WaitForSingleObject(linksync[vbaid], linktimeout); //wait until last sent data received
						ResetEvent(linksync[vbaid]); //mark it as unread/unreceived
						linkmem->rfu_linktime[vbaid] = linktime;
						linkmem->rfu_q[vbaid] = rfu_masterq; //linkmem->rfu_q[vbaid];
						memcpy(linkmem->rfu_data[vbaid],rfu_masterdata,rfu_masterq<<2); //(rfu_counter+1)<<2 //linkmem->rfu_q[vbaid]<<2 //rfu_qsend<<2
						linkmem->rfu_qid[vbaid] = rfu_id; //mark to whom the data for
						SetEvent(linksync[vbaid]);
						//log("Send%02X[%d] : %02X  %0s\n", rfu_cmd, GetTickCount(), rfu_masterq*4, (LPCTSTR)DataHex((char*)rfu_masterdata,rfu_masterq*4) );
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x26: //receive data
						//read data from currently connected adapter
						WaitForSingleObject(linksync[gbaid], linktimeout);
						ResetEvent(linksync[gbaid]); //to prevent data being changed while still being read
						if((linkmem->rfu_qid[gbaid]==(vbaid<<3)+0x61f1)||(linkmem->rfu_qid[gbaid]<0x61f1 && !(linkmem->rfu_qid[gbaid] & (1<<vbaid)))) { //only receive data intended for this gba
							rfu_masterq = linkmem->rfu_q[gbaid];
							memcpy(rfu_masterdata, linkmem->rfu_data[gbaid], rfu_masterq<<2); //128 //sizeof(rfu_masterdata)
						} else rfu_masterq = 0;
						SetEvent(linksync[gbaid]); //mark it as received
						rfu_qrecv = rfu_masterq; //rfu_cacheq;
						
						if(rfu_qrecv!=0)
						{
							rfu_state = RFU_RECV;
							rfu_counter = 0;

							WaitForSingleObject(linksync[gbaid], linktimeout);
							ResetEvent(linksync[gbaid]);
							if(linkmem->rfu_qid[gbaid]>=0x61f1) //only when the data intended for this gba
							linkmem->rfu_q[gbaid] = 0; else linkmem->rfu_qid[gbaid] |= (1<<vbaid); //to prevent receiving an already received data
							SetEvent(linksync[gbaid]);
						}
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;
					
					case 0x30: //Terminate connection?
						WaitForSingleObject(linksync[vbaid], linktimeout);
						ResetEvent(linksync[vbaid]);
						linkmem->rfu_q[vbaid] = 0;
						linkmem->rfu_qid[vbaid] = 0;
						linkmem->rfu_request[vbaid] = 0;
						linkmem->rfu_reqid[vbaid] = 0;
						linkmem->rfu_bdata[vbaid][0] = 0;
						SetEvent(linksync[vbaid]); //vbaid
						numtransfers = 0;
						rfu_masterq = 0;
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x16:	// send broadcast data (ie. broadcast room name)
						//copy bdata to all gbas
						{
							WaitForSingleObject(linksync[vbaid], linktimeout);
							ResetEvent(linksync[vbaid]);
							linkmem->rfu_bdata[vbaid][0] = (vbaid<<3)+0x61f1;
							memcpy(&linkmem->rfu_bdata[vbaid][1],rfu_masterdata,sizeof(linkmem->rfu_bdata[vbaid])-4);
							SetEvent(linksync[vbaid]);
						}
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x17:	// setup or something ? (broadcast game id?)
						//copy gdata to all gbas
						{
							WaitForSingleObject(linksync[vbaid], linktimeout);
							ResetEvent(linksync[vbaid]);
							linkmem->rfu_gdata[vbaid] = rfu_masterdata[0];
							SetEvent(linksync[vbaid]);
						}
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x1f:	// pick/join a server
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x10:	// init?
					case 0x3d:	// init?
						rfu_qrecv = 1;
						rfu_state = RFU_RECV;
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x19:	//?
					case 0x1c:	//??
					case 0x27:	// wait for data ?
					default:
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0xa5:	//	2nd part of send&wait function 0x25
					case 0xa7:	//	2nd part of wait function 0x27
					case 0x28:
						WaitForSingleObject(linksync[vbaid], linktimeout);
						ResetEvent(linksync[vbaid]); //to make sure nobody changing it until it finishes reading it
						rfu_transfer_end = linkmem->rfu_linktime[vbaid] - linktime + 256;
						SetEvent(linksync[vbaid]); //mark it as received
						
						//if (rfu_transfer_end < 256)
						//	rfu_transfer_end = 256;

						linktime = -rfu_transfer_end;
						rfu_transfer_end = 2048; //256 //0 //might fasten the response (but slowdown performance), but if incoming data isn't ready the game might get timeout faster
						rfu_polarity = 1;
						//check if there is incoming data for this gba
						ok = false;
						for(int i=0; i<linkmem->numgbas; i++)
							if(i!=vbaid) {
								WaitForSingleObject(linksync[i], linktimeout);
								ResetEvent(linksync[i]);
								if(linkmem->rfu_q[i]>0 && ((linkmem->rfu_qid[i]&(1<<vbaid))==0||(linkmem->rfu_qid[i]==(vbaid<<3)+0x61f1))) {ok=true;SetEvent(linksync[i]);break;}
								SetEvent(linksync[i]);
							}
						if(!ok) return value; //don't change anything yet if there is no incoming data
						rfu_cmd = 0x28;
						break;
					}
					UPDATE_REG(COMM_SIODATA32_H, 0x9966);
					UPDATE_REG(COMM_SIODATA32_L, (rfu_qrecv<<8) | rfu_cmd);
					#ifdef GBA_LOGGING
						if(systemVerbose & VERBOSE_LINK) {
							static CString st = _T("");
							if(rfu_masterq>0 && (rfu_cmd>=0xa4 && rfu_cmd<=0xa6))
							st = DataHex((char*)rfu_masterdata, rfu_masterq<<2); else st = _T("");
							log("CMD2 : %08X  %d  %s\n", READ32LE(&ioMem[COMM_SIODATA32_L]), GetTickCount(), (LPCTSTR)st);
							st = _T("");
						}
					#endif

				} else { //unknown command word

					UPDATE_REG(COMM_SIODATA32_L, 0);
					UPDATE_REG(COMM_SIODATA32_H, 0x8000);
				}
				break;

			case RFU_SEND:
				if(--rfu_qsend == 0) {
					rfu_state = RFU_COMM;
				}

				switch (rfu_cmd) {
				case 0x16: //broadcast room data
					rfu_masterdata[rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
					break;

				case 0x17: //set game id?
					rfu_masterdata[0] = READ32LE(&ioMem[COMM_SIODATA32_L]);
					break;

				case 0x1f: //join a room
					WaitForSingleObject(linksync[vbaid], linktimeout);
					ResetEvent(linksync[vbaid]);
					linkmem->rfu_reqid[vbaid] = READ16LE(&ioMem[COMM_SIODATA32_L]); //(rfu_id-0x61f1)>>3
					linkmem->rfu_request[vbaid] = 1;
					rfu_id = READ16LE(&ioMem[COMM_SIODATA32_L]);
					gbaid = (rfu_id-0x61f1)>>3;
					SetEvent(linksync[vbaid]);
					break;

				case 0x24: //send data
				case 0x25: //send & wait
					rfu_masterdata[rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
					break;
				}
				UPDATE_REG(COMM_SIODATA32_L, 0);
				UPDATE_REG(COMM_SIODATA32_H, 0x8000);
				break;

			case RFU_RECV:
				if (--rfu_qrecv == 0) {
					rfu_state = RFU_COMM;
				}

				switch (rfu_cmd) {
				case 0x9d:
				case 0x9e:
					UPDATE_REG(COMM_SIODATA32_L, rfu_masterdata[rfu_counter]&0xffff);
					UPDATE_REG(COMM_SIODATA32_H, rfu_masterdata[rfu_counter++]>>16);
					break;

				case 0xa6:
					UPDATE_REG(COMM_SIODATA32_L, rfu_masterdata[rfu_counter]&0xffff);
					UPDATE_REG(COMM_SIODATA32_H, rfu_masterdata[rfu_counter++]>>16);
					break;

				case 0x90:
				case 0xbd:
					UPDATE_REG(COMM_SIODATA32_L, (vbaid<<3)+0x61f1); //this adapter id?
					UPDATE_REG(COMM_SIODATA32_H, 0x0000);
					break;

				case 0x93:	// it seems like the game doesn't care about this value //vendor id?
					UPDATE_REG(COMM_SIODATA32_L, 0x1234);	// put anything in here
					UPDATE_REG(COMM_SIODATA32_H, 0x0200);	// also here, but it should be 0200
					break;

				case 0xa0:
				case 0xa1:
					UPDATE_REG(COMM_SIODATA32_L, (vbaid<<3)+0x61f1/*0x641b*/); //this adapter id?
					UPDATE_REG(COMM_SIODATA32_H, 0x0000);
					break;

				case 0x9a:
					UPDATE_REG(COMM_SIODATA32_L, rfu_masterdata[rfu_counter]&0xffff);
					UPDATE_REG(COMM_SIODATA32_H, rfu_masterdata[rfu_counter++]>>16);
					break;

				case 0x91:
					UPDATE_REG(COMM_SIODATA32_L, 0x00ff); //signal strength, more than 0xff seems to be invalid signal
					UPDATE_REG(COMM_SIODATA32_H, 0x0000);
					break;

				case 0x94:
					UPDATE_REG(COMM_SIODATA32_L, rfu_id); //current active connected adapter ID?
					UPDATE_REG(COMM_SIODATA32_H, 0x0000);
					break;

				default:
					UPDATE_REG(COMM_SIODATA32_L, 0x0001/*0x0173*/); //not 0x0000 as default?
					UPDATE_REG(COMM_SIODATA32_H, 0x0000);
					break;
				}
				break;
			}
			transfer = 1;
		}

		//if(transfer) //&& rfu_state!=RFU_INIT
			if (value & 8) //Transfer Enable Flag Send (bit.3, 1=Disable Transfer/Not Ready)
				value &= 0xfffb; //Transfer enable flag receive (0=Enable Transfer/Ready, bit.2=bit.3 of otherside)	// A kind of acknowledge procedure
			else //(Bit.3, 0=Enable Transfer/Ready)
				value |= 4; //bit.2=1 (otherside is Not Ready)

		if (rfu_polarity)
			value ^= 4;	// sometimes it's the other way around

	default: //other SIO modes
		return value;
	}
}

u16 StartRFU3(u16 value)
{
	static char inbuffer[1028], outbuffer[1028];
	u16 *u16inbuffer = (u16*)inbuffer;
	u16 *u16outbuffer = (u16*)outbuffer;
	u32 *u32inbuffer = (u32*)inbuffer;
	u32 *u32outbuffer = (u32*)outbuffer;
	static int outsize, insize;
	static int gbaid = 0;
	int initid = 0;
	int ngbas = 0;
	BOOL recvd = false;
	bool ok = false;

	switch (GetSIOMode(value, READ16LE(&ioMem[COMM_RCNT]))) {
	case NORMAL8:
		rfu_polarity = 0;
		return value;
		break;

	case NORMAL32:
		/*if (value & 8) //Transfer Enable Flag Send (bit.3, 1=Disable Transfer/Not Ready)
			value &= 0xfffb; //Transfer enable flag receive (0=Enable Transfer/Ready, bit.2=bit.3 of otherside)	// A kind of acknowledge procedure
		else //(Bit.3, 0=Enable Transfer/Ready)
			value |= 4; //bit.2=1 (otherside is Not Ready)*/

		if (value & 0x80) //start/busy bit
		{
			value |= 0x02; //wireless always use 2Mhz speed right?

			if ((value & 3) == 1) //internal clock w/ 256KHz speed
				rfu_transfer_end = 2048;
			else //external clock or any clock w/ 2MHz speed
				rfu_transfer_end = 0; //256; //0

			u16 a = READ16LE(&ioMem[COMM_SIODATA32_H]);

			switch (rfu_state) {
			case RFU_INIT:
				if (READ32LE(&ioMem[COMM_SIODATA32_L]) == 0xb0bb8001) {
					rfu_state = RFU_COMM;	// end of startup
					WaitForSingleObject(linksync[vbaid], linktimeout);
					ResetEvent(linksync[vbaid]);
					linkmem->rfu_q[vbaid] = 0;
					linkmem->rfu_qid[vbaid] = 0;
					linkmem->rfu_request[vbaid] = 0;
					linkmem->rfu_reqid[vbaid] = 0;
					linkmem->rfu_bdata[vbaid][0] = 0;
					numtransfers = 0;
					rfu_masterq = 0;
					SetEvent(linksync[vbaid]); //vbaid
				}
				/*if((value & 0x1081)==0x1080) { //Pre-initialization (used to detect whether wireless adapter is available or not)
					UPDATE_REG(COMM_SIODATA32_L, 0x494e); //0x494e
					UPDATE_REG(COMM_SIODATA32_H, 0x0000); //0x0000
					rfu_transfer_end = 256;
					transfer = 1;
					if (value & 8) //Transfer Enable Flag Send (bit.3, 1=Disable Transfer/Not Ready)
						value &= 0xfffb; //Transfer enable flag receive (0=Enable Transfer/Ready, bit.2=bit.3 of otherside)	// A kind of acknowledge procedure
					else //(Bit.3, 0=Enable Transfer/Ready)
						value |= 4; //bit.2=1 (otherside is Not Ready)
					value |= 0x02; // (value & 0xff7f)|0x02; //bit.1 need to be Set(2Mhz) otherwise some games might not be able to detect wireless adapter existance
					return value;
				} else*/ {
				UPDATE_REG(COMM_SIODATA32_H, READ16LE(&ioMem[COMM_SIODATA32_L]));
				UPDATE_REG(COMM_SIODATA32_L, a);
				}
				break;

			case RFU_COMM:
				if (a == 0x9966) //initialize command
				{
					rfu_cmd = ioMem[COMM_SIODATA32_L];
					if ((rfu_qsend=ioMem[COMM_SIODATA32_L+1]) != 0) { //COMM_SIODATA32_L+1, following word size
						rfu_state = RFU_SEND;
						rfu_counter = 0;
					}
					if (rfu_cmd == 0x25 || rfu_cmd == 0x24) { //send data
						rfu_masterq = rfu_qsend;
					} else 
					if(rfu_cmd == 0xa8) {
						WaitForSingleObject(linksync[vbaid], linktimeout); //wait until this gba allowed to move
						ResetEvent(linksync[vbaid]);
						if(linkmem->rfu_reqid[vbaid]==0) { //the host
							SetEvent(linksync[vbaid]);
							ok = false;
							initid = gbaid;
							do {
								gbaid++;
								gbaid %= linkmem->numgbas;
								WaitForSingleObject(linksync[gbaid], linktimeout); //wait until this gba allowed to move
								ResetEvent(linksync[gbaid]);
								ok = (gbaid!=initid && (gbaid==vbaid || linkmem->rfu_reqid[gbaid]!=(vbaid<<3)+0x61f1));
								SetEvent(linksync[gbaid]);
							} while (ok); //only include connected gbas
							rfu_id = (gbaid<<3)+0x61f1; //(u16)linkmem->rfu_bdata[gbaid][0];
						} else { //not the host
							rfu_id = linkmem->rfu_reqid[vbaid];
							gbaid = (rfu_id-0x61f1)>>3;
						}
						SetEvent(linksync[vbaid]);
					}
					#ifdef GBA_LOGGING
						if(systemVerbose & VERBOSE_LINK) {
							log("CMD1 : %08X  %d\n", READ32LE(&ioMem[COMM_SIODATA32_L]), GetTickCount());
						}
					#endif
					UPDATE_REG(COMM_SIODATA32_L, 0);
					UPDATE_REG(COMM_SIODATA32_H, 0x8000);
				}
				else if (a == 0x8000) //finalize command
				{
					switch (rfu_cmd) {
					case 0x1a:	// check if someone joined
						rfu_qrecv = 0;
						ok = false;
						initid = gbaid;
						do {
							gbaid++;
							gbaid %= linkmem->numgbas;
							WaitForSingleObject(linksync[gbaid], linktimeout); //wait until this gba allowed to move
							ResetEvent(linksync[gbaid]);
							ok = (gbaid!=initid && (gbaid==vbaid || (linkmem->rfu_request[gbaid]==0 || linkmem->rfu_reqid[gbaid]!=(vbaid<<3)+0x61f1)));
							SetEvent(linksync[gbaid]);
						} while(ok); //only include connected gbas
						//rfu_id = (gbaid<<3)+0x61f1;
						WaitForSingleObject(linksync[gbaid], linktimeout); //wait until this gba allowed to move
						ResetEvent(linksync[gbaid]);
						if(linkmem->rfu_reqid[gbaid]==(vbaid<<3)+0x61f1 && linkmem->rfu_request[gbaid]) {
							rfu_masterdata[rfu_qrecv] = (gbaid<<3)+0x61f1;
							rfu_qrecv++;
							//linkmem->rfu_request[gbaid] = 0; //to prevent receiving the same request, it seems the same request need to be received more than once
						}
						SetEvent(linksync[gbaid]);

						if(rfu_qrecv!=0) {
							rfu_state = RFU_RECV;
							rfu_counter = 0;

							rfu_id = rfu_masterdata[rfu_qrecv-1];
							gbaid = (rfu_id-0x61f1)>>3;
						}
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x1e:	// receive broadcast data
					case 0x1d:	// no visible difference
						rfu_polarity = 0;
						rfu_state = RFU_RECV;
						rfu_counter = 0;
						rfu_qrecv = 0; //7*(linkmem->numgbas-1);
						//int ctr = 0;
						for(int i=0; i<linkmem->numgbas; i++) {
							WaitForSingleObject(linksync[i], linktimeout); //wait until this gba allowed to move
							ResetEvent(linksync[i]); //don't allow this gba to move (send data)
							if(i!=vbaid && linkmem->rfu_bdata[i][0]!=0) 
							//if(linkmem->rfu_gdata[i]==linkmem->rfu_gdata[vbaid]) //only matching game id will be shown
							{
								memcpy(&rfu_masterdata[rfu_qrecv],linkmem->rfu_bdata[i],sizeof(linkmem->rfu_bdata[i]));
								rfu_qrecv+=sizeof(linkmem->rfu_bdata[i]);
							}
							SetEvent(linksync[i]);
						}
						rfu_qrecv = rfu_qrecv >> 2;
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x11:	// ? always receives 0xff - I suspect it's something for 3+ players
						rfu_qrecv = 0;
						if(rfu_qrecv==0) { //not the host
							rfu_qrecv++;
						}
						if(rfu_qrecv!=0)
							rfu_state = RFU_RECV;
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x13:	// unknown
					case 0x14:	// unknown, seems to have something to do with 0x11
					case 0x20:	// this has something to do with 0x1f
					case 0x21:	// this too
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						rfu_polarity = 0;
						rfu_state = RFU_RECV;
						rfu_qrecv = 1;
						break;

					case 0x24:	// send data (broadcast to all connected adapters?)
						if((numtransfers++)==0) linktime = 1; //numtransfers doesn't seems to be used?
						WaitForSingleObject(linksync[vbaid], linktimeout); //wait until the last sent data has been received/read
						ResetEvent(linksync[vbaid]); //mark it as unread/unreceived data
						linkmem->rfu_linktime[vbaid] = linktime;
						linkmem->rfu_q[vbaid] = rfu_masterq; //linkmem->rfu_q[vbaid];
						memcpy(linkmem->rfu_data[vbaid],rfu_masterdata,rfu_masterq<<2);
						linkmem->rfu_qid[vbaid] = 0; //rfu_id; //mark to whom the data for, 0=to all connected gbas
						SetEvent(linksync[vbaid]);
						//log("Send%02X[%d] : %02X  %0s\n", rfu_cmd, GetTickCount(), rfu_masterq*4, (LPCTSTR)DataHex((char*)rfu_masterdata,rfu_masterq*4) );
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						linktime = 0;
						break;

					case 0x25:	// send (to currently connected adapter) & wait for data reply
						if((numtransfers++)==0) linktime = 1; //numtransfers doesn't seems to be used?
						WaitForSingleObject(linksync[vbaid], linktimeout); //wait until last sent data received
						ResetEvent(linksync[vbaid]); //mark it as unread/unreceived
						linkmem->rfu_linktime[vbaid] = linktime;
						linkmem->rfu_q[vbaid] = rfu_masterq; //linkmem->rfu_q[vbaid];
						memcpy(linkmem->rfu_data[vbaid],rfu_masterdata,rfu_masterq<<2); //(rfu_counter+1)<<2 //linkmem->rfu_q[vbaid]<<2 //rfu_qsend<<2
						linkmem->rfu_qid[vbaid] = rfu_id; //mark to whom the data for
						SetEvent(linksync[vbaid]);
						//log("Send%02X[%d] : %02X  %0s\n", rfu_cmd, GetTickCount(), rfu_masterq*4, (LPCTSTR)DataHex((char*)rfu_masterdata,rfu_masterq*4) );
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x26: //receive data
						//read data from currently connected adapter
						WaitForSingleObject(linksync[gbaid], linktimeout);
						ResetEvent(linksync[gbaid]); //to prevent data being changed while still being read
						if((linkmem->rfu_qid[gbaid]==(vbaid<<3)+0x61f1)||(linkmem->rfu_qid[gbaid]<0x61f1 && !(linkmem->rfu_qid[gbaid] & (1<<vbaid)))) { //only receive data intended for this gba
							rfu_masterq = linkmem->rfu_q[gbaid];
							memcpy(rfu_masterdata, linkmem->rfu_data[gbaid], rfu_masterq<<2); //128 //sizeof(rfu_masterdata)
						} else rfu_masterq = 0;
						SetEvent(linksync[gbaid]); //mark it as received
						rfu_qrecv = rfu_masterq; //rfu_cacheq;
						
						if(rfu_qrecv!=0)
						{
							rfu_state = RFU_RECV;
							rfu_counter = 0;

							WaitForSingleObject(linksync[gbaid], linktimeout);
							ResetEvent(linksync[gbaid]);
							if(linkmem->rfu_qid[gbaid]>=0x61f1) //only when the data intended for this gba
							linkmem->rfu_q[gbaid] = 0; else linkmem->rfu_qid[gbaid] |= (1<<vbaid); //to prevent receiving an already received data
							SetEvent(linksync[gbaid]);
						}
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;
					
					case 0x30: //Terminate connection?
						WaitForSingleObject(linksync[vbaid], linktimeout);
						ResetEvent(linksync[vbaid]);
						linkmem->rfu_q[vbaid] = 0;
						linkmem->rfu_qid[vbaid] = 0;
						linkmem->rfu_request[vbaid] = 0;
						linkmem->rfu_reqid[vbaid] = 0;
						linkmem->rfu_bdata[vbaid][0] = 0;
						SetEvent(linksync[vbaid]); //vbaid
						numtransfers = 0;
						rfu_masterq = 0;
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x16:	// send broadcast data (ie. broadcast room name)
						//copy bdata to all gbas
						{
							WaitForSingleObject(linksync[vbaid], linktimeout);
							ResetEvent(linksync[vbaid]);
							linkmem->rfu_bdata[vbaid][0] = (vbaid<<3)+0x61f1;
							memcpy(&linkmem->rfu_bdata[vbaid][1],rfu_masterdata,sizeof(linkmem->rfu_bdata[vbaid])-4);
							SetEvent(linksync[vbaid]);
						}
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x17:	// setup or something ? (broadcast game id?)
						//copy gdata to all gbas
						{
							WaitForSingleObject(linksync[vbaid], linktimeout);
							ResetEvent(linksync[vbaid]);
							linkmem->rfu_gdata[vbaid] = rfu_masterdata[0];
							SetEvent(linksync[vbaid]);
						}
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x1f:	// pick/join a server
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x10:	// init?
					case 0x3d:	// init?
						rfu_qrecv = 1;
						rfu_state = RFU_RECV;
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0x19:	//?
					case 0x1c:	//??
					case 0x27:	// wait for data ?
					default:
						rfu_cmd |= 0x80; //rfu_cmd ^= 0x80
						break;

					case 0xa5:	//	2nd part of send&wait function 0x25
					case 0xa7:	//	2nd part of wait function 0x27
					case 0x28:
						WaitForSingleObject(linksync[vbaid], linktimeout);
						ResetEvent(linksync[vbaid]); //to make sure nobody changing it until it finishes reading it
						rfu_transfer_end = linkmem->rfu_linktime[vbaid] - linktime + 256;
						SetEvent(linksync[vbaid]); //mark it as received
						
						//if (rfu_transfer_end < 256)
						//	rfu_transfer_end = 256;

						linktime = -rfu_transfer_end;
						rfu_transfer_end = 2048; //256 //0 //might fasten the response (but slowdown performance), but if incoming data isn't ready the game might get timeout faster
						rfu_polarity = 1;
						//check if there is incoming data for this gba
						ok = false;
						for(int i=0; i<linkmem->numgbas; i++)
							if(i!=vbaid) {
								WaitForSingleObject(linksync[i], linktimeout);
								ResetEvent(linksync[i]);
								if(linkmem->rfu_q[i]>0 && ((linkmem->rfu_qid[i]&(1<<vbaid))==0||(linkmem->rfu_qid[i]==(vbaid<<3)+0x61f1))) {ok=true;SetEvent(linksync[i]);break;}
								SetEvent(linksync[i]);
							}
						if(!ok) return value; //don't change anything yet if there is no incoming data
						rfu_cmd = 0x28;
						break;
					}
					UPDATE_REG(COMM_SIODATA32_H, 0x9966);
					UPDATE_REG(COMM_SIODATA32_L, (rfu_qrecv<<8) | rfu_cmd);
					#ifdef GBA_LOGGING
						if(systemVerbose & VERBOSE_LINK) {
							static CString st = _T("");
							if(rfu_masterq>0 && (rfu_cmd>=0xa4 && rfu_cmd<=0xa6))
							st = DataHex((char*)rfu_masterdata, rfu_masterq<<2); else st = _T("");
							log("CMD2 : %08X  %d  %s\n", READ32LE(&ioMem[COMM_SIODATA32_L]), GetTickCount(), (LPCTSTR)st);
							st = _T("");
						}
					#endif

				} else { //unknown command word

					UPDATE_REG(COMM_SIODATA32_L, 0);
					UPDATE_REG(COMM_SIODATA32_H, 0x8000);
				}
				break;

			case RFU_SEND:
				if(--rfu_qsend == 0) {
					rfu_state = RFU_COMM;
				}

				switch (rfu_cmd) {
				case 0x16: //broadcast room data
					rfu_masterdata[rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
					break;

				case 0x17: //set game id?
					rfu_masterdata[0] = READ32LE(&ioMem[COMM_SIODATA32_L]);
					break;

				case 0x1f: //join a room
					WaitForSingleObject(linksync[vbaid], linktimeout);
					ResetEvent(linksync[vbaid]);
					linkmem->rfu_reqid[vbaid] = READ16LE(&ioMem[COMM_SIODATA32_L]); //(rfu_id-0x61f1)>>3
					linkmem->rfu_request[vbaid] = 1;
					rfu_id = READ16LE(&ioMem[COMM_SIODATA32_L]);
					gbaid = (rfu_id-0x61f1)>>3;
					SetEvent(linksync[vbaid]);
					break;

				case 0x24: //send data
				case 0x25: //send & wait
					rfu_masterdata[rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
					break;
				}
				UPDATE_REG(COMM_SIODATA32_L, 0);
				UPDATE_REG(COMM_SIODATA32_H, 0x8000);
				break;

			case RFU_RECV:
				if (--rfu_qrecv == 0) {
					rfu_state = RFU_COMM;
				}

				switch (rfu_cmd) {
				case 0x9d:
				case 0x9e:
					UPDATE_REG(COMM_SIODATA32_L, rfu_masterdata[rfu_counter]&0xffff);
					UPDATE_REG(COMM_SIODATA32_H, rfu_masterdata[rfu_counter++]>>16);
					break;

				case 0xa6:
					UPDATE_REG(COMM_SIODATA32_L, rfu_masterdata[rfu_counter]&0xffff);
					UPDATE_REG(COMM_SIODATA32_H, rfu_masterdata[rfu_counter++]>>16);
					break;

				case 0x90:
				case 0xbd:
					UPDATE_REG(COMM_SIODATA32_L, (vbaid<<3)+0x61f1); //this adapter id?
					UPDATE_REG(COMM_SIODATA32_H, 0x0000);
					break;

				case 0x93:	// it seems like the game doesn't care about this value //vendor id?
					UPDATE_REG(COMM_SIODATA32_L, 0x1234);	// put anything in here
					UPDATE_REG(COMM_SIODATA32_H, 0x0200);	// also here, but it should be 0200
					break;

				case 0xa0:
				case 0xa1:
					UPDATE_REG(COMM_SIODATA32_L, (vbaid<<3)+0x61f1/*0x641b*/); //this adapter id?
					UPDATE_REG(COMM_SIODATA32_H, 0x0000);
					break;

				case 0x9a:
					UPDATE_REG(COMM_SIODATA32_L, rfu_masterdata[rfu_counter]&0xffff);
					UPDATE_REG(COMM_SIODATA32_H, rfu_masterdata[rfu_counter++]>>16);
					break;

				case 0x91:
					UPDATE_REG(COMM_SIODATA32_L, 0x00ff); //signal strength, more than 0xff seems to be invalid signal
					UPDATE_REG(COMM_SIODATA32_H, 0x0000);
					break;

				case 0x94:
					UPDATE_REG(COMM_SIODATA32_L, rfu_id); //current active connected adapter ID?
					UPDATE_REG(COMM_SIODATA32_H, 0x0000);
					break;

				default:
					UPDATE_REG(COMM_SIODATA32_L, 0x0001/*0x0173*/); //not 0x0000 as default?
					UPDATE_REG(COMM_SIODATA32_H, 0x0000);
					break;
				}
				break;
			}
			transfer = 1;
		}

		//if(transfer) //&& rfu_state!=RFU_INIT
			if (value & 8) //Transfer Enable Flag Send (bit.3, 1=Disable Transfer/Not Ready)
				value &= 0xfffb; //Transfer enable flag receive (0=Enable Transfer/Ready, bit.2=bit.3 of otherside)	// A kind of acknowledge procedure
			else //(Bit.3, 0=Enable Transfer/Ready)
				value |= 4; //bit.2=1 (otherside is Not Ready)

		if (rfu_polarity)
			value ^= 4;	// sometimes it's the other way around

	default: //other SIO modes
		return value;
	}
}

// The GBA wireless RFU (see adapter3.txt)
// Just try to avert your eyes for now ^^ (note, it currently can be called, tho)
u16 StartRFU(u16 value)
{
	switch (GetSIOMode(value, READ16LE(&ioMem[COMM_RCNT]))) {
	case NORMAL8:
		rfu_polarity = 0;
		return value;
		break;

	case NORMAL32:
		if (value & 8) //Transfer Enable Flag Send (bit.3, 1=Disable Transfer/Not Ready)
			value &= 0xfffb; //Transfer enable flag receive (0=Enable Transfer/Ready, bit.2=bit.3 of otherside)	// A kind of acknowledge procedure
		else //(Bit.3, 0=Enable Transfer/Ready)
			value |= 4; //bit.2=1 (otherside is Not Ready)

		if (value & 0x80) //start/busy bit
		{
			value |= 0x02; //wireless always use 2Mhz speed right?

			if ((value&3) == 1) //internal clock w/ 256KHz speed
				rfu_transfer_end = 2048;
			else //external clock or any clock w/ 2MHz speed
				rfu_transfer_end = 256;

			u16 a = READ16LE(&ioMem[COMM_SIODATA32_H]);

			switch (rfu_state) {
			case RFU_INIT:
				if (READ32LE(&ioMem[COMM_SIODATA32_L]) == 0xb0bb8001)
					rfu_state = RFU_COMM;	// end of startup

				UPDATE_REG(COMM_SIODATA32_H, READ16LE(&ioMem[COMM_SIODATA32_L]));
				UPDATE_REG(COMM_SIODATA32_L, a);
				break;

			case RFU_COMM:
				if (a == 0x9966)
				{
					rfu_cmd = ioMem[COMM_SIODATA32_L];
					if ((rfu_qsend=ioMem[0x121]) != 0) { //COMM_SIODATA32_L+1, following data
						rfu_state = RFU_SEND;
						rfu_counter = 0;
					}
					if (rfu_cmd == 0x25 || rfu_cmd == 0x24) {
						linkmem->rfu_q[vbaid] = rfu_qsend;
					}
					UPDATE_REG(COMM_SIODATA32_L, 0);
					UPDATE_REG(COMM_SIODATA32_H, 0x8000);
				}
				else if (a == 0x8000)
				{
					switch (rfu_cmd) {
					case 0x1a:	// check if someone joined
						if (linkmem->rfu_request[vbaid] != 0) {
							rfu_state = RFU_RECV;
							rfu_qrecv = 1;
						}
						linkid = -1;
						rfu_cmd |= 0x80;
						break;

					case 0x1e:	// receive broadcast data
					case 0x1d:	// no visible difference
						rfu_polarity = 0;
						rfu_state = RFU_RECV;
						rfu_qrecv = 7;
						rfu_counter = 0;
						rfu_cmd |= 0x80;
						break;

					case 0x30:
						linkmem->rfu_request[vbaid] = 0;
						linkmem->rfu_q[vbaid] = 0;
						linkid = 0;
						numtransfers = 0;
						rfu_cmd |= 0x80;
						if (linkmem->numgbas == 2)
							SetEvent(linksync[1-vbaid]); //allow other gba to move
						break;

					case 0x11:	// ? always receives 0xff - I suspect it's something for 3+ players
					case 0x13:	// unknown
					case 0x20:	// this has something to do with 0x1f
					case 0x21:	// this too
						rfu_cmd |= 0x80;
						rfu_polarity = 0;
						rfu_state = 3;
						rfu_qrecv = 1;
						break;

					case 0x26:
						if ( linkid > 0) { //may not be needed
							memcpy(rfu_masterdata, linkmem->rfu_data[1-vbaid], 128);
							rfu_masterq = linkmem->rfu_q[1-vbaid];
						}
						if(linkid>0){
							rfu_qrecv = rfu_masterq;
						}
						if((rfu_qrecv=linkmem->rfu_q[1-vbaid])!=0){
							rfu_state = RFU_RECV;
							rfu_counter = 0;
						}
						//if(rfu_state == RFU_RECV)
						//if(linkid>0)
						//log("Recv26[%d] : %02X  %0s\n", GetTickCount(), rfu_qrecv*4, (LPCTSTR)DataHex((char*)rfu_masterdata,rfu_qrecv*4) ); else
						//log("Recv26[%d] : %02X  %0s\n", GetTickCount(), rfu_qrecv*4, (LPCTSTR)DataHex((char*)linkmem->rfu_data[1-vbaid],rfu_qrecv*4) );
						rfu_cmd |= 0x80;
						break;

					case 0x24:	// send data
						if((numtransfers++)==0) linktime = 1; //numtransfers doesn't seems to be used?
						linkmem->rfu_linktime[vbaid] = linktime;
						if(linkmem->numgbas==2){
							SetEvent(linksync[1-vbaid]); //allow other gba to move (read data)
							WaitForSingleObject(linksync[vbaid], linktimeout); //wait until this gba allowed to move
							ResetEvent(linksync[vbaid]); //don't allow this gba to move (send data)
						}
						//log("Send%02X[%d] : %02X  %0s\n", rfu_cmd, GetTickCount(), linkmem->rfu_q[vbaid]*4, (LPCTSTR)DataHex((char*)linkmem->rfu_data[vbaid],linkmem->rfu_q[vbaid]*4) );
						rfu_cmd |= 0x80;
						linktime = 0;
						linkid = -1;
						break;

					case 0x25:	// send & wait for data
						//log("Send%02X[%d] : %02X  %0s\n", rfu_cmd, GetTickCount(), linkmem->rfu_q[vbaid]*4, (LPCTSTR)DataHex((char*)linkmem->rfu_data[vbaid],linkmem->rfu_q[vbaid]*4) );
						rfu_cmd |= 0x80;
						break;
					case 0x1f:	// pick/join a server
					case 0x10:	// init
					case 0x16:	// send broadcast data (ie. room name)
					case 0x17:	// setup or something ?
					case 0x27:	// wait for data ?
					case 0x3d:	// init
					default:
						rfu_cmd |= 0x80;
						break;

					case 0xa5:	//	2nd part of send&wait function 0x25
					case 0xa7:	//	2nd part of wait function 0x27
						if (linkid == -1) {
							linkid++;
							linkmem->rfu_linktime[vbaid] = 0;
						}
						if (linkid&&linkmem->rfu_request[1-vbaid] == 0) {
							linkmem->rfu_q[1-vbaid] = 0;
							rfu_transfer_end = 256;
							rfu_polarity = 1;
							rfu_cmd = 0x29;
							linktime = 0;
							break;
						}
						if ((numtransfers++) == 0)
							linktime = 0;
						linkmem->rfu_linktime[vbaid] = linktime;
						if (linkmem->numgbas == 2) {
							if (!linkid || (linkid && numtransfers))
								SetEvent(linksync[1-vbaid]); //allow other gba to move (send data)
							WaitForSingleObject(linksync[vbaid], linktimeout); //wait until this gba allowed to move
							ResetEvent(linksync[vbaid]); //don't allow this gba to move(read data)
						}
						if ( linkid > 0) {
							memcpy(rfu_masterdata, linkmem->rfu_data[1-vbaid], 128);
							rfu_masterq = linkmem->rfu_q[1-vbaid];
						}
						rfu_transfer_end = linkmem->rfu_linktime[1-vbaid] - linktime + 256;
						
						if (rfu_transfer_end < 256)
							rfu_transfer_end = 256;

						linktime = -rfu_transfer_end;
						rfu_polarity = 1;
						rfu_cmd = 0x28;
						break;
					}
					UPDATE_REG(COMM_SIODATA32_H, 0x9966);
					UPDATE_REG(COMM_SIODATA32_L, (rfu_qrecv<<8) | rfu_cmd);

				} else {

					UPDATE_REG(COMM_SIODATA32_L, 0);
					UPDATE_REG(COMM_SIODATA32_H, 0x8000);
				}
				break;

			case RFU_SEND:
				if(--rfu_qsend == 0) {
					rfu_state = RFU_COMM;
				}

				switch (rfu_cmd) {
				case 0x16:
					linkmem->rfu_bdata[vbaid][rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
					break;

				case 0x17:
					linkid = 1;
					break;

				case 0x1f:
					linkmem->rfu_request[1-vbaid] = 1;
					break;

				case 0x24:
				case 0x25:
					linkmem->rfu_data[vbaid][rfu_counter++] = READ32LE(&ioMem[COMM_SIODATA32_L]);
					break;
				}
				UPDATE_REG(COMM_SIODATA32_L, 0);
				UPDATE_REG(COMM_SIODATA32_H, 0x8000);
				break;

			case RFU_RECV:
				if (--rfu_qrecv == 0)
					rfu_state = RFU_COMM;

				switch (rfu_cmd) {
				case 0x9d:
				case 0x9e:
					if (rfu_counter == 0) {
						UPDATE_REG(COMM_SIODATA32_L, 0x61f1);
						UPDATE_REG(COMM_SIODATA32_H, 0);
						rfu_counter++;
						break;
					}
					UPDATE_REG(COMM_SIODATA32_L, linkmem->rfu_bdata[1-vbaid][rfu_counter-1]&0xffff);
					UPDATE_REG(COMM_SIODATA32_H, linkmem->rfu_bdata[1-vbaid][rfu_counter-1]>>16);
					rfu_counter++;
					break;

			case 0xa6:
				if (linkid>0) {
					UPDATE_REG(COMM_SIODATA32_L, rfu_masterdata[rfu_counter]&0xffff);
					UPDATE_REG(COMM_SIODATA32_H, rfu_masterdata[rfu_counter++]>>16);
				} else {
					UPDATE_REG(COMM_SIODATA32_L, linkmem->rfu_data[1-vbaid][rfu_counter]&0xffff);
					UPDATE_REG(COMM_SIODATA32_H, linkmem->rfu_data[1-vbaid][rfu_counter++]>>16);
				}
				break;

			case 0x93:	// it seems like the game doesn't care about this value
				UPDATE_REG(COMM_SIODATA32_L, 0x1234);	// put anything in here
				UPDATE_REG(COMM_SIODATA32_H, 0x0200);	// also here, but it should be 0200
				break;

			case 0xa0:
			case 0xa1:
				UPDATE_REG(COMM_SIODATA32_L, 0x641b);
				UPDATE_REG(COMM_SIODATA32_H, 0x0000);
				break;

			case 0x9a:
				UPDATE_REG(COMM_SIODATA32_L, 0x61f9);
				UPDATE_REG(COMM_SIODATA32_H, 0);
				break;

			case 0x91:
				UPDATE_REG(COMM_SIODATA32_L, 0x00ff);
				UPDATE_REG(COMM_SIODATA32_H, 0x0000);
				break;

			default:
				UPDATE_REG(COMM_SIODATA32_L, 0x0173);
				UPDATE_REG(COMM_SIODATA32_H, 0x0000);
				break;
			}
			break;
		}
		transfer = 1;
	}

	if (rfu_polarity)
		value ^= 4;	// sometimes it's the other way around

	default:
		return value;
	}
}

//////////////////////////////////////////////////////////////////////////
// Probably from here down needs to be replaced with SFML goodness :)

int InitLink()
{
	WSADATA wsadata;
	BOOL disable = true;
	DWORD timeout = linktimeout;
	int sz = 32768;
	//int len = sizeof(int);
	unsigned long notblock = 1; //AdamN: 0=blocking, non-zero=non-blocking

	linkid = 0;

	if(WSAStartup(MAKEWORD(1,1), &wsadata)!=0){
		WSACleanup();
		return 0;
	}

	if((lanlink.tcpsocket=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))==INVALID_SOCKET){
		MessageBox(NULL, _T("Couldn't create socket."), _T("Error!"), MB_OK);
		WSACleanup();
		return 0;
	}

	setsockopt(lanlink.tcpsocket, IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately

	setsockopt(lanlink.tcpsocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(DWORD)); //setting recv timeout
	setsockopt(lanlink.tcpsocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(DWORD)); //setting send timeout
	//getsockopt(lanlink.tcpsocket, SOL_SOCKET, SO_RCVBUF, (char*)&sz, (int*)&len); //setting recv buffer
	setsockopt(lanlink.tcpsocket, SOL_SOCKET, SO_RCVBUF, (char*)&sz, sizeof(int)); //setting recv buffer

	/*if(ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock)==SOCKET_ERROR) { //AdamN: activate non-blocking mode (by default sockets are using blocking mode after created)
		MessageBox(NULL, _T("Couldn't enable non-blocking socket."), _T("Error!"), MB_OK);
		closesocket(lanlink.tcpsocket);
		WSACleanup();
		return 0;
	}*/

	if((mmf=CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(LINKDATA), _T("VBA link memory")))==NULL){
		closesocket(lanlink.tcpsocket);
		WSACleanup();
		MessageBox(NULL, _T("Error creating file mapping"), _T("Error"), MB_OK|MB_ICONEXCLAMATION);
		return 0;
	}

	if(GetLastError() == ERROR_ALREADY_EXISTS)
		vbaid = 1;
	else
		vbaid = 0;

	if((linkmem=(LINKDATA *)MapViewOfFile(mmf, FILE_MAP_WRITE, 0, 0, sizeof(LINKDATA)))==NULL){
		closesocket(lanlink.tcpsocket);
		WSACleanup();
		CloseHandle(mmf);
		MessageBox(NULL, _T("Error mapping file"), _T("Error"), MB_OK|MB_ICONEXCLAMATION);
		return 0;
	}

	if(linkmem->linkflags & LINK_PARENTLOST)
		vbaid = 0;

	if(vbaid==0){
		linkid = 0;
		if(linkmem->linkflags & LINK_PARENTLOST){
			linkmem->numgbas++;
			linkmem->linkflags &= ~LINK_PARENTLOST;
		}
		else
			linkmem->numgbas=1;

		for(i=0;i<5;i++){
			linkevent[15]=(char)i+'1';
			if((linksync[i]=CreateEvent(NULL, true, false, linkevent))==NULL){
				closesocket(lanlink.tcpsocket);
				WSACleanup();
				UnmapViewOfFile(linkmem);
				CloseHandle(mmf);
				for(j=0;j<i;j++){
					CloseHandle(linksync[j]);
				}
				MessageBox(NULL, _T("Error opening event"), _T("Error"), MB_OK|MB_ICONEXCLAMATION);
				return 0;
			}
		}
	} else {
		vbaid=linkmem->numgbas;
		linkid = vbaid;
		linkmem->numgbas++;

		if(linkmem->numgbas>5){
			linkmem->numgbas=5;
			closesocket(lanlink.tcpsocket);
			WSACleanup();
			MessageBox(NULL, _T("6 or more GBAs not supported."), _T("Error!"), MB_OK|MB_ICONEXCLAMATION);
			UnmapViewOfFile(linkmem);
			CloseHandle(mmf);
			return 0;
		}
		for(i=0;i<5;i++){
			linkevent[15]=(char)i+'1';
			if((linksync[i]=OpenEvent(EVENT_ALL_ACCESS, false, linkevent))==NULL){
				closesocket(lanlink.tcpsocket);
				WSACleanup();
				CloseHandle(mmf);
				UnmapViewOfFile(linkmem);
				for(j=0;j<i;j++){
					CloseHandle(linksync[j]);
				}
				MessageBox(NULL, _T("Error opening event"), _T("Error"), MB_OK|MB_ICONEXCLAMATION);
				return 0;
			}
		}
	}
	rfu_thisid = (vbaid<<3)+0x61f1; //0x61f1+vbaid;
	linkmem->lastlinktime=0xffffffff;
	linkmem->numtransfers=0;
	linkmem->linkflags=0;
	lanlink.connected = false;
	lanlink.thread = NULL;
	lanlink.speed = false;
	for(i=0;i<4;i++){
		linkmem->linkdata[i] = 0xffff;
		linkdata[i] = 0xffff;
		linkdatarecvd[i] = false;
	}
	return 1; //1=sucessful?
}

void CloseLink(void){
	if(/*lanlink.active &&*/ lanlink.connected){
		if(linkid){
			char outbuffer[4];
			outbuffer[0] = 4;
			outbuffer[1] = -32;
			if(lanlink.type==0) send(lanlink.tcpsocket, outbuffer, 4, 0);
		} else {
			char outbuffer[12];
			int i;
			outbuffer[0] = 12;
			outbuffer[1] = -32;
			for(i=1;i<=lanlink.numgbas;i++){
				if(lanlink.type==0){
					send(ls.tcpsocket[i], outbuffer, 12, 0);
				}
				closesocket(ls.tcpsocket[i]);
			}
		}
	}
	linkmem->numgbas--;
	if(!linkid&&linkmem->numgbas!=0)
		linkmem->linkflags|=LINK_PARENTLOST;
	CloseHandle(mmf);
	UnmapViewOfFile(linkmem);

	for(i=0;i<4;i++){
		if(linksync[i]!=NULL){
			PulseEvent(linksync[i]);
			CloseHandle(linksync[i]);
		}
	}
	regSetDwordValue("LAN", lanlink.active);
	closesocket(lanlink.tcpsocket);
	WSACleanup();
	return;
}

// Server
lserver::lserver(void){
	intinbuffer = (int*)inbuffer;
	u16inbuffer = (u16*)inbuffer;
	u32inbuffer = (u32*)inbuffer;
	intoutbuffer = (int*)outbuffer;
	u16outbuffer = (u16*)outbuffer;
	u32outbuffer = (u32*)outbuffer;
	oncewait = false;
}

int lserver::Init(void *serverdlg){
	SOCKADDR_IN info;
	DWORD nothing;
	char str[100];
	unsigned long notblock = 1; //0=blocking, non-zero=non-blocking

	info.sin_family = AF_INET;
	info.sin_addr.S_un.S_addr = INADDR_ANY;
	info.sin_port = htons(5738);

	ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //use non-blocking mode?

	if(lanlink.thread!=NULL){
		c_s.Lock(); //AdamN: Locking resource to prevent deadlock
		lanlink.terminate = true;
		c_s.Unlock(); //AdamN: Unlock it after use
		WaitForSingleObject(linksync[vbaid], 2000); //500
		lanlink.thread = NULL;
		ResetEvent(linksync[vbaid]); //should it be reset?
	}
	lanlink.terminate = false;

	CloseLink(); //AdamN: close connections gracefully
	InitLink(); //AdamN: reinit sockets
	outsize = 0;
	insize = 0;

	//notblock = 1; //0;
	//ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //AdamN: need to be blocking mode? It seems Server Need to be in Non-blocking mode

	if(bind(lanlink.tcpsocket, (LPSOCKADDR)&info, sizeof(SOCKADDR_IN))==SOCKET_ERROR){
		closesocket(lanlink.tcpsocket);
		if((lanlink.tcpsocket=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))==INVALID_SOCKET)
			return WSAGetLastError();
		if(bind(lanlink.tcpsocket, (LPSOCKADDR)&info, sizeof(SOCKADDR_IN))==SOCKET_ERROR)
			return WSAGetLastError();
	}

	if(listen(lanlink.tcpsocket, lanlink.numgbas)==SOCKET_ERROR)
		return WSAGetLastError();

	linkid = 0;

	gethostname(str, 100);
	int i = 0;
	CString stringb = _T("");
	if (gethostbyname(str)->h_addrtype == AF_INET) {
            while (gethostbyname(str)->h_addr_list[i] != 0) {
				stringb.AppendFormat(_T("%s/"), inet_ntoa(*(LPIN_ADDR)(gethostbyname(str)->h_addr_list[i++]))); //AdamN: trying to shows all IP that was binded
			}
			if(stringb.GetLength()>0)
				stringb.Delete(stringb.GetLength()-1);
	}
	((ServerWait*)serverdlg)->m_serveraddress.Format(_T("Server IP address is: %s"), (LPCTSTR)stringb /*inet_ntoa(*(LPIN_ADDR)(gethostbyname(str)->h_addr_list[0]))*/);
	
	lanlink.thread = CreateThread(NULL, 0, LinkServerThread, serverdlg, 0, &nothing);

	return 0;

}

DWORD WINAPI LinkServerThread(void *serverdlg){ 
	fd_set fdset;
	timeval wsocktimeout;
	SOCKADDR_IN info;
	int infolen;
	char inbuffer[256], outbuffer[256];
	int *intinbuffer = (int*)inbuffer;
	u16 *u16inbuffer = (u16*)inbuffer;
	int *intoutbuffer = (int*)outbuffer;
	u16 *u16outbuffer = (u16*)outbuffer;
	BOOL disable = true;
	DWORD timeout = linktimeout;
	int sz = 32768;
	//DWORD nothing;
	unsigned long notblock = 1; //0=blocking, non-zero=non-blocking

	wsocktimeout.tv_sec = linktimeout / 1000; //1;
	wsocktimeout.tv_usec = linktimeout % 1000; //0;
	i = 0;
	BOOL shown = true;

	while(shown && i<lanlink.numgbas){ //AdamN: this may not be thread-safe
		fdset.fd_count = 1;
		fdset.fd_array[0] = lanlink.tcpsocket;
		int sel = select(0, &fdset, NULL, NULL, &wsocktimeout);
		if(sel==1){ //AdamN: output from select can also be SOCKET_ERROR, it seems ServerWait window got stucked when Cancel pressed because select will only return 1 if a player connected
		    c_s.Lock(); //AdamN: Locking resource to prevent deadlock
			bool canceled=lanlink.terminate;
			c_s.Unlock(); //AdamN: Unlock it after use
			if(canceled){ //AdamN: check if ServerWait was Canceled, might not be thread-safe
				//SetEvent(linksync[vbaid]); //AdamN: i wonder what is this needed for?
				//return 0; //AdamN: exiting the thread here w/o closing the ServerWait window will cause the window to be stucked
				break;
			}
			if((ls.tcpsocket[i+1]=accept(lanlink.tcpsocket, NULL, NULL))==INVALID_SOCKET){
				for(int j=1;j<i;j++) closesocket(ls.tcpsocket[j]);
				MessageBox(NULL, _T("Network error."), _T("Error"), MB_OK);
				//return 1; //AdamN: exiting the thread here w/o closing the ServerWait window will cause the window to be stucked
				break;
			} else {
				setsockopt(ls.tcpsocket[i+1], IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
				setsockopt(ls.tcpsocket[i+1], SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(DWORD)); //setting recv timeout
				setsockopt(ls.tcpsocket[i+1], SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(DWORD)); //setting send timeout
				setsockopt(ls.tcpsocket[i+1], SOL_SOCKET, SO_RCVBUF, (char*)&sz, sizeof(int)); //setting recv buffer
				/*notblock = 1;
				ioctlsocket(ls.tcpsocket[i+1], FIONBIO, &notblock); //AdamN: need to be in non-blocking mode?*/
				u16outbuffer[0] = i+1;
				u16outbuffer[1] = lanlink.numgbas;
				DWORD lat = GetTickCount();
				send(ls.tcpsocket[i+1], outbuffer, 4, 0); //Sending index and #gba to client
				lat = GetTickCount() - lat;
				infolen = sizeof(SOCKADDR_IN);
				getpeername(ls.tcpsocket[i+1], (LPSOCKADDR)&info , &infolen);
				((ServerWait*)serverdlg)->m_plconn[i].Format(_T("Player %d connected  (IP: %s, Latency: %dms)"), i+1, inet_ntoa(info.sin_addr), lat);
				if(IsWindow(((ServerWait*)serverdlg)->m_hWnd)) { //AdamN: not really useful for UpdateData
					//((ServerWait*)serverdlg)->UpdateData(false); //AdamN: this seems to cause 2 assertion failed errors when a player connected, seems to be not thread-safe
					((ServerWait*)serverdlg)->GetDlgItem(IDC_STATIC1)->SetWindowText((LPCTSTR)((ServerWait*)serverdlg)->m_serveraddress); //((ServerWait*)serverdlg)->Invalidate(); //((ServerWait*)serverdlg)->SendMessage(WM_PAINT, 0, 0); //AdamN: using message might be safer than UpdateData but might not works
					((ServerWait*)serverdlg)->GetDlgItem(IDC_STATIC2)->SetWindowText((LPCTSTR)((ServerWait*)serverdlg)->m_plconn[0]); //m_plconn[0]
					((ServerWait*)serverdlg)->GetDlgItem(IDC_STATIC3)->SetWindowText((LPCTSTR)((ServerWait*)serverdlg)->m_plconn[1]); //m_plconn[1]
					((ServerWait*)serverdlg)->GetDlgItem(IDC_STATIC4)->SetWindowText((LPCTSTR)((ServerWait*)serverdlg)->m_plconn[2]); //m_plconn[2]
					//((ServerWait*)serverdlg)->GetDlgItem(IDC_SERVERWAIT)->Invalidate(); //m_prgctrl
					((ServerWait*)serverdlg)->Invalidate();
				}
				i++;
			}
		}
		shown = IsWindow(((ServerWait*)serverdlg)->m_hWnd); //AdamN: trying to detect when Cancel button pressed (which cause Waiting for Player window to be closed)
		if(shown) {
			((ServerWait*)serverdlg)->m_prgctrl.StepIt(); //AdamN: this will cause assertion failed if the Waiting for Player window is Canceled
		}
		c_s.Lock(); //AdamN: Locking resource to prevent deadlock
		bool canceled=lanlink.terminate; //AdamN: w/o locking might not be thread-safe
		c_s.Unlock(); //AdamN: Unlock it after use
		if(canceled) break;
	}
	
	if(i>0) { //AdamN: if canceled after 1 or more player has been connected link will stil be marked as connected
		MessageBox(NULL, _T("All players connected"), _T("Link"), MB_OK);
		c_s.Lock(); //AdamN: Locking resource to prevent deadlock
		lanlink.numgbas = i; //AdamN: update # of GBAs according to connected players before server got canceled
		c_s.Unlock(); //AdamN: Unlock it after use
	}
	if(shown) {
	    ((ServerWait*)serverdlg)->SendMessage(WM_CLOSE, 0, 0); //AdamN: this will also cause assertion failed if the Waiting for Player window was Canceled/no longer existed
	}

	shown = (i>0); //AdamN: if canceled after 1 or more player has been connected connecteion will still be established
	for(i=1;i<=lanlink.numgbas;i++){ //AdamN: this should be i<lanlink.numgbas isn't?(just like in the while above), btw it might not be thread-safe (may be i'm being paranoid)
		outbuffer[0] = 4;
		send(ls.tcpsocket[i], outbuffer, 4, 0);
	}
	
	if(shown) { //AdamN: if one or more players connected before server got canceled connecteion will still be established
		c_s.Lock(); //AdamN: Locking resource to prevent deadlock
		lanlink.connected = true;
		c_s.Unlock(); //AdamN: Unlock it after use
	}
	/*else*/ //SetEvent(linksync[vbaid]); //AdamN: saying the lanlink.thread is exiting? might cause thread to stuck when app exited

	c_s.Lock();
	if(lanlink.connected) {
		lanlink.terminate = false;
		//lanlink.thread = CreateThread(NULL, 0, LinkHandlerThread, NULL, 0, &nothing); //AdamN: trying to reduce the lag by handling sockets in a different thread, not working properly yet
	}
	c_s.Unlock();

	return 0;
}

DWORD WINAPI LinkHandlerThread(void *param){ //AdamN: Trying to reduce the lag by handling sockets in a different thread, but doesn't works quite right
	//SetEvent(linksync[vbaid]); //AdamN: will cause VBA-M to stuck in memory after exit
	return 0;
	
	LINKCMDPRM cmdprm;
	ULONG tmp;
	bool done = false;
	c_s.Lock();
	LinkHandlerActive = true;
	c_s.Unlock();
	do {
		//AdamN: Locking cs might cause an exception if application closed down while the thread still running
		c_s.Lock(); //AdamN: Locking resource to prevent deadlock
		int LinkCmd=LinkCommand;
		cmdprm.Command = 0;
		if(LinkCmdList.GetCount()>0) {
		tmp = (ULONG)*&LinkCmdList.GetHead();
		cmdprm.Command = tmp & 0xffff;
		cmdprm.Param = tmp >> 16;
		//log("Rem: %04X %04X\n",cmdprm.Command,cmdprm.Param); //AdamN: calling "log" in here seems to cause deadlock
		LinkCmdList.RemoveHead();
		}
		c_s.Unlock(); //AdamN: Locking resource to prevent deadlock
		LinkCmd = cmdprm.Command;
		if(LinkCmd & 1) { //StartLink
			int prm = cmdprm.Param; //LinkParam1;
			StartLink2(prm); //AdamN: Might not be thread-safe w/o proper locking inside StartLink
			c_s.Lock();
			LinkCommand&=0xfffffffe;
			c_s.Unlock();
		}
		if(LinkCmd & 2) { //StartGPLink, might not be needed as it doesn't use socket
			int prm = cmdprm.Param; //LinkParam2;
			StartGPLink(prm); //AdamN: Might not be thread-safe w/o proper locking inside StartLink
			c_s.Lock();
			LinkCommand&=0xfffffffd;
			c_s.Unlock();
		}
		if(LinkCmd & 4) { //StartRFU, might not be needed as it doesn't use socket currently
			int prm = cmdprm.Param; //LinkParam4;
			StartRFU(prm); //AdamN: Might not be thread-safe w/o proper locking inside StartLink
			c_s.Lock();
			LinkCommand&=0xfffffffb;
			c_s.Unlock();
		}
		if(LinkCmd & 8) { //LinkUpdate
			int prm = cmdprm.Param; //LinkParam8;
			LinkUpdate2(prm, 0); //AdamN: Might not be thread-safe w/o proper locking inside StartLink
			c_s.Lock();
			LinkCommand&=0xfffffff7;
			c_s.Unlock();
		}
		SleepEx(1,true);

		/*c_s.Lock();
		done=(lanlink.connected && linkid&&lc.numtransfers==0);
		c_s.Unlock();
		if(done) lc.CheckConn();*/

		c_s.Lock();
		done=(/*lanlink.terminate &&*/ AppTerminated || !lanlink.connected);
		c_s.Unlock();
	} while (!done);
	//SetEvent(linksync[vbaid]);
	c_s.Lock();
	LinkHandlerActive = false;
	c_s.Unlock();
	return 0;
}

BOOL lserver::Send(void){
	//return false;
	BOOL sent = false;
	BOOL disable = true;
	if(lanlink.type==0){	// TCP
		if(savedlinktime==-1){
			outbuffer[0] = 4;
			outbuffer[1] = -32;	//0xe0 //Closing mark?
			for(i=1;i<=lanlink.numgbas;i++){
				unsigned long notblock = 1; //0=blocking, non-zero=non-blocking
				ioctlsocket(tcpsocket[i], FIONBIO, &notblock); //AdamN: use non-blocking for sending
				setsockopt(tcpsocket[i], IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
				#ifdef GBA_LOGGING
					if(systemVerbose & VERBOSE_LINK) {
						log("%sSS1\n",(LPCTSTR)LogStr);
						log("SSend to : %d  Size : %d  %s\n", i, 4, (LPCTSTR)DataHex(outbuffer,4));
					}
				#endif
				if(send(tcpsocket[i], outbuffer, 4, 0)!=SOCKET_ERROR) { //AdamN: should check for socket error to reduce the lag
					sent = true;
					unsigned long notblock = 0; //0=blocking, non-zero=non-blocking
					ioctlsocket(tcpsocket[i], FIONBIO, &notblock); //AdamN: use blocking for receiving
					int cnt=recv(tcpsocket[i], inbuffer, 4, 0);
					#ifdef GBA_LOGGING
						if(systemVerbose & VERBOSE_LINK) {
							log("%sSS2\n",(LPCTSTR)LogStr);
							log("Srecv from : %d  Size : %d  %s\n", i, cnt, (LPCTSTR)DataHex(inbuffer,cnt));
						}
					#endif
				}
			}
			//return sent;
		}
		outbuffer[1] = tspeed;
		u16outbuffer[1] = linkdata[0];
		intoutbuffer[1] = savedlinktime;
		if(lanlink.numgbas==1){
			if(lanlink.type==0){
				outbuffer[0] = 8;
				unsigned long notblock = 1; //0=blocking, non-zero=non-blocking
				ioctlsocket(tcpsocket[1], FIONBIO, &notblock); //AdamN: use non-blocking for sending
				setsockopt(tcpsocket[1], IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
				#ifdef GBA_LOGGING
					if(systemVerbose & VERBOSE_LINK) {
						log("%sSS3\n",(LPCTSTR)LogStr);
						log("SSend to : %d  Size : %d  %s\n", 1, 8, (LPCTSTR)DataHex(outbuffer,8));
					}
				#endif
				sent=(send(tcpsocket[1], outbuffer, 8, 0)>=0);
				int ern=WSAGetLastError();
				if(ern!=0) {
				log("WSAError = %d\n",ern);
				lanlink.connected = false;
				}
			}
		}
		else if(lanlink.numgbas==2){
			u16outbuffer[4] = linkdata[2];
			if(lanlink.type==0){
				outbuffer[0] = 10;
				#ifdef GBA_LOGGING
					if(systemVerbose & VERBOSE_LINK) {
						log("%sSS4\n",(LPCTSTR)LogStr);
						log("SSend to : %d  Size : %d  %s\n", 1, 10, (LPCTSTR)DataHex(outbuffer,10));
					}
				#endif
				sent=(send(tcpsocket[1], outbuffer, 10, 0)>=0);
				u16outbuffer[4] = linkdata[1];
				#ifdef GBA_LOGGING
					if(systemVerbose & VERBOSE_LINK) {
						log("%sSS5\n",(LPCTSTR)LogStr);
						log("SSend to : %d  Size : %d  %s\n", 2, 10, (LPCTSTR)DataHex(outbuffer,10));
					}
				#endif
				sent=(send(tcpsocket[2], outbuffer, 10, 0)>=0);
			}
		} else {
			if(lanlink.type==0){
				outbuffer[0] = 12;
				u16outbuffer[4] = linkdata[2];
				u16outbuffer[5] = linkdata[3];
				#ifdef GBA_LOGGING
					if(systemVerbose & VERBOSE_LINK) {
						log("%sSS6\n",(LPCTSTR)LogStr);
						log("SSend to : %d  Size : %d  %s\n", 1, 12, (LPCTSTR)DataHex(outbuffer,12));
					}
				#endif
				sent=(send(tcpsocket[1], outbuffer, 12, 0)>=0);
				u16outbuffer[4] = linkdata[1];
				#ifdef GBA_LOGGING
					if(systemVerbose & VERBOSE_LINK) {
						log("%sSS7\n",(LPCTSTR)LogStr);
						log("SSend to : %d  Size : %d  %s\n", 2, 12, (LPCTSTR)DataHex(outbuffer,12));
					}
				#endif
				sent=(send(tcpsocket[2], outbuffer, 12, 0)>=0);
				u16outbuffer[5] = linkdata[2];
				#ifdef GBA_LOGGING
					if(systemVerbose & VERBOSE_LINK) {
						log("%sSS8\n",(LPCTSTR)LogStr);
						log("SSend to : %d  Size : %d  %s\n", 3, 12, (LPCTSTR)DataHex(outbuffer,12));
					}
				#endif
				sent=(send(tcpsocket[3], outbuffer, 12, 0)>=0);
			}
		}
	}
	return sent;
}

BOOL lserver::Recv(void){
	//return false;
	BOOL recvd = false;
	BOOL disable = true;
	unsigned long arg = 0;

	int numbytes;
	if(lanlink.type==0){	// TCP
		wsocktimeout.tv_sec = linktimeout / 1000; //AdamN: setting this too small may cause disconnection in game, too large will cause great lag
		wsocktimeout.tv_usec = linktimeout % 1000; //0; //AdamN: remainder should be set also isn't?
		fdset.fd_count = lanlink.numgbas;
		for(i=0;i<lanlink.numgbas;i++) {
			fdset.fd_array[i] = tcpsocket[i+1];
			unsigned long notblock = 0; //0=blocking, non-zero=non-blocking
			ioctlsocket(tcpsocket[i+1], FIONBIO, &notblock); //AdamN: use blocking for receiving
		}
		if(select(0, &fdset, NULL, NULL, &wsocktimeout)<=0 ){ //AdamN: may cause noticible delay, result can also be SOCKET_ERROR, Select seems to be needed to maintain stability
			int ern=WSAGetLastError();
			if(ern!=0)
			log("%sSR1[%d]\n",(LPCTSTR)LogStr,ern); //AdamN: seems to be getting 3x timeout when multiplayer established in game
			return recvd;
		}
		/*int cnt=0;
		for(i=0;i<lanlink.numgbas;i++) {
			arg = 0;
			if(ioctlsocket(tcpsocket[i+1], FIONREAD, &arg)!=0) {
				int ern=WSAGetLastError(); //AdamN: this seems to get ern=10038(invalid socket handle) often
				if(ern!=0)
				log("%sSR1-%d[%d]\n",(LPCTSTR)LogStr,i,ern);
				continue; //break;
			} else if(arg>0) cnt++;
		}
		if(cnt<=0) return recvd;*/
		howmanytimes++;
		for(i=0;i<lanlink.numgbas;i++){
			arg = 0;
			if(ioctlsocket(tcpsocket[i+1], FIONREAD, &arg)!=0) {
				int ern=WSAGetLastError(); //AdamN: this seems to get ern=10038(invalid socket handle) often
				if(ern!=0)
				log("%sSR1-%d[%d]\n",(LPCTSTR)LogStr,i,ern);
				continue; //break;
			}
			numbytes = 0;
			inbuffer[0] = 1;
			while(numbytes<howmanytimes*inbuffer[0]) {
				int cnt = recv(tcpsocket[i+1], inbuffer+numbytes, 256-numbytes, 0);
				if(cnt==SOCKET_ERROR) break; //AdamN: to prevent stop responding due to infinite loop on socket error
				numbytes += cnt;
				#ifdef GBA_LOGGING
					if(systemVerbose & VERBOSE_LINK) {
						log("%sSR2\n",(LPCTSTR)LogStr);
						log("SRecv from : %d  Size : %d  %s\n", i+1, cnt, (LPCTSTR)DataHex(inbuffer,cnt));
					}
				#endif
			}
			recvd=(WSAGetLastError()==0);
			if(howmanytimes>1) memcpy(inbuffer, inbuffer+inbuffer[0]*(howmanytimes-1), inbuffer[0]);
			if(inbuffer[1]==-32 /*|| WSAGetLastError()!=0*/){ //AdamN: Should be checking for possible of socket error isn't?
				char message[30];
				lanlink.connected = false;
				sprintf(message, _T("Player %d disconnected."), i+2);
				MessageBox(NULL, message, _T("Link"), MB_OK);
				outbuffer[0] = 4;
				outbuffer[1] = -32;
				for(i=1;i<lanlink.numgbas;i++){
					unsigned long notblock = 1; //0=blocking, non-zero=non-blocking
					ioctlsocket(tcpsocket[i], FIONBIO, &notblock); //AdamN: use non-blocking for sending
					setsockopt(tcpsocket[i], IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
					#ifdef GBA_LOGGING
						if(systemVerbose & VERBOSE_LINK) {
							log("%sSR3\n",(LPCTSTR)LogStr);
							log("Ssend to : %d  Size : %d  %s\n", i, 12, (LPCTSTR)DataHex(outbuffer,12));
						}
					#endif
					if(send(tcpsocket[i], outbuffer, 12, 0)!=SOCKET_ERROR) { //AdamN: should check for socket error to reduce the lag
					notblock = 0; //0=blocking, non-zero=non-blocking
					ioctlsocket(tcpsocket[i], FIONBIO, &notblock); //AdamN: use blocking for receiving
					int cnt=recv(tcpsocket[i], inbuffer, 256, 0);
					#ifdef GBA_LOGGING
						if(systemVerbose & VERBOSE_LINK) {
							log("%sSR4\n",(LPCTSTR)LogStr);
							log("SRecv from : %d  Size : %d  %s\n", i, cnt, (LPCTSTR)DataHex(inbuffer,cnt));
						}
					#endif
					}
					closesocket(tcpsocket[i]);
				}
				recvd=(WSAGetLastError()==0);
				return recvd;
			}
			linkdata[i+1] = u16inbuffer[1];
		}
		howmanytimes = 0;
	}
	after = false;
	return recvd;
}

BOOL lserver::SendData(int size, int nretry){
	//return false;
	BOOL sent = false;
	BOOL disable = true;
	unsigned long notblock = 0; //1; //0=blocking, non-zero=non-blocking
	for(int i=1; i<=lanlink.numgbas; i++) {
		ioctlsocket(tcpsocket[i], FIONBIO, &notblock); //AdamN: use non-blocking for sending
		setsockopt(tcpsocket[i], IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
		#ifdef GBA_LOGGING
			if(systemVerbose & VERBOSE_LINK) {
				//log("%sSS0%d\n",(LPCTSTR)LogStr, i);
				log("%sSSend0%d Size : %d  %s\n", (LPCTSTR)LogStr, i, size, (LPCTSTR)DataHex(outbuffer,size));
			}
		#endif
		BOOL sent2 = false;
		int j = nretry+1;
		do {
			DWORD lat = GetTickCount();
			sent2=(send(tcpsocket[i], outbuffer, size, 0)>=0);
			latency[i] = GetTickCount() - lat;
			int ern = WSAGetLastError();
			if(ern!=0) lanlink.connected = false;
			if(!lanlink.connected) {
				char message[40];
				sprintf(message, _T("Player %d disconnected."), i+1);
				MessageBox(NULL, message, _T("Link"), MB_OK);
			}
			j--;
		} while (j!=0 && lanlink.connected && !sent2);
		sent|=sent2;
	}
	return sent;
}

BOOL lserver::SendData(const char *buf, int size, int nretry){
	//return false;
	BOOL sent = false;
	BOOL disable = true;
	unsigned long notblock = 0; //1; //0=blocking, non-zero=non-blocking
	for(int i=1; i<=lanlink.numgbas; i++) {
		ioctlsocket(tcpsocket[i], FIONBIO, &notblock); //AdamN: use non-blocking for sending
		setsockopt(tcpsocket[i], IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
		#ifdef GBA_LOGGING
			if(systemVerbose & VERBOSE_LINK) {
				//log("%sSS0%d\n",(LPCTSTR)LogStr, i);
				log("%sSSend0%d Size : %d  %s\n", (LPCTSTR)LogStr, i, size, (LPCTSTR)DataHex(buf,size));
			}
		#endif
		BOOL sent2 = false;
		int j = nretry+1;
		do {
			DWORD lat = GetTickCount();
			sent2=(send(tcpsocket[i], buf, size, 0)>=0);
			latency[i] = GetTickCount() - lat;
			int ern = WSAGetLastError();
			if(ern!=0) lanlink.connected = false;
			if(!lanlink.connected) {
				char message[40];
				sprintf(message, _T("Player %d disconnected."), i+1);
				MessageBox(NULL, message, _T("Link"), MB_OK);
			}
			j--;
		} while (j!=0 && lanlink.connected && !sent2);
		sent|=sent2;
	}
	return sent;
}

BOOL lserver::RecvData(int size, int idx, bool peek){
	//return false;
	BOOL recvd = false;
	BOOL disable = true;
	unsigned long notblock = 0; //0=blocking, non-zero=non-blocking
	ioctlsocket(tcpsocket[idx], FIONBIO, &notblock); //AdamN: use blocking for receiving
	setsockopt(tcpsocket[idx], IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
	int flg = 0;
	if(peek) flg = MSG_PEEK;
	int rsz = size;
	do {
		int cnt=recv(tcpsocket[idx], inbuffer+(size-rsz), rsz, flg);
		if(cnt>=0) rsz-=cnt; else lanlink.connected = false;
		int ern = WSAGetLastError();
		if(ern!=0) lanlink.connected = false;
		if(!lanlink.connected) {
			char message[40];
			sprintf(message, _T("Player %d disconnected."), idx+1);
			MessageBox(NULL, message, _T("Link"), MB_OK);
		}
	} while (rsz>0 && lanlink.connected);
	insize = size-rsz;
	recvd = (rsz<=0);
	#ifdef GBA_LOGGING
		if(systemVerbose & VERBOSE_LINK) {
			//log("%sSR0%d\n",(LPCTSTR)LogStr, idx);
			log("%sSRecv%d(%d) Size : %d/%d  %s\n", (LPCTSTR)LogStr, idx, peek, insize, size, (LPCTSTR)DataHex(inbuffer,size));
		}
	#endif
	return recvd;
}

BOOL lserver::WaitForData(int ms) {
	unsigned long notblock = 1; //AdamN: 0=blocking, non-zero=non-blocking
	unsigned long arg = 0;
	//fd_set fdset;
	//timeval wsocktimeout;
	MSG msg;
	int ready = 0;
	EmuCtr++;
	//DWORD needms = ms;
	if(EmuReseted) 
		for(int i=1; i<=lanlink.numgbas; i++) DiscardData(i);
	EmuReseted = false;
	DWORD starttm = GetTickCount();
	do { //Waiting for incomming data before continuing CPU execution
		SleepEx(0,true); //SleepEx(0,true); //to give time for incoming data
		if(PeekMessage(&msg, 0/*theApp.GetMainWnd()->m_hWnd*/,  0, 0, PM_NOREMOVE)) {
			if(msg.message==WM_CLOSE) AppTerminated=true; else theApp.PumpMessage(); //seems to be processing message only if it has message otherwise it halt the program
		}

		//fdset.fd_count = lanlink.numgbas;
		notblock = 0;
		for(int i=1; i<=lanlink.numgbas; i++) {
			//fdset.fd_array[i-1] = tcpsocket[i];
			ioctlsocket(tcpsocket[i], FIONBIO, &notblock); //AdamN: temporarily use blocking for reading
			int ern=WSAGetLastError();
			if(ern!=0) {
				log("slIOCTL Error: %d\n",ern);
				//if(ern==10054 || ern==10053 || ern==10057 || ern==10051 || ern==10050 || ern==10065) lanlink.connected = false;
			}
			arg = 0; //1;
			if(ioctlsocket(tcpsocket[i], FIONREAD, &arg)!=0) { //AdamN: Alternative(Faster) to Select(Slower)
				int ern=WSAGetLastError(); 
				if(ern!=0) {
					log("%sSC Error: %d\n",(LPCTSTR)LogStr,ern);
					char message[40];
					lanlink.connected = false;
					sprintf(message, _T("Player %d disconnected."), i+1);
					MessageBox(NULL, message, _T("Link"), MB_OK);
					break;
				}
			}
			if(arg>0) ready++;
		}
		//wsocktimeout.tv_sec = linktimeout / 1000;
		//wsocktimeout.tv_usec = linktimeout % 1000; //0; //AdamN: remainder should be set also isn't?
		//ready = select(0, &fdset, NULL, NULL, &wsocktimeout);
		//int ern=WSAGetLastError();
		//if(ern!=0) {
		//	log("slCC Error: %d\n",ern);
		//	if(ern==10054 || ern==10053 || ern==10057 || ern==10051 || ern==10050 || ern==10065) lanlink.connected = false;
		//}
	} while (lanlink.connected && ready==0 && /*(int)*/(GetTickCount()-starttm)<(DWORD)ms && !AppTerminated && !EmuReseted); //with ms<0 might not gets a proper result?
	//if((GetTickCount()-starttm)>=(DWORD)ms) log("TimeOut:%d\n",ms);
	EmuCtr--;
	return (ready>0);
}

BOOL lserver::IsDataReady(void) {
	unsigned long arg;// = 0;
	int ready = 0;
	for(int i=1; i<=lanlink.numgbas; i++) {
		if(EmuReseted) DiscardData(i);
		arg = 0;
		if(ioctlsocket(tcpsocket[i], FIONREAD, &arg)!=0) { //AdamN: Alternative(Faster) to Select(Slower)
			int ern=WSAGetLastError(); 
			if(ern!=0) {
				log("%sSC Error: %d\n",(LPCTSTR)LogStr,ern);
				char message[40];
				lanlink.connected = false;
				sprintf(message, _T("Player %d disconnected."), i+1);
				MessageBox(NULL, message, _T("Link"), MB_OK);
				break;
			}
		}
		if(arg>0) ready++;
	}
	EmuReseted = false;
	return(arg>0);
}

int lserver::DiscardData(int idx) {
	char buff[256];
	unsigned long arg;
	int sz = 0;
	do {
		arg = 0;
		if(ioctlsocket(tcpsocket[idx], FIONREAD, &arg)!=0) { //AdamN: Alternative(Faster) to Select(Slower)
			int ern=WSAGetLastError(); 
			if(ern!=0) {
				log("%sCC Error: %d\n",(LPCTSTR)LogStr,ern);
				char message[40];
				lanlink.connected = false;
				sprintf(message, _T("Player %d disconnected."), idx+1);
				MessageBox(NULL, message, _T("Link"), MB_OK);
			}
		}
		if(arg>0) {
			int cnt=recv(tcpsocket[idx], buff, min(arg,256), 0);
			if(cnt>0) sz+=cnt;
		}
	} while (arg>0 && lanlink.connected);
	return(sz);
}

// Client
lclient::lclient(void){
	intinbuffer = (int*)inbuffer;
	u16inbuffer = (u16*)inbuffer;
	u32inbuffer = (u32*)inbuffer;
	intoutbuffer = (int*)outbuffer;
	u16outbuffer = (u16*)outbuffer;
	u32outbuffer = (u32*)outbuffer;
	numtransfers = 0;
	oncesend = false;
	return;
}

int lclient::Init(LPHOSTENT hostentry, void *waitdlg){
	unsigned long notblock = 1; //0=blocking, non-zero=non-blocking
	DWORD nothing;
	
	serverinfo.sin_family = AF_INET;
	serverinfo.sin_port = htons(5738);
	serverinfo.sin_addr = *((LPIN_ADDR)*hostentry->h_addr_list);

	if(ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock)==SOCKET_ERROR) //use non-blocking mode
		return WSAGetLastError();
	
	if(lanlink.thread!=NULL){
		c_s.Lock(); //AdamN: Locking resource to prevent deadlock
		lanlink.terminate = true;
		c_s.Unlock(); //AdamN: Unlock it after use
		WaitForSingleObject(linksync[vbaid], 2000); //500
		
		lanlink.thread = NULL;
		ResetEvent(linksync[vbaid]); //should it be reset?
	}

	CloseLink(); //AdamN: close connections gracefully
	InitLink(); //AdamN: reinit sockets
	outsize = 0;
	insize = 0;

	//notblock = 0;
	//ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //AdamN: need to be in blocking mode? It seems Client Need to be in Blocking mode otherwise Select will generate error 10038, But Select on blocking socket will cause delays
	
	//((ServerWait*)waitdlg)->SetWindowText("Connecting..."); //AdamN: SetWindowText seems to cause problem on client connect
	((ServerWait*)waitdlg)->m_serveraddress.Format(_T("Connecting to %s"), inet_ntoa(*(LPIN_ADDR)hostentry->h_addr_list[0]));
	
	lanlink.terminate = false;
	
	lanlink.thread = CreateThread(NULL, 0, LinkClientThread, waitdlg, 0, &nothing);
	
	return 0;
}

DWORD WINAPI LinkClientThread(void *waitdlg){
	fd_set fdset;
	timeval wsocktimeout;
	int numbytes, cnt;
	char inbuffer[16];
	u16 *u16inbuffer = (u16*)inbuffer;

	unsigned long block = 0;
	BOOL shown = true;
	//DWORD nothing;

	if(connect(lanlink.tcpsocket, (LPSOCKADDR)&lc.serverinfo, sizeof(SOCKADDR_IN))==SOCKET_ERROR){
		if(WSAGetLastError()!=WSAEWOULDBLOCK){
			MessageBox(NULL, _T("Couldn't connect to server."), _T("Link"), MB_OK);
			return 1;
		}
		wsocktimeout.tv_sec = linktimeout / 1000; //1;
		wsocktimeout.tv_usec = linktimeout % 1000; //0;
		do{
			//WinHelper::CCriticalSection::CLock lock(&c_s);
			c_s.Lock(); //AdamN: Locking resource to prevent deadlock
			bool canceled=lanlink.terminate; //AdamN: w/o locking might not be thread-safe
			c_s.Unlock(); //AdamN: Unlock it after use
			if(canceled) return 0;
			fdset.fd_count = 1;
			fdset.fd_array[0] = lanlink.tcpsocket;
			shown = IsWindow(((ServerWait*)waitdlg)->m_hWnd); //AdamN: trying to detect when Cancel button pressed (which cause Waiting for Player window to be closed)
			if(shown)
				((ServerWait*)waitdlg)->m_prgctrl.StepIt();
		} while(select(0, NULL, &fdset, NULL, &wsocktimeout)!=1/*&&connect(lanlink.tcpsocket, (LPSOCKADDR)&lc.serverinfo, sizeof(SOCKADDR_IN))!=0*/);
	}

	ioctlsocket(lanlink.tcpsocket, FIONBIO, &block); //AdamN: temporary using blocking mode

	numbytes = 0;
	DWORD lat = GetTickCount();
	while(numbytes<4) {
		cnt = recv(lanlink.tcpsocket, inbuffer+numbytes, 16, 0); //AdamN: receiving index and #of gbas
		if((cnt<=0/*SOCKET_ERROR*/)||(WSAGetLastError()!=0)) break; //AdamN: to prevent stop responding due to infinite loop on socket error
		numbytes += cnt;
		//if(IsWindow(((ServerWait*)waitdlg)->m_hWnd))
		//	((ServerWait*)waitdlg)->m_prgctrl.StepIt(); //AdamN: update progressbar so it won't look stucked
	}
	lat = GetTickCount() - lat;
	linkid = (int)u16inbuffer[0];
	lanlink.numgbas = (int)u16inbuffer[1];

	((ServerWait*)waitdlg)->m_serveraddress.Format(_T("Connected as #%d  (Latency: %dms)"), linkid+1, lat);
	if(lanlink.numgbas!=linkid)	((ServerWait*)waitdlg)->m_plconn[0].Format(_T("Waiting for %d more players to join"), lanlink.numgbas-linkid);
	else ((ServerWait*)waitdlg)->m_plconn[0].Format(_T("All players joined."));
	if(IsWindow(((ServerWait*)waitdlg)->m_hWnd)) { //AdamN: not really useful for UpdateData
		//((ServerWait*)waitdlg)->UpdateData(false); //AdamN: refreshing static text after being modified above, may not be thread-safe (causing assertion failed)
		((ServerWait*)waitdlg)->GetDlgItem(IDC_STATIC1)->SetWindowText((LPCTSTR)((ServerWait*)waitdlg)->m_serveraddress); //((ServerWait*)waitdlg)->Invalidate(); //((ServerWait*)waitdlg)->SendMessage(WM_PAINT, 0, 0); //AdamN: using message might be safer than UpdateData but might not works
		((ServerWait*)waitdlg)->GetDlgItem(IDC_STATIC2)->SetWindowText((LPCTSTR)((ServerWait*)waitdlg)->m_plconn[0]); //m_plconn[0]
		//((ServerWait*)waitdlg)->GetDlgItem(IDC_SERVERWAIT)->Invalidate(); //m_prgctrl
		((ServerWait*)waitdlg)->Invalidate();
	}
	numbytes = 0;
	inbuffer[0] = 1;
	while(numbytes<inbuffer[0]) { //AdamN: loops until all players connected or is it until the game initialize multiplayer mode?, progressbar should be updated tho
		cnt = recv(lanlink.tcpsocket, inbuffer+numbytes, 16, 0);
		if(cnt==SOCKET_ERROR) break; //AdamN: to prevent stop responding due to infinite loop on socket error
		numbytes += cnt;
		if(IsWindow(((ServerWait*)waitdlg)->m_hWnd))
			((ServerWait*)waitdlg)->m_prgctrl.StepIt(); //AdamN: update progressbar so it won't look stucked
	}
	MessageBox(NULL, _T("Connected."), _T("Link"), MB_OK); //AdamN: shown when the game initialize multiplayer mode (on VBALink it's shown after players connected to server), is it really needed to show this thing?
	if(IsWindow(((ServerWait*)waitdlg)->m_hWnd))
		((ServerWait*)waitdlg)->SendMessage(WM_CLOSE, 0, 0); //AdamN: may cause assertion failed when window no longer existed

	/*block = 1; //AdamN: 1=non-blocking
	ioctlsocket(lanlink.tcpsocket, FIONBIO, &block); //AdamN: back to non-blocking mode?*/

	lanlink.connected = true;

	//SetEvent(linksync[vbaid]); //AdamN: saying the lanlink.thread is exiting? might cause thread to stuck when app exited

	c_s.Lock();
	if(lanlink.connected) {
		lanlink.terminate = false;
		//lanlink.thread = CreateThread(NULL, 0, LinkHandlerThread, NULL, 0, &nothing); //AdamN: trying to reduce the lag by handling sockets in a different thread, not working properly yet
	}
	c_s.Unlock();

	return 0;
}

void lclient::CheckConn(void){ //AdamN: used on Idle, needed for Client to works properly
	//return;
	unsigned long arg = 0;
	BOOL recvd = false;
	BOOL disable = true;
	
	unsigned long notblock = 0; //0=blocking, non-zero=non-blocking
	ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //AdamN: use blocking for receiving

	/*fd_set fdset;
	timeval wsocktimeout;
	fdset.fd_count = 1;
	fdset.fd_array[0] = lanlink.tcpsocket;
	wsocktimeout.tv_sec = linktimeout / 1000;
	wsocktimeout.tv_usec = linktimeout % 1000; //0; //AdamN: remainder should be set also isn't?
	if(select(0, &fdset, NULL, NULL, &wsocktimeout)<=0){ //AdamN: may cause noticible delay, result can also be SOCKET_ERROR
		//numtransfers = 0;
		int ern=WSAGetLastError(); //AdamN: this seems to get ern=10038(invalid socket handle) when used on non-blocking socket
		if(ern!=0)
		log("%sCC0[%d]\n",(LPCTSTR)LogStr,ern);
		return;
	} else*/
	arg = 0;
	if(ioctlsocket(lanlink.tcpsocket, FIONREAD, &arg)!=0) { //AdamN: Alternative(Faster) to Select(Slower)
		int ern=WSAGetLastError(); 
		if(ern!=0) {
			log("%sCC0[%d]\n",(LPCTSTR)LogStr,ern);
			lanlink.connected = false;
			MessageBox(NULL, _T("Server disconnected improperly."), _T("Link"), MB_OK);
		}
		return;
	}
	if(arg>0)
	if((numbytes=recv(lanlink.tcpsocket, inbuffer, 256, 0))>=0){ //>0 //AdamN: this socket need to be in non-blocking mode otherwise it will wait forever until server's game entering multiplayer mode
		/*#ifdef GBA_LOGGING
			if(systemVerbose & VERBOSE_LINK) {
				log("%sCC1\n",(LPCTSTR)LogStr);
				log("CCrecv Size : %d  %s\n", numbytes, (LPCTSTR)DataHex(inbuffer,numbytes));
			}
		#endif*/
		if(numbytes>=0) //AdamN: otherwise socket error
		while(numbytes<inbuffer[0]) {
			int cnt=recv(lanlink.tcpsocket, inbuffer+numbytes, 256, 0);
			if(cnt==SOCKET_ERROR) break; //AdamN: to prevent stop responding due to infinite loop on socket error
			numbytes += cnt;
			#ifdef GBA_LOGGING
				if(systemVerbose & VERBOSE_LINK) {
					log("%sCC2\n",(LPCTSTR)LogStr);
					log("CCrecv Size : %d  %s\n", cnt, (LPCTSTR)DataHex(inbuffer/*+numbytes*/,cnt));
				}
			#endif
		}
		recvd = (numbytes>0);
		if(inbuffer[1]==-32){ //AdamN: only true if server was closed gracefully
			outbuffer[0] = 4;
			notblock = 1; //0=blocking, non-zero=non-blocking
			ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //AdamN: use non-blocking for sending
			setsockopt(lanlink.tcpsocket, IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
			#ifdef GBA_LOGGING
				if(systemVerbose & VERBOSE_LINK) {
					log("%sCC3\n",(LPCTSTR)LogStr);
					log("CCsend Size : %d  %s\n", 4, (LPCTSTR)DataHex(outbuffer,4));
				}
			#endif
			send(lanlink.tcpsocket, outbuffer, 4, 0);
			lanlink.connected = false;
			MessageBox(NULL, _T("Server disconnected."), _T("Link"), MB_OK);
			return;
		}
		if(recvd) {
		numtransfers = 1;
		savedlinktime = 0;
		linkdata[0] = u16inbuffer[1];
		tspeed = inbuffer[1] & 3;
		for(i=1, numbytes=4;i<=lanlink.numgbas;i++)
			if(i!=linkid) linkdata[i] = u16inbuffer[numbytes++];
		after = false;
		oncewait = true;
		oncesend = true;
		}
	}
	return;
}

BOOL lclient::Recv(void){
	//return false;
	BOOL recvd = false;
	BOOL disable = true;
	unsigned long arg = 0;
	unsigned long notblock = 0; //0=blocking, non-zero=non-blocking
	ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //AdamN: use blocking for receiving
	fdset.fd_count = 1;
	fdset.fd_array[0] = lanlink.tcpsocket;
	wsocktimeout.tv_sec = linktimeout / 1000;
	wsocktimeout.tv_usec = linktimeout % 1000; //0; //AdamN: remainder should be set also isn't?
	if(select(0, &fdset, NULL, NULL, &wsocktimeout)<=0){ //AdamN: may cause noticible delay, result can also be SOCKET_ERROR, Select seems to be needed to maintain stability
		numtransfers = 0;
		int ern=WSAGetLastError();
		if(ern!=0)
		log("%sCR1[%d]\n",(LPCTSTR)LogStr,ern);
		return recvd;
	}
	/*arg = 0;
	if(ioctlsocket(lanlink.tcpsocket, FIONREAD, &arg)!=0) {
		int ern=WSAGetLastError(); //AdamN: this seems to get ern=10038(invalid socket handle) often
		numtransfers = 0;
		if(ern!=0)
		log("%sCC0[%d]\n",(LPCTSTR)LogStr,ern);
		return recvd;
	}
	if(arg==0) return recvd;*/
	numbytes = 0;
	inbuffer[0] = 1;
	while(numbytes<inbuffer[0]){
		int cnt=recv(lanlink.tcpsocket, inbuffer+numbytes, 256, 0);
		if(cnt==SOCKET_ERROR) break; //AdamN: to prevent stop responding due to infinite loop on socket error
		numbytes += cnt;
		#ifdef GBA_LOGGING
			if(systemVerbose & VERBOSE_LINK) {
				log("%sCR2\n",(LPCTSTR)LogStr);
				log("CRecv Size : %d  %s\n", cnt, (LPCTSTR)DataHex(inbuffer,cnt));
			}
		#endif
	}
	recvd=(WSAGetLastError()==0);
	if(inbuffer[1]==-32){ //AdamN: only true if server was closed gracefully
		outbuffer[0] = 4;
		notblock = 1; //0=blocking, non-zero=non-blocking
		ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //AdamN: use non-blocking for sending
		setsockopt(lanlink.tcpsocket, IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
		#ifdef GBA_LOGGING
			if(systemVerbose & VERBOSE_LINK) {
				log("%sCR3\n",(LPCTSTR)LogStr);
				log("Csend Size : %d  %s\n", 4, (LPCTSTR)DataHex(outbuffer,4));
			}
		#endif
		send(lanlink.tcpsocket, outbuffer, 4, 0);
		lanlink.connected = false;
		MessageBox(NULL, _T("Server disconnected."), _T("Link"), MB_OK);
		//recvd=(WSAGetLastError()==0);
		return recvd;
	}
	tspeed = inbuffer[1] & 3;
	linkdata[0] = u16inbuffer[1];
	savedlinktime = intinbuffer[1];
	for(i=1, numbytes=4;i<lanlink.numgbas+1;i++)
		if(i!=linkid) linkdata[i] = u16inbuffer[numbytes++];
	numtransfers++;
	if(numtransfers==0) numtransfers = 2;
	after = false;
	return recvd;
}

BOOL lclient::Send(){
	//return false;
	BOOL sent = false;
	BOOL disable = true;
	outbuffer[0] = 4;
	outbuffer[1] = linkid<<2;
	u16outbuffer[1] = linkdata[linkid];
	unsigned long notblock = 1; //0=blocking, non-zero=non-blocking
	ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //AdamN: use non-blocking for sending
	setsockopt(lanlink.tcpsocket, IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
	#ifdef GBA_LOGGING
		if(systemVerbose & VERBOSE_LINK) {
			log("%sCS1\n",(LPCTSTR)LogStr);
			log("CSend Size : %d  %s\n", 4, (LPCTSTR)DataHex(outbuffer,4));
		}
	#endif
	sent=(send(lanlink.tcpsocket, outbuffer, 4, 0)>=0);
	return sent;
}

BOOL lclient::SendData(int size, int nretry){
	//return false;
	BOOL sent = false;
	BOOL disable = true;
	unsigned long notblock = 0; //1; //0=blocking, non-zero=non-blocking
	ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //AdamN: use non-blocking for sending
	setsockopt(lanlink.tcpsocket, IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
	#ifdef GBA_LOGGING
		if(systemVerbose & VERBOSE_LINK) {
			//log("%sCS01\n",(LPCTSTR)LogStr);
			log("%sCSend00 Size : %d  %s\n", (LPCTSTR)LogStr, size, (LPCTSTR)DataHex(outbuffer,size));
		}
	#endif
	int i = nretry+1;
	do {
		DWORD lat = GetTickCount();
		sent=(send(lanlink.tcpsocket, outbuffer, size, 0)>=0);
		lanlink.latency = GetTickCount() - lat;
		int ern = WSAGetLastError();
		if(ern!=0) lanlink.connected = false;
		if(!lanlink.connected) MessageBox(NULL, _T("Player 1 disconnected."), _T("Link"), MB_OK);
		i--;
	} while (i!=0 && lanlink.connected && !sent);
	return sent;
}

BOOL lclient::SendData(const char *buf, int size, int nretry){
	//return false;
	BOOL sent = false;
	BOOL disable = true;
	unsigned long notblock = 0; //1; //0=blocking, non-zero=non-blocking
	ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //AdamN: use non-blocking for sending
	setsockopt(lanlink.tcpsocket, IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
	#ifdef GBA_LOGGING
		if(systemVerbose & VERBOSE_LINK) {
			//log("%sCS01\n",(LPCTSTR)LogStr);
			log("%sCSend00 Size : %d  %s\n", (LPCTSTR)LogStr, size, (LPCTSTR)DataHex(buf,size));
		}
	#endif
	int i = nretry+1;
	do {
		DWORD lat = GetTickCount();
		sent=(send(lanlink.tcpsocket, buf, size, 0)>=0);
		lanlink.latency = GetTickCount() - lat;
		int ern = WSAGetLastError();
		if(ern!=0) lanlink.connected = false;
		if(!lanlink.connected) MessageBox(NULL, _T("Player 1 disconnected."), _T("Link"), MB_OK);
		i--;
	} while (i!=0 && lanlink.connected && !sent);
	return sent;
}

BOOL lclient::RecvData(int size, bool peek){
	//return false;
	BOOL recvd = false;
	BOOL disable = true;
	unsigned long notblock = 0; //0=blocking, non-zero=non-blocking
	ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //AdamN: use blocking for receiving
	setsockopt(lanlink.tcpsocket, IPPROTO_TCP, TCP_NODELAY, (char*)&disable, sizeof(BOOL)); //true=send packet immediately
	int flg = 0;
	if(peek) flg = MSG_PEEK;
	int rsz = size;
	do {
		int cnt=recv(lanlink.tcpsocket, inbuffer+(size-rsz), rsz, flg);
		if(cnt>=0) rsz-=cnt; else lanlink.connected = false;
		int ern = WSAGetLastError();
		if(ern!=0) lanlink.connected = false;
		if(!lanlink.connected) MessageBox(NULL, _T("Player 1 disconnected."), _T("Link"), MB_OK);
	} while (rsz>0 && lanlink.connected);
	insize = size-rsz;
	recvd = (rsz<=0);
	#ifdef GBA_LOGGING
		if(systemVerbose & VERBOSE_LINK) {
			//log("%sCR01\n",(LPCTSTR)LogStr);
			log("%sCRecv(%d) Size : %d/%d  %s\n", (LPCTSTR)LogStr, peek, insize, size, (LPCTSTR)DataHex(inbuffer,size));
		}
	#endif
	return recvd;
}

BOOL lclient::WaitForData(int ms) {
	unsigned long notblock = 1; //AdamN: 0=blocking, non-zero=non-blocking
	unsigned long arg = 0;
	//fd_set fdset;
	//timeval wsocktimeout;
	MSG msg;
	int ready = 0;
	//DWORD needms = ms;
	EmuCtr++;
	if(EmuReseted) DiscardData();
	EmuReseted = false;
	DWORD starttm = GetTickCount();
	do { //Waiting for incomming data before continuing CPU execution
		SleepEx(0,true); //SleepEx(0,true); //to give time for incoming data
		if(PeekMessage(&msg, 0/*theApp.GetMainWnd()->m_hWnd*/,  0, 0, PM_NOREMOVE)) {
			if(msg.message==WM_CLOSE) AppTerminated=true; else theApp.PumpMessage(); //seems to be processing message only if it has message otherwise it halt the program
		}		
		//fdset.fd_count = 1;
		notblock = 0;
		//fdset.fd_array[0] = lanlink.tcpsocket;
		ioctlsocket(lanlink.tcpsocket, FIONBIO, &notblock); //AdamN: temporarily use blocking for reading
		int ern=WSAGetLastError();
		if(ern!=0) {
			log("clIOCTL Error: %d\n",ern);
			//if(ern==10054 || ern==10053 || ern==10057 || ern==10051 || ern==10050 || ern==10065) lanlink.connected = false;
		}
		arg = 0; //1;
		if(ioctlsocket(lanlink.tcpsocket, FIONREAD, &arg)!=0) { //AdamN: Alternative(Faster) to Select(Slower)
			int ern=WSAGetLastError(); 
			if(ern!=0) {
				log("%sCC Error: %d\n",(LPCTSTR)LogStr,ern);
				lanlink.connected = false;
				MessageBox(NULL, _T("Player 1 disconnected."), _T("Link"), MB_OK);
				break;
			}
		}
		if(arg>0) ready++;
		//wsocktimeout.tv_sec = linktimeout / 1000;
		//wsocktimeout.tv_usec = linktimeout % 1000; //0; //AdamN: remainder should be set also isn't?
		//ready = select(0, &fdset, NULL, NULL, &wsocktimeout);
		//int ern=WSAGetLastError();
		//if(ern!=0) {
		//	log("slCC Error: %d\n",ern);
		//	if(ern==10054 || ern==10053 || ern==10057 || ern==10051 || ern==10050 || ern==10065) lanlink.connected = false;
		//}
	} while (lanlink.connected && ready==0 && /*(int)*/(GetTickCount()-starttm)<(DWORD)ms && !AppTerminated && !EmuReseted); //with ms<0 might not gets a proper result?
	//if((GetTickCount()-starttm)>=(DWORD)ms) log("TimeOut:%d\n",ms);
	EmuCtr--;
	return (ready>0);
}

BOOL lclient::IsDataReady(void) {
	unsigned long arg = 0;
	if(EmuReseted) DiscardData();
	EmuReseted = false;
	if(ioctlsocket(lanlink.tcpsocket, FIONREAD, &arg)!=0) { //AdamN: Alternative(Faster) to Select(Slower)
		int ern=WSAGetLastError(); 
		if(ern!=0) {
			log("%sCC Error: %d\n",(LPCTSTR)LogStr,ern);
			lanlink.connected = false;
			MessageBox(NULL, _T("Player 1 disconnected."), _T("Link"), MB_OK);
		}
	}
	return(arg>0);
}

int lclient::DiscardData(void) {
	char buff[256];
	unsigned long arg;
	int sz = 0;
	do {
		arg = 0;
		if(ioctlsocket(lanlink.tcpsocket, FIONREAD, &arg)!=0) { //AdamN: Alternative(Faster) to Select(Slower)
			int ern=WSAGetLastError(); 
			if(ern!=0) {
				log("%sCC Error: %d\n",(LPCTSTR)LogStr,ern);
				lanlink.connected = false;
				MessageBox(NULL, _T("Player 1 disconnected."), _T("Link"), MB_OK);
			}
		}
		if(arg>0) {
			int cnt=recv(lanlink.tcpsocket, buff, min(arg,256), 0);
			if(cnt>0) sz+=cnt;
		}
	} while (arg>0 && lanlink.connected);
	return(sz);
}

BOOL LinkSendData(const char *buf, int size, int nretry) {
	BOOL sent = false;
	if(linkid) //client
		sent = lc.SendData(buf, size, nretry);
	else //server
		sent = ls.SendData(buf, size, nretry);
	return(sent);
}

BOOL LinkIsDataReady(void) {
	BOOL rdy = false;
	if(linkid) //client
		rdy = lc.IsDataReady();
	else //server
		rdy = ls.IsDataReady();
	return(rdy);
}

BOOL LinkWaitForData(int ms) {
	BOOL rdy = false;
	if(linkid) //client
		rdy = lc.WaitForData(ms);
	else //server
		rdy = ls.WaitForData(ms);
	return(rdy);
}


// Uncalled
void LinkSStop(void){
	if(!oncewait){
		if(linkid){
			if(lanlink.numgbas==1) return;
			lc.Recv();
		}
		else ls.Recv();

		oncewait = true;
		UPDATE_REG(COMM_SIODATA32_H, linkdata[1]);
		UPDATE_REG(0x124, linkdata[2]);
		UPDATE_REG(0x126, linkdata[3]);
	}
	return;
}

// ??? Called when COMM_SIODATA8 written
void LinkSSend(u16 value){
	if(linkid&&!lc.oncesend){
		linkdata[linkid] = value;
		lc.Send();
		lc.oncesend = true;
	}
}

#endif // _MSC_VER

#else // NO_LINK
void JoyBusUpdate(int ticks) {}
#endif // NO_LINK
