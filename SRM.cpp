﻿// SRM-view.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#pragma once
#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/video/background_segm.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <iostream>
#include<math.h>
#include "omp.h"
#include<vector>
#include"Predictor.h"
#include "Serialport.h"

using namespace cv;
using namespace std;

#define T_ANGLE_THRE 2
#define T_SIZE_THRE 5
#define THREAD_NUM 8
#define GRAYVALUE 50

//滑动条所需全局变量
/*
int h1min = 0, h1max = 12, h2min = 156, h2max = 180;
int smin = 2, smax = 255;
int vmin = 46, vmax = 200;         //红色hsv参数
*/

int h1min = 100, h1max = 124, h2min = 0, h2max = 255;
int smin = 43, smax = 255;
int vmin = 46, vmax = 255;       //蓝色的hsv参数

int b = 8;                         //曝光时间
int off_y = 0;                    //摄像头和云台中心的偏差
int fact_x = 12;                   //装甲板的实际宽
int fact_y = 6;                    //装甲板的实际高度
int bullet_speed = 10;          //子弹初速度
int offset_y_barrel_ptz = 0;       //云台与炮管的偏移

//////////////   函数的声明   ///////////////////////
void getDiffImage(Mat src,Mat dst); //二值化 HSV
vector<RotatedRect> armorDetect(vector<RotatedRect> vEllipse); //检测装甲
void drawBox(RotatedRect box, Mat img); //标记装甲5
void getangle(RotatedRect &rect, Mat cam_matrix, Mat distortion_coeff, Mat& rot_camera2ptz1, Mat& trans_camera2ptz1, Point2f & offset, double&diz, double &angle_x, double& angle_y);

