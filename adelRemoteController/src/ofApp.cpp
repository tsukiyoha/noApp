#include "ofApp.h"

using namespace std;
using namespace cv;

//-------------- Estimator parameters
// Path to trees
string g_treepath;
// Number of trees
int g_ntrees;
// Patch width
int g_p_width;
// Patch height
int g_p_height;
//maximum distance form the sensor - used to segment the person
int g_max_z;
//head threshold - to classify a cluster of votes as a head
int g_th;
//threshold for the probability of a patch to belong to a head
float g_prob_th;
//threshold on the variance of the leaves
float g_maxv;
//stride (how densely to sample test patches - increase for higher speed)
int g_stride;
//radius used for clustering votes into possible heads
float g_larger_radius_ratio;
//radius used for mean shift
float g_smaller_radius_ratio;
//pointer to the actual estimator
CRForestEstimator* g_Estimate;
//input 3D image
Mat g_im3D;
// estimator trees properly loaded
bool bTreesLoaded = false;
//-------------------------------------------------------
// I copied this code from the library demo code
// this kind of nomenclature is unknown to me
std::vector< cv::Vec<float,POSE_SIZE> > g_means; //outputs
std::vector< std::vector< Vote > > g_clusters; //full clusters of votes
std::vector< Vote > g_votes; //all votes returned by the forest

//-------------------------------------------------------
// kinect motor tilt angle
int kTilt = 0;
// stuff to calculate average fpg
float kFPS = 0;
float avgkFPS = 0;
int frameCount = 0;
int lastMillis = 0;
// draw / hide poincloud
bool bDrawCloud = true;

