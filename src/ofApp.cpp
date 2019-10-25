#include "ofApp.h"

/*
    If you are struggling to get the device to connect ( especially Windows Users )
    please look at the ReadMe: in addons/ofxKinect/README.md
*/

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetLogLevel(OF_LOG_VERBOSE);
	
	// enable depth->video image calibration
	kinect.setRegistration(true);
    
	kinect.init();
	//kinect.init(true); // shows infrared instead of RGB video image
	//kinect.init(false, false); // disable video image (faster fps)
	
	kinect.open();		// opens first available kinect
	//kinect.open(1);	// open a kinect by id, starting with 0 (sorted by serial # lexicographically))
	//kinect.open("A00362A08602047A");	// open a kinect using it's unique serial #
	
	// print the intrinsic IR sensor values
	if(kinect.isConnected()) {
		ofLogNotice() << "sensor-emitter dist: " << kinect.getSensorEmitterDistance() << "cm";
		ofLogNotice() << "sensor-camera dist:  " << kinect.getSensorCameraDistance() << "cm";
		ofLogNotice() << "zero plane pixel size: " << kinect.getZeroPlanePixelSize() << "mm";
		ofLogNotice() << "zero plane dist: " << kinect.getZeroPlaneDistance() << "mm";
	}
	
#ifdef USE_TWO_KINECTS
	kinect2.init();
	kinect2.open();
#endif
	
	colorImg.allocate(kinect.width, kinect.height);
	grayImage.allocate(kinect.width, kinect.height);
	grayThreshNear.allocate(kinect.width, kinect.height);
	grayThreshFar.allocate(kinect.width, kinect.height);
	
	nearThreshold = 230;
	farThreshold = 150;
	bThreshWithOpenCV = true;
	
	ofSetFrameRate(60);
	
	// zero the tilt on startup
	angle = 28;
	kinect.setCameraTiltAngle(angle);
	
	// start state
	state = "setup";
	mirror = true;
	level = 6;
	updateNum = 0;
	numBullets = 1;
	untilMoreBullets = 10;
	colission = false;
	regen = 0;
	high_score = 0;
	total_score = 0;
	games = 0;

	///////////////////////// IMPORT SPRITES ///////////////////////////
	sprites["player"].loadImage("heart.png");
	sprites["player"].setAnchorPercent(0.5, 0.5);

	sprites["red"].loadImage("red_bullet.png");
	sprites["yellow"].loadImage("yellow_bullet.png");
	sprites["green"].loadImage("green_bullet.png");
	sprites["cyan"].loadImage("cyan_bullet.png");
	sprites["blue"].loadImage("blue_bullet.png");
	sprites["pink"].loadImage("pink_bullet.png");
	sprites["life"].loadImage("life.png");
	//sprites["tracker"].loadImage("tracker.png");

	//////////////////////// IMPORT AUDIO ////////////////////////
	song = "";

	//heartbreak sound - when player gets hit
	audio["heartbreak"].load("heartbreak.mp3");

	//Tohou Music
	int offset = 0;
	for (int i = 1+offset; i <= 10+offset; i++)
	{
		cout << "Loading song " << i-offset << "/10 ..." << endl;
		string file = "T_" + std::to_string(i) + ".mp3";
		audio["T_" + std::to_string(i)].load(file);
	}
	cout << "Done!" << endl;

	//////////////////////////// FONTS ////////////////////
	font.load("verdana.ttf", 30, true, true);
	font.setLineHeight(34.0f);
	font.setLetterSpacing(1.035);
}

