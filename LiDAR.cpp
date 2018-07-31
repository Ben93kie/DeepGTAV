#define NOMINMAX

#include "LiDAR.h"
#include "Scenario.h"
#include <math.h>
#include <stdio.h>
#include <cmath>
#include <string>
#include <sstream>
#include <algorithm>

#include "Functions.h"
#include "Constants.h"

LiDAR::LiDAR()
{
    m_pPointClouds = NULL;
    m_pRaycastPointCloud = NULL;
    m_lidar2DPoints = NULL;
    m_pointsHit = 0;
    m_maxRange = 0;
    m_vertiUpLimit = 0;
    m_vertiUnLimit = 0;
    m_horizLeLimit = 0;
    m_horizRiLimit = 0;
    m_vertiSmplNum = 0;
    m_horizSmplNum = 0;
    m_vertiResolu = 0;
    m_horizResolu = 0;
    m_camera = 0;
    m_ownCar = 0;
    m_initType = _LIDAR_NOT_INIT_YET_;
    m_isAttach = false;
}

LiDAR::~LiDAR()
{
    DestroyLiDAR();
}

void LiDAR::Init2DLiDAR_SmplNum(float maxRange, int horizSmplNum, float horizLeLimit, float horizRiLimit)
{
    if (m_initType != _LIDAR_NOT_INIT_YET_)
        DestroyLiDAR();

    m_horizSmplNum = horizSmplNum;
    m_maxRange = maxRange;
    if (horizRiLimit <= horizLeLimit)
    {
        printf("\nHorizontal FOV angle parameters error");
        return;
    }
    m_horizLeLimit = horizLeLimit;
    m_horizRiLimit = horizRiLimit;
    m_horizResolu = (m_horizLeLimit + 360.0 - m_horizRiLimit) / m_horizSmplNum;
    if (!m_pPointClouds)
        m_pPointClouds = (float *)malloc(m_horizSmplNum * sizeof(float));
    if (m_pPointClouds == NULL)
        printf("\nLiDAR: memory alloc err");

    m_initType = _LIDAR_INIT_AS_2D_;

#ifdef DEBUG_CONFIG
    printf("\nDEBUG_CONFIG: function: %s", __FUNCTION__);
    printf("\ncamera=%d, ownCar=%d, maxRange=%f, horizSmplNum=%d, horizLeLimit=%f, horizRiLimit=%f",
        m_camera, m_ownCar, m_maxRange, m_horizSmplNum, m_horizLeLimit, m_horizRiLimit);
    printf("\nHorizontal FOV(yaw definition): %f to %f", -m_horizLeLimit, 360.0 - m_horizRiLimit);
    printf("\nHorizontal angel resolution(deg): %f", m_horizResolu);
    printf("\n");
#endif // DEBUG_CONFIG
}