int main()
{
	//变量的定义
	Mat frame0;//摄像头采集到的图片
	RotatedRect s;   //定义旋转矩形
	vector<RotatedRect> vEllipse; //定以旋转矩形的向量，用于存储发现的目标区域
	vector<RotatedRect> vRlt; //定义储存装甲板的变量
	bool bFlag = false;
	Point2f  offset;//二维点的偏移
	double  angle_x;//算出的角度x
	double  angle_y;//算出的角度y
	int x, y;//给串口发的数据值
	int i = 0;//统计多少帧图片
	vector<vector<Point> > contour;//轮廓的定义
	Predictor prex; //x角度预测
	Predictor prey; //y角度预测
	double diz;     //z轴距离
	double pre_diz = 0;

	///////////////////////solvepnp 所需内参/////////
	double camD[9] = { 498.5673259258161, 0, 323.2000525530471,
		0, 498.3701474019903, 225.0731190707511,
		0, 0, 1 };
	double distCoeffD[5] = { -0.07067215495428462, 0.3336418667923584, 0.004455346447950378, -0.01120722754745704, -0.5235104596836849
	};

	Mat camera_matrix = Mat(3, 3, CV_64FC1, camD);
	Mat distortion_coefficients = Mat(5, 1, CV_64FC1, distCoeffD);

	/////////////摄像头坐标到云台坐标的转换
	Mat rot_camera2ptz1 = cv::Mat::eye(3, 3, CV_64FC1);
	Mat trans_camera2ptz1 = cv::Mat::zeros(3, 1, CV_64FC1);

	///////////////滑动条//////////////
	namedWindow("contral1", WINDOW_NORMAL);
	cvCreateTrackbar("h1min 色调", "contral1", &h1min, 255);
	cvCreateTrackbar("h1max 色调", "contral1", &h1max, 255);
	cvCreateTrackbar("h2min 色调", "contral1", &h2min, 255);
	cvCreateTrackbar("h2max 色调", "contral1", &h2max, 255); 

	cvCreateTrackbar("smin  饱和度", "contral1", &smin, 255);
	cvCreateTrackbar("smax  饱和度", "contral1", &smax, 255);
	cvCreateTrackbar("vmin  明度", "contral1", &vmin, 255);
	cvCreateTrackbar("vmax  明度", "contral1", &vmax, 255);
	cvCreateTrackbar("装甲板宽", "contral1", &fact_x, 50);
	cvCreateTrackbar("装甲板高", "contral1", &fact_y, 50);
	cvCreateTrackbar("云台摄像头偏差", "contral1", &off_y, 100);
	cvCreateTrackbar("云台炮管偏差", "contral1", &offset_y_barrel_ptz, 50);
	cvCreateTrackbar("子弹初速度", "contral1", &bullet_speed, 100);

	VideoCapture cap0(0);
	cap0.set(CV_CAP_PROP_EXPOSURE, -1 * b);//调整曝光
	cap0 >> frame0;
	Size imgSize;  //定义图像大小
	imgSize = frame0.size();
	Mat rawImg = Mat(imgSize, CV_8UC3);
	Mat binary = Mat(imgSize, CV_8UC1);
	Mat rlt = Mat(imgSize, CV_8UC1);
	Mat temp = Mat(imgSize, CV_8UC1);
	Mat element = getStructuringElement(MORPH_RECT, Size(2, 2));
	Mat ex = imread("1.jpg");

	//Serialport sp(115200);

	


	while (1)
	{
		double t = (double)cvGetTickCount();
		if (cap0.read(frame0))
		{
			rawImg=frame0-GRAYVALUE;
			//二值化图片
			getDiffImage(rawImg, binary);

			//先膨胀在腐蚀属于闭运算
			dilate(binary, temp, element, Point(-1, -1), 3);
			erode(temp, rlt, element, Point(-1, -1), 1);  

			imshow("acr1", rlt);

			
			//在原图画中心圆
			Point center1;
			center1.x = frame0.size().width / 2;
			center1.y = frame0.size().height / 2;
			circle(frame0, center1, 1, CV_RGB(255, 0, 0), 2);
			

			//在二值图像中寻找轮廓
			findContours(rlt, contour, RETR_CCOMP, CHAIN_APPROX_SIMPLE); 
			for (int i = 0; i<contour.size(); i++)
			{
				if (contour[i].size()> 7)  //判断当前轮廓是否大于10个像素点    （1）第一个判断处
				{
					bFlag = true;   //如果大于10个，则检测到目标区域
					s = fitEllipse(contour[i]);//拟合椭圆
					if (bFlag)
						vEllipse.push_back(s); //将发现的目标保存
				}
			}

			//调用子程序，在输入的LED所在旋转矩形的vector中找出装甲的位置，并包装成旋转矩形，存入vector并返回
			if (vEllipse.size() >= 2) //如果检测到的旋转矩形个数小于2，则直接返回     （3）第三个判断处
			{
				vRlt = armorDetect(vEllipse);
				if (!vRlt.empty())
				{
					getangle(vRlt[0], camera_matrix, distortion_coefficients, rot_camera2ptz1, trans_camera2ptz1, offset, diz, angle_x, angle_y);
					cout << "x角度: " << angle_x << "  y角度： " << angle_y << endl;
					//串口发送数据
					if (abs(diz - pre_diz) < 30)
					{
						for (unsigned int nI = 0; nI < vRlt.size(); nI++) //在当前图像中标出装甲的位置
							drawBox(vRlt[0], frame0);

						prex.setRecord(angle_x, t);
						double t1 = (double)cvGetTickCount();
						double pre_x;
						pre_x = prex.predict(t1);
						prey.setRecord(angle_y, t);
						double pre_y;
						pre_y = prey.predict(t1);
						cout << "  x的预测值：" << pre_x << "  y的预测值：" << pre_y << endl;

						//sp.sendData(angle_x, angle_y);
					}
					pre_diz = diz;
					imshow("Raw", frame0);
					
				}
			}

			waitKey(60);
			
			imshow("Raw", frame0);
			if (waitKey(2) == 2)
				break;

			vEllipse.clear();
			vRlt.clear();


			t = ((double)cvGetTickCount() - t) / (cvGetTickFrequency() * 1000);
			cout << "处理时间: " << t << "ms" << endl;
			cout << "帧率:" << 1000 / t << endl << endl;

		}
		else
			break;

	}
	cap0.release();
	return 0;

}