//--------------------------------------------------------------
void ofApp::update() {

	if (state != "game")
	{
		ofBackground(100, 100, 100);
	}

	else
	{
		if(!colission)
			ofBackground(0, 0, 0);
		else
		{
			ofBackground(regen*2, 0, 0);
			regen--;
			if (regen < 0)
			{
				colission = false;
				regen = -1;
			}
		}

		if (player.isPlaying())
		{
			updateNum++;
			if (updateNum % 20 == 0)
			{
				for (int i = 0; i < numBullets; i++)
				{
					//cout << "spawn bullet " << endl;
					bullets.push_back(Bullet((int)ofRandom(3), level));
					if (level > 20)
					{
						if (untilMoreBullets < 0)
						{
							numBullets++;
							if (numBullets > 10)
								numBullets = 10;
							cout << numBullets << endl;
							untilMoreBullets = 10;
						}
						else
							untilMoreBullets--;
					}
				}
				level++;
				player.score();
			}
			if(updateNum % 200 == 0)
				bullets.push_back(Bullet((int)ofRandom(3), level, player.getPos()));
			if (updateNum == 1000)
				updateNum == 0;
			//////////////// UPDATE BULLETS ////////////////
			if (player.isPlaying())
			{

				for (int i = 0; i < bullets.size(); i++)
				{
					bullets[i].update();

					if (bullets[i].collision(player.getPos()) && regen <= 0)
					{
						if (bullets[i].getColor() != "life")
						{
							cout << "colission" << endl;

							player.collision();
							colission = true;
							regen = 50;
							level -= 10;
							if (level < 6)
								level = 6;
							numBullets = 1;
						}
						else
						{
							player.addLife();
							bullets.erase(bullets.begin()+i);
						}
					}
					if (bullets[i].outOfBounds())
					{
						bullets.erase(bullets.begin() + i);
					}
					audio[song].setVolume((50-regen)/50.0);
					if (regen == 48)
					{
						audio["heartbreak"].play();
						audio["heartbreak"].setPositionMS(2000);
					}
				}
			}
		}
		if (!player.isPlaying() && !bullets.empty())
			bullets.clear();
	}
	
	kinect.update();
	
	// there is a new frame and we are connected
	if(kinect.isFrameNew()) {
		
		// load grayscale depth image from the kinect source
		grayImage.setFromPixels(kinect.getDepthPixels());
		
		// we do two thresholds - one for the far plane and one for the near plane
		// we then do a cvAnd to get the pixels which are a union of the two thresholds
		if(bThreshWithOpenCV) {
			grayThreshNear = grayImage;
			grayThreshFar = grayImage;
			grayThreshNear.threshold(nearThreshold, true);
			grayThreshFar.threshold(farThreshold);
			cvAnd(grayThreshNear.getCvImage(), grayThreshFar.getCvImage(), grayImage.getCvImage(), NULL);
		} else {
			
			// or we do it ourselves - show people how they can work with the pixels
			ofPixels & pix = grayImage.getPixels();
			int numPixels = pix.size();
			for(int i = 0; i < numPixels; i++) {
				if(pix[i] < nearThreshold && pix[i] > farThreshold) {
					pix[i] = 255;
				} else {
					pix[i] = 0;
				}
			}
		}
		
		// update the cv images
		grayImage.flagImageChanged();
		
		// find contours which are between the size of 20 pixels and 1/3 the w*h pixels.
		// also, find holes is set to true so we will get interior contours as well....
		contourFinder.findContours(grayImage, 10, (kinect.width*kinect.height)/2, 20, false);
	}
	
#ifdef USE_TWO_KINECTS
	kinect2.update();
#endif
}

