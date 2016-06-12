
#include <ros/ros.h>
#include <std_msgs/Bool.h>
#include <sensor_msgs/Joy.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>


#include <iostream>
#include <ctype.h>
#include <stdlib.h>
#include<stdio.h>
#include <math.h>
#include <unistd.h>

#include <opencv2/opencv.hpp>

#include "RobotClass.hpp"


using namespace std;


// this struct should hold all the interfaces for a given robot, each node should initialise the ones that it needs to use.


class location_node{

    ros::Subscriber isInitSub;
    bool isInit= 0;
    int numberFedu = 4;
    cv::Point2f camWobble;  // stored so that we can search a min bounding box for feducials. then Min bounding boxes are more possible for the robots.
    int isSetup = 0; // are we happy with the thresholds and the wobble perameters.
    vector<RobotClass*> robotInterface;
    int number_robots;
    ros::NodeHandle nh_;
    image_transport::ImageTransport it_;
    image_transport::Subscriber camera;

    struct pairs{
        int head;
        int led;
        float error;
    };

    int FLowH = 0;
    int FHighH = 15;

    int FLowS = 170;
    int FHighS = 255;

    int FLowV = 54;
    int FHighV = 99;

    int LLowH = 77;
    int LHighH = 111;

    int LLowS = 0;
    int LHighS = 66;

    int LLowV = 227;

    int LHighV = 255;

    // Setup SimpleBlobDetector parameters.
    cv::SimpleBlobDetector::Params params;

    cv::Mat image;

    int radius = 40; // should be initialised to the number of pixels between the front marker and the led.

    bool debug = 1;

public:
    location_node(int robotNum, char* argin);
    void isInitCB(const std_msgs::BoolConstPtr& msg);
    void joyCB(const sensor_msgs::JoyConstPtr& msg);
    void imageCB(const sensor_msgs::ImageConstPtr& msg);
    vector<pairs> findPairs(std::vector<cv::KeyPoint> Fkeypoints,std::vector<cv::KeyPoint>  Lkeypoints,float radius);
    float findDistance(cv::Point2f A,cv::Point2f B);
    void thesholdImage(cv::Mat &imageIn,bool needHead,std::vector<cv::KeyPoint> &Fkeypoints,bool needLED,std::vector<cv::KeyPoint> &Lkeypoints );
};

location_node::location_node(int robotNum, char* argin):it_(nh_){
    if (*argin != 'y'){
        debug= 0;
    }
    camera = it_.subscribe("/camera",1,&location_node::imageCB,this);
    number_robots = robotNum;
    ROS_INFO("there are %d robots in this location system",number_robots);
    //make a vector of the right size with pub/sub for all features.
    isInitSub= nh_.subscribe("urobot_init_node/isInit", 1,&location_node::isInitCB,this);
    //do launching of stuff here, find all the robots and give them names. When complete call the 'publishIsInit' function. This is best to hapen only once, so lauch this node last and have others wait for this signal.
    for(int i= 0; i < number_robots; i ++){
        RobotClass* newRobot = new RobotClass(nh_,i,1);
        robotInterface.push_back(newRobot);
    }

    // Change thresholds
    params.minThreshold = 10;
    params.maxThreshold = 12;

    // Filter by Area.
    params.filterByArea =false;
    params.minArea = 8;
    params.maxArea = 150;

    // Filter by Circularity
    params.filterByCircularity = false;
    params.minCircularity = 0.1;

    // Filter by Convexity
    params.filterByConvexity = false;
    params.minConvexity = 0.87;

    // Filter by Inertia
    params.filterByInertia = false;
    params.minInertiaRatio = 0.01;

    params.thresholdStep = 1;

    cv::namedWindow("ControlLoc", CV_WINDOW_AUTOSIZE); //create a window called "Control"
    //imshow( "Display window", image );
    //Create trackbars in "Control" window
    cvCreateTrackbar("FLowH", "Control", &FLowH, 179); //Hue (0 - 179)
    cvCreateTrackbar("FHighH", "Control", &FHighH, 179);

    cvCreateTrackbar("FLowS", "Control", &FLowS, 255); //Saturation (0 - 255)
    cvCreateTrackbar("FHighS", "Control", &FHighS, 255);

    cvCreateTrackbar("FLowV", "Control", &FLowV, 255); //Value (0 - 255)
    cvCreateTrackbar("FHighV", "Control", &FHighV, 255);

    cvCreateTrackbar("LLowH", "Control", &LLowH, 179); //Hue (0 - 179)
    cvCreateTrackbar("LHighH", "Control", &LHighH, 179);

    cvCreateTrackbar("LLowS", "Control", &LLowS, 255); //Saturation (0 - 255)
    cvCreateTrackbar("LHighS", "Control", &LHighS, 255);

    cvCreateTrackbar("LLowV", "Control", &LLowV, 255); //Value (0 - 255)
    cvCreateTrackbar("LHighV", "Control", &LHighV, 255);


    cvCreateTrackbar("radius", "Control", &radius, 255);


}


void location_node::isInitCB(const std_msgs::BoolConstPtr& msg)
{
    if (msg->data == true){
        isInit =1;
    }


}