//--------------------------------------------------------------
void ofApp::setup(){

    ofSetVerticalSync(true);
    glEnable(GL_DEPTH_TEST);
    ofSetFrameRate(30);
    //ofBackground(0,0,0); // black
    ofBackground(255,255,255); // white

    // window elements
    verdana14Font.load("verdana.ttf", 14, true, true);
    verdana14Font.setLineHeight(18.0f);
    verdana14Font.setLetterSpacing(1.037);


    // setup gui from ofxGui
    _gui.setup("Gui");
    _gui.setPosition(0 , 0);
    _gui.minimizeAll();

    //setup vision
    w = 640;
    h = 480;

//    movie.initGrabber(w, h, true);
//    //reserve memory for cv images
    rgb.allocate(w, h);
    hsb.allocate(w, h);
    hue.allocate(w, h);
    sat.allocate(w, h);
    bri.allocate(w, h);
    filtered.allocate(w, h);
    findHue = 0.0;
//    faceTracker.setup("haarcascade_frontalface_default.xml");
//    faceTracker.setPreset(ObjectFinder::Fast);

    // kinect
    kinect.setRegistration(true);
    kinect.init();
    kinect.open();
    kinect.setDepthClipping(500,g_max_z);
    // setup the estimator
    setupEstimator();


    //setup arbotix
    fArbotixPortName ="";
    fArbotixRate=0;
    fArduinoPortName ="";
    fArduinoRate=0;
    fTrackHead = false;
    fMotorsEnabled = false;

    arbotix = new arbotixController();
    loadArbotixConfiguration("arbotixConfig.xml");


    for (int i=0;i<kNbOfServos;i++)
    {
        //arbotix->attachServo(i);
    }

    printf("setup servos % i %i",fServosNames.size(),fServosIds.size());


    servo1.setController(arbotix);
    servo1.setName("servo1");
    servo1.setId(1);
    servo1.setSpeed(fServosSpeeds[0]); // 256

    servo2.setController(arbotix);
    servo2.setName("servo2");
    servo2.setId(2);
    servo2.setSpeed(fServosSpeeds[1]); //50

    servo3.setController(arbotix);
    servo3.setName("servo3");
    servo3.setId(3);
    servo3.setSpeed(fServosSpeeds[2]); //85

    servo4.setController(arbotix);
    servo4.setName("servo4");
    servo4.setId(4);
    servo4.setSpeed(fServosSpeeds[3]);

    servo5.setController(arbotix);
    servo5.setName("servo5");
    servo5.setId(5);
    servo5.setSpeed(fServosSpeeds[4]);

    printf("setup servos done");


    //setup Ossia
    printf("setup Ossia");
    fOssia.setup();
    //fOssia.setup("OSCQuery", "ofxOssiaDevice", 9000, 5678);
    fOssiaAngleControl1.setup(fOssia.get_root_node(), "Control 1");
    fOssiaAngleControl2.setup(fOssia.get_root_node(), "Control 2");
    fOssiaAngleControl3.setup(fOssia.get_root_node(), "Control 3");
    fOssiaAngleControl4.setup(fOssia.get_root_node(), "Control 4");
    fOssiaAngleControl5.setup(fOssia.get_root_node(), "Control 5");

    fOssiaHeadPositionControl.setup(fOssia.get_root_node(), "Head");
    fOssiaHeadPositionX.setup(fOssiaHeadPositionControl,"X",w/2,0,w);
    fOssiaHeadPositionY.setup(fOssiaHeadPositionControl,"Y",h/2,0,h);


    float initialPosValue ;

    fInitialPosServo1 = ofMap(fServosInitialPos[0], fServosMins[0], fServosMax[0],0.,1.);
    printf("set initial pos to %f\n",initialPosValue);
    fOssiaAngleServo1.setup(fOssiaAngleControl1, servo1.getName(), fInitialPosServo1, 0., 1.);

    fInitialPosServo2 = ofMap(fServosInitialPos[1], fServosMins[1], fServosMax[1],0.,1.);
    fOssiaAngleServo2.setup(fOssiaAngleControl2, servo2.getName(), fInitialPosServo2, 0., 1.);

    fInitialPosServo3 = ofMap(fServosInitialPos[2], fServosMins[2], fServosMax[2],0.,1.);
    fOssiaAngleServo3.setup(fOssiaAngleControl3, servo3.getName(), fInitialPosServo3, 0., 1.);

    fInitialPosServo4 = ofMap(fServosInitialPos[3], fServosMins[3], fServosMax[3],0.,1.);
    fmeanHeadPositionY = fServosMins[3];
    fOssiaAngleServo4.setup(fOssiaAngleControl4, servo4.getName(), fInitialPosServo4, 0., 1.);

    fInitialPosServo5 = ofMap(fServosInitialPos[4], fServosMins[4], fServosMax[4],0.,1.);
    fmeanHeadPositionX = fServosMins[4];
    fOssiaAngleServo5.setup(fOssiaAngleControl5, servo5.getName(), fInitialPosServo5, 0., 1.);

    servo1.setOssiaParams(fOssia.get_root_node(), "servo1");
    servo2.setOssiaParams(fOssia.get_root_node(), "servo2");
    servo3.setOssiaParams(fOssia.get_root_node(), "servo3");
    servo4.setOssiaParams(fOssia.get_root_node(), "servo4");
    servo5.setOssiaParams(fOssia.get_root_node(), "servo5");


    // create a control zone named servo x with root node as parent
    servo1.setup(fServosMins[0], fServosMax[0]);
    servo2.setup(fServosMins[1], fServosMax[1]);
    servo3.setup(fServosMins[2], fServosMax[2]);
    servo4.setup(fServosMins[3], fServosMax[3]);
    servo5.setup(fServosMins[4], fServosMax[4]);


    //ofSetLogLevel(OF_LOG_VERBOSE);


    arbotix->connectController(fArbotixPortName,fArbotixRate);

    _gui.add (fOssia.get_root_node());

    cas = 2;
    objectDetectionStartTime = clock();
}


void ofApp::setupArduino(const int &version)
{
    ofRemoveListener(ard.EInitialized, this, &ofApp::setupArduino);
    printf("received\n");
    printf("get infos\n");
    ofLogNotice() << ard.getFirmwareName();
    ofLogNotice() << "firmata v" << ard.getMajorFirmwareVersion() << "." << ard.getMinorFirmwareVersion();
    printf("done\n");

    printf("Setup done\n");

}

