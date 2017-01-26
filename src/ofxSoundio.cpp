#include "ofxSoundio.h"
#include "ofConstants.h"

#include "ofSoundStream.h"
#include "ofMath.h"
#include "ofUtils.h"
#include "SoundIo.h"

static void write_callback(struct SoundIoOutStream *outstream, int frame_count_min, int frame_count_max);
static void read_callback(struct SoundIoInStream *instream, int frame_count_min, int frame_count_max);


//------------------------------------------------------------------------------
ofxSoundioSoundStream::ofxSoundioSoundStream(){
	outDeviceID		= -1;
	inDeviceID		= -1;
	soundOutputPtr	= nullptr;
	soundInputPtr	= nullptr;
	tickCount= 0;
	nOutputChannels = 0;
	nInputChannels = 0;
	bufferSize = 0;
	sampleRate = 0;
}

//------------------------------------------------------------------------------
ofxSoundioSoundStream::~ofxSoundioSoundStream(){
	stop();
	close();
}

static int getMaxNumChannels(struct SoundIoDevice * device){
	int res = 0;
	for( int i = 0; i < device->layout_count; i++){
		SoundIoChannelLayout layout = device->layouts[i];
		res = max(layout.channel_count, res);
	}
	
	return res;
}

static vector<unsigned int> getSampleRates(struct SoundIoDevice * device){
	set<unsigned int> ranges;
	for( int i = 0; i < device->sample_rate_count; i++){
		SoundIoSampleRateRange range = device->sample_rates[i];
		// throw in all common ranges to be sure we have them
		// inserting the same thing multiple times is no problem
		if( range.min <= 44100 && 44100 <= range.max ) ranges.insert(44100);
		if( range.min <= 48000 && 48000 <= range.max ) ranges.insert(48000);
		if( range.min <= 88200 && 88200 <= range.max ) ranges.insert(88200);
		if( range.min <= 96000 && 96000 <= range.max ) ranges.insert(96000);
		if( range.min <= 176400 && 176400 <= range.max ) ranges.insert(176400);
		if( range.min <= 192000 && 192000 <= range.max ) ranges.insert(192000);
		ranges.insert(range.min);
		ranges.insert(range.max);
	}

	vector<unsigned int> res(ranges.begin(), ranges.end());
	sort(res.begin(), res.end());
	return res;
}

//------------------------------------------------------------------------------
vector<ofSoundDevice> ofxSoundioSoundStream::getDeviceList() const{
	SoundIo * soundio = soundio_create();
	if(!soundio){
		cerr << "ofxSoundio: couldn't create soundio instance :(" << endl;
		return vector<ofSoundDevice>();
	}
	
	//todo: set backend manually!
	int err = soundio_connect(soundio);
	if(err){
		cerr << "ofxSoundio: couldn't connect to system audio :(" << endl;
		soundio_destroy(soundio); 
		return vector<ofSoundDevice>();
	}
	
	soundio_flush_events(soundio);
	
	int output_count = soundio_output_device_count(soundio);
	int input_count = soundio_input_device_count(soundio);
	int default_output = soundio_default_output_device_index(soundio);
	int default_input = soundio_default_input_device_index(soundio);
	
	vector<ofSoundDevice> deviceList;
	
	for (int i = 0; i < input_count; i++) {
		struct SoundIoDevice *device = soundio_get_input_device(soundio, i);
		if(device){
			ofSoundDevice dev;
			dev.deviceID = i;
			dev.name = string(device->name); // utf8
			dev.outputChannels = 0;
			dev.inputChannels = getMaxNumChannels(device);
			dev.sampleRates = getSampleRates(device);
			dev.isDefaultInput = i == default_input;
			dev.isDefaultOutput = false;
			deviceList.emplace_back(dev);
			soundio_device_unref(device);
		}
		else{
			cerr << "ofxSoundio: Couldn't probe output device #" << i << ". " << endl;
		}
	}
	
	for (int i = 0; i < output_count; i++) {
		struct SoundIoDevice *device = soundio_get_output_device(soundio, i);
		if(device){
			ofSoundDevice dev;
			dev.deviceID = i;
			dev.name = string(device->name); // utf8
			dev.outputChannels = getMaxNumChannels(device);
			dev.inputChannels = 0;
			dev.sampleRates = getSampleRates(device);
			dev.isDefaultInput = false;
			dev.isDefaultOutput = i == default_output;
			deviceList.emplace_back(dev);
			soundio_device_unref(device);
		}
		else{
			cerr << "ofxSoundio: Couldn't probe output device #" << i << ". " << endl;
		}
	}
	
	soundio_disconnect(soundio);
	soundio_destroy(soundio);
	
	return deviceList;
}

