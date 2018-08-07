#include "Scenario.h"
#include "lib/utils.h"
#include "lib/rapidjson/writer.h"
#include "Rewarders\GeneralRewarder.h"
#include "Rewarders\LaneRewarder.h"
#include "Rewarders\SpeedRewarder.h"
#include "defaults.h"
#include <time.h>
#include <fstream>
#include <string>
#include <sstream>
#include "Functions.h"
#include "RAGETransforms.h"
#include "Constants.h"
#include <Eigen/Core>
#include "lodepng.h"

extern "C" {
    __declspec(dllimport) int export_get_depth_buffer(void** buf, bool updateWithOffsetDepth);
    __declspec(dllexport) int export_get_color_buffer(void** buf);
    __declspec(dllexport) int export_get_stencil_buffer(void** buf);
    __declspec(dllimport) int export_get_previous_depth_stencil_buffers(void** dBuf, void** sBuf, int &stencilSize);
}

const float VERT_CAM_FOV = 59; //In degrees
//Need to input the vertical FOV with GTA functions.
//90 degrees horizontal (KITTI) corresponds to 59 degrees vertical (https://www.gtaall.com/info/fov-calculator.html).
const float HOR_CAM_FOV = 90; //In degrees

const float CAM_OFFSET_FORWARD = 0.5;
const float CAM_OFFSET_UP = 0.8;

char* Scenario::weatherList[14] = { "CLEAR", "EXTRASUNNY", "CLOUDS", "OVERCAST", "RAIN", "CLEARING", "THUNDER", "SMOG", "FOGGY", "XMAS", "SNOWLIGHT", "BLIZZARD", "NEUTRAL", "SNOW" };
char* Scenario::vehicleList[3] = { "blista", "blista", "blista" };//voltic, packer

void Scenario::parseScenarioConfig(const Value& sc, bool setDefaults) {
	const Value& location = sc["location"];
	const Value& time = sc["time"];
	const Value& weather = sc["weather"];
	const Value& vehicle = sc["vehicle"];
	const Value& drivingMode = sc["drivingMode"];

	if (location.IsArray()) {
		if (!location[0].IsNull()) x = location[0].GetFloat();
		else if (setDefaults) x = 5000 * ((float)rand() / RAND_MAX) - 2500;

		if (!location[1].IsNull()) y = location[1].GetFloat(); 
		else if (setDefaults) y = 8000 * ((float)rand() / RAND_MAX) - 2000;

        if (!location[2].IsNull()) z = location[2].GetFloat();
        else if (setDefaults) z = 0;

        if (!location[3].IsNull()) {
            log("Location 2 is not null");
            startHeading = location[3].GetFloat();
        }
        else if (setDefaults) {
            log("Location 3 is NULL");
            startHeading = 0;
        }
	}
	else if (setDefaults) {
		x = 5000 * ((float)rand() / RAND_MAX) - 2500;
		y = 8000 * ((float)rand() / RAND_MAX) - 2000;
	}

	if (time.IsArray()) {
		if (!time[0].IsNull()) hour = time[0].GetInt();
		else if (setDefaults) hour = rand() % 24;

		if (!time[1].IsNull()) minute = time[1].GetInt();
		else if (setDefaults) minute = rand() % 60;
	}
	else if (setDefaults) {
        hour = 16;//TODO Do we want random times? rand() % 24;
		minute = rand() % 60;
	}

	if (!weather.IsNull()) _weather = weather.GetString();
    //TODO: Do we want other weather?
    else if (setDefaults) _weather = "CLEAR";// weatherList[rand() % 14];

	if (!vehicle.IsNull()) _vehicle = vehicle.GetString();
	else if (setDefaults) _vehicle = vehicleList[rand() % 3];

	if (drivingMode.IsArray()) {
		if (!drivingMode[0].IsNull()) _drivingMode = drivingMode[0].GetInt();
		else if (setDefaults)  _drivingMode = rand() % 4294967296;
		if (drivingMode[1].IsNull()) _setSpeed = drivingMode[1].GetFloat(); 
		else if (setDefaults) _setSpeed = 1.0*(rand() % 10) + 10;
	}
	else if (setDefaults) {
		_drivingMode = -1;
	}
}

