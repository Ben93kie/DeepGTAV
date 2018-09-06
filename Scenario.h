#pragma once

#include <stdlib.h>
#include <ctime>

#include "lib/script.h"
#include "lib/utils.h"

#include "lib/rapidjson/document.h"
#include "lib/rapidjson/stringbuffer.h"

#include "ScreenCapturer.h"
#include "Rewarders\Rewarder.h"
#include "LiDAR.h"
#include "Functions.h"

using namespace rapidjson;

//#define DEBUG 1

class Scenario {
private:
	static char* weatherList[14];
	static char* vehicleList[3];

	Vehicle vehicle = NULL;
	Player player = NULL;
	Ped ped = NULL;
	Cam camera = NULL;
	Vector3 dir;

	float x, y, z;
    float startHeading;
	int hour, minute;
	const char* _weather;
	const char* _vehicle;
	int width, height;

	bool vehicles;
	bool peds;
	bool trafficSigns; //TODO
	bool direction;
	bool reward;
	bool throttle;
	bool brake;
	bool steering;
	bool speed;
	bool yawRate;
	bool drivingMode; //TODO
	bool location;
	bool time;
    bool offscreen;
    bool showBoxes;
    bool pointclouds;
    bool stationaryScene;

    std::string baseFolder;

	float currentThrottle = 0.0;
	float currentBrake = 0.0;
	float currentSteering = 0.0;

    Vector3 currentPos;
    Vector3 currentForwardVector;
    Vector3 currentUpVector;
    Vector3 currentRightVector;

	Rewarder* rewarder;
	std::clock_t lastSafetyCheck;
	int _drivingMode;
	float _setSpeed;

	bool running = false;
	Document d;

    //LiDAR variables
    LiDAR lidar;
    bool lidar_initialized = false;
    int pointCloudSize = 0;
    std::unordered_map<int, HitLidarEntity*> entitiesHit;
    int lidar_param = 7;

    //Depth Map variables
    float* depth_map = NULL;
    float* m_stencilBuffer = NULL;
    float* m_pDMPointClouds;
    float m_nearClip;
    float m_fov;
    float m_ncHeight;
    float m_ncWidth;
    bool m_depthInit = false;
    unsigned char* color_buf;
    std::string m_prevDepthFilename;
    std::string m_prevDepthPCFilename;
    std::string m_prevPCFilename;

    bool vehicles_created = false;
    std::vector<VehicleToCreate> vehiclesToCreate;
    std::vector<PedToCreate> pedsToCreate;

    //For tracking: first frame in a series that an entity appears
    std::unordered_map<int, int> trackFirstFrame;
    //For calculating real speed
    Vector3 m_trackLastPos;
    float m_trackRealSpeed;
    float m_trackLastRealSpeed;
    float m_trackDist;
    float m_trackDistVar;
    float m_trackDistErrorTotal;
    float m_trackDistErrorTotalVar;
    float m_trackDistErrorTotalCount;
    int m_trackLastIndex = -1;
    int m_trackLastSeqIndex = 0;
    std::string m_timeTrackFile;

    //Camera intrinsic parameters
    float intrinsics[3];

public:
	float rate;

	void start(const Value& sc, const Value& dc);
	void stop();
	void config(const Value& sc, const Value& dc);
	void setCommands(float throttle, float brake, float steering);
	void run();

    //Depth buffer fn/var needs to be accessed by server
    void setDepthBuffer(bool prevDepth = false);
    bool m_prevDepth = false;

	ScreenCapturer* screenCapturer;
	StringBuffer generateMessage();

    int instance_index = 0;
    int series_index = 0;
    std::string series_string = "0000";
    std::string instance_string;
    int baseTrackingIndex = instance_index;

    //Tracking variables
    bool collectTracking;
    //# of instances in one series
    const int trSeriesLength = 500;
    //# of seconds between series
    const int trSeriesGapTime = 30;
    //Used for keeing track of when to add the gap
    bool trSeriesGap = false;

private:
	void parseScenarioConfig(const Value& sc, bool setDefaults);
	void parseDatasetConfig(const Value& dc, bool setDefaults);
	void buildScenario();

	void setVehiclesList();
	void setPedsList();
	void setTrafficSignsList();
	void setDirection();
	void setReward();
	void setThrottle();
	void setBrake();
	void setSteering();
	void setSpeed();
	void setYawRate();
	void setDrivingMode();
	void setLocation();
	void setTime();
    void setupLiDAR();
    void collectLiDAR();
    void setIndex();
    void drawBoxes(Vector3 BLL, Vector3 FUR, Vector3 dim, Vector3 upVector, Vector3 rightVector, Vector3 forwardVector, Vector3 position, int colour);
    void calcCameraIntrinsics();
    void setFocalLength();
    bool getEntityVector(Value &_entity, Document::AllocatorType& allocator, int entityID, Hash model, int classid);
    void setPosition();
    float observationAngle(Vector3 position);
    void drawVectorFromPosition(Vector3 vector, int blue, int green);
    void createVehicles();
    void createVehicle(const char* model, float relativeForward, float relativeRight, float heading, int color, int color2);
    void createPed(int model, float relativeForward, float relativeRight, float heading, int task);
    void increaseIndex();
    void setDepthParams();
    Vector3 depthToCamCoords(float depth, float screenX, float screenY);
    std::string getStandardFilename(std::string subDir, std::string extension);
    void outputRealSpeed();

    BBox2D BBox2DFrom3DObject(Vector3 position, Vector3 dim, Vector3 forwardVector, Vector3 rightVector, Vector3 upVector);

    //Do not use this function. Causes GTA to crash - need to figure out why
    void setColorBuffer();
};