//------------------------------------------------------------------------------
void ofxSoundioSoundStream::setDeviceID(int _deviceID){
	inDeviceID = outDeviceID = _deviceID;
}

int ofxSoundioSoundStream::getDeviceID()  const{
	return inDeviceID;
}

void ofxSoundioSoundStream::setInDeviceID(int _deviceID){
	inDeviceID = _deviceID;
}

void ofxSoundioSoundStream::setOutDeviceID(int _deviceID){
	outDeviceID = _deviceID;
}

//------------------------------------------------------------------------------
void ofxSoundioSoundStream::setInput(ofBaseSoundInput * soundInput){
	soundInputPtr		= soundInput;
}

//------------------------------------------------------------------------------
void ofxSoundioSoundStream::setOutput(ofBaseSoundOutput * soundOutput){
	soundOutputPtr		= soundOutput;
}

//------------------------------------------------------------------------------
bool ofxSoundioSoundStream::setup(int outChannels, int inChannels, int _sampleRate, int _bufferSize, int nBuffers){
	if( audio != nullptr ){
		close();
	}
	
	nInputChannels		= inChannels;
	nOutputChannels		= outChannels;
	
	sampleRate			=  _sampleRate;
	tickCount			=  0;
	bufferSize			= ofNextPow2(_bufferSize);	// must be pow2
	
	audio = soundio_create();
	int err = soundio_connect(audio);
	if(err){
		soundio_destroy(audio); 
		audio = nullptr;
		return false;
	}

	audio->userdata = nullptr;
	//TODO: add watch listener
	soundio_flush_events(audio);
	
	int inputIdActive = -1;
	int outputIdActive = -1;
	
	if(nInputChannels>0){
		if( inDeviceID >= 0 ){
			inputIdActive = inDeviceID;
		}else{
			inputIdActive = soundio_default_input_device_index(audio);
		}
		
		
		struct SoundIoDevice *device = soundio_get_input_device(audio, inputIdActive);
		if(device){
			inStream = soundio_instream_create(device);
			inStream->userdata = this;
			inStream->sample_rate = sampleRate;
			//TODO: would it be better to just set the number of channels on the existing object?
			inStream->layout = *soundio_channel_layout_get_default(nInputChannels);
			inStream->read_callback = &read_callback;
			if ((err = soundio_instream_open(inStream))) {
				cerr << "ofxSoundio: unable to open input stream: " << soundio_strerror(err) << endl;
				close();
				return false;
			}
			
			soundio_device_unref(device);
		}
		else{
			cerr << "ofxSoundio: unable to open device #" << inputIdActive << endl;
			close();
			return false;
		}
	}
	
	if(nOutputChannels>0){
		if( outDeviceID >= 0 ){
			outputIdActive = outDeviceID;
		}
		else{
			outputIdActive = soundio_default_output_device_index(audio);
		}
		
		struct SoundIoDevice *device = soundio_get_output_device(audio, outputIdActive);
		if(device){
			outStream = soundio_outstream_create(device);
			outStream->userdata = this;
			outStream->sample_rate = sampleRate;
			//TODO: would it be better to just set the number of channels on the existing object?
			outStream->layout = *soundio_channel_layout_get_default(nOutputChannels);
			outStream->write_callback = &write_callback;
			if ((err = soundio_outstream_open(outStream))) {
				cerr << "ofxSoundio: unable to open output stream: " << soundio_strerror(err) << endl;
				close();
				return false;
			}
			
			soundio_device_unref(device);
		}
		else{
			cerr << "ofxSoundio: unable to open device #" << inputIdActive << endl;
			close();
			return false;
		}
	}
	
	//todo: get rid of buffersize alltogether?
	unsigned int bufferFrames = (unsigned int)bufferSize; // 256 sample frames
	
	outputBuffer.setDeviceID(outputIdActive);
	inputBuffer.setDeviceID(inputIdActive);
	
	return true;
}

//------------------------------------------------------------------------------
bool ofxSoundioSoundStream::setup(ofBaseApp * app, int outChannels, int inChannels, int sampleRate, int bufferSize, int nBuffers){
	setInput(app);
	setOutput(app);
	return setup(outChannels,inChannels,sampleRate,bufferSize,nBuffers);
}