void Scenario::parseDatasetConfig(const Value& dc, bool setDefaults) {
	if (!dc["rate"].IsNull()) rate = dc["rate"].GetFloat();
	else if (setDefaults) rate = _RATE_;

    if (!dc["startIndex"].IsNull()) {
        instance_index = dc["startIndex"].GetInt();
        baseTrackingIndex = instance_index;
    }

    char temp[] = "%06d";
    char strComp[sizeof temp + 100];
    sprintf(strComp, temp, instance_index);
    instance_string = strComp;

    if (!dc["lidarParam"].IsNull()) lidar_param = dc["lidarParam"].GetInt();
	
	if (!dc["frame"].IsNull()) {
		if (!dc["frame"][0].IsNull()) width = dc["frame"][0].GetInt();
		else if (setDefaults) width = _WIDTH_;

		if (!dc["frame"][1].IsNull()) height = dc["frame"][1].GetInt();
		else if (setDefaults) height = _HEIGHT_;
	}
	else if (setDefaults) {
		width = _WIDTH_;
		height = _HEIGHT_;
	}	

	if (!dc["vehicles"].IsNull()) vehicles = dc["vehicles"].GetBool();
	else if (setDefaults) vehicles = _VEHICLES_;

	if (!dc["peds"].IsNull()) peds = dc["peds"].GetBool();
	else if (setDefaults) peds = _PEDS_;

	if (!dc["trafficSigns"].IsNull()) trafficSigns = dc["trafficSigns"].GetBool();
	else if (setDefaults) trafficSigns = _TRAFFIC_SIGNS_;

	if (!dc["direction"].IsNull()) {
		direction = true;
		if (!dc["direction"][0].IsNull()) dir.x = dc["direction"][0].GetFloat();
		else if (setDefaults) direction = _DIRECTION_;

		if (!dc["direction"][1].IsNull()) dir.y = dc["direction"][1].GetFloat();
		else if (setDefaults) direction = _DIRECTION_;

		if (!dc["direction"][2].IsNull()) dir.z = dc["direction"][2].GetFloat();
		else if (setDefaults) direction = _DIRECTION_;
	}
	else if (setDefaults) direction = _DIRECTION_;

	if (dc["reward"].IsArray()) {
		if (dc["reward"][0].IsFloat() && dc["reward"][1].IsFloat()) {
			rewarder = new GeneralRewarder((char*)(GetCurrentModulePath() + "paths.xml").c_str(), dc["reward"][0].GetFloat(), dc["reward"][1].GetFloat());
			reward = true;
		}
		else if (setDefaults) reward = _REWARD_;
	}
	else if (setDefaults) reward = _REWARD_;

	if (!dc["throttle"].IsNull()) throttle = dc["throttle"].GetBool();
	else if (setDefaults) throttle = _THROTTLE_;
	if (!dc["brake"].IsNull()) brake = dc["brake"].GetBool();
	else if (setDefaults) brake = _BRAKE_;
	if (!dc["steering"].IsNull()) steering = dc["steering"].GetBool();
	else if (setDefaults) steering = _STEERING_;
	if (!dc["speed"].IsNull()) speed = dc["speed"].GetBool();
	else if (setDefaults) speed = _SPEED_;
	if (!dc["yawRate"].IsNull()) yawRate = dc["yawRate"].GetBool();
	else if (setDefaults) yawRate = _YAW_RATE_;
	if (!dc["drivingMode"].IsNull()) drivingMode = dc["drivingMode"].GetBool();
	else if (setDefaults) drivingMode = _DRIVING_MODE_;
	if (!dc["location"].IsNull()) location = dc["location"].GetBool();
	else if (setDefaults) location = _LOCATION_;
	if (!dc["time"].IsNull()) time = dc["time"].GetBool();
	else if (setDefaults) time = _TIME_;
    if (!dc["offscreen"].IsNull()) offscreen = dc["offscreen"].GetBool();
    else if (setDefaults) offscreen = _OFFSCREEN_;
    if (!dc["showBoxes"].IsNull()) showBoxes = dc["showBoxes"].GetBool();
    else if (setDefaults) showBoxes = _SHOWBOXES_;
    if (!dc["pointclouds"].IsNull()) pointclouds = dc["pointclouds"].GetBool();
    else if (setDefaults) pointclouds = _POINTCLOUDS_;
    if (!dc["stationaryScene"].IsNull()) stationaryScene = dc["stationaryScene"].GetBool();
    else if (setDefaults) stationaryScene = _STATIONARY_SCENE_;
    if (!dc["collectTracking"].IsNull()) collectTracking = dc["collectTracking"].GetBool();
    else if (setDefaults) collectTracking = _COLLECT_TRACKING_;

    //Export folder for pointcloud/depth map
    baseFolder = "E:\\data\\";
    if (collectTracking) {
        baseFolder += "tracking\\";
    }

    if (stationaryScene) {
        vehiclesToCreate.clear();
        log("About to get vehicles");
        if (!dc["vehiclesToCreate"].IsNull()) {
            log("Vehicles non-null");
            const rapidjson::Value& jsonVehicles = dc["vehiclesToCreate"];
            for (rapidjson::SizeType i = 0; i < jsonVehicles.Size(); i++) {
                log("At least one");
                bool noHit = false;
                VehicleToCreate vehicleToCreate;
                const rapidjson::Value& jVeh = jsonVehicles[i];

                if (!jVeh[0].IsNull()) vehicleToCreate.model = jVeh[0].GetString();
                if (!jVeh[1].IsNull()) vehicleToCreate.forward = jVeh[1].GetFloat();
                if (!jVeh[2].IsNull()) vehicleToCreate.right = jVeh[2].GetFloat();
                if (!jVeh[3].IsNull()) vehicleToCreate.heading = jVeh[3].GetFloat();
                if (!jVeh[4].IsNull()) vehicleToCreate.color = jVeh[4].GetInt();
                if (!jVeh[5].IsNull()) vehicleToCreate.color2 = jVeh[5].GetInt();
                else noHit = true;

                if (!noHit) {
                    log("Pushing back vehicle");
                    vehiclesToCreate.push_back(vehicleToCreate);
                }
            }
        }
        pedsToCreate.clear();
        log("About to get ped");
        if (!dc["pedsToCreate"].IsNull()) {
            log("ped non-null");
            const rapidjson::Value& jsonPeds = dc["pedsToCreate"];
            for (rapidjson::SizeType i = 0; i < jsonPeds.Size(); i++) {
                log("At least one");
                bool noHit = false;
                PedToCreate pedToCreate;
                const rapidjson::Value& jPed = jsonPeds[i];

                if (!jPed[0].IsNull()) pedToCreate.model = jPed[0].GetInt();
                if (!jPed[1].IsNull()) pedToCreate.forward = jPed[1].GetFloat();
                if (!jPed[2].IsNull()) pedToCreate.right = jPed[2].GetFloat();
                if (!jPed[3].IsNull()) pedToCreate.heading = jPed[3].GetFloat();
                else noHit = true;

                if (!noHit) {
                    log("Pushing back ped");
                    pedsToCreate.push_back(pedToCreate);
                }
            }
        }
    }

    //Create camera intrinsics matrix
    calcCameraIntrinsics();

	//Create JSON DOM
	d.SetObject();
	Document::AllocatorType& allocator = d.GetAllocator();
	Value a(kArrayType);

	if (vehicles) d.AddMember("vehicles", a, allocator);
	if (peds) d.AddMember("peds", a, allocator);
	if (trafficSigns) d.AddMember("trafficSigns", a, allocator);
	if (direction) d.AddMember("direction", a, allocator);
	if (reward) d.AddMember("reward", 0.0, allocator);
	if (throttle) d.AddMember("throttle", 0.0, allocator);
	if (brake) d.AddMember("brake", 0.0, allocator);
	if (steering) d.AddMember("steering", 0.0, allocator);
	if (speed) d.AddMember("speed", 0.0, allocator);
	if (yawRate) d.AddMember("yawRate", 0.0, allocator);
	if (drivingMode) d.AddMember("drivingMode", 0, allocator);
	if (location) d.AddMember("location", a, allocator);
	if (time) d.AddMember("time", 0, allocator);
    d.AddMember("index", 0, allocator);
    d.AddMember("focalLen", 0.0, allocator);
    d.AddMember("curPosition", a, allocator);
    d.AddMember("seriesIndex", a, allocator);

	screenCapturer = new ScreenCapturer(width, height);
}

void Scenario::buildScenario() {
	Vector3 pos, rotation;
	Hash vehicleHash;
	float heading;

    if (!stationaryScene) {
        GAMEPLAY::SET_RANDOM_SEED(std::time(NULL));
        while (!PATHFIND::_0xF7B79A50B905A30D(-8192.0f, 8192.0f, -8192.0f, 8192.0f)) WAIT(0);
        PATHFIND::GET_CLOSEST_VEHICLE_NODE_WITH_HEADING(x, y, 0, &pos, &heading, 0, 0, 0);
    }

	ENTITY::DELETE_ENTITY(&vehicle);
	vehicleHash = GAMEPLAY::GET_HASH_KEY((char*)_vehicle);
	STREAMING::REQUEST_MODEL(vehicleHash);
	while (!STREAMING::HAS_MODEL_LOADED(vehicleHash)) WAIT(0);
    if (stationaryScene) {
        pos.x = x;
        pos.y = y;
        pos.z = 0;
        heading = startHeading;
        std::ostringstream oss;
        oss << "Start heading: " << startHeading;
        std::string str = oss.str();
        log(str);
        vehicles_created = false;
    }
	while (!ENTITY::DOES_ENTITY_EXIST(vehicle)) {
		vehicle = VEHICLE::CREATE_VEHICLE(vehicleHash, pos.x, pos.y, pos.z, heading, FALSE, FALSE);
		WAIT(0);
	}
	VEHICLE::SET_VEHICLE_ON_GROUND_PROPERLY(vehicle);

	while (!ENTITY::DOES_ENTITY_EXIST(ped)) {
		ped = PLAYER::PLAYER_PED_ID();
		WAIT(0);
	}

	player = PLAYER::PLAYER_ID();
	PLAYER::START_PLAYER_TELEPORT(player, pos.x, pos.y, pos.z, heading, 0, 0, 0);
	while (PLAYER::IS_PLAYER_TELEPORT_ACTIVE()) WAIT(0);

	PED::SET_PED_INTO_VEHICLE(ped, vehicle, -1);
	STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(vehicleHash);

	TIME::SET_CLOCK_TIME(hour, minute, 0);

	GAMEPLAY::SET_WEATHER_TYPE_NOW_PERSIST((char*)_weather);

	rotation = ENTITY::GET_ENTITY_ROTATION(vehicle, 1);
	CAM::DESTROY_ALL_CAMS(TRUE);
	camera = CAM::CREATE_CAM("DEFAULT_SCRIPTED_CAMERA", TRUE);
	if (strcmp(_vehicle, "packer") == 0) CAM::ATTACH_CAM_TO_ENTITY(camera, vehicle, 0, 2.35, 1.7, TRUE);
	else CAM::ATTACH_CAM_TO_ENTITY(camera, vehicle, 0, CAM_OFFSET_FORWARD, CAM_OFFSET_UP, TRUE);
	CAM::SET_CAM_FOV(camera, VERT_CAM_FOV);
	CAM::SET_CAM_ACTIVE(camera, TRUE);
	CAM::SET_CAM_ROT(camera, rotation.x, rotation.y, rotation.z, 1);
	CAM::SET_CAM_INHERIT_ROLL_VEHICLE(camera, TRUE);

    if (stationaryScene) {
        _setSpeed = 0;
    }

    CAM::RENDER_SCRIPT_CAMS(TRUE, FALSE, 0, TRUE, TRUE);

	AI::CLEAR_PED_TASKS(ped);
	if (_drivingMode >= 0 && !stationaryScene) AI::TASK_VEHICLE_DRIVE_WANDER(ped, vehicle, _setSpeed, _drivingMode);
}