void LiDAR::Init3DLiDAR_SmplNum(float maxRange, int horizSmplNum, float horizLeLimit, float horizRiLimit,
    int vertiSmplNum, float vertiUpLimit, float vertiUnLimit)
{
    std::ostringstream oss;
    oss << "Vertical sample number: " << vertiSmplNum;
    std::string str = oss.str();
    log(str);

    if (m_initType != _LIDAR_NOT_INIT_YET_)
        DestroyLiDAR();

    m_vertiSmplNum = vertiSmplNum;
    m_horizSmplNum = horizSmplNum;
    m_maxRange = maxRange;
    //Vertical:
    if (vertiUnLimit <= vertiUpLimit)
    {
        printf("\nVertical FOV angle parameters error");
        return;
    }
    m_vertiUpLimit = vertiUpLimit;
    m_vertiUnLimit = vertiUnLimit;
    m_vertiResolu = (m_vertiUnLimit - m_vertiUpLimit) / m_vertiSmplNum;

    //Horizontal: 
    if (horizRiLimit <= horizLeLimit)
    {
        printf("\nHorizontal FOV angle parameters error");
        return;
    }
    m_horizLeLimit = horizLeLimit;
    m_horizRiLimit = horizRiLimit;
    m_horizResolu = (m_horizLeLimit + 360.0 - m_horizRiLimit) / m_horizSmplNum;

    if (!m_pPointClouds)
        //Malloc 4*max number of points bytes for the point cloud
        m_pPointClouds = (float*)malloc((MAX_POINTS * sizeof(float)));
    if (m_pPointClouds == NULL)
        printf("\nLiDAR: memory alloc err");

    if (!m_pRaycastPointCloud)
        //Malloc 4*max number of points bytes for the point cloud
        m_pRaycastPointCloud = (float*)malloc((MAX_POINTS * sizeof(float)));
    if (m_pRaycastPointCloud == NULL)
        printf("\nLiDAR: memory alloc err3");

    //Malloc 2*max number of points bytes for the 3d -> 2d point map
    if (GENERATE_2D_POINTMAP) {
        m_lidar2DPoints = (float*)malloc((MAX_POINTS * sizeof(float)) / 2);
        if (m_lidar2DPoints == NULL)
            printf("\nLiDAR: memory alloc err2");
    }

    m_initType = _LIDAR_INIT_AS_3D_;

#ifdef DEBUG_CONFIG
    printf("\nDEBUG_CONFIG: function: %s", __FUNCTION__);
    printf("\ncamera=%d, ownCar=%d, maxRange=%f, horizSmplNum=%d, horizLeLimit=%f, horizRiLimit=%f, vertiSmplNum=%d, vertiUpLimit=%f, vertiUnLimit=%f",
        m_camera, m_ownCar, m_maxRange, m_horizSmplNum, m_horizLeLimit, m_horizRiLimit, m_vertiSmplNum, m_vertiUpLimit, m_vertiUnLimit);
    printf("\nHorizontal FOV(yaw definition): %f to %f", -m_horizLeLimit, 360.0 - m_horizRiLimit);
    printf("\nVertical FOV(pitch definition): %f to %f", 90.0 - m_vertiUpLimit, 90.0 - m_vertiUnLimit);
    printf("\nHorizontal angel resolution(deg): %f", m_horizResolu);
    printf("\nVertical angel resolution(deg): %f", m_vertiResolu);
    printf("\n");
#endif // DEBUG_CONFIG
}

void LiDAR::Init2DLiDAR_FOV(float maxRange, float horizFOV, float horizAngResolu)
{
    Init2DLiDAR_SmplNum(maxRange, horizFOV / horizAngResolu, horizFOV / 2, 360.0 - horizFOV / 2);
}

void LiDAR::Init3DLiDAR_FOV(float maxRange, float horizFOV, float horizAngResolu, float vertiFOV, float vertiAngResolu, float vertiUpLimit)
{
    Init3DLiDAR_SmplNum(maxRange, horizFOV / horizAngResolu, horizFOV / 2, 360.0 - horizFOV / 2, vertiFOV / vertiAngResolu, 90.0 - vertiUpLimit, 90.0 + vertiFOV - vertiUpLimit);
}

void LiDAR::AttachLiDAR2Camera(Cam camera, Entity ownCar, int scrWidth, int scrHeight)
{
    if (!m_isAttach)
    {
        m_camera = camera;
        m_ownCar = ownCar;
        m_isAttach = true;
        m_scrHeight = scrHeight;
        m_scrWidth = scrWidth;
        m_nearClip = CAM::GET_CAM_NEAR_CLIP(camera);
        m_fov = CAM::GET_CAM_FOV(camera);
        m_ncHeight = 2 * m_nearClip * tan(m_fov / 2. * (PI / 180.)); // field of view is returned vertically
        m_ncWidth = m_ncHeight * GRAPHICS::_GET_SCREEN_ASPECT_RATIO(false);
        log("LiDAR attached to car");
    }
    else
    {
        log("LiDAR is already attached to car");
    }
}