void getDiffImage(Mat src, Mat dst)
{
	Mat bgr[3];
	Mat hsv_image(src.size(), src.type());
	//转HSV
	cvtColor(src, hsv_image, CV_BGR2HSV);
	//分离通道
	split(hsv_image, bgr);

	//开启多线程
	omp_set_num_threads(THREAD_NUM);
#pragma omp parallel for

	for (int nI = 0; nI<bgr[0].rows; nI++)
	{
		uchar* pchar1 = bgr[0].ptr<uchar>(nI);
		uchar* pchar2 = bgr[1].ptr<uchar>(nI);
		uchar* pchar3 = bgr[2].ptr<uchar>(nI);
		uchar* pchar4 = dst.ptr<uchar>(nI);
		for (int j = 0; j <bgr[0].cols; j++)
		{
			if (((pchar1[j] <= h1max && pchar1[j] >= h1min) || (pchar1[j] <= h2max && pchar1[j] >= h2min)) && (pchar2[j]>smin && pchar2[j]<smax) && (pchar3[j]>vmin && pchar3[j]<vmax))//找颜色
			{
				pchar4[j] = 255;
			}
			else  pchar4[j] = 0;;
		}
	}
}

vector<RotatedRect> armorDetect(vector<RotatedRect> vEllipse)
{
	vector<RotatedRect> vRlt;
	RotatedRect armor; //定义装甲区域的旋转矩形
	int nL, nW;
	double dAngle;

	for (unsigned int nI = 0; nI < vEllipse.size() - 1; nI++) //求任意两个旋转矩形的夹角
	{
		for (unsigned int nJ = nI + 1; nJ < vEllipse.size(); nJ++)
		{
			dAngle = abs(vEllipse[nI].angle - vEllipse[nJ].angle);
			if(dAngle > 180)
				dAngle -= 180;
			//判断这两个旋转矩形是否是一个装甲的两个LED等条
			if ((dAngle < T_ANGLE_THRE || 180 - dAngle < T_ANGLE_THRE) && abs(vEllipse[nI].size.height - vEllipse[nJ].size.height) < (vEllipse[nI].size.height + vEllipse[nJ].size.height) / T_SIZE_THRE && abs(vEllipse[nI].size.width - vEllipse[nJ].size.width) < (vEllipse[nI].size.width + vEllipse[nJ].size.width) / T_SIZE_THRE)
			{
				armor.center.x = (vEllipse[nI].center.x + vEllipse[nJ].center.x) / 2; //装甲中心的x坐标 
				armor.center.y = (vEllipse[nI].center.y + vEllipse[nJ].center.y) / 2; //装甲中心的y坐标
				armor.angle = (vEllipse[nI].angle + vEllipse[nJ].angle) / 2;   //装甲所在旋转矩形的旋转角度
				if (180 - dAngle < T_ANGLE_THRE)
					armor.angle += 90;
				nL = (vEllipse[nI].size.height + vEllipse[nJ].size.height) / 2; //装甲的高度
				nW = sqrt((vEllipse[nI].center.x - vEllipse[nJ].center.x) * (vEllipse[nI].center.x - vEllipse[nJ].center.x) + (vEllipse[nI].center.y - vEllipse[nJ].center.y) * (vEllipse[nI].center.y - vEllipse[nJ].center.y)); //装甲的宽度等于两侧LED所在旋转矩形中心坐标的距离
				if (nL < nW)
				{
					armor.size.height = nL;
					armor.size.width = nW;
				}
				else
				{
					armor.size.height = nW;
					armor.size.width = nL;
				}
				vRlt.push_back(armor); //将找出的装甲的旋转矩形保存到vector
			}
		}
	}
	return vRlt;
}

void drawBox(RotatedRect box, Mat img)
{
	Point2f pt[4];
	int i;
	for (i = 0; i<4; i++)
	{
		pt[i].x = 0;
		pt[i].y = 0;
	}
	box.points(pt); //计算二维盒子顶点 
	line(img, pt[0], pt[1], CV_RGB(0, 0, 255), 2, 8, 0);
	line(img, pt[1], pt[2], CV_RGB(0, 0, 255), 2, 8, 0);
	line(img, pt[2], pt[3], CV_RGB(0, 0, 255), 2, 8, 0);
	line(img, pt[3], pt[0], CV_RGB(0, 0, 255), 2, 8, 0);

}