void Scenario::start(const Value& sc, const Value& dc) {
	if (running) return;

	//Parse options
	srand(std::time(NULL));
	parseScenarioConfig(sc, true);
	parseDatasetConfig(dc, true);

	//Build scenario
	buildScenario();

	running = true;
	lastSafetyCheck = std::clock();
}

void Scenario::config(const Value& sc, const Value& dc) {
	if (!running) return;

	running = false;

	//Parse options
	srand(std::time(NULL));
	parseScenarioConfig(sc, false);
	parseDatasetConfig(dc, false);

	//Build scenario
	buildScenario();

	running = true;
	lastSafetyCheck = std::clock();
}

void Scenario::run() {
	if (running) {
		std::clock_t now = std::clock();

		Vector3 rotation = ENTITY::GET_ENTITY_ROTATION(vehicle, 1);
        //TODO Should this be in here? Shouldn't cam automatically rotate with vehicle?
		CAM::SET_CAM_ROT(camera, rotation.x, rotation.y, rotation.z, 1);

		if (_drivingMode < 0) {
			CONTROLS::_SET_CONTROL_NORMAL(27, 71, currentThrottle); //[0,1]
			CONTROLS::_SET_CONTROL_NORMAL(27, 72, currentBrake); //[0,1]
			CONTROLS::_SET_CONTROL_NORMAL(27, 59, currentSteering); //[-1,1]
		}
		
		float delay = ((float)(now - lastSafetyCheck)) / CLOCKS_PER_SEC;
		if (delay > 10) {
			lastSafetyCheck = std::clock();
			//Avoid bad things such as getting killed by the police, robbed, dying in car accidents or other horrible stuff
			PLAYER::SET_EVERYONE_IGNORE_PLAYER(player, TRUE);
			PLAYER::SET_POLICE_IGNORE_PLAYER(player, TRUE);
			PLAYER::CLEAR_PLAYER_WANTED_LEVEL(player); // Never wanted

			// Put on seat belt
			PED::SET_PED_CONFIG_FLAG(ped, 32, FALSE);

			// Invincible vehicle
			VEHICLE::SET_VEHICLE_TYRES_CAN_BURST(vehicle, FALSE);
			VEHICLE::SET_VEHICLE_WHEELS_CAN_BREAK(vehicle, FALSE);
			VEHICLE::SET_VEHICLE_HAS_STRONG_AXLES(vehicle, TRUE);

			VEHICLE::SET_VEHICLE_CAN_BE_VISIBLY_DAMAGED(vehicle, FALSE);
			ENTITY::SET_ENTITY_INVINCIBLE(vehicle, TRUE);
			ENTITY::SET_ENTITY_PROOFS(vehicle, 1, 1, 1, 1, 1, 1, 1, 1);

			// Player invincible
			PLAYER::SET_PLAYER_INVINCIBLE(player, TRUE);

			// Driving characteristics
			PED::SET_DRIVER_AGGRESSIVENESS(ped, 0.0);
			PED::SET_DRIVER_ABILITY(ped, 100.0);

            //Setup LiDAR before collecting
            setupLiDAR();

            //Create vehicles if it is a stationary scenario
            createVehicles();
		}
	}
	scriptWait(0);
}

void Scenario::stop() {
	if (!running) return;
	running = false;
	CAM::DESTROY_ALL_CAMS(TRUE);
	CAM::RENDER_SCRIPT_CAMS(FALSE, TRUE, 500, FALSE, FALSE);
	AI::CLEAR_PED_TASKS(ped);
	setCommands(0.0, 0.0, 0.0);
}

void Scenario::setCommands(float throttle, float brake, float steering) {
	currentThrottle = throttle;
	currentBrake = brake;
	currentSteering = steering;
}

StringBuffer Scenario::generateMessage() {
	StringBuffer buffer;
	buffer.Clear();
	Writer<StringBuffer> writer(buffer);
	
    log("About to pause game");
    GAMEPLAY::SET_GAME_PAUSED(true);
    //Time synchronization seems to be correct with 2 render calls
    CAM::RENDER_SCRIPT_CAMS(TRUE, FALSE, 0, FALSE, FALSE);
    scriptWait(0);
    CAM::RENDER_SCRIPT_CAMS(TRUE, FALSE, 0, FALSE, FALSE);
    scriptWait(0);
    log("Script cams rendered");
	screenCapturer->capture();
    log("Screen captured");

    //TODO pass this through
    bool depthMap = true;

    setIndex();
    setPosition();
    //if (depthMap && lidar_initialized && m_prevDepth) setDepthBuffer(true);
    if (depthMap && lidar_initialized) setDepthBuffer();
    if (pointclouds && lidar_initialized) collectLiDAR();
	if (vehicles) setVehiclesList();
	if (peds) setPedsList();
	if (trafficSigns); //TODO
	if (direction) setDirection();
	if (reward) setReward();
	if (throttle) setThrottle();
	if (brake) setBrake();
	if (steering) setSteering();
	if (speed) setSpeed();
	if (yawRate) setYawRate();
	if (drivingMode); //TODO
	if (location) setLocation();
	if (time) setTime();
    setFocalLength();
    increaseIndex();
    GAMEPLAY::SET_GAME_PAUSED(false);

	d.Accept(writer);

	return buffer;
}