//--------------------------------------------------------------
void ofApp::update(){

    int elapsedTime = ofGetElapsedTimeMillis();
    //ofLogNotice() << "Elapsed time : " <<  elapsedTime ;

//    movie.update();


    //--------------------------- VISON --------------------------

    // kinext based
    kinect.update();
    if( cas == 2)
    {
        if (kinect.isFrameNew())
        {
            calcAvgFPS();
            updateCloud();

            g_means.clear();
            g_votes.clear();
            g_clusters.clear();

            //do the actual estimation
            g_Estimate->estimate( 	g_im3D,
                                    g_means,
                                    g_clusters,
                                    g_votes,
                                    g_stride,
                                    g_maxv,
                                    g_prob_th,
                                    g_larger_radius_ratio,
                                    g_smaller_radius_ratio,
                                    false,
                                    g_th
                                );
        }
    }


    if( cas == 1)
    {

        if (kinect.isFrameNew()) {

            //copy webcam pixels to rgb image
            rgb.setFromPixels(kinect.getPixels()); //, w, h,1);

            //mirror horizontal
            //rgb.mirror(false, true);

            //duplicate rgb
            hsb = rgb;

            //convert to hsb
            hsb.convertRgbToHsv();

            //store the three channels as grayscale images
            hsb.convertToGrayscalePlanarImages(hue, sat, bri);

            //filter image based on the hue value were looking for
            for (int i=0; i<w*h; i++) {
                filtered.getPixels()[i] = ofInRange(hue.getPixels()[i],findHue-8,findHue+8) ? 255 : 0;
            }

            filtered.flagImageChanged();
            //run the contour finder on the filtered image to find blobs with a certain hue
            contours.findContours(filtered, 50, w*h/2, 1, false);
        }
    }

//    if (cas == 2)
//    {
//        if (movie.isFrameNew())
//        {
//            faceTracker.update(movie);
//        }
//    }

    //--------------------------- MOTORS --------------------------

    // check head position;
    if (fTrackHead==true)
    {
        //usleep(500000);

        //take mean of head positions
    //    int sumValX = 0;
    //    int sumValY = 0;

    //    int count =0;
    //    for (int i=0;i<500;i++)
    //    {
    //        sumValX +=fOssiaHeadPositionX;
    //        sumValY +=fOssiaHeadPositionY;

    //        count +=1;
    //    }

        //fmeanHeadPositionX = ofMap(1024-sumValX/count,0,1024,0.,1.);
        //fmeanHeadPositionY = ofMap(768-sumValY/count,0,768,0.,1.);

        //take head positions at each frame
        fmeanHeadPositionX = ofMap(1024-fOssiaHeadPositionX,0,1024,0.,1.);
        fmeanHeadPositionY = ofMap(768-fOssiaHeadPositionY,0,768,0.,1.);

//        fmeanHeadPositionX = ofMap(fOssiaHeadPositionX,0,1024,0.,1.);
//        fmeanHeadPositionY = ofMap(768-fOssiaHeadPositionY,0,768,0.,1.);


        fOssiaAngleServo4.set(fmeanHeadPositionY);
        fOssiaAngleServo5.set(fmeanHeadPositionX);
        //printf("X HEAD : %f\n",fmeanHeadPositionX);
        //printf("Y HEAD : %f\n",fmeanHeadPositionY);
    }


    // set or get servos angles

//    if (fMotorsEnabled==false)
//    {
//        int posServo1 = servo1.getPos();
//        int posServo2 = servo2.getPos();
//        int posServo3 = servo3.getPos();
//        int posServo4 = servo4.getPos();
//        int posServo5 = servo5.getPos();

//        float pos1 = ofMap(posServo1, fServosMins[0], fServosMax[0],0.,1.);
//        float pos2 = ofMap(posServo2, fServosMins[1], fServosMax[1],0.,1.);
//        float pos3 = ofMap(posServo3, fServosMins[2], fServosMax[2],0.,1.);
//        float pos4 = ofMap(posServo4, fServosMins[3], fServosMax[3],0.,1.);
//        float pos5 = ofMap(posServo5, fServosMins[4], fServosMax[4],0.,1.);

//        fOssiaAngleServo1.set(pos1);
//        fOssiaAngleServo2.set(pos2);
//        fOssiaAngleServo3.set(pos3);
//        fOssiaAngleServo4.set(pos4);
//        fOssiaAngleServo5.set(pos5);
//        int timeSleepMs = 300;
//        usleep(timeSleepMs*1000);
//    }



//    servo1.setAngle(fOssiaAngleServo1);
//    servo2.setAngle(fOssiaAngleServo2);
//    servo3.setAngle(fOssiaAngleServo3);
//    servo4.setAngle(fOssiaAngleServo4);
//    servo5.setAngle(fOssiaAngleServo5);

    //servo2.setAngle(fOssiaAngleServo2);

    //Rq : need to be called after arbotix->connect
    {
    boost::mutex::scoped_lock lock(fPosMutex);

        arbotix->update();

        //update servos
        if (arbotix->isInitialized() && fMotorsEnabled)
        {
            servo1.setAngle(fOssiaAngleServo1);
            servo2.setAngle(fOssiaAngleServo2);
            servo3.setAngle(fOssiaAngleServo3);
            servo4.setAngle(fOssiaAngleServo4);
            servo5.setAngle(fOssiaAngleServo5);
            servo1.update();
            servo2.update();
            servo3.update();
            servo4.update();
            servo5.update();
            arbotix->moveServos();
         }
    }


    // check servos parameters
    //int tempServo3 = arbotix->getServoTemp(3);
    //printf ("temp servo 3 :%i\n",tempServo3);

    //bool ret = arbotix->waitForSysExMessage(SYSEX_DYNAMIXEL_GET_REGISTER, 2);


     // check servos temp

     if (elapsedTime>=2000)
     {
         // read a first value, otherwise temp is false in second reading (0x2B)
        arbotix->getDynamixelRegister(3,0x24,2);

        fServo2Temp = servo2.getTemp();
        fServo3Temp = servo3.getTemp();
        //printf ("Temp Servo 2 = %i °C\n",fServo2Temp);
        //printf ("Temp Servo 3 = %i °C\n",fServo3Temp);

        ofResetElapsedTimeCounter() ;
     }

    //arbotix->getDynamixelRegister(4,0x2B,2);
    //arbotix->getDynamixelRegister(5,0x2B,2);

    //usleep(100000);
    //ofResetElapsedTimeCounter() ;


}