void getangle(RotatedRect &rect, Mat cam_matrix, Mat distortion_coeff, Mat & rot_camera2ptz1, Mat &trans_camera2ptz1, Point2f &offset, double& diz, double &angle_x, double& angle_y)
{
	vector<cv::Point3f> point3d;//储存实际的装甲板各个坐标
	vector<Point2f> target2d;        //屏幕中的装甲板坐标
	Point2f vertices[4];             //接受屏幕中装甲板坐标
	Mat  trans;                      //solvepnp之后的平移矩阵
	Mat r;                           //solvepnp之后的旋转向量
	Mat rot;                         //罗德里格斯变化之后的平移矩阵
	Mat transed_pos;                 //从摄像头变换到云台中心之后的平移矩阵

	double half_x = fact_x / 2.0;
	double half_y = fact_y / 2.0;
	point3d.push_back(Point3f(-half_x, -half_y, 0));
	point3d.push_back(Point3f(half_x, -half_y, 0));
	point3d.push_back(Point3f(half_x, half_y, 0));
	point3d.push_back(Point3f(-half_x, half_y, 0));

	//////////////////////得到屏幕中装甲板的坐标///////
	rect.size.width = rect.size.height * fact_x / fact_y; //屏幕中装甲板大小与实际装甲板的匹配
	rect.points(vertices);
	Point2f lu, ld, ru, rd;
	sort(vertices, vertices + 4, [](const Point2f & p1, const Point2f & p2) { return p1.x < p2.x; });
	if (vertices[0].y < vertices[1].y) {
		lu = vertices[0];
		ld = vertices[1];
	}
	else {
		lu = vertices[1];
		ld = vertices[0];
	}
	if (vertices[2].y < vertices[3].y) {
		ru = vertices[2];
		rd = vertices[3];
	}
	else {
		ru = vertices[3];
		rd = vertices[2];
	}
	target2d.clear();
	target2d.push_back(lu + offset);
	target2d.push_back(ru + offset);
	target2d.push_back(rd + offset);
	target2d.push_back(ld + offset);

	cv::solvePnP(point3d, target2d, cam_matrix, distortion_coeff, r, trans); //solvepnp得到摄像头的平移矩阵
	trans_camera2ptz1.at<double>(1, 0) = off_y;
	transed_pos = rot_camera2ptz1 * trans - trans_camera2ptz1;               //转换到云台中心

																			 /////////////通过各个方向偏移算出角度并且补偿/////////////////////////
	const double *_xyz = (const double *)transed_pos.data;
	double down_t = 0.0;
	down_t = _xyz[2] / 100.0 / bullet_speed;                 //计算下落时间  速度为m/s，100为单位换算
	diz = _xyz[2];
	cout << "  z轴：" << diz << "  y轴" << _xyz[1] << "  x轴" << _xyz[0] << endl;
	double offset_gravity = -0.5 * 9.8 * down_t * down_t * 100;               //垂直距离单位ms
	double xyz[3] = { _xyz[0], _xyz[1], _xyz[2] };                           //云台与装甲板在运动坐标系中，各个轴向距离；
	double alpha = 0.0, theta = 0.0;
	alpha = asin(offset_y_barrel_ptz / sqrt(xyz[1] * xyz[1] + xyz[2] * xyz[2]));
	if (xyz[1] < 0) {
		theta = atan(-xyz[1] / xyz[2]);
		angle_y = -(alpha + theta);                                           // camera coordinate
	}
	else if (xyz[1] < offset_y_barrel_ptz) {
		theta = atan(xyz[1] / xyz[2]);
		angle_y = -(alpha - theta);                                            // camera coordinate
	}
	else {
		theta = atan(xyz[1] / xyz[2]);
		angle_y = (theta - alpha);                                              // camera coordinate
	}
	angle_x = atan2(xyz[0], xyz[2]);
	angle_x = angle_x * 180 / 3.1415926;
	angle_y = angle_y * 180 / 3.1415926;
}