void location_node::thesholdImage(cv::Mat &imageIn,bool needHead,std::vector<cv::KeyPoint> &Fkeypoints,bool needLED,std::vector<cv::KeyPoint> &Lkeypoints ){
    cv::Mat imgHSV,FimgThresholded,LimgThresholded;
    cv::cvtColor(imageIn, imgHSV, cv::COLOR_BGR2HSV); //Convert the captured frame from BGR to HSV
    if(needHead) cv::inRange(imgHSV, cv::Scalar(FLowH, FLowS, FLowV), cv::Scalar(FHighH,FHighS, FHighV), FimgThresholded); //Threshold the image
    if(needLED)cv::inRange(imgHSV, cv::Scalar(LLowH, LLowS, LLowV), cv::Scalar(LHighH,LHighS, LHighV), LimgThresholded); //Threshold the image

    int erodeVal = 2;
    if(erodeVal !=0){
        if(needHead){
            cv::erode(FimgThresholded, FimgThresholded, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(erodeVal,erodeVal)) );
            cv::dilate( FimgThresholded, FimgThresholded, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(erodeVal,erodeVal)) );
            cv::dilate( FimgThresholded, FimgThresholded, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(erodeVal,erodeVal)) );
            cv::erode(FimgThresholded, FimgThresholded, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(erodeVal,erodeVal)) );
        }

        if(needLED){
            cv::erode(LimgThresholded, LimgThresholded, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(erodeVal,erodeVal)) );
            cv::dilate( LimgThresholded, LimgThresholded, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(erodeVal,erodeVal)) );
            cv::dilate( LimgThresholded, LimgThresholded, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(erodeVal,erodeVal)) );
            cv::erode(LimgThresholded, LimgThresholded, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(erodeVal,erodeVal)) );
        }
    }
    // Set up the detector with default parameters.
    cv::Ptr<cv::SimpleBlobDetector> detector = cv::SimpleBlobDetector::create(params);
    if(needHead) {
        cv::threshold(FimgThresholded, FimgThresholded, 0.5, 255,cv::THRESH_BINARY_INV);
        detector->detect(  FimgThresholded, Fkeypoints);
    }
    if(needLED) {
        cv::threshold(LimgThresholded, LimgThresholded, 0.5, 255,cv::THRESH_BINARY_INV);
        detector->detect(  LimgThresholded, Lkeypoints);
    }
    if(debug){
        cv::imshow("heads", FimgThresholded );
        cv::imshow("leds", LimgThresholded );
    }

}

void location_node::imageCB(const sensor_msgs::ImageConstPtr& msg){

    //get image from message.
    cv_bridge::CvImagePtr cv_ptr;
    try
    {
        cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
    }
    catch (cv_bridge::Exception& e)
    {
        ROS_ERROR("cv_bridge exception: %s", e.what());
        return;
    }
    image = cv_ptr->image;

    std::vector<cv::KeyPoint> Fkeypoints,Lkeypoints;
    thesholdImage(image,1,Fkeypoints,1,Lkeypoints );


    vector<location_node::pairs> ourPairs = location_node::findPairs(Fkeypoints, Lkeypoints, radius);

    cv::Mat im_with_keypoints;
    cv::drawKeypoints( image, Fkeypoints, im_with_keypoints, cv::Scalar(255,255,255), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS );
    cv::drawKeypoints( im_with_keypoints, Lkeypoints, im_with_keypoints, cv::Scalar(0,255,255), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS );
    for(int i = 0; i < ourPairs.size() && i < number_robots; i++){
        if(ourPairs.at(i).error <130){
            if(debug){
                cv::circle(im_with_keypoints, Lkeypoints.at(ourPairs.at(i).led).pt, radius, cv::Scalar(0,0,255) );


                line(im_with_keypoints, Lkeypoints.at(ourPairs.at(i).led).pt, Fkeypoints.at(ourPairs.at(i).head).pt, cv::Scalar(255,0,255));
                cv::Point2f centre = 0.55*Lkeypoints.at(ourPairs.at(i).led).pt + 0.45*Fkeypoints.at(ourPairs.at(i).head).pt;
                cv::circle(im_with_keypoints, centre, 3, cv::Scalar(0,0,255) );
            }
            //get robot IDS here.
            robotInterface.at(i)->publishTF(Lkeypoints.at(ourPairs.at(i).led).pt, Fkeypoints.at(ourPairs.at(i).head).pt, msg->header.stamp);

        }
    }
    if(debug){
        // Show blobs
        if(ourPairs.size() > 0) cv::imshow("keypoints", im_with_keypoints );
        cv::imshow("raw",image);
    }
    ////// end of image testing.
    cv::waitKey(1);


}

vector<location_node::pairs> location_node::findPairs(std::vector<cv::KeyPoint> Fkeypoints, std::vector<cv::KeyPoint> Lkeypoints,float radius){
    //for all leds
    vector<location_node::pairs> thePairs;
    for(int i = 0; i < Lkeypoints.size(); i ++){
        //find distance to heads.
        location_node::pairs thisPair;
        thisPair.led = i;
        float bestError = 100000000;
        for (int j = 0; j < Fkeypoints.size(); j++){
            cv::Point2f  A= Lkeypoints.at(i).pt;
            cv::Point2f  B= Fkeypoints.at(j).pt;
            float distance = findDistance(A,B);
            float error  = pow(distance - radius,2);
            if(error< bestError){
                thisPair.head = j;
                thisPair.error = error;
                bestError = error;
            }

        }
        if(bestError <2000)thePairs.push_back(thisPair);

    }
    return thePairs;
}

float location_node::findDistance(cv::Point2f A,cv::Point2f B){
    float xChange = A.x - B.x;
    float yChange = A.y - B.y;
    return sqrt(pow(xChange,2)+pow(yChange,2));

}


int main(int argc, char** argv) {

    //ros setup
    ros::init(argc, argv, "location_node");
    location_node LN(atoi(argv[1]),argv[2]);
    ros::spin();
    return 0;
}