void ofApp::standUp()
{
     fOssiaAngleServo3.set(1.0);
//     int timeSleepS = 1;
//     usleep(timeSleepS*1000000);
     fOssiaAngleServo2.set(1.0);
     fOssiaAngleServo4.set(0.6);
     fOssiaAngleServo5.set(0.5);


}

void ofApp::goToRest()
{

    fOssiaAngleServo3.set(fInitialPosServo3);
    fOssiaAngleServo2.set(fInitialPosServo2);
    fOssiaAngleServo4.set(fInitialPosServo4);
    fOssiaAngleServo5.set(fInitialPosServo5);
    fTrackHead = false;

}



//--------------------------------------------------------------
void ofApp::draw(){
 _gui.draw();
 _gui.setPosition(ofGetWidth()-_gui.getWidth()-10,0);


 //kinect based
  if( cas == 2)
  {
     easyCam.begin();
     if (bDrawCloud) {
         drawPointCloud();
         drawPoses();
     }
     easyCam.end();
     //drawReport();
  }

     if( cas == 1)
     {
         ofSetColor(255,255,255);

         //draw all cv images
         rgb.draw(0,0); // Seul cette image m'interesse
         //hsb.draw(640,0);
         //hue.draw(0,240);
         //sat.draw(320,240);
         //bri.draw(640,240);
         //filtered.draw(0,240);   // Celle ci et la suivante sont interessante mais
         //contours.draw(0,240);   // Pas la place de la mettre pour avoir un bon rendu

         ofSetColor(255, 0, 0);
         ofFill();

         //draw red circles for found blobs
         for (int i=0; i<contours.nBlobs; i++)
         {
             ofCircle(contours.blobs[i].centroid.x, contours.blobs[i].centroid.y, 20);
             ofPoint pos = contours.blobs[i].centroid;

             float xcm = pos.x/37.795275590551;
             float ycm = pos.y/37.795275590551;


             double timeDiffMs = diffclock(clock(),objectDetectionStartTime);
             //printf("diff MS = %f\n",timeDiffMs);
             if (timeDiffMs>=50.0)
             {
                 fOssiaHeadPositionX.set(xcm*64);
                 fOssiaHeadPositionY.set(ycm*64);
                 objectDetectionStartTime = clock();

             }
//             lastObjectPositonX = pos.x;
//             lastObjectPositonY = pos.y;

             //1cm = 37.795275590551 pixel
         }
     }

//     if (cas == 2)
//     {
//         ofSetColor(255,255,255);
//         movie.draw(0,0);
//         faceTracker.draw();
//         ofDrawBitmapStringHighlight(ofToString(faceTracker.size()), 10, 20);

//         float xcm ;
//         float ycm ;

//         for(int i = 0; i < faceTracker.size(); i++) {
//                 ofRectangle face = faceTracker.getObjectSmoothed(i);
//                 xcm = face.x/37.795275590551;
//                 ycm = face.y/37.795275590551;
//                 fOssiaHeadPositionX.set(xcm*64);
//                 fOssiaHeadPositionY.set(ycm*64);
//         }



//     }

 fCircleButtonRadius = 25;
 int offset = 20 ;
 fCircleButton.set(w-200, h+fCircleButtonRadius+offset);


 if (fMotorsEnabled)
 {
    ofSetColor(ofColor::red);
    verdana14Font.drawString("Disable Motors", w -200 + fCircleButtonRadius + 10 , h+fCircleButtonRadius+offset);
 }
 else
 {
    ofSetColor(ofColor::green);
    verdana14Font.drawString("Enable Motors", w- 200 + fCircleButtonRadius + 10 , h + fCircleButtonRadius+offset);
 }

 ofCircle(fCircleButton, fCircleButtonRadius);

 // display temp servo 2
 ofSetColor(ofColor::black);
 if (fServo2Temp>=60 and fServo2Temp<=70)
 {
    ofSetColor(ofColor::orange);
 }
 else if (fServo2Temp>=70)
 {
     ofSetColor(ofColor::red);
 }
 verdana14Font.drawString("Temp Servo 2 :" + ofToString(fServo2Temp) + "°C", w- fCircleButtonRadius - 200,h+fCircleButtonRadius+offset+50);


 // display temp servo 3

 ofSetColor(ofColor::black);
 if (fServo3Temp>=60 and fServo3Temp<=70)
 {
    ofSetColor(ofColor::orange);
 }
 else if (fServo3Temp>=70)
 {
     ofSetColor(ofColor::red);
 }
 verdana14Font.drawString("Temp Servo 3 :" + ofToString(fServo3Temp) + "°C", w- fCircleButtonRadius - 200,h+fCircleButtonRadius+offset+100);

 // display help

 ofDrawBitmapString(ofToString((int) ofGetFrameRate()), 10 , h + offset);
 ofDrawBitmapStringHighlight(
         string() +
         "z : Repose-toi\n" +
         "s : Leve toi\n" +
         "t : Activation suivi objet / visage\n"+
         "+ : Augmentation angle kinect\n" +
         "- : Diminution angle kinect\n" +
         "UP : Move Head up\n" +
         "DOWN :  Move Head down\n" +
         "LEFT :  Move Head left\n" +
         "RIGHT :  Move Head right\n"
         ,10,  h + offset);

 ofDrawBitmapString(ofToString((int) ofGetFrameRate()), 10 , h + offset);
 ofDrawBitmapStringHighlight(
         string() +
         "0 - Pas de detection\n" +
         "1 - Detection objet\n" +
         "2 - Detection visage\n"
         ,10,  h + offset + 14*12);

// ofDrawBitmapString(ofToString((int) ofGetFrameRate()), 10 , h + offset);
// ofDrawBitmapStringHighlight(
//         string() +
//         "r - Suvi objet rouge\n" +
//         "b - Suvi objet bleu\n" +
//         "g - Suvi objet vert\n" +
//         "o - Suvi objet orange\n" +
//         "y - Suvi objet jaune\n"
//         ,10,  h + offset + 14*12);

}


