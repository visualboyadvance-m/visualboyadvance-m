#include "stdafx.h"

#include "AVIWrite.h"
#pragma comment( lib, "Vfw32" )


AVIWrite::AVIWrite()
{
	m_failed = false;
	m_file = NULL;
	m_videoStream = NULL;
	m_audioStream = NULL;
	ZeroMemory( &m_videoCompSettings, sizeof( m_videoCompSettings ) );
	ZeroMemory( &m_audioCompSettings, sizeof( m_audioCompSettings ) );
	m_videoCompressed = NULL;
	m_audioCompressed = NULL;
	m_frameRate = 0;
	m_frameCounter = 0;
	m_sampleCounter = 0;
	m_videoFrameSize = 0;
	m_audioFrameSize = 0;
	m_audioBlockAlign = 0;

	AVIFileInit();
}


AVIWrite::~AVIWrite()
{
	if( m_audioCompressed ) {
		AVIStreamRelease( m_audioCompressed );
	}

	if( m_audioStream ) {
		AVIStreamRelease( m_audioStream );
	}

	if( m_videoCompressed ) {
		AVIStreamRelease( m_videoCompressed );
	}

	if( m_videoStream ) {
		AVIStreamRelease( m_videoStream );
	}

	if( m_file ) {
		AVIFileRelease( m_file );
	}

	AVIFileExit();
}


bool AVIWrite::CreateAVIFile( LPCTSTR filename )
{
	if( m_file || m_failed ) return false;

	HRESULT err = 0;

	// -- create the AVI file --
	err = AVIFileOpen(
		&m_file,
		filename,
		OF_CREATE | OF_WRITE | OF_SHARE_EXCLUSIVE,
		NULL
		);

	if( FAILED( err ) ) {
		m_failed = true;
		return false;
	}

	return true;
}


// colorBits: 16, 24 or 32
bool AVIWrite::CreateVideoStream( LONG imageWidth, LONG imageHeight, WORD colorBits, DWORD framesPerSecond, HWND parentWnd )
{
	if( m_videoStream || m_failed ) return false;

	HRESULT err = 0;
	AVISTREAMINFO videoInfo;
	BITMAPINFOHEADER bitmapInfo;
	AVICOMPRESSOPTIONS *settings[1];
	ZeroMemory( &videoInfo, sizeof( videoInfo ) );
	ZeroMemory( &bitmapInfo, sizeof( bitmapInfo ) );
	settings[0] = &m_videoCompSettings;

	// -- initialize the video stream information --
	videoInfo.fccType = streamtypeVIDEO;
	videoInfo.fccHandler = 0;
	videoInfo.dwScale = 1;
	videoInfo.dwRate = framesPerSecond;
	videoInfo.dwSuggestedBufferSize = imageWidth * imageHeight * ( colorBits >> 3 );

	// -- create the video stream --
	err = AVIFileCreateStream(
		m_file,
		&m_videoStream,
		&videoInfo
		);

	if( FAILED( err ) ) {
		m_failed = true;
		return false;
	}


	// -- ask for compression settings --
	if( AVISaveOptions(
		parentWnd,
		0,
		1,
		&m_videoStream,
		settings ) )
	{
		err = AVIMakeCompressedStream(
			&m_videoCompressed,
			m_videoStream,
			settings[0],
			NULL
			);

		AVISaveOptionsFree( 1, settings );
		if( FAILED( err ) ) {
			m_failed = true;
			return false;
		}
	} else {
		AVISaveOptionsFree( 1, settings );
		m_failed = true;
		return false;
	}


	// -- initialize the video stream format --
	bitmapInfo.biSize = sizeof( bitmapInfo );
	bitmapInfo.biWidth = imageWidth;
	bitmapInfo.biHeight = imageHeight;
	bitmapInfo.biBitCount = colorBits;
	bitmapInfo.biPlanes = 1;
	bitmapInfo.biCompression = BI_RGB;
	bitmapInfo.biSizeImage = imageWidth * imageHeight * ( colorBits >> 3 );

	// -- set the video stream format --
	err = AVIStreamSetFormat(
		m_videoCompressed,
		0,
		&bitmapInfo,
		bitmapInfo.biSize + ( bitmapInfo.biClrUsed * sizeof( RGBQUAD ) )
		);

	if( FAILED( err ) ) {
		m_failed = true;
		return false;
	}


	m_frameRate = framesPerSecond;
	m_videoFrameSize = imageWidth * imageHeight * ( colorBits >> 3 );

	return true;
}