//Returns the angle between a relative position vector and the forward vector (rotated about up axis)
float Scenario::observationAngle(Vector3 position) {
    float x1 = currentRightVector.x;
    float y1 = currentRightVector.y;
    float z1 = currentRightVector.z;
    float x2 = position.x;
    float y2 = position.y;
    float z2 = position.z;
    float xn = currentUpVector.x;
    float yn = currentUpVector.y;
    float zn = currentUpVector.z;

    float dot = x1 * x2 + y1 * y2 + z1 * z2;
    float det = x1 * y2*zn + x2 * yn*z1 + xn * y1*z2 - z1 * y2*xn - z2 * yn*x1 - zn * y1*x2;
    float observationAngle = atan2(det, dot);

    if (DEBUG_LOGGING) {
        std::ostringstream oss;
        oss << "Forward is: " << x1 << ", " << y1 << ", " << z1 <<
            "\nNormal is: " << x2 << ", " << y2 << ", " << z2 <<
            "\nPosition is: " << position.x << ", " << position.y << ", " << position.z << " and angle is: " << observationAngle;
        std::string str = oss.str();
        log(str);
    }

    return observationAngle;
}

void Scenario::drawVectorFromPosition(Vector3 vector, int blue, int green) {
    GRAPHICS::DRAW_LINE(currentPos.x, currentPos.y, currentPos.z, vector.x*1000 + currentPos.x, vector.y * 1000 + currentPos.y, vector.z * 1000 + currentPos.z, 0, green, blue, 200);
    WAIT(0);
}

//Saves the position and vectors of the capture vehicle
void Scenario::setPosition() {
    //NOTE: The forward and right vectors are swapped (compared to native function labels) to keep consistency with coordinate system
    ENTITY::GET_ENTITY_MATRIX(vehicle, &currentForwardVector, &currentRightVector, &currentUpVector, &currentPos); //Blue or red pill
    float heading = GAMEPLAY::GET_HEADING_FROM_VECTOR_2D(currentForwardVector.x, currentForwardVector.y);

    Value _vector(kArrayType);
    Document::AllocatorType& allocator = d.GetAllocator();

    _vector.SetArray();
    _vector.PushBack(currentPos.x, allocator).PushBack(currentPos.y, allocator).PushBack(currentPos.z, allocator);

    float roll = atan2(currentUpVector.z, -currentUpVector.x);
    float pitch = atan2(-currentForwardVector.z, currentForwardVector.y);
    //Should use heading2 over heading
    float heading2 = atan2(currentForwardVector.y, currentForwardVector.x);

    _vector.PushBack(roll, allocator).PushBack(pitch, allocator).PushBack(heading2, allocator).PushBack(heading, allocator);

    d["curPosition"] = _vector;
}

BBox2D Scenario::BBox2DFrom3DObject(Vector3 position, Vector3 dim, Vector3 forwardVector, Vector3 rightVector, Vector3 upVector) {
    BBox2D bbox;
    bbox.left = 1.0;// width;
    bbox.right = 0.0;
    bbox.top = 1.0;// height;
    bbox.bottom = 0.0;
    //return bbox;
    //TODO Figure out how to transform world to screen coordinates
    for (int right = -1; right <= 1; right += 2) {
        for (int forward = -1; forward <= 1; forward += 2) {
            for (int up = -1; up <= 1; up += 2) {
                Vector3 pos;
                pos.x = position.x + forward * dim.y*forwardVector.x + right * dim.x*rightVector.x + up * dim.z*upVector.x;
                pos.y = position.y + forward * dim.y*forwardVector.y + right * dim.x*rightVector.y + up * dim.z*upVector.y;
                pos.z = position.z + forward * dim.y*forwardVector.z + right * dim.x*rightVector.z + up * dim.z*upVector.z;

                float scrX, scrY;
                bool success = UI::_0xF9904D11F1ACBEC3(pos.x, pos.y, pos.z, &scrX, &scrY);

                float screenX = scrX;// *width;
                float screenY = scrY;// *height;

                std::ostringstream oss2;
                oss2 << "ScreenX: " << screenX << " ScreenY: " << screenY;
                std::string str2 = oss2.str();
                log(str2);

                if (screenX < 0 || screenY < 0 || screenX > 1 || screenY > 1) {
                    log("Screen position is out of bounds.");
                }
                if (true) {//Why does the function always return false
                    if (screenX < bbox.left) bbox.left = screenX;
                    if (screenX > bbox.right) bbox.right = screenX;
                    if (screenY < bbox.top) bbox.top = screenY;
                    if (screenY > bbox.bottom) bbox.bottom = screenY;
                }
                else {
                    log("screen position from world returned false");
                }
            }
        }
    }

    std::ostringstream oss2;
    oss2 << "BBox left: " << bbox.left << " right: " << bbox.right << " top: " << bbox.top << " bot: " << bbox.bottom << std::endl <<
        "PosX: " << bbox.posX() << " PosY: " << bbox.posY() << " Width: " << bbox.width() << " Height: " << bbox.height();
    std::string str2 = oss2.str();
    log(str2);
    if (showBoxes) {
        GRAPHICS::DRAW_RECT(bbox.posX(), bbox.posY(), bbox.width(), bbox.height(), 0, 255, 0, 100);
        WAIT(0);
    }
    return bbox;
}