//------------------------------------------------------------------------------
void ofxSoundioSoundStream::start(){
	setup(nOutputChannels, nInputChannels, sampleRate, bufferSize, 0);
}

//------------------------------------------------------------------------------
void ofxSoundioSoundStream::stop(){
	if( audio == nullptr ) return;
	
	try {
		if(audio->isStreamRunning()) {
			audio->stopStream();
		}
	} catch (std::exception &error) {
		ofLogError() << error.what();
	}
}

//------------------------------------------------------------------------------
void ofxSoundioSoundStream::close(){
	if( audio == nullptr ) return;
	
	try {
		if(audio->isStreamOpen()) {
			audio->closeStream();
		}
	} catch (std::exception &error) {
		ofLogError() << error.what();
	}
	soundOutputPtr	= nullptr;
	soundInputPtr	= nullptr;
	audio.reset();	// delete
}

//------------------------------------------------------------------------------
long unsigned long ofxSoundioSoundStream::getTickCount() const{
	return tickCount;
}

//------------------------------------------------------------------------------
int ofxSoundioSoundStream::getNumInputChannels() const{
	return nInputChannels;
}

//------------------------------------------------------------------------------
int ofxSoundioSoundStream::getNumOutputChannels() const{
	return nOutputChannels;
}

//------------------------------------------------------------------------------
int ofxSoundioSoundStream::getSampleRate() const{
	return sampleRate;
}

//------------------------------------------------------------------------------
int ofxSoundioSoundStream::getBufferSize() const{
	return bufferSize;
}

//------------------------------------------------------------------------------
int ofxSoundioSoundStream::SoundIoCallback(void *outputBuffer, void *inputBuffer, unsigned int nFramesPerBuffer, double streamTime, SoundIoStreamStatus status, void *data){
	ofxSoundioSoundStream * rtStreamPtr = (ofxSoundioSoundStream *)data;
	
	if ( status ) {
		ofLogWarning("ofxSoundioSoundStream") << "stream over/underflow detected";
	}
	
	// 	SoundIo uses a system by which the audio
	// 	can be of different formats
	// 	char, float, etc.
	// 	we choose float
	float * fPtrOut = (float *)outputBuffer;
	float * fPtrIn  = (float *)inputBuffer;
	// [zach] memset output to zero before output call
	// this is because of how SoundIo works: duplex w/ one callback
	// you need to cut in the middle. if the simpleApp
	// doesn't produce audio, we pass silence instead of duplex...
	
	unsigned int nInputChannels = rtStreamPtr->getNumInputChannels();
	unsigned int nOutputChannels = rtStreamPtr->getNumOutputChannels();
	
	if(nInputChannels > 0){
		if( rtStreamPtr->soundInputPtr != nullptr ){
			rtStreamPtr->inputBuffer.copyFrom(fPtrIn, nFramesPerBuffer, nInputChannels, rtStreamPtr->getSampleRate());
			rtStreamPtr->inputBuffer.setTickCount(rtStreamPtr->tickCount);
			rtStreamPtr->soundInputPtr->audioIn(rtStreamPtr->inputBuffer);
		}
		// [damian] not sure what this is for? assuming it's for underruns? or for when the sound system becomes broken?
		memset(fPtrIn, 0, nFramesPerBuffer * nInputChannels * sizeof(float));
	}
	
	if (nOutputChannels > 0) {
		if( rtStreamPtr->soundOutputPtr != nullptr ){
			
			if ( rtStreamPtr->outputBuffer.size() != nFramesPerBuffer*nOutputChannels || rtStreamPtr->outputBuffer.getNumChannels()!=nOutputChannels ){
				rtStreamPtr->outputBuffer.setNumChannels(nOutputChannels);
				rtStreamPtr->outputBuffer.resize(nFramesPerBuffer*nOutputChannels);
			}
			rtStreamPtr->outputBuffer.setTickCount(rtStreamPtr->tickCount);
			rtStreamPtr->soundOutputPtr->audioOut(rtStreamPtr->outputBuffer);
		}
		rtStreamPtr->outputBuffer.copyTo(fPtrOut, nFramesPerBuffer, nOutputChannels,0);
		rtStreamPtr->outputBuffer.set(0);
	}
	
	// increment tick count
	rtStreamPtr->tickCount++;
	
	return 0;
}