void LiDAR::DestroyLiDAR()
{
    if (m_pPointClouds) {
        free(m_pPointClouds);
        m_pPointClouds = NULL;
    }
    if (m_pRaycastPointCloud) {
        free(m_pRaycastPointCloud);
        m_pRaycastPointCloud = NULL;
    }
    if (m_lidar2DPoints) {
        free(m_lidar2DPoints);
        m_lidar2DPoints = NULL;
    }
    m_maxRange = 0;
    m_vertiUpLimit = 0;
    m_vertiUnLimit = 0;
    m_horizLeLimit = 0;
    m_horizRiLimit = 0;
    m_vertiSmplNum = 0;
    m_horizSmplNum = 0;
    m_vertiResolu = 0;
    m_horizResolu = 0;
    m_camera = 0;
    m_ownCar = 0;
    m_initType = _LIDAR_NOT_INIT_YET_;
    m_isAttach = false;
}

float * LiDAR::Get2DPoints(int &size) {
    size = m_beamCount;
    return m_lidar2DPoints;
}

float * LiDAR::GetRaycastPointcloud(int &size) {
    size = m_raycastPoints;
    return m_pRaycastPointCloud;
}

float * LiDAR::GetPointClouds(int &size, std::unordered_map<int, HitLidarEntity*> *entitiesHit, int param, float* depthMap)
{
    m_depthMap = depthMap;
    native_param = param;

    std::ostringstream oss;
    oss << "Native param: " << native_param;
    std::string str = oss.str();
    log(str);

    m_entitiesHit = entitiesHit;
    m_pointsHit = 0;
    m_raycastPoints = 0;
    m_depthMapPoints = 0;
    m_beamCount = 0;

    if (m_pPointClouds == NULL || m_initType == _LIDAR_NOT_INIT_YET_ || !m_isAttach)
        return NULL;
    switch (m_initType)
    {
    case _LIDAR_INIT_AS_2D_: GenerateHorizPointClouds(90, m_pPointClouds);
    case _LIDAR_INIT_AS_3D_:
    {
        m_max_dist = 0;
        m_min_dist = 5555555555;

        //log("Trying to generate pointcloud");
        float phi = m_vertiUnLimit;
        int horizBeamCount = 0;
        for (int k = 0; k < m_vertiSmplNum; k++)
        {
            if (phi > m_vertiUpLimit - m_vertiResolu)
                phi = m_vertiUnLimit - k * m_vertiResolu;
            else
                break;

            ++horizBeamCount;
            GenerateHorizPointClouds(phi, m_pPointClouds);
        }
        std::ostringstream oss;
        oss << "************************ Max distance: " << m_max_dist << " min distance: " << m_min_dist
            << "\nBeamCount: " << horizBeamCount;
        log(oss.str());
    }
    default:
        break;
    }
    //log("After obtaining pointcloud\n");

    std::ostringstream oss1;
    oss1 << "Raycast points: " << m_raycastPoints << " DM points: " << m_depthMapPoints << " total: " << m_pointsHit;
    std::string str1 = oss1.str();
    log(str1);

    size = m_pointsHit;
    return m_pPointClouds;
}

int LiDAR::getTotalSmplNum()
{
    switch (m_initType)
    {
    case _LIDAR_INIT_AS_2D_:
        return m_horizSmplNum;
    case _LIDAR_INIT_AS_3D_:
        return m_horizSmplNum * m_vertiSmplNum;
    default:
        return 0;
    }
}

int LiDAR::getVertiSmplNum()
{
    return m_vertiSmplNum;
}

int LiDAR::getHorizSmplNum()
{
    return m_horizSmplNum;
}

int LiDAR::getCurType()
{
    return m_initType;
}