bool Scenario::getEntityVector(Value &_entity, Document::AllocatorType& allocator, int entityID, Hash model, int classid) {
    bool success = false;

    Vector3 FUR; //Front Upper Right
    Vector3 BLL; //Back Lower Lelft
    Vector3 dim; //Vehicle dimensions
    Vector3 upVector, rightVector, forwardVector, position; //Vehicle position
    Vector3 min;
    Vector3 max;
    Vector3 speedVector;
    float heading, speed;

    Vector3 theta = CAM::GET_CAM_ROT(camera, 0);
    Vector3 cam_pos = CAM::GET_CAM_COORD(camera);
    float fov = CAM::GET_CAM_FOV(camera);
    float near_clip = CAM::GET_CAM_NEAR_CLIP(camera);

    bool isOnScreen = ENTITY::IS_ENTITY_ON_SCREEN(entityID);
    if (offscreen || isOnScreen) {
        //Check if it is in screen
        ENTITY::GET_ENTITY_MATRIX(entityID, &forwardVector, &rightVector, &upVector, &position); //Blue or red pill
        float distance = sqrt(SYSTEM::VDIST2(currentPos.x, currentPos.y, currentPos.z, position.x, position.y, position.z));
        if (distance < 150) {
            //An attempt to try to hit vehicles reliably past 30m
            ENTITY::SET_ENTITY_LOD_DIST(entityID, 0xFFFF);

            int pointsHit = 0;
            float maxBack = 0;
            float maxFront = 0;
            if (entitiesHit.find(entityID) != entitiesHit.end()) {
                HitLidarEntity* hitLidarEnt = entitiesHit[entityID];
                pointsHit = hitLidarEnt->pointsHit;
                maxBack = hitLidarEnt->maxBack;
                maxFront = hitLidarEnt->maxFront;
            }

            if (ENTITY::HAS_ENTITY_CLEAR_LOS_TO_ENTITY(vehicle, entityID, 19) || pointsHit > 0) {
                success = true;
                //Check if we see it (not occluded)
                GAMEPLAY::GET_MODEL_DIMENSIONS(model, &min, &max);

                speedVector = ENTITY::GET_ENTITY_SPEED_VECTOR(entityID, false);
                speed = ENTITY::GET_ENTITY_SPEED(entityID);
                if (speed > 0) {
                    heading = GAMEPLAY::GET_HEADING_FROM_VECTOR_2D(speedVector.x - currentForwardVector.x, speedVector.y - currentForwardVector.y);
                }
                else {
                    heading = GAMEPLAY::GET_HEADING_FROM_VECTOR_2D(forwardVector.x - currentForwardVector.x, forwardVector.y - currentForwardVector.y);
                }

                //Calculate size
                dim.x = 0.5*(max.x - min.x);
                dim.y = 0.5*(max.y - min.y);
                dim.z = 0.5*(max.z - min.z);

                BBox2D bbox2d = BBox2DFrom3DObject(position, dim, forwardVector, rightVector, upVector);
                float min_x = 2;
                float min_y = 2;
                float max_x = -1;
                float max_y = -1;
                Eigen::Matrix3f R;
                R.col(0) = Eigen::Vector3f(forwardVector.x, forwardVector.y, forwardVector.z);
                R.col(1) = Eigen::Vector3f(rightVector.x, rightVector.y, rightVector.z);
                R.col(2) = Eigen::Vector3f(upVector.x, upVector.y, upVector.z);
                for (int i = 0; i < 8; ++i) {
                    Eigen::Vector3f pt = Eigen::Vector3f(position.x, position.y, position.z) + R * Eigen::Vector3f(dim.x, dim.y, dim.z).cwiseProduct(2 * coefficients[i]);
                    Eigen::Vector2f uv = get_2d_from_3d(pt,
                        Eigen::Vector3f(cam_pos.x, cam_pos.y, cam_pos.z),
                        Eigen::Vector3f(theta.x, theta.y, theta.z), near_clip, fov);
                    min_x = min(uv(0), min_x);
                    min_y = min(uv(1), min_y);
                    max_x = max(uv(0), max_x);
                    max_y = max(uv(1), max_y);
                }
                //Set the 2d bbox to eigen-calculated values
                bbox2d.bottom = max_y;bbox2d.top = min_y;bbox2d.right = max_x;bbox2d.left = min_x;

                //Amount dimensions are offcenter
                Vector3 offcenter;
                offcenter.x = 0;// max.x + min.x;
                offcenter.y = 0;// max.y + min.y;
                offcenter.z = min.z; //KITTI position is at object ground plane

                //Correct offcenter with LiDAR info
                if (maxFront > dim.y) {
                    offcenter.y = maxFront - dim.y;
                    log("Hit offcenter from front.");
                }
                if (-maxBack > dim.y) {
                    offcenter.y = dim.y + maxBack;
                    log("Hit offcenter from back.");
                }

                Vector3 offcenterPosition = convertCoordinateSystem(offcenter, forwardVector, rightVector, upVector);

                //Seems like the offcenter is not actually correct
                //Update object position to be consistent with KITTI (symmetrical dimensions except for z which is ground)
                position.x = position.x + offcenterPosition.x;
                position.y = position.y + offcenterPosition.y;
                position.z = position.z + offcenterPosition.z;

                //Kitti dimensions
                float kittiHeight = 2 * dim.z;
                float kittiWidth = 2 * dim.x;
                float kittiLength = 2 * dim.y;
                
                if (abs(offcenter.y) > 0.5 && classid == 0) {
                    std::ostringstream oss2;
                    oss2 << "Instance Index: " << instance_index << " Dimensions are: " << dim.x << ", " << dim.y << ", " << dim.z;
                    oss2 << "\nMax: " << max.x << ", " << max.y << ", " << max.z;
                    oss2 << "\nMin: " << min.x << ", " << min.y << ", " << min.z;
                    oss2 << "\noffset: " << offcenter.x << ", " << offcenter.y << ", " << offcenter.z;
                    std::string str2 = oss2.str();
                    log(str2);
                }

                Vector3 relativePos;
                relativePos.x = position.x - currentPos.x;
                relativePos.y = position.y - currentPos.y;
                relativePos.z = position.z - currentPos.z;

                Vector3 kittiForwardVector = convertCoordinateSystem(forwardVector, currentForwardVector, currentRightVector, currentUpVector);
                float rot_y = -atan2(kittiForwardVector.y, kittiForwardVector.x);

                relativePos = convertCoordinateSystem(relativePos, currentForwardVector, currentRightVector, currentUpVector);

                //Update object position to be consistent with KITTI (symmetrical dimensions except for z which is ground)
                relativePos.y = relativePos.y - CAM_OFFSET_FORWARD;
                relativePos.z = relativePos.z - CAM_OFFSET_UP;

                //Convert to KITTI camera coordinates
                Vector3 kittiPos;
                kittiPos.x = relativePos.x;
                kittiPos.y = -relativePos.z;
                kittiPos.z = relativePos.y;

                //See cs_overview.pdf in the kitti tracking devkit
                //for details on the calculation of alpha
                //TODO: Need to check this
                float beta_kitti = -atan2(kittiPos.z, kittiPos.x);
                float alpha_kitti = -(rot_y + beta_kitti);
                /*std::ostringstream oss3;
                oss3 << "alpha_kitti: " << alpha_kitti << " rot_y: " << rot_y << " beta_kitti: " << beta_kitti
                    << "\nkittiPos.x: " << kittiPos.x << " kittiPos.y: " << kittiPos.y
                    << "\kittiForwardVector: " << kittiForwardVector.x << " kittiForwardVector.y: " << kittiForwardVector.y;
                std::string str3 = oss3.str();
                log(str3);*/

                Value _vector(kArrayType);
                _entity.AddMember("speed", speed, allocator).AddMember("heading", heading, allocator).AddMember("classID", classid, allocator);
                _entity.AddMember("offscreen", offscreen, allocator);
                _vector.SetArray();
                _vector.PushBack(kittiHeight, allocator).PushBack(kittiWidth, allocator).PushBack(kittiLength, allocator);
                _entity.AddMember("dimensions", _vector, allocator);
                _vector.SetArray();
                _vector.PushBack(offcenter.z, allocator).PushBack(offcenter.x, allocator).PushBack(offcenter.y, allocator);
                _entity.AddMember("offcenter", _vector, allocator);
                _vector.SetArray();
                _vector.PushBack(kittiPos.x, allocator).PushBack(kittiPos.y, allocator).PushBack(kittiPos.z, allocator);
                _entity.AddMember("location", _vector, allocator);
                _entity.AddMember("rotation_y", rot_y, allocator);
                _entity.AddMember("alpha", alpha_kitti, allocator);
                _entity.AddMember("entityID", entityID, allocator);
                _entity.AddMember("distance", distance, allocator);
                _vector.SetArray();
                _vector.PushBack(bbox2d.left*width, allocator).PushBack(bbox2d.top*height, allocator).PushBack(bbox2d.right*width, allocator).PushBack(bbox2d.bottom*height, allocator);
                _entity.AddMember("bbox2d", _vector, allocator);
                _entity.AddMember("pointsHit", pointsHit, allocator);

                if (trackFirstFrame.find(entityID) == trackFirstFrame.end()) {
                    trackFirstFrame.insert(std::pair<int, int>(entityID, instance_index));
                }
                _entity.AddMember("trackFirstFrame", trackFirstFrame[entityID], allocator);

                drawBoxes(BLL, FUR, dim, upVector, rightVector, forwardVector, position, 1);
            }
        }
    }

    return success;
}