//--------------------------------------------------------------
void ofApp::draw() {
	
	ofSetColor(255, 255, 255);
	
	if (state == "game")
	{
		int max_area = 0;
		for (int i = 0; i < contourFinder.blobs.size(); i++)
		{
			if (contourFinder.blobs[i].area > max_area)
			{
				max_area = contourFinder.blobs[i].area;
			}
		}
		for (int i = 0; i < contourFinder.blobs.size(); i++)
		{
			if (max_area > 0 && contourFinder.blobs[i].area == max_area)
			{
				int x = contourFinder.blobs[i].centroid.x;
				int y = contourFinder.blobs[i].centroid.y;

				int kinectMax = 500;
				int kinectMin = 20;
				int slope = (ofGetWidth() - 0) / (kinectMax - kinectMin);
				x = 0 + slope * (x - kinectMin);


				kinectMax = 350;
				kinectMin = 200;
				slope = (ofGetHeight() - 0) / (kinectMax - kinectMin);
				y = 0 + slope * (y - kinectMin);


				if (x > ofGetWidth() - sprites["player"].getWidth())
					x = ofGetWidth() - sprites["player"].getWidth();
				if (y > ofGetHeight() - sprites["player"].getHeight())
					y = ofGetHeight() - sprites["player"].getHeight();

				if (x < sprites["player"].getWidth())
					x = sprites["player"].getWidth();
				if (y < sprites["player"].getHeight())
					y = sprites["player"].getHeight();

				//cout << ofGetWidth() << ", " << x << endl;

				if (mirror)
				{
					x = ofGetWidth() - x;
				}

				player.update(ofVec3f(x, y, 0));
				if (song != "")
					audio[song].setPan(x / (float)ofGetWidth());
			}
			//Reset Box
			if (!player.isPlaying())
			{
				ofSetRectMode(OF_RECTMODE_CENTER);
				ofDrawRectangle(ofGetWidth() / 2, ofGetHeight() / 2, 50, 50);
				ofSetRectMode(OF_RECTMODE_CORNER);
			}
			////////////////////////////////////// DRAW PLAYER ///////////////////////////////
			ofSetRectMode(OF_RECTMODE_CENTER);
			player.draw(sprites["player"]); 
			ofSetRectMode(OF_RECTMODE_CORNER);
		}
		////////////////////////////// DRAW BULLETS/////////////////////////////
		if (player.isPlaying())
		{
			ofSetRectMode(OF_RECTMODE_CENTER);
			for (int i = 0; i < bullets.size(); i++)
			{
				bullets[i].draw(sprites[bullets[i].getColor()]);
			}
			ofSetRectMode(OF_RECTMODE_CORNER);
		}
	}
	else if(state == "pointcloud")
	{
		easyCam.begin();
		drawPointCloud();
		easyCam.end();
	}
	else 
	{
		// draw from the live kinect
		kinect.drawDepth(10, 10, 400, 300);
		kinect.draw(420, 10, 400, 300);
		
		grayImage.draw(10, 320, 400, 300);
		contourFinder.draw(10, 320, 400, 300);
		int max_area = 0;
		for (int i = 0; i < contourFinder.blobs.size(); i++)
		{
			if (contourFinder.blobs[i].area > max_area)
			{
				max_area = contourFinder.blobs[i].area;
			}
		}
		for (int i = 0; i < contourFinder.blobs.size(); i++)
		{
			if (max_area > 0 && contourFinder.blobs[i].area == max_area)
			{
				cout << contourFinder.blobs[i].centroid << endl;
			}
		}
		
		
#ifdef USE_TWO_KINECTS
		kinect2.draw(420, 320, 400, 300);
#endif
	}
	
	// draw instructions
	ofSetColor(255, 255, 255);
	stringstream reportStream;
        
    if(kinect.hasAccelControl()) {
        reportStream << "accel is: " << ofToString(kinect.getMksAccel().x, 2) << " / "
        << ofToString(kinect.getMksAccel().y, 2) << " / "
        << ofToString(kinect.getMksAccel().z, 2) << endl;
    } else {
        reportStream << "Note: this is a newer Xbox Kinect or Kinect For Windows device," << endl
		<< "motor / led / accel controls are not currently supported" << endl << endl;
    }
    
	reportStream << "press p to switch between images and point cloud, rotate the point cloud with the mouse" << endl
	<< "using opencv threshold = " << bThreshWithOpenCV <<" (press spacebar)" << endl
	<< "set near threshold " << nearThreshold << " (press: + -)" << endl
	<< "set far threshold " << farThreshold << " (press: < >) num blobs found " << contourFinder.nBlobs
	<< ", fps: " << ofGetFrameRate() << endl
	<< "press c to close the connection and o to open it again, connection is: " << kinect.isConnected() << endl;

    if(kinect.hasCamTiltControl()) {
    	reportStream << "press UP and DOWN to change the tilt angle: " << angle << " degrees" << endl
        << "press 1-5 & 0 to change the led mode" << endl;
    }
    
	if(state != "game")
		ofDrawBitmapString(reportStream.str(), 20, 652);
	else if(player.isPlaying())
	{
		// draw game info
		ofSetColor(255, 255, 255);
		stringstream gameStream;

		/*gameStream << "Score: " << player.getScore() << endl;
		gameStream << "Life: " << player.getLife() << endl;*/
		font.drawString("Score: " + std::to_string(player.getScore()), 10, ofGetHeight()-200);
		font.drawString("Life: " + std::to_string(player.getLife()), 10, ofGetHeight()-100);

		ofDrawBitmapString(gameStream.str(), 20, 652);
	}
	else
	{
		if (song != "")
		{
			font.drawString("Game Over", ofGetWidth() / 2 - 100, 29);
			font.drawString("Score: " + std::to_string(player.getScore()), ofGetWidth() / 2 - 100, 329);
			if (player.getScore() > high_score)
			{
				high_score = player.getScore();
			}
			font.drawString("High Score: " + std::to_string(high_score), ofGetWidth() / 2 - 200, 129);

			total_score += player.getScore();
			games++;
			font.drawString("AVG Score: " + std::to_string(total_score/games), ofGetWidth() / 2 - 200, 229);
		}
		font.drawString("Move Over Box to Start", ofGetWidth() / 2 - 240, ofGetHeight()/2 + 100);
		font.drawString("Circle = DEATH, Star = LIFE", ofGetWidth() / 2 - 260, ofGetHeight() / 2 + 200);
		font.drawString("Simple? ;-)", ofGetWidth() / 2 - 210, ofGetHeight() / 2 + 300);


		if (player.getPos().x > ofGetWidth()/2 - 25 && player.getPos().x < ofGetWidth()/2 + 25)
		{
			if (player.getPos().y > ofGetHeight()/2 - 25 && player.getPos().y < ofGetHeight()/2 + 25)
			{
				if (state == "game" && !player.isPlaying())
				{
					if (song != "" && audio[song].isPlaying())
					{
						audio[song].stop();
					}
					song = "T_" + std::to_string((int)ofRandom(10) + 1);
					audio[song].play();

					level = 6;
					updateNum = 0;
					numBullets = 1;
					untilMoreBullets = 10;
					colission = false;
					regen = 0;

					player.reset();
				}
			}
		}

	}
}