static float ReverseFloat(const float inFloat)
{
    float retVal;
    char *floatToConvert = (char*)& inFloat;
    char *returnFloat = (char*)& retVal;

    // swap the bytes into a temporary buffer
    returnFloat[0] = floatToConvert[3];
    returnFloat[1] = floatToConvert[2];
    returnFloat[2] = floatToConvert[1];
    returnFloat[3] = floatToConvert[0];

    return retVal;
}

float LiDAR::depthFromNDC(int x, int y, float screenX, float screenY) {
    if (x >= m_scrWidth) {
        x = m_scrWidth - 1;
        log("x out of bounds");
        std::ostringstream oss3;
        oss3 << "x: " << x << " y: " << y
            << "ScreenX: " << screenX << " ScreenY: " << screenY
            << "\nscrWidth: " << m_scrWidth << " scrHeight: " << m_scrHeight;
        std::string str3 = oss3.str();
        log(str3);
    }
    if (y >= m_scrHeight) {
        y = m_scrHeight - 1;
        log("y out of bounds");
        std::ostringstream oss3;
        oss3 << "x: " << x << " y: " << y
            << "ScreenX: " << screenX << " ScreenY: " << screenY
            << "\nscrWidth: " << m_scrWidth << " scrHeight: " << m_scrHeight;
        std::string str3 = oss3.str();
        log(str3);
    }

    float xNorm = (float)x / (m_scrWidth - 1);
    float yNorm = (float)y / (m_scrHeight - 1);
    float normScreenX = abs(2 * xNorm - 1);
    float normScreenY = abs(2 * yNorm - 1);

    float ncX = normScreenX * m_ncWidth / 2;
    float ncY = normScreenY * m_ncHeight / 2;

    //Distance to near clip (hypotenus)
    float d2nc = sqrt(m_nearClip * m_nearClip + ncX * ncX + ncY * ncY);

    //depth value in normalized device coordinates (NDC)
    float ndc = m_depthMap[y * m_scrWidth + x];

    //Actual depth in camera coordinates
    float depth = d2nc / ndc;

    return depth;
}

static bool isPositionOnScreen(float screenX, float screenY) {
    bool onScreen = true;
    if (screenX < 0 || screenY < 0 || screenX > 1 || screenY > 1) {
        onScreen = false;
    }
    return onScreen;
}

float LiDAR::getDepthFromScreenPos(float screenX, float screenY) {
    float depth;
    float halfW = 0.5 / m_scrWidth;
    float halfH = 0.5 / m_scrHeight;
    bool interpolated = false;
    if (screenX > halfW && screenX < 1 - halfW
        && screenY > halfH && screenY < 1 - halfH) {
        float x = screenX * m_scrWidth - 0.5;
        float y = screenY * m_scrHeight - 0.5;

        int x0 = (int)floor(x);
        int x1 = (int)ceil(x);
        int y0 = (int)floor(y);
        int y1 = (int)ceil(y);

        float d00 = depthFromNDC(x0, y0);
        float d01 = depthFromNDC(x0, y1);
        float d10 = depthFromNDC(x1, y0);
        float d11 = depthFromNDC(x1, y1);

        //Normalize x/y to be between 0 and 1 to simplify interpolation
        float normX = (float)x - x0;
        float normY = (float)y - y0;

        //Bilinear interpolation
        //TODO: Would be better to use depth/stencil buffer to only interpolate on pixels of same object
        bool outsideThreshold = false;
        float minDepth = std::min(d00, std::min(d01, std::min(d10, d11)));
        float maxDepth = std::max(d00, std::max(d01, std::max(d10, d11)));
        if (maxDepth > minDepth * 1.08) {
            outsideThreshold = true;
        }
        //float thresh = 0.5; //Threshold value in metres
        //if (abs(d00 - d01) > thresh || abs(d00 - d10) > thresh || abs(d00 - d11) > thresh ||
        //    abs(d01 - d10) > thresh || abs(d01 - d11) > thresh || abs(d10 - d11) > thresh) {
        //    outsideThreshold = true;
        //}
        if (!outsideThreshold) {
            interpolated = true;
            depth = (1 - normX)*(1 - normY)*d00 + normX * (1 - normY)*d10 + (1 - normX)*normY*d01 + normX * normY*d11;
        }
    }

    if (!interpolated) {
        //Pixels are 0 indexed
        int x = (int)floor(screenX * m_scrWidth);
        int y = (int)floor(screenY * m_scrHeight);

        depth = depthFromNDC(x, y, screenX, screenY);
    }
    return depth;
}