void Scenario::setVehiclesList() {
    log("Setting vehicles list.");
	const int ARR_SIZE = 1024;
	Vehicle vehicles[ARR_SIZE];
	Value _vehicles(kArrayType);
	Document::AllocatorType& allocator = d.GetAllocator();

    Hash model;
	int classid;

	int count = worldGetAllVehicles(vehicles, ARR_SIZE);
	for (int i = 0; i < count; i++) {
		if (vehicles[i] == vehicle) continue; //Don't process own car!

        model = ENTITY::GET_ENTITY_MODEL(vehicles[i]);
        if (VEHICLE::IS_THIS_MODEL_A_CAR(model)) classid = 0;
        else if (VEHICLE::IS_THIS_MODEL_A_BIKE(model)) classid = 1;
        else if (VEHICLE::IS_THIS_MODEL_A_BICYCLE(model)) classid = 2;
        else if (VEHICLE::IS_THIS_MODEL_A_QUADBIKE(model)) classid = 3;
        else if (VEHICLE::IS_THIS_MODEL_A_BOAT(model)) classid = 4;
        else if (VEHICLE::IS_THIS_MODEL_A_PLANE(model)) classid = 5;
        else if (VEHICLE::IS_THIS_MODEL_A_HELI(model)) classid = 6;
        else if (VEHICLE::IS_THIS_MODEL_A_TRAIN(model)) classid = 7;
        else if (VEHICLE::_IS_THIS_MODEL_A_SUBMERSIBLE(model)) classid = 8;
        else classid = 9; //unknown (ufo?)

        Value _vehicle(kObjectType);
        bool success = getEntityVector(_vehicle, allocator, vehicles[i], model, classid);
        if (success) {
            _vehicles.PushBack(_vehicle, allocator);
        }
	}
			
	d["vehicles"] = _vehicles;
}

void Scenario::setPedsList(){
    log("Setting peds list.");
	const int ARR_SIZE = 1024;
	Ped peds[ARR_SIZE];
	Value _peds(kArrayType);
	Document::AllocatorType& allocator = d.GetAllocator();

	Hash model;
	int classid;

	int count = worldGetAllPeds(peds, ARR_SIZE);
	for (int i = 0; i < count; i++) {
		if (PED::IS_PED_IN_ANY_VEHICLE(peds[i], TRUE)) continue; //Don't process peds in vehicles!

        if (PED::GET_PED_TYPE(peds[i]) == 28) classid = 11; //animal
        else classid = 10;

        if (RETAIN_ANIMALS || classid != 11) {
            model = ENTITY::GET_ENTITY_MODEL(peds[i]);

            Value _ped(kObjectType);
            bool success = getEntityVector(_ped, allocator, peds[i], model, classid);
            if (success) {
                _peds.PushBack(_ped, allocator);
            }
        }
	}		
	d["peds"] = _peds;
}


void Scenario::setThrottle(){
	d["throttle"] = getFloatValue(vehicle, 0x92C);
}

void Scenario::setBrake(){
	d["brake"] = getFloatValue(vehicle, 0x930);
}

void Scenario::setSteering(){
	d["steering"] = -getFloatValue(vehicle, 0x924) / 0.6981317008;
}

void Scenario::setSpeed(){
	d["speed"] = ENTITY::GET_ENTITY_SPEED(vehicle);
}

void Scenario::setYawRate(){
	Vector3 rates = ENTITY::GET_ENTITY_ROTATION_VELOCITY(vehicle);
	d["yawRate"] = rates.z*180.0 / 3.14159265359;
}

void Scenario::setLocation(){
	Document::AllocatorType& allocator = d.GetAllocator();
	Vector3 pos = ENTITY::GET_ENTITY_COORDS(vehicle, false);
	Value location(kArrayType);
	location.PushBack(pos.x, allocator).PushBack(pos.y, allocator).PushBack(pos.z, allocator);
	d["location"] = location;
}

void Scenario::setTime(){
	d["time"] = TIME::GET_CLOCK_HOURS();
}

void Scenario::setDirection(){
	int direction;
	float distance;
	Vehicle temp_vehicle;
	Document::AllocatorType& allocator = d.GetAllocator();
	PATHFIND::GENERATE_DIRECTIONS_TO_COORD(dir.x, dir.y, dir.z, TRUE, &direction, &temp_vehicle, &distance);
	Value _direction(kArrayType);
	_direction.PushBack(direction, allocator).PushBack(distance, allocator);
	d["direction"] = _direction;
}

void Scenario::setReward() {
	d["reward"] = rewarder->computeReward(vehicle);
}

void Scenario::createVehicle(const char* model, float relativeForward, float relativeRight, float heading, int color, int color2) {
    Hash vehicleHash = GAMEPLAY::GET_HASH_KEY(const_cast<char*>(model));
    Vector3 pos;
    pos.x = currentPos.x + currentForwardVector.x * relativeForward + currentRightVector.x * relativeRight;
    pos.y = currentPos.y + currentForwardVector.y * relativeForward + currentRightVector.y * relativeRight;
    pos.z = currentPos.z + currentForwardVector.z * relativeForward + currentRightVector.z * relativeRight;
    STREAMING::REQUEST_MODEL(vehicleHash);
    while (!STREAMING::HAS_MODEL_LOADED(vehicleHash)) WAIT(0);
    Vehicle tempV = VEHICLE::CREATE_VEHICLE(vehicleHash, pos.x, pos.y, pos.z, heading, FALSE, FALSE);
    WAIT(0);
    if (color != -1) {
        VEHICLE::SET_VEHICLE_COLOURS(tempV, color, color2);
    }
    VEHICLE::SET_VEHICLE_ON_GROUND_PROPERLY(tempV);
    ENTITY::SET_ENTITY_AS_NO_LONGER_NEEDED(&tempV);
}

void Scenario::createPed(int model, float relativeForward, float relativeRight, float heading, int task) {
    //Ped hashes found at: https://www.se7ensins.com/forums/threads/request-pc-ped-hashes.1317848/
    Hash hash = 0x505603B9;// GAMEPLAY::GET_HASH_KEY(const_cast<char*>(model));
    Vector3 pos;
    pos.x = currentPos.x + currentForwardVector.x * relativeForward + currentRightVector.x * relativeRight;
    pos.y = currentPos.y + currentForwardVector.y * relativeForward + currentRightVector.y * relativeRight;
    pos.z = currentPos.z + currentForwardVector.z * relativeForward + currentRightVector.z * relativeRight;
    STREAMING::REQUEST_MODEL(hash);
    while (!STREAMING::HAS_MODEL_LOADED(hash)) WAIT(0);
    Ped temp = PED::CREATE_PED(4, hash, pos.x, pos.y, pos.z, heading, FALSE, FALSE);
    WAIT(0);
    AI::TASK_STAND_STILL(temp, -1);
    if (task == 0) {
        PED::SET_PED_DUCKING(temp, true);
    }
    else if (task == 1) {
        PED::SET_PED_PINNED_DOWN(temp, true, -1);
    }
    else if (task == 2) {
        AI::TASK_WRITHE(temp, player, 999999, false);
    }
    ENTITY::SET_ENTITY_AS_NO_LONGER_NEEDED(&temp);
}

void Scenario::createVehicles() {
    setPosition();
    if (stationaryScene && !vehicles_created) {
        log("Creating peds");
        for (int i = 0; i < pedsToCreate.size(); i++) {
            PedToCreate p = pedsToCreate[i];
            createPed(p.model, p.forward, p.right, p.heading, i);
        }
        log("Creating vehicles");
        for (int i = 0; i < vehiclesToCreate.size(); i++) {
            VehicleToCreate v = vehiclesToCreate[i];
            createVehicle(v.model.c_str(), v.forward, v.right, v.heading, v.color, v.color2);
        }
        /*createVehicle("benson", 20.0, 0.0, 60.0);
        createVehicle("voltic", 15.0, 5.0, 180.0);*/
        vehicles_created = true;
    }
}