double ofApp::diffclock(clock_t clock1, clock_t clock2)
{
    double diffticks = clock1 - clock2;
    double diffms = (diffticks) / (CLOCKS_PER_SEC / 1000);
    return diffms;
}

void ofApp::enableMotors(bool state)
{
    if (state==true)

    {
        int posServo1 = servo1.getPos();
        int posServo2 = servo2.getPos();
        int posServo3 = servo3.getPos();
        int posServo4 = servo4.getPos();
        int posServo5 = servo5.getPos();

        float pos1 = ofMap(posServo1, fServosMins[0], fServosMax[0],0.,1.);
        float pos2 = ofMap(posServo2, fServosMins[1], fServosMax[1],0.,1.);
        float pos3 = ofMap(posServo3, fServosMins[2], fServosMax[2],0.,1.);
        float pos4 = ofMap(posServo4, fServosMins[3], fServosMax[3],0.,1.);
        float pos5 = ofMap(posServo5, fServosMins[4], fServosMax[4],0.,1.);

        fOssiaAngleServo1.set(pos1);
        fOssiaAngleServo2.set(pos2);
        fOssiaAngleServo3.set(pos3);
        fOssiaAngleServo4.set(pos4);
        fOssiaAngleServo5.set(pos5);
        fMotorsEnabled = true;
    }
    else
    {
        servo3.disable();
        servo2.disable();
        servo4.disable();
        servo5.disable();
        servo1.disable();
        fMotorsEnabled = false;
    }
}
//--------------------------------------------------------------
void ofApp::keyPressed(int key){

    switch(key){

    case '0':
        cas = 0;
        break;

    case '1':
        cas = 1;
        lastObjectPositonX = 0;
        lastObjectPositonY = 0;

        break;

    case '2':
        cas = 2;
        break;


//    case 'r':
//        if (cas == 1)
//        {
//          findHue = 170;
//        }
//        break;

//    case 'b':
//        if (cas == 1)
//        {
//          findHue = 100;
//        }
//        break;

//    case 'g':
//        if (cas == 1)
//        {
//          findHue = 60;
//        }
//        break;

//    case 'o':
//        if (cas == 1)
//        {
//          findHue = 10;
//        }
//        break;

//    case 'y':
//        if (cas == 1)
//        {
//          findHue = 30;
//        }
//        break;

//    case 'a':
//        if (fMotorsEnabled ==false)
//        {
//            enableMotors(true);

//        }
//        else if (fMotorsEnabled==true)
//        {
//            enableMotors(false);
//        }
//        break;


    case 't' :
        if (fTrackHead ==false)
        {
            fTrackHead = true;
        }
        else if (fTrackHead==true)
        {
            fTrackHead = false;
        }
        break;

    case 's' :
        standUp();
        break;

    case 'z' :
        goToRest();
        break;

    case OF_KEY_DOWN :
        {
            printf("key down pressed\n");
            {
                float currentVal = fOssiaAngleServo4;
                currentVal -=0.05;
                if (currentVal<=0.0)
                {
                    currentVal = 0.0;
                }
                fOssiaAngleServo4.set(currentVal);
            }
        }
        break;

    case OF_KEY_UP :
        {

            {
                float currentVal = fOssiaAngleServo4;
                currentVal +=0.05;
                if (currentVal>=1.0)
                {
                    currentVal = 1.0;
                }
                fOssiaAngleServo4.set(currentVal);
                }
        }
        break;

    case OF_KEY_RIGHT :
        {
            printf("key down pressed\n");
            {
                float currentVal = fOssiaAngleServo5;
                currentVal -=0.05;
                if (currentVal<=0.0)
                {
                    currentVal = 0.0;
                }
                fOssiaAngleServo5.set(currentVal);
            }
        }
        break;

    case OF_KEY_LEFT :
        {

            {
                float currentVal = fOssiaAngleServo5;
                currentVal +=0.05;
                if (currentVal>=1.0)
                {
                    currentVal = 1.0;
                }
                fOssiaAngleServo5.set(currentVal);
                }
        }
        break;

    case '+': kTilt += 1; if (kTilt > 30) kTilt = 30; kinect.setCameraTiltAngle(kTilt); break;
    case '-': kTilt -= 1; if (kTilt < -30) kTilt = -30; kinect.setCameraTiltAngle(kTilt); break;

    default:
        break;
    }


}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
    if (fCircleButton.distance(ofPoint(x,y)) < fCircleButtonRadius) {
            bool motorsEnabled = fMotorsEnabled;
            enableMotors(!motorsEnabled);
        }
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}