// call AddVideoStream() first
// channelCount: max. 2
// sampleBits: max. 16
bool AVIWrite::CreateAudioStream( WORD channelCount, DWORD sampleRate, WORD sampleBits, HWND parentWnd )
{
	if( m_audioStream || m_failed ) return false;

	HRESULT err = 0;
	AVISTREAMINFO audioInfo;
	WAVEFORMATEX waveInfo;
	ZeroMemory( &audioInfo, sizeof( audioInfo ) );
	ZeroMemory( &waveInfo, sizeof( waveInfo ) );

	// -- initialize the audio stream information --
	audioInfo.fccType = streamtypeAUDIO;
	audioInfo.dwQuality = (DWORD)-1;
	audioInfo.dwScale = channelCount * ( sampleBits >> 3 );
	audioInfo.dwRate = channelCount * ( sampleBits >> 3 ) * sampleRate;
	audioInfo.dwInitialFrames = 1;
	audioInfo.dwSampleSize = channelCount * ( sampleBits >> 3 );
	audioInfo.dwSuggestedBufferSize = 0;

	// -- create the audio stream --
	err = AVIFileCreateStream(
		m_file,
		&m_audioStream,
		&audioInfo
		);

	if( FAILED( err ) ) {
		m_failed = true;
		return false;
	}


	// -- initialize the audio stream format --
	waveInfo.wFormatTag = WAVE_FORMAT_PCM;
	waveInfo.nChannels = channelCount;
	waveInfo.nSamplesPerSec = sampleRate;
	waveInfo.nAvgBytesPerSec = channelCount * ( sampleBits >> 3 ) * sampleRate;
	waveInfo.nBlockAlign = channelCount * ( sampleBits >> 3 );
	waveInfo.wBitsPerSample = sampleBits;
	waveInfo.cbSize = 0;

	// -- set the audio stream format --
	err = AVIStreamSetFormat(
		m_audioStream,
		0,
		&waveInfo,
		sizeof( waveInfo )
		);

	if( FAILED( err ) ) {
		m_failed = true;
		return false;
	}

	m_audioBlockAlign = channelCount * ( sampleBits >> 3 );
	m_audioFrameSize = channelCount * ( sampleBits >> 3 ) * ( sampleRate / m_frameRate );

	return true;
}


bool AVIWrite::AddVideoFrame( LPVOID imageData )
{
	if( !m_videoStream || m_failed ) return false;

	HRESULT err = 0;

	err = AVIStreamWrite(
		m_videoCompressed,
		m_frameCounter,
		1,
		imageData,
		m_videoFrameSize,
		AVIIF_KEYFRAME,
		NULL,
		NULL
		);

	if( FAILED( err ) ) {
		m_failed = true;
		return false;
	}

	m_frameCounter++;

	return true;
}


bool AVIWrite::AddAudioFrame( LPVOID soundData )
{
	if( !m_audioStream || m_failed ) return false;

	HRESULT err = 0;

	err = AVIStreamWrite(
		m_audioStream,
		m_sampleCounter,
		m_audioFrameSize / m_audioBlockAlign,
		soundData,
		m_audioFrameSize,
		0,
		NULL,
		NULL
		);

	if( FAILED( err ) ) {
		m_failed = true;
		return false;
	}

	m_sampleCounter += m_audioFrameSize / m_audioBlockAlign;

	return true;
}