void Scenario::setupLiDAR() {
    if (pointclouds && !lidar_initialized) //flag if activate the LiDAR
    {
        //Specs on Velodyne HDL-64E
        //0.09f azimuth resolution
        //26.8 vertical fov (+2 degrees up to -24.8 degrees down)
        //0.420 vertical resolution
        lidar.Init3DLiDAR_FOV(MAX_LIDAR_DIST, 90.0f, 0.09f, 26.9f, 0.420f, 2.0f);
        lidar.AttachLiDAR2Camera(camera, ped, width, height);
        lidar_initialized = true;
        m_pDMPointClouds = (float *)malloc(width * height * FLOATS_PER_POINT * sizeof(float));
    }
}

void Scenario::collectLiDAR() {
    entitiesHit.clear();
    lidar.updateCurrentPosition(currentForwardVector, currentRightVector, currentUpVector);
    float * pointCloud = lidar.GetPointClouds(pointCloudSize, &entitiesHit, lidar_param, depth_map);

    //U for updated from next depth map
    m_prevPCFilename = getStandardFilename("velodyneU", ".bin");

    std::string filename = getStandardFilename("velodyne", ".bin");
    std::ofstream ofile(filename, std::ios::binary);
    ofile.write((char*)pointCloud, FLOATS_PER_POINT * sizeof(float)*pointCloudSize);
    ofile.close();

    if (OUTPUT_RAYCAST_POINTS) {
        int pointCloudSize2;
        float* pointCloud2 = lidar.GetRaycastPointcloud(pointCloudSize2);

        std::string filename2 = getStandardFilename("velodyneRaycast", ".bin");
        std::ofstream ofile2(filename2, std::ios::binary);
        ofile2.write((char*)pointCloud2, FLOATS_PER_POINT * sizeof(float)*pointCloudSize2);
        ofile2.close();
    }

    if (GENERATE_2D_POINTMAP) {
        //Used for obtaining the 2D points for sampling depth map to convert to velodyne pointcloud
        int size;
        float * points2D = lidar.Get2DPoints(size);

        filename = getStandardFilename("2dpoints", ".bin");
        std::ofstream ofile2(filename, std::ios::binary);
        ofile2.write((char*)points2D, 2 * sizeof(float) * size);
        ofile2.close();

        //Prints out the real values for a sample of the 
        filename = getStandardFilename("2dpoints", ".txt");
        FILE* f = fopen(filename.c_str(), "w");
        fclose(f);
        f = fopen(filename.c_str(), "a");
        int i = 0;
        std::ostringstream oss;
        while (i < 100) {
            oss << "num: " << i << " x: " << points2D[2 * i] << " y: " << points2D[2 * i + 1] << "\n";
            ++i;
        }
        i = (size / 2);
        int maxPrint = i + 100;
        while (i < maxPrint) {
            oss << "num: " << i << " x: " << points2D[2 * i] << " y: " << points2D[2 * i + 1] << "\n";
            ++i;
        }
        i = size - 100;
        while (i < size) {
            oss << "num: " << i << " x: " << points2D[2 * i] << " y: " << points2D[2 * i + 1] << "\n";
            ++i;
        }
        std::string str = oss.str();
        fprintf(f, str.c_str());
        fclose(f);
    }
}

//TODO Calls to export_get_color_buffer are causing GTA to crash
void Scenario::setColorBuffer() {
    int size = export_get_color_buffer((void**)&color_buf);

    std::string filename = getStandardFilename("colorBuffer", ".png");
    std::vector<std::uint8_t> ImageBuffer;
    lodepng::encode(ImageBuffer, color_buf, width, height);
    lodepng::save_file(ImageBuffer, filename);
}

void Scenario::setDepthBuffer(bool prevDepth) {
    int size;
    std::string filename;
    std::string pcFilename;
    log("About to get depth buffer");
    if (prevDepth) {
        filename = m_prevDepthFilename;
        pcFilename = m_prevDepthPCFilename;
        log("previous depth buffer");
        int stencilSize = -1;
        size = export_get_previous_depth_stencil_buffers((void**)&depth_map, (void**)&m_stencilBuffer, stencilSize);
        if (size == -1) {
            return;
        }
        m_prevDepth = false;

        float * pointCloud = lidar.UpdatePointCloud(pointCloudSize, depth_map);
        std::ofstream ofile1(m_prevPCFilename, std::ios::binary);
        ofile1.write((char*)pointCloud, FLOATS_PER_POINT * sizeof(float) * pointCloudSize);
        ofile1.close();

        //TODO Use stencil buffer
    }
    else {
        log("current depth buffer");
        filename = getStandardFilename("depth", ".raw");
        pcFilename = getStandardFilename("depthPC", ".bin");
        size = export_get_depth_buffer((void**)&depth_map, UPDATE_PC_WITH_OFFSET_DEPTH);

        m_prevDepthFilename = filename;
        m_prevDepthPCFilename = pcFilename;
        m_prevDepth = true;
    }
    log("After getting depth buffer");

    std::ofstream ofile(filename, std::ios::binary);
    ofile.write((char*)depth_map, size);
    ofile.close();

    if (OUTPUT_DM_POINTCLOUD) {
        setDepthParams();
        int pointCount = 0;
        float maxDepth = 0;
        float minDepth = 1;
        for (int j = 0; j < height; ++j) {
            for (int i = 0; i < width; ++i) {
                float ndc = depth_map[j*width + i];
                Vector3 relPos = depthToCamCoords(ndc, i, j);

                float distance = sqrt(SYSTEM::VDIST2(0, 0, 0, relPos.x, relPos.y, relPos.z));
                if (distance <= MAX_LIDAR_DIST) {
                    float* p = m_pDMPointClouds + (pointCount * 4);
                    *p = relPos.y;
                    *(p + 1) = -relPos.x;
                    *(p + 2) = relPos.z;
                    *(p + 3) = 0;

                    pointCount++;
                }
            }
        }
        std::ostringstream oss;
        oss << "Min depth: " << minDepth << " max: " << maxDepth << " pointCount: " << pointCount;
        std::string str = oss.str();
        log(str);

        std::ofstream ofile1(pcFilename, std::ios::binary);
        ofile1.write((char*)m_pDMPointClouds, FLOATS_PER_POINT * sizeof(float) * pointCount);
        ofile1.close();

        log("After saving DM pointcloud");
    }
}

void Scenario::setDepthParams() {
    m_nearClip = CAM::GET_CAM_NEAR_CLIP(camera);
    m_fov = CAM::GET_CAM_FOV(camera);
    m_ncHeight = 2 * m_nearClip * tan(m_fov / 2. * (PI / 180.)); // field of view is returned vertically
    m_ncWidth = m_ncHeight * GRAPHICS::_GET_SCREEN_ASPECT_RATIO(false);
    m_depthInit = true;
}