void ofApp::exit()
{
    ofLogNotice() << "Exit app";
    arbotix->disconnect();
}


void ofApp::loadArbotixConfiguration(const std::string &fileName)
{
    std::string message;
    if( fXMLReader.loadFile(fileName) ){
        message = fileName + " loaded!";
        ofLogNotice() << message;
    }else{
        message = "unable to load " + fileName + " - check data/ folder";
        ofLogError() << message;
    }

    // find port name and baudrate
    std::string portName = fXMLReader.getValue("port::name", "");
    int rate = fXMLReader.getValue("port::rate", 0);

    if (portName == "" || rate == 0)
    {
        ofLogError() << "name of the port and baud rate not find in xml";
    }
    else
    {
        fArbotixPortName = portName ;
        fArbotixRate = rate;
    }

    // find servos names
    fServosList.clear();
    fServosNames.clear();
    fServosIds.clear();
    fServosMins.clear();
    fServosMax.clear();
    fServosInitialPos.clear();
    fServosSpeeds.clear();

    int nbServos = fXMLReader.getNumTags("servo");
    if (nbServos>0)
    {
        for(int i = 0; i < nbServos; i++)
        {
              fXMLReader.pushTag("servo", i);
              std::string servoName = fXMLReader.getValue("name","");
              int id = fXMLReader.getValue("id",0);
              int pinNb = fXMLReader.getValue("pinNb",0);
              int pos = fXMLReader.getValue("initialPos",0);
              int min = fXMLReader.getValue("min",0);
              int max = fXMLReader.getValue("max",300);
              int speed = fXMLReader.getValue("speed",300);

              ofLogNotice () << "Servo " << servoName << " id : " <<  id << " initial pos " << pos << "speed : " << speed;

              if (servoName!="" && id!=0 && speed !=0)
              {
                  fServosNames.push_back(servoName);
                  fServosIds.push_back(id);
                  fServosMins.push_back(min);
                  fServosMax.push_back(max);
                  fServosSpeeds.push_back(speed);
                  printf("initial pos : %i\n",pos);
                  fServosInitialPos.push_back(pos);

              }
              fXMLReader.popTag();
         }

    }
}