Vector3 LiDAR::adjustEndCoord(Vector3 pos, Vector3 relPos) {
    float scrX, scrY;
    //Use this function over native function as native function fails at edges of screen
    Eigen::Vector2f uv = get_2d_from_3d(Eigen::Vector3f(pos.x, pos.y, pos.z),
        Eigen::Vector3f(m_curPos.x, m_curPos.y, m_curPos.z),
        Eigen::Vector3f(m_theta.x, m_theta.y, m_theta.z), m_nearClip, m_fov);

    scrX = uv(0);
    scrY = uv(1);

    float screenX = scrX;// *width;
    float screenY = scrY;// *height;

    if (isPositionOnScreen(screenX, screenY)) {
        //It is expected for some LiDAR positions to be out of screen bounds
        //log("Screen position is out of bounds.");
    }
    else {
        float depth = getDepthFromScreenPos(screenX, screenY);

        float originalDepth = sqrt(relPos.x * relPos.x + relPos.y * relPos.y + relPos.z * relPos.z);
        float multiplier = depth / originalDepth;

        /*std::ostringstream oss1;
        oss1 << "\nAdjust depth ScreenX: " << screenX << " screenY: " << screenY << 
            "\nAdjust depth NormScreenX: " << normScreenX << " NormScreenY: " << normScreenY <<
            "\nAdjust depth ncX: " << ncX << " ncY: " << ncY <<
            "\nAdjust depth near_clip: " << m_nearClip << " d2nc: " << d2nc <<
            "\nAdjust depth X: " << x << " Y: " << y <<
            "\noriginalDepth: " << originalDepth << "depth: " << depth << 
            "\nmultiplier: " << multiplier;
        std::string str1 = oss1.str();
        log(str1);*/
        relPos.x = relPos.x * multiplier;
        relPos.y = relPos.y * multiplier;
        relPos.z = relPos.z * multiplier;
    }
    return relPos;
}

