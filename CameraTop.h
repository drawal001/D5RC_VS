/**
 * @file CameraTop.h
 * @author worranhin (worranhin@foxmail.com)
 * @author drawal (2581478521@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-11-28
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include "GxCamera.h"

namespace D5R {

    struct Model {
        cv::Mat img;
        cv::Point2f center;
        cv::Point2f point;
        cv::Point2f jaw_Circle_Center;
        std::vector<cv::KeyPoint> keypoints;
        cv::Mat descriptors;
    };

    enum ModelType {
        JAW = 0,
        CLAMP = 1
    };
    enum PosModel {
        Rough = 0,
        Fine = 1
    };

    class CameraTop : public GxCamera {
    public:
        CameraTop(std::string id);
        ~CameraTop();
        bool SIFT(cv::Mat img, ModelType modelname, std::vector<cv::Point2f>& pst);
        void GetMapParam(cv::Mat Calibration_board);
        std::vector<std::vector<float>> GetPixelPos(PosModel m, float angle = 90);
        std::vector<double> GetPhysicError(PosModel m, float angle = 90);
        void GetROI(cv::Mat img);

    private:
        Model _jaw;
        Model _clamp;
        cv::Mat _img;

        cv::Mat _posTemplate_1;

        cv::Point2f _roiPos;
        cv::Point2f _roughPosPoint;
        double _mapParam;
    };

} // namespace D5R