void ofApp::loadArduinoConfiguration(const std::string &fileName)
{
    std::string message;
    if( fXMLReader.loadFile(fileName) ){
        message = fileName + " loaded!";
        ofLogNotice() << message;
    }else{
        message = "unable to load " + fileName + " - check data/ folder";
        ofLogError() << message;
    }

    // find port name and baudrate
    std::string portName = fXMLReader.getValue("port::name", "");
    int rate = fXMLReader.getValue("port::rate", 0);

    if (portName == "" || rate == 0)
    {
        ofLogError() << "name of the port and baud rate not find in xml";
    }
    else
    {
        fArbotixPortName = portName ;
        fArbotixRate = rate;
    }
}

void ofApp::setupEstimator() {
    // Number of trees
    g_ntrees = 10;
    //maximum distance form the sensor - used to segment the person
    g_max_z = 2000;
    //head threshold - to classify a cluster of votes as a head
    g_th = 500;
    //threshold for the probability of a patch to belong to a head
    g_prob_th = 1.0f;
    //threshold on the variance of the leaves
    g_maxv = 800.f;
    //stride (how densely to sample test patches - increase for higher speed)
    g_stride = 5;
    //radius used for clustering votes into possible heads
    g_larger_radius_ratio = 1.6f;
    //radius used for mean shift
    g_smaller_radius_ratio = 5.f;

    g_im3D.create(KH,KW,CV_32FC3);
    g_Estimate =  new CRForestEstimator();
    g_treepath = "./data/trees/tree";

    if( !g_Estimate->loadForest(g_treepath.c_str(), g_ntrees) ){
                ofLog(OF_LOG_ERROR, "could not read forest!");
        bTreesLoaded = false;
        } else bTreesLoaded = true;
}

