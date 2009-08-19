#include <vfw.h>


// info: recreate the whole AVIWrite object if any method fails
class AVIWrite
{
public:
	AVIWrite();
	virtual ~AVIWrite();

	bool CreateAVIFile( LPCTSTR filename );
	bool CreateVideoStream( LONG imageWidth, LONG imageHeight, WORD colorBits, DWORD framesPerSecond, HWND parentWnd );
	bool CreateAudioStream( WORD channelCount, DWORD sampleRate, WORD sampleBits, HWND parentWnd );
	bool AddVideoFrame( LPVOID imageData );
	bool AddAudioFrame( LPVOID soundData );

private:
	bool m_failed;
	PAVIFILE m_file;
	PAVISTREAM m_videoStream;
	PAVISTREAM m_audioStream;
	AVICOMPRESSOPTIONS m_videoCompSettings;
	AVICOMPRESSOPTIONS m_audioCompSettings;
	PAVISTREAM m_videoCompressed;
	PAVISTREAM m_audioCompressed;
	DWORD m_frameRate;
	LONG m_frameCounter;
	LONG m_sampleCounter;
	LONG m_videoFrameSize;
	LONG m_audioFrameSize;
	WORD m_audioBlockAlign;
};
