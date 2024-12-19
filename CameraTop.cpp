/**
 * @file CameraTop.cpp
 * @author worranhin (worranhin@foxmail.com)
 * @author drawal (2581478521@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-11-28
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "CameraTop.h"

namespace D5R {
    /**
     * @brief ���캯��
     *
     * @param id �����Mac��ַ
     */
    CameraTop::CameraTop(std::string id) : GxCamera(id) {
        // ��ǯģ��
        _clamp.img = cv::imread("./model/clampTemplate/clamp.png", 0);
        _clamp.center = cv::Point2f(330.0f, 29.0f);
        _clamp.point = cv::Point2f(331.2f, 149.3f);
        cv::FileStorage fs1( "./model/clampTemplate/KeyPoints_Clamp.yml", cv::FileStorage::READ);
        fs1["keypoints"] >> _clamp.keypoints;
        fs1.release();
        cv::FileStorage fs2( "./model/clampTemplate/Descriptors_Clamp.yml", cv::FileStorage::READ);
        fs2["descriptors"] >> _clamp.descriptors;
        fs2.release();
        // ǯ��ģ��
        _jaw.img = cv::imread( "./model/jawTemplate/jaw.png", 0);
        _jaw.center = cv::Point2f(299.0f, 379.5f);
        _jaw.point = cv::Point2f(299.0f, 15.0f);
        _jaw.jaw_Circle_Center = cv::Point2f(300.648f, 66.6704f);
        cv::FileStorage fs3( "./model/jawTemplate/KeyPoints_Jaw.yml", cv::FileStorage::READ);
        fs3["keypoints"] >> _jaw.keypoints;
        fs3.release();
        cv::FileStorage fs4( "./model/jawTemplate/Descriptors_Jaw.yml", cv::FileStorage::READ);
        fs4["descriptors"] >> _jaw.descriptors;
        fs4.release();

        // ǯ�ڿⶨλģ��1
        _posTemplate_1 = cv::imread( "./model/posTemplate/PosTemple_rect.png", 0);
        // �ֶ�λ���������_roiPos���Ե�
        _roughPosPoint = cv::Point2f(435, 1050);

        _mapParam = 0.00945084;
    }

    /**
     * @brief �����������
     *
     */
    CameraTop::~CameraTop() {}

    /**
     * @brief ����ƥ���㷨��image��ģ�����ƥ�䣬����ģ����image�е�λ����Ϣ����Ҫע����ǣ������JAW����ģ��ƥ�䣬��ȷ���Ѿ���ȡJAWģ��
     *
     * @param image ���ʵʱͼ�񣬻Ҷ�ͼ
     * @param modelname JAW-ǯ�� CLAMP-��ǯ
     * @param pst ģ������������ϵ�µ�λ����Ϣ��pst[0]:center;pst[1]:positon point
     *
     * @todo �����ģ��ƥ�䲻�ɹ�ʱ�Ľ������
     *
     */
    bool D5R::CameraTop::SIFT(cv::Mat image, ModelType modelname,
        std::vector<cv::Point2f>& pst) {
        cv::Mat model;
        std::vector<cv::Point2f> modelPosition;
        std::vector<cv::KeyPoint> keyPoints_Model;
        cv::Mat descriptors_model;
        if (modelname == ModelType::CLAMP) {
            model = _clamp.img.clone();
            modelPosition.push_back(_clamp.center);
            modelPosition.push_back(_clamp.point);
            keyPoints_Model = _clamp.keypoints;
            descriptors_model = _clamp.descriptors;
        }
        else {
            model = _jaw.img.clone();
            modelPosition.push_back(_jaw.center);
            modelPosition.push_back(_jaw.point);
            keyPoints_Model = _jaw.keypoints;
            descriptors_model = _jaw.descriptors;
        }

        // ROI
        cv::Rect roi = cv::Rect2f(_roiPos, cv::Size2f(850.0f, 2046.0f - _roiPos.y));
        cv::Mat ROI = image(roi).clone();

        // SIFT������
        cv::Ptr<cv::SIFT> sift = cv::SIFT::create();
        std::vector<cv::KeyPoint> keyPoints_Img;
        sift->detect(ROI, keyPoints_Img);
        // ����
        cv::Mat descriptors_Img;
        sift->compute(ROI, keyPoints_Img, descriptors_Img);
        // ƥ��
        cv::Ptr<cv::DescriptorMatcher> matcher =
            cv::DescriptorMatcher::create(cv::DescriptorMatcher::BRUTEFORCE);
        std::vector<std::vector<cv::DMatch>> knn_matches;
        const float ratio_thresh = 0.7f;
        std::vector<cv::DMatch> goodMatches;
        matcher->knnMatch(descriptors_model, descriptors_Img, knn_matches, 2);
        for (auto& knn_matche : knn_matches) {
            if (knn_matche[0].distance < ratio_thresh * knn_matche[1].distance) {
                goodMatches.push_back(knn_matche[0]);
            }
        }
        std::cout << goodMatches.size() << std::endl;
        // cv::Mat img_matches;
        // cv::drawMatches(ROI, keyPoints_Img, model, keyPoints_Model, goodMatches, img_matches,
        //                 cv::Scalar::all(-1), cv::Scalar::all(-1), std::vector<char>(), cv::DrawMatchesFlags::DEFAULT);

        // ��ʾƥ��ͼ
        // cv::imshow("Matches", img_matches);
        // cv::waitKey(0);

        // ����
        std::vector<cv::Point2f>
            model_P, img_P;
        for (const auto& match : goodMatches) {
            model_P.push_back(keyPoints_Model[match.queryIdx].pt);
            img_P.push_back(keyPoints_Img[match.trainIdx].pt);
        }

        if (img_P.size() < 8) {
            throw RobotException(ErrorCode::VisialError, "Failed to SIFT");
            return false;
        }

        cv::Mat homography = cv::findHomography(model_P, img_P, cv::RANSAC);

        cv::perspectiveTransform(modelPosition, pst, homography);
        return true;
    }

    /**
     * @brief ��ȡ���ӳ�����
     *
     * @param img �궨ͼƬ
     */
    void CameraTop::GetMapParam(cv::Mat img) {
        std::vector<cv::Point2f> corner;
        if (!cv::findChessboardCorners(img, cv::Size(19, 15), corner)) {
            std::cerr << "Failed to find corners in image" << std::endl;
            return;
        }
        const cv::TermCriteria criteria{ cv::TermCriteria::EPS | cv::TermCriteria::COUNT, 10, 0.001 };
        cv::cornerSubPix(img, corner, cv::Size(6, 6), cv::Size(-1, -1), criteria);
        cv::drawChessboardCorners(img, cv::Size(19, 15), corner, true);

        std::string imagename = "Calibration_board";
        cv::namedWindow(imagename, cv::WINDOW_NORMAL);
        cv::resizeWindow(imagename, cv::Size(1295, 1024));
        cv::imshow(imagename, img);

        float sum = 0;
        cv::Point2f last_point{};
        int count = 0;
        for (cv::Point2f point : corner) {
            count++;
            if (count != 1 && (count - 1) % 19 != 0) {
                auto a = sqrt(powf(point.x - last_point.x, 2) + powf(point.y - last_point.y, 2));
                sum += a;
            }
            last_point = point;
        }
        float map_param = 18 * 15 / sum;
        std::cout << "map_param: " << map_param << " (mm/pixel)" << std::endl;
        cv::waitKey(0);
    }

    /**
     * @brief ��ȡ���ͼƬ�����ؼ�ǯ��ǯ�������������λ����Ϣ
     *
     * @return std::vector<std::vector<float>> ���ز����б� {{jaw.center.x,
     * jaw.center.y, jaw_angle}, {clamp.center.x, clamp.center.y, clamp_angle}}
     */
    std::vector<std::vector<float>> CameraTop::GetPixelPos(PosModel m, float angle) {
        std::vector<std::vector<float>> pos;
        cv::Mat img;
        Read(img);
        std::vector<cv::Point2f> pos_clamp;
        if (!SIFT(img, CLAMP, pos_clamp)) {
            throw RobotException(ErrorCode::VisialError, "Failed to match Clamp");
        }
        float angle_clamp = static_cast<float>(atan2f(pos_clamp[0].y - pos_clamp[1].y, pos_clamp[0].x - pos_clamp[1].x) * (-180) / CV_PI);
        pos.push_back({ pos_clamp[0].x, pos_clamp[0].y, angle_clamp });

        if (m == Fine) {
            std::vector<cv::Point2f> pos_jaw;
            if (!SIFT(img, JAW, pos_jaw)) {
                throw RobotException(ErrorCode::VisialError, "Failed to match JAW");
            }
            float angle_jaw = static_cast<float>(atan2f(pos_jaw[1].y - pos_jaw[0].y, pos_jaw[1].x - pos_jaw[0].x) * (-180) / CV_PI);
            pos.push_back({ pos_jaw[0].x, pos_jaw[0].y, angle_jaw });

            // ��ͼ
            cv::line(img, pos_jaw[0] + _roiPos, pos_jaw[1] + _roiPos, cv::Scalar(0), 4);
            cv::line(img, pos_clamp[0] + _roiPos, pos_clamp[1] + _roiPos, cv::Scalar(0), 4);
            cv::putText(img, std::to_string(angle_jaw), pos_jaw[1] + _roiPos,
                cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0), 4);
            cv::putText(img, std::to_string(angle_clamp), pos_clamp[0] + _roiPos,
                cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0), 4);
            std::string windowname = "image";
            cv::namedWindow(windowname, cv::WINDOW_NORMAL);
            cv::resizeWindow(windowname, cv::Size(1295, 1024));
            cv::imshow(windowname, img);
            cv::waitKey(0);
        }
        else {
            pos.push_back({ _roughPosPoint.x, _roughPosPoint.y, angle });
            // ��ͼ
            cv::line(img, pos_clamp[0] + _roiPos, pos_clamp[1] + _roiPos, cv::Scalar(0), 4);
            cv::circle(img, _roughPosPoint + _roiPos, 2, cv::Scalar(0), 2);
            cv::putText(img, std::to_string(angle_clamp), pos_clamp[1] + _roiPos,
                cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0), 4);
            cv::putText(img, std::to_string(angle), _roughPosPoint + _roiPos,
                cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0), 4);
            std::string windowname = "image";
            cv::namedWindow(windowname, cv::WINDOW_NORMAL);
            cv::resizeWindow(windowname, cv::Size(1295, 1024));
            cv::imshow(windowname, img);
            cv::waitKey(0);
        }

        return pos;
    }

    /**
     * @brief �����ǯ��ǯ������������ϵ�µ�λ�ò�
     *
     * @return std::vector<double> ���ز����б�
     * {jaw.center.x - clamp.center.x, jaw.center.y - clamp.center.y, jaw_angle -
     * clamp_angle}
     */
    std::vector<double> CameraTop::GetPhysicError(PosModel m, float angle) {
        auto pixelPos = GetPixelPos(m, angle);
        std::vector<double> posError;
        posError.push_back((pixelPos[0][0] - pixelPos[1][0]) * _mapParam);
        posError.push_back((pixelPos[0][1] - pixelPos[1][1]) * _mapParam);
        posError.push_back(pixelPos[0][2] - pixelPos[1][2]);
        return posError;
    }

    /**
     * @brief ��ȡǯ�ڿ�ROI��λλ�ã������´ֶ�λ�㣬ÿ�λ�ǯ��ǰ�����
     *
     * @param img ����ʵʱͼ��Ϊ�Ҷ�ͼ
     */
    void CameraTop::GetROI(cv::Mat img) {
        cv::Mat result;
        cv::matchTemplate(img, _posTemplate_1, result, cv::TM_SQDIFF_NORMED);
        cv::Point minLoc, maxLoc;
        double minVal, maxVal;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
        _roiPos = cv::Point2f(minLoc.x - 300.0f, minLoc.y + 300.0f);
    }

} // namespace D5R