//--------------------------------------------------------------
void ofApp::calcAvgFPS() {
    int currMillis = ofGetElapsedTimeMillis();
    avgkFPS += (1000.0/(currMillis-lastMillis))/FPS_MEAN;
    lastMillis = currMillis;
    frameCount++;
    if (frameCount >= FPS_MEAN) {
        kFPS = avgkFPS;
        avgkFPS = frameCount =  0;
    }
}
//--------------------------------------------------------------
void ofApp::updateCloud() {
    //generate 3D image
    // I copied part of this code from the library demo code
    // this kind of nomenclature is unknown to me: g_im3D.ptr<Vec3f>(y)
        for(int y = 0; y < g_im3D.rows; y++)
        {
                Vec3f* Mi = g_im3D.ptr<Vec3f>(y);
                for(int x = 0; x < g_im3D.cols; x++){
                        ofVec3f thePoint = kinect.getWorldCoordinateAt(x,y);

                        if ( (thePoint.z < g_max_z) && (thePoint.z > 0) ){
                                Mi[x][0] = thePoint.x;
                                Mi[x][1] = thePoint.y;
                                Mi[x][2] = thePoint.z;
                        }
                        else
                                Mi[x] = 0;
                }
        }
}
//--------------------------------------------------------------
void ofApp::drawPointCloud() {
        ofMesh mesh;

        mesh.setMode(OF_PRIMITIVE_POINTS);
        int step = 2;
        for(int y = 0; y < h; y += step) {
                for(int x = 0; x < w; x += step) {
                        if ((kinect.getDistanceAt(x, y) > 0) && (kinect.getDistanceAt(x, y) < g_max_z)) {
                                mesh.addColor(kinect.getColorAt(x,y));
                                mesh.addVertex(kinect.getWorldCoordinateAt(x, y));
                        }
                }
        }
        ofPushMatrix();
        glPointSize(3);
        // the projected points are 'upside down' and 'backwards'
        ofScale(1, -1, -1);
        ofTranslate(0, 0, -1000); // center the points a bit
        glEnable(GL_DEPTH_TEST);
        mesh.drawVertices();
        glDisable(GL_DEPTH_TEST);
        ofPopMatrix();
}
//--------------------------------------------------------------
void ofApp::drawPoses() {
    ofPushMatrix();
        // the projected points are 'upside down' and 'backwards'
        ofScale(1, -1, -1);
        ofTranslate(0, 0, -1000); // center the points a bit
    ofSetColor(0,0,255);
    glLineWidth(3);
    if(g_means.size()>0)
    {
            for(unsigned int i=0;i<1/*g_means.size()*/;++i)
            {
                ofVec3f pos = ofVec3f(g_means[i][0], g_means[i][1], g_means[i][2]);
                ofVec3f dir = ofVec3f(0,0,-150);
                dir.rotate(g_means[i][3], g_means[i][4], g_means[i][5]);
                dir += pos;
                ofLine(pos.x, pos.y, pos.z, dir.x, dir.y, dir.z);
                //printf("pos x = %.1f\n",pos.x);
                //printf("pos y = %.1f\n",pos.y);
                float xcm = (pos.x + float(w)/2)/37.795275590551;
                float ycm = (pos.y + float(h)/2)/37.795275590551;

                double timeDiffMs = diffclock(clock(),objectDetectionStartTime);
                //printf("diff MS = %f\n",timeDiffMs);
                if (timeDiffMs>=50.0)
                {
                    fOssiaHeadPositionX.set(xcm*64);
                    fOssiaHeadPositionY.set(ycm*64);
                    objectDetectionStartTime = clock();
                }
           }
        }
        ofPopMatrix();
}
//--------------------------------------------------------------
void ofApp::drawReport() {
    ofPushMatrix();
    ofSetColor(0);
    char reportStr[1024];
    sprintf(reportStr, "framecount: %i   FPS: %.2f", frameCount, kFPS);
    ofDrawBitmapString(reportStr, 10, 10);
    ofPopMatrix();
}