//ndc is Normalized Device Coordinates which is value received from depth buffer
Vector3 Scenario::depthToCamCoords(float ndc, float screenX, float screenY) {
    if (!m_depthInit) {
        setDepthParams();
    }

    float normScreenX = (2 * screenX - width) / width;
    float normScreenY = (2 * screenY - height) / height;

    float ncX = normScreenX * m_ncWidth / 2;
    float ncY = normScreenY * m_ncHeight / 2;

    //Distance to near clip (hypotenus)
    float d2nc = sqrt(m_nearClip * m_nearClip + ncX * ncX + ncY * ncY);
    float worldDepth = d2nc / ndc;

    //X is right, Y is forward, Z is up (GTA coordinate frame)
    Vector3 unitVec;
    unitVec.x = ncX / d2nc;
    unitVec.y = m_nearClip / d2nc;
    unitVec.z = -ncY / d2nc;

    Vector3 relPos;
    relPos.x = unitVec.x * worldDepth;
    relPos.y = unitVec.y * worldDepth;
    relPos.z = unitVec.z * worldDepth;

    /*std::ostringstream oss1;
    oss1 << "\nAdjust depth ScreenX: " << screenX << " screenY: " << screenY <<
        "\nAdjust depth NormScreenX: " << normScreenX << " NormScreenY: " << normScreenY <<
        "\nAdjust depth ncX: " << ncX << " ncY: " << ncY <<
        "\nAdjust depth near_clip: " << m_nearClip << " d2nc: " << d2nc <<
        "\nUnit vec X: " << unitVec.x << " Y: " << unitVec.y << " Z: " << unitVec.z <<
        "\ndepth: " << ndc << " worldDepth: " << worldDepth;
    std::string str1 = oss1.str();
    log(str1);*/

    return relPos;
}

void Scenario::increaseIndex() {
    if (pointclouds) {
        if (lidar_initialized) {
            ++instance_index;
        }
    }
    else {
        ++instance_index;
    }
    if (collectTracking && instance_index == trSeriesLength) {
        instance_index = 0;
        ++series_index;
        trSeriesGap = true;
        trackFirstFrame.clear();

        //update the string
        char temp[] = "%04d";
        char strComp[sizeof temp + 100];
        sprintf(strComp, temp, series_index);
        series_string = strComp;
    }

    char temp[] = "%06d";
    char strComp[sizeof temp + 100];
    sprintf(strComp, temp, instance_index);
    instance_string = strComp;
}

void Scenario::setIndex() {
    d["index"] = instance_index;
    d["seriesIndex"] = series_index;
}

//Camera intrinsics are focal length, and center in horizontal (x) and vertical (y)
void Scenario::calcCameraIntrinsics() {
    float f = width / (2 * tan(HOR_CAM_FOV * PI / 360));
    float cx = width / 2;
    float cy = height / 2;

    intrinsics[0] = f;
    intrinsics[1] = cx;
    intrinsics[2] = cy;

    if (DEBUG_LOGGING) {
        std::ostringstream oss;
        oss << "Focal length is: " << f;
        std::string str = oss.str();
        log(str);
    }

    d["focalLen"] = f;
}

//Camera intrinsics are focal length, and center in horizontal (x) and vertical (y)
void Scenario::setFocalLength() {
    d["focalLen"] = intrinsics[0];
}

void Scenario::drawBoxes(Vector3 BLL, Vector3 FUR, Vector3 dim, Vector3 upVector, Vector3 rightVector, Vector3 forwardVector, Vector3 position, int colour) {
    //log("Inside draw boxes");
    if (showBoxes) {
        log("Inside show boxes");
        Vector3 edge1 = BLL;
        Vector3 edge2;
        Vector3 edge3;
        Vector3 edge4;
        Vector3 edge5 = FUR;
        Vector3 edge6;
        Vector3 edge7;
        Vector3 edge8;

        int green = colour * 255;
        int blue = abs(colour - 1) * 255;

        edge2.x = edge1.x + 2 * dim.y*rightVector.x;
        edge2.y = edge1.y + 2 * dim.y*rightVector.y;
        edge2.z = edge1.z + 2 * dim.y*rightVector.z;

        edge3.x = edge2.x + 2 * dim.z*upVector.x;
        edge3.y = edge2.y + 2 * dim.z*upVector.y;
        edge3.z = edge2.z + 2 * dim.z*upVector.z;

        edge4.x = edge1.x + 2 * dim.z*upVector.x;
        edge4.y = edge1.y + 2 * dim.z*upVector.y;
        edge4.z = edge1.z + 2 * dim.z*upVector.z;

        edge6.x = edge5.x - 2 * dim.y*rightVector.x;
        edge6.y = edge5.y - 2 * dim.y*rightVector.y;
        edge6.z = edge5.z - 2 * dim.y*rightVector.z;

        edge7.x = edge6.x - 2 * dim.z*upVector.x;
        edge7.y = edge6.y - 2 * dim.z*upVector.y;
        edge7.z = edge6.z - 2 * dim.z*upVector.z;

        edge8.x = edge5.x - 2 * dim.z*upVector.x;
        edge8.y = edge5.y - 2 * dim.z*upVector.y;
        edge8.z = edge5.z - 2 * dim.z*upVector.z;

        GRAPHICS::DRAW_LINE(edge1.x, edge1.y, edge1.z, edge2.x, edge2.y, edge2.z, 0, green, blue, 200);
        GRAPHICS::DRAW_LINE(edge1.x, edge1.y, edge1.z, edge4.x, edge4.y, edge4.z, 0, green, blue, 200);
        GRAPHICS::DRAW_LINE(edge2.x, edge2.y, edge2.z, edge3.x, edge3.y, edge3.z, 0, green, blue, 200);
        GRAPHICS::DRAW_LINE(edge3.x, edge3.y, edge3.z, edge4.x, edge4.y, edge4.z, 0, green, blue, 200);

        GRAPHICS::DRAW_LINE(edge5.x, edge5.y, edge5.z, edge6.x, edge6.y, edge6.z, 0, green, blue, 200);
        GRAPHICS::DRAW_LINE(edge5.x, edge5.y, edge5.z, edge8.x, edge8.y, edge8.z, 0, green, blue, 200);
        GRAPHICS::DRAW_LINE(edge6.x, edge6.y, edge6.z, edge7.x, edge7.y, edge7.z, 0, green, blue, 200);
        GRAPHICS::DRAW_LINE(edge7.x, edge7.y, edge7.z, edge8.x, edge8.y, edge8.z, 0, green, blue, 200);

        GRAPHICS::DRAW_LINE(edge1.x, edge1.y, edge1.z, edge7.x, edge7.y, edge7.z, 0, green, blue, 200);
        GRAPHICS::DRAW_LINE(edge2.x, edge2.y, edge2.z, edge8.x, edge8.y, edge8.z, 0, green, blue, 200);
        GRAPHICS::DRAW_LINE(edge3.x, edge3.y, edge3.z, edge5.x, edge5.y, edge5.z, 0, green, blue, 200);
        GRAPHICS::DRAW_LINE(edge4.x, edge4.y, edge4.z, edge6.x, edge6.y, edge6.z, 0, green, blue, 200);
        WAIT(0);
    }
}

std::string Scenario::getStandardFilename(std::string subDir, std::string extension) {
    std::string filename = baseFolder + subDir + "\\";
    CreateDirectory(filename.c_str(), NULL);
    if (collectTracking) {
        filename.append(series_string);
        CreateDirectory(filename.c_str(), NULL);
    }
    filename.append("\\");
    filename.append(instance_string);
    filename.append(extension);
    return filename;
}