void LiDAR::GenerateSinglePoint(float phi, float theta, float* p)
{
    if (m_pointsHit >= MAX_POINTS) {
        log("WARNING: MAX NUMBER OF POINTS REACHED! INCREASE MAX_POINTS\n", true);
    }
    BOOL isHit;
    Entity hitEntity;
    Vector3 target, endCoord, surfaceNorm;
    int raycast_handle;
    float phi_rad = phi * D2R, theta_rad = theta * D2R;

    endCoord.x = -m_maxRange * sin(phi_rad) * sin(theta_rad);	//rightward(east) is positive
    endCoord.y = m_maxRange * sin(phi_rad) * cos(theta_rad);	//forward(north) is positive
    endCoord.z = m_maxRange * cos(phi_rad);						//upward(up) is positive

    target.x = m_rotDCM[0] * endCoord.x + m_rotDCM[1] * endCoord.y + m_rotDCM[2] * endCoord.z + m_curPos.x;
    target.y = m_rotDCM[3] * endCoord.x + m_rotDCM[4] * endCoord.y + m_rotDCM[5] * endCoord.z + m_curPos.y;
    target.z = m_rotDCM[6] * endCoord.x + m_rotDCM[7] * endCoord.y + m_rotDCM[8] * endCoord.z + m_curPos.z;

    //options: -1=everything
    //New function is called _START_SHAPE_TEST_RAY
    raycast_handle = WORLDPROBE::_CAST_RAY_POINT_TO_POINT(m_curPos.x, m_curPos.y, m_curPos.z, target.x, target.y, target.z, -1, m_ownCar, native_param);

    //New function is called GET_SHAPE_TEST_RESULT
    WORLDPROBE::_GET_RAYCAST_RESULT(raycast_handle, &isHit, &endCoord, &surfaceNorm, &hitEntity);

    //The 2D screen coords of the target
    //This is what should be used for sampling depth map as endCoord will not hit same points as depth map
    Eigen::Vector2f target2D = get_2d_from_3d(Eigen::Vector3f(target.x, target.y, target.z),
        Eigen::Vector3f(m_curPos.x, m_curPos.y, m_curPos.z),
        Eigen::Vector3f(m_theta.x, m_theta.y, m_theta.z), m_nearClip, m_fov);

    if (GENERATE_2D_POINTMAP) {
        *(m_lidar2DPoints + 2 * m_beamCount) = target2D(0);
        *(m_lidar2DPoints + 2 * m_beamCount + 1) = target2D(1);
        ++m_beamCount;
    }

    /*std::ostringstream oss2;
    oss2 << "***Endcoord is: " << endCoord.x << ", " << endCoord.y << ", " << endCoord.z <<
        "\n Current position is: " << m_curPos.x << ", " << m_curPos.y << ", " << m_curPos.z;
    std::string str = oss2.str();
    */
    //log(str);

    bool targetOnScreen = isPositionOnScreen(target2D(0), target2D(1));
    Vector3 depthEndCoord;
    if (targetOnScreen) {
        Vector3 unitVec;
        float distance = sqrt(SYSTEM::VDIST2(m_curPos.x, m_curPos.y, m_curPos.z, target.x, target.y, target.z));
        unitVec.x = (target.x - m_curPos.x) / distance;
        unitVec.y = (target.y - m_curPos.y) / distance;
        unitVec.z = (target.z - m_curPos.z) / distance;

        float depth = getDepthFromScreenPos(target2D(0), target2D(1));
        depthEndCoord.x = unitVec.x * depth;
        depthEndCoord.y = unitVec.y * depth;
        depthEndCoord.z = unitVec.z * depth;

        //Depth is already in relative coordinates
        /*Vector3 vec;
        vec.x = depthEndCoord.x - m_curPos.x;
        vec.y = depthEndCoord.y - m_curPos.y;
        vec.z = depthEndCoord.z - m_curPos.z;*/

        //To convert from world coordinates to GTA vehicle coordinates (where y axis is forward)
        Vector3 vec_cam_coord = convertCoordinateSystem(depthEndCoord, currentForwardVec, currentRightVec, currentUpVec);

        /*std::ostringstream oss2;
        oss2 << "***unitVec is: " << unitVec.x << ", " << unitVec.y << ", " << unitVec.z <<
        "\n depthEndCoord is: " << depthEndCoord.x << ", " << depthEndCoord.y << ", " << depthEndCoord.z <<
            "\nvec is: " << vec.x << ", " << vec.y << ", " << vec.z <<
            "\n m_curPos is: " << m_curPos.x << ", " << m_curPos.y << ", " << m_curPos.z;
        std::string str = oss2.str();
        log(str);*/

        float newDistance = sqrt(SYSTEM::VDIST2(0, 0, 0, vec_cam_coord.x, vec_cam_coord.y, vec_cam_coord.z));
        if (newDistance <= MAX_LIDAR_DIST) {
            //Note: The y/x axes are changed to conform with KITTI velodyne axes
            *p = vec_cam_coord.y;
            *(p + 1) = -vec_cam_coord.x;
            *(p + 2) = vec_cam_coord.z;
            *(p + 3) = 0;//We don't have the entityID if we're using the depth map
            ++m_pointsHit;
            ++m_depthMapPoints;
        }
    }

    if (isHit) {
        int entityID = 0;
        if ((ENTITY::IS_ENTITY_A_PED(hitEntity) && PED::GET_PED_TYPE(hitEntity) != 28) //PED_TYPE 28 are animals
            || ENTITY::IS_ENTITY_A_VEHICLE(hitEntity)) {
            entityID = hitEntity;
        }

        Vector3 vec;
        vec.x = endCoord.x - m_curPos.x;
        vec.y = endCoord.y - m_curPos.y;
        vec.z = endCoord.z - m_curPos.z;

        //To convert from world coordinates to GTA vehicle coordinates (where y axis is forward)
        Vector3 vec_cam_coord = convertCoordinateSystem(vec, currentForwardVec, currentRightVec, currentUpVec);

        if (!targetOnScreen) {
            //Note: The y/x axes are changed to conform with KITTI velodyne axes
            *p = vec_cam_coord.y;
            *(p + 1) = -vec_cam_coord.x;
            *(p + 2) = vec_cam_coord.z;
            *(p + 3) = entityID;//This is the entityID (Only non-zero for pedestrians and vehicles)
            ++m_pointsHit;
        }

        if (OUTPUT_RAYCAST_POINTS) {
            //Note: The y/x axes are changed to conform with KITTI velodyne axes
            float* ptr = m_pRaycastPointCloud + (m_raycastPoints * FLOATS_PER_POINT);
            *ptr = vec_cam_coord.y;
            *(ptr + 1) = -vec_cam_coord.x;
            *(ptr + 2) = vec_cam_coord.z;
            *(ptr + 3) = entityID;//This is the entityID (Only non-zero for pedestrians and vehicles)
            ++m_raycastPoints;
        }

        //Likely just for testing purposes
        if (OUTPUT_ADJUSTED_POINTS) {
            //To convert from world coordinates to GTA vehicle coordinates (where y axis is forward)
            vec_cam_coord = convertCoordinateSystem(vec, currentForwardVec, currentRightVec, currentUpVec);

            vec_cam_coord = adjustEndCoord(endCoord, vec_cam_coord);

            float distance = sqrt(SYSTEM::VDIST2(0, 0, 0, vec_cam_coord.x, vec_cam_coord.y, vec_cam_coord.z));
            if (distance <= MAX_LIDAR_DIST) {
                //Note: The y/x axes are changed to conform with KITTI velodyne axes
                *(p + 4) = vec_cam_coord.y;
                *(p + 5) = -vec_cam_coord.x;
                *(p + 6) = vec_cam_coord.z;
                *(p + 7) = 65535;//This is the entityID (Only non-zero for pedestrians and vehicles)
                ++m_pointsHit;
            }
        }

        if (CORRECT_BBOXES_WITH_RAYCASTING) {
            //Corrects 3D bounding boxes with raycasting points
            if (m_entitiesHit->find(entityID) != m_entitiesHit->end()) {
                HitLidarEntity* hitEnt = m_entitiesHit->at(entityID);
                hitEnt->pointsHit++;
                Vector3 vecFromObjCenter = subtractVector(vec, hitEnt->position);
                float forwardFromObjCenter = vecFromObjCenter.x * hitEnt->forward.x + vecFromObjCenter.y * hitEnt->forward.y + vecFromObjCenter.z * hitEnt->forward.z;
                if (forwardFromObjCenter > hitEnt->maxFront) hitEnt->maxFront = forwardFromObjCenter;
                if (forwardFromObjCenter < hitEnt->maxBack) hitEnt->maxBack = forwardFromObjCenter;
            }
            else {
                Vector3 forwardVector;
                Vector3 rightVector;
                Vector3 upVector;
                Vector3 position;
                ENTITY::GET_ENTITY_MATRIX(entityID, &forwardVector, &rightVector, &upVector, &position); //Blue or red pill
                position = subtractVector(position, m_curPos);
                HitLidarEntity* hitEnt = new HitLidarEntity(forwardVector, position);
                m_entitiesHit->insert(std::pair<int, HitLidarEntity*>(entityID, hitEnt));
            }
        }
    }

#ifdef DEBUG_LOG
    printf("\nDEBUG_LOG: function: %s", __FUNCTION__);
    printf("\ntheta=%f, endcoord:x=%f, y=%f, z=%f", __FUNCTION__, theta, endCoord.x, endCoord.y, endCoord.z);
#endif //DEBUG_LOG

#ifdef DEBUG_GRAPHICS_LIDAR
    //GRAPHICS::DRAW_BOX(endCoord.x - 0.05, endCoord.y - 0.05, endCoord.z - 0.05, endCoord.x + 0.05, endCoord.y + 0.05, endCoord.z + 0.05, 0, 255, 0, 255);
    GRAPHICS::DRAW_LINE(endCoord.x - 0.03, endCoord.y - 0.03, endCoord.z - 0.03, endCoord.x + 0.03, endCoord.y + 0.03, endCoord.z + 0.03, 255, 255, 255, 255);
#endif //DEBUG_GRAPHICS_LIDAR
}

