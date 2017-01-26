# DO NOT USE

**This is not usable. Please do not report bugs. 
Pull requests are welcome. **

#ofxSoundio

A wrapper around libsoundio for openFrameworks. 

This is meant as an alternative to the built-in RtAudio. 


# Getting started

ofxSoundio is based around the ofSoundStream class, so the transition should be very simple. 

#### ofApp.h

	#include "ofxSoundio.h"
	
	class ofApp : public ofBaseApp{
	
		void setup(); 
		
		void audioOut(); 
		void audioIn(); 
		
		ofxSoundio soundStream; 
	}; 
	
#### ofApp.cpp

	ofApp::setup(){
		// connect to the soundstream. 
		// note: the sample rate is automatically determined if not provided. 
		// buffer size and number of buffers cannot be provided. 
		soundStream.setup( this, 2, 1, /*optional: sampleRate parameter*/ ); 
	}
	
	void 
	
	
# Device changes