void ofApp::drawPointCloud() {
	int w = 640;
	int h = 480;
	ofMesh mesh;
	mesh.setMode(OF_PRIMITIVE_POINTS);
	int step = 2;
	for(int y = 0; y < h; y += step) {
		for(int x = 0; x < w; x += step) {
			if(kinect.getDistanceAt(x, y) > 0) {
				mesh.addColor(kinect.getColorAt(x,y));
				mesh.addVertex(kinect.getWorldCoordinateAt(x, y));
			}
		}
	}
	glPointSize(3);
	ofPushMatrix();
	// the projected points are 'upside down' and 'backwards' 
	ofScale(1, -1, -1);
	ofTranslate(0, 0, -1000); // center the points a bit
	ofEnableDepthTest();
	mesh.drawVertices();
	ofDisableDepthTest();
	ofPopMatrix();
}

//--------------------------------------------------------------
void ofApp::exit() {
	kinect.setCameraTiltAngle(0); // zero the tilt on exit
	kinect.close();
	
#ifdef USE_TWO_KINECTS
	kinect2.close();
#endif
}

//--------------------------------------------------------------
void ofApp::keyPressed (int key) {
	switch (key) {

		case ' ':
			bThreshWithOpenCV = !bThreshWithOpenCV;
			break;

		case OF_KEY_RETURN:
			if (state == "game" && !player.isPlaying())
			{
				if (song != "" && audio[song].isPlaying())
				{
					audio[song].stop();
				}
				song = "T_" + std::to_string((int)ofRandom(10) + 1);
				audio[song].play();
				
				level = 6;
				updateNum = 0;
				numBullets = 1;
				untilMoreBullets = 10;
				colission = false;
				regen = 0;

				player.reset();
			}
			break;

		case 'g':
			if (state == "game")
				state = "setup";
			else
				state = "game";
			break;
			
		case'p':
			if (state == "pointcloud")
				state = "setup";
			else
				state = "pointcloud";
			break;

		case 'm':
			mirror = !mirror;
			break;
			
		case '>':
		case '.':
			farThreshold ++;
			if (farThreshold > 255) farThreshold = 255;
			break;
			
		case '<':
		case ',':
			farThreshold --;
			if (farThreshold < 0) farThreshold = 0;
			break;
			
		case '+':
		case '=':
			nearThreshold ++;
			if (nearThreshold > 255) nearThreshold = 255;
			break;
			
		case '-':
			nearThreshold --;
			if (nearThreshold < 0) nearThreshold = 0;
			break;
			
		case 'w':
			kinect.enableDepthNearValueWhite(!kinect.isDepthNearValueWhite());
			break;
			
		case 'o':
			kinect.setCameraTiltAngle(angle); // go back to prev tilt
			kinect.open();
			break;
			
		case 'c':
			kinect.setCameraTiltAngle(0); // zero the tilt
			kinect.close();
			break;
			
		case '1':
			kinect.setLed(ofxKinect::LED_GREEN);
			break;
			
		case '2':
			kinect.setLed(ofxKinect::LED_YELLOW);
			break;
			
		case '3':
			kinect.setLed(ofxKinect::LED_RED);
			break;
			
		case '4':
			kinect.setLed(ofxKinect::LED_BLINK_GREEN);
			break;
			
		case '5':
			kinect.setLed(ofxKinect::LED_BLINK_YELLOW_RED);
			break;
			
		case '0':
			kinect.setLed(ofxKinect::LED_OFF);
			break;
			
		case OF_KEY_UP:
			angle++;
			if(angle>30) angle=30;
			kinect.setCameraTiltAngle(angle);
			break;
			
		case OF_KEY_DOWN:
			angle--;
			if(angle<-30) angle=-30;
			kinect.setCameraTiltAngle(angle);
			break;
	}
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button)
{
	
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button)
{

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{

}