void LiDAR::GenerateHorizPointClouds(float phi, float *p)
{
    int i, j;
    float theta = 0.0, quaterion[4];

    m_curPos = CAM::GET_CAM_COORD(m_camera);
    calcDCM();

    //Right side:
    theta = m_horizRiLimit;
    for (j = 0; j < m_horizSmplNum; j++)
    {
        if (theta < 360.0 - m_horizResolu)
            theta = m_horizRiLimit + j * m_horizResolu;
        else
            break;
        GenerateSinglePoint(phi, theta, p + (m_pointsHit * FLOATS_PER_POINT));
    }
    //Left side:
    theta = theta - 360.0;
    for (i = 0; i < m_horizSmplNum - j; i++)
    {
        if (theta < m_horizLeLimit - m_horizResolu)
            theta = 0.0 + i * m_horizResolu;
        else
            break;
        GenerateSinglePoint(phi, theta, p + (m_pointsHit * FLOATS_PER_POINT));
    }
}

void LiDAR::calcDCM()
{
    ENTITY::GET_ENTITY_QUATERNION(m_ownCar, &m_quaterion[0], &m_quaterion[1], &m_quaterion[2], &m_quaterion[3]);
    //m_quaterion: R - coord spins to b - coord
    float q00 = m_quaterion[3] * m_quaterion[3], q11 = m_quaterion[0] * m_quaterion[0], q22 = m_quaterion[1] * m_quaterion[1], q33 = m_quaterion[2] * m_quaterion[2];
    float q01 = m_quaterion[3] * m_quaterion[0], q02 = m_quaterion[3] * m_quaterion[1], q03 = m_quaterion[3] * m_quaterion[2], q12 = m_quaterion[0] * m_quaterion[1];
    float q13 = m_quaterion[0] * m_quaterion[2], q23 = m_quaterion[1] * m_quaterion[2];

    //convert b-vector to R-vector, CbR
    m_rotDCM[0] = q00 + q11 - q22 - q33;
    m_rotDCM[1] = 2 * (q12 - q03);
    m_rotDCM[2] = 2 * (q13 + q02);
    m_rotDCM[3] = 2 * (q12 + q03);
    m_rotDCM[4] = q00 - q11 + q22 - q33;
    m_rotDCM[5] = 2 * (q23 - q01);
    m_rotDCM[6] = 2 * (q13 - q02);
    m_rotDCM[7] = 2 * (q23 + q01);
    m_rotDCM[8] = q00 - q11 - q22 + q33;
}

void LiDAR::updateCurrentPosition(Vector3 currentForwardVector, Vector3 currentRightVector, Vector3 currentUpVector) {
    currentForwardVec = currentForwardVector;
    currentRightVec = currentRightVector;
    currentUpVec = currentUpVector;
    m_theta = CAM::GET_CAM_ROT(m_camera, 0);
}