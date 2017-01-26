#pragma once

#include "ofConstants.h"
#include "soundio.h"

#include "ofBaseSoundStream.h"
#include "ofTypes.h"
#include "ofSoundBuffer.h"

struct SoundIo;
struct SoundIoOutStream;
struct SoundIoInStream;

class ofxSoundioSoundStream : public ofBaseSoundStream{
public:
	ofxSoundioSoundStream();
	~ofxSoundioSoundStream();
	
	std::vector<ofSoundDevice> getDeviceList() const;
	void setDeviceID(int deviceID);
	void setInDeviceID(int deviceID);
	void setOutDeviceID(int deviceID);
	
	void setInput(ofBaseSoundInput * soundInput);
	void setOutput(ofBaseSoundOutput * soundOutput);
	bool setup(int outChannels, int inChannels, int sampleRate, int bufferSize, int nBuffers);
	bool setup(ofBaseApp * app, int outChannels, int inChannels, int sampleRate, int bufferSize, int nBuffers);
	
	void start();
	void stop();
	void close();
	
	long unsigned long getTickCount() const;
	
	int getNumInputChannels() const;
	int getNumOutputChannels() const;
	int getSampleRate() const;
	int getBufferSize() const;
	int getDeviceID() const;
	
private:
	long unsigned long tickCount;
	SoundIo * audio{nullptr};
	int sampleRate;
	int outDeviceID;
	int inDeviceID;
	int bufferSize;
	int nInputChannels;
	int nOutputChannels;
	ofBaseSoundInput * soundInputPtr{nullptr};
	ofBaseSoundOutput * soundOutputPtr{nullptr};
	ofSoundBuffer inputBuffer;
	ofSoundBuffer outputBuffer;
	
	SoundIoOutStream * outStream{nullptr};
	SoundIoInStream * inStream{nullptr};
};


