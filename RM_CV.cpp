// RM_CV.cpp: 定义控制台应用程序的入口点。
//
#include "stdafx.h"
#include<opencv2/opencv.hpp>
#include<opencv2/imgproc/imgproc.hpp>
#include<opencv2/core/core.hpp>
#include<opencv2/highgui/highgui.hpp>
#include<opencv2/stitching.hpp>
#include<iostream>
#include <omp.h>
#include "putText.h"

using namespace cv;
using namespace std;

#define u16 unsigned int
#define u8 unsigned char

//常量定义
#define T_ANGLE_THRE 2 //旋转矩形的角度差阈值
#define T_SIZE_THRE 5
#define THREAD_NUM 8 //线程数
#define GRAY_VALUE 50 //蒙版值

//红色HSV参数
#define H_MIN1 0 //色相范围
#define H_MAX1 12
#define H_MIN2 156
#define H_MAX2 180
#define S_MIN 2 //饱和度
#define S_MAX 255
#define V_MIN 46 //亮度
#define V_MAX 200

//蓝色HSV参数
/*
#define H_MIN1 100 //色相范围
#define H_MAX1 124
#define H_MIN2 0
#define H_MAX2 255
#define S_MIN 43 //饱和度
#define S_MAX 255
#define V_MIN 46 //亮度
#define V_MAX 200
*/


//阈值
#define CONTOUR_POINT_NUM 20 //轮廓的阈值

#define OFF_Y  0                    //摄像头和云台中心的偏差
#define FACT_X 12                   //装甲板的实际宽
#define FACT_Y 6					//装甲板的实际高度
#define BULLET_SPEED 10				//子弹初速度
#define OFFSET_Y_BARREL 0			//云台与炮管的偏移


void getDiffImage(Mat &src, Mat &dst); //二值化 HSV
vector<RotatedRect> armorDetect(vector<RotatedRect> vEllipse); //检测装甲
void drawBox(RotatedRect box, Mat img); //标记装甲5
void getangle(RotatedRect &rect, Mat cam_matrix, Mat distortion_coeff, Mat& rot_camera2ptz1, Mat& trans_camera2ptz1, Point2f & offset, double&diz, double &angle_x, double& angle_y);

int main()
{
	vector<vector<Point> > contour;//轮廓线条
	RotatedRect s;
	vector<RotatedRect> vEllipse; //定以旋转矩形的向量，用于存储发现的目标区域
	vector<RotatedRect> vRlt; //定义储存装甲板的变量

	VideoCapture capture("E://OpenCV//res//v1.mp4");//取视频
	//VideoCapture capture(0);
	Mat frame;
	capture >> frame;

	Size imgSize = frame.size();
	Point centerPoint(imgSize.width / 2, imgSize.height / 2);

	Mat edges,out;
	Mat binary=Mat(imgSize,CV_8UC1);
	Mat img = Mat(imgSize, CV_8UC1);
	Mat element = getStructuringElement(MORPH_RECT, Size(2, 2));

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

	while (1)
	{
		double t = (double)cvGetTickCount();
		capture >> frame;
		frame -= -GRAY_VALUE;
		//二值化
		getDiffImage(frame, binary);
		//闭预算
		dilate(binary, img, element, Point(-1, -1), 3);
		erode(img, img, element, Point(-1, -1), 1);
		//画个中心点
		circle(frame, centerPoint, 5, Scalar(255, 0, 0));

		//在二值图像中寻找轮廓
		findContours(img, contour, RETR_CCOMP, CHAIN_APPROX_SIMPLE);
		vEllipse.clear();
		for (int i = 0; i<contour.size(); i++)
		{
			if (contour[i].size()> CONTOUR_POINT_NUM)  //判断当前轮廓是否大于10个像素点    （1）第一个判断处
			{
				s = fitEllipse(contour[i]);//拟合椭圆
				vEllipse.push_back(s); //将发现的目标保存

				//用于测试
				//ellipse(frame, s, Scalar(255, 0, 0));
			}
		}

		if (vEllipse.size() >= 2) //旋转矩形数量应大于2
		{
			//获得装甲的旋转矩形
			vRlt = armorDetect(vEllipse);
			if (!vRlt.empty())
			{
				int c = 0;
				for (u16 i = 0; i < vRlt.size(); i++) { //在当前图像中标出装甲的位置
					c++;
					ellipse(frame,vRlt[i], Scalar(255, 0, 0));
				}
				cout << "c=" << c << endl;
					
				
				getangle(vRlt[0], camera_matrix, distortion_coefficients, rot_camera2ptz1, trans_camera2ptz1, offset, diz, angle_x, angle_y);
				cout << "x角度: " << angle_x << "  y角度： " << angle_y << endl;
				/*
				//串口发送数据
				if (abs(diz - pre_diz) < 30)
				{
					

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
				*/

			}
		}
		
		imshow("读取视频", img);
		imshow("对比", frame);

		if (waitKey(1) >= 0)break;
		t = ((double)cvGetTickCount() - t) / (cvGetTickFrequency() * 1000);
		cout << "处理时间: " << t << "ms" << endl;
		cout << "帧率:" << 1000 / t << endl << endl;
	}

    return 0;
}

void getDiffImage(Mat &src, Mat &dst)
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
	//二值化
	for (int nI = 0; nI<bgr[0].rows; nI++)
	{
		uchar* pchar1 = bgr[0].ptr<uchar>(nI);
		uchar* pchar2 = bgr[1].ptr<uchar>(nI);
		uchar* pchar3 = bgr[2].ptr<uchar>(nI);
		uchar* pchar4 = dst.ptr<uchar>(nI);
		for (int j = 0; j <bgr[0].cols; j++)
		{
			//h色相 s饱和度 v明度
			if (((pchar1[j] <= H_MAX1 && pchar1[j] >= H_MIN1) || (pchar1[j] <= H_MAX2 && pchar1[j] >= H_MIN2)) && (pchar2[j]>S_MIN && pchar2[j]<S_MAX) && (pchar3[j]>V_MIN && pchar3[j]<V_MAX)) //找颜色
				pchar4[j] = 255;
			else  pchar4[j] = 0;
		}
	}
}

vector<RotatedRect> armorDetect(vector<RotatedRect> vEllipse)
{
	double dAngle;
	vector<RotatedRect> v;
	float nL, nW;
	RotatedRect armor; //定义装甲区域的旋转矩形


	for (u16 i = 0; i < vEllipse.size() - 1; i++)
	{
		for (u16 j = i + 1; j < vEllipse.size(); j++)
		{
			dAngle = abs(vEllipse[i].angle - vEllipse[j].angle);
			if (dAngle > 180)dAngle -= 180;
			//判断任意两个旋转矩形是否是一个装甲的两个LED灯条
			if ((dAngle < T_ANGLE_THRE || 180 - dAngle < T_ANGLE_THRE) && //两矩形的角度相差在一定范围
				abs(vEllipse[i].size.height - vEllipse[j].size.height) < (vEllipse[i].size.height + vEllipse[j].size.height) / T_SIZE_THRE && //高度差在一定范围
				abs(vEllipse[i].size.width - vEllipse[j].size.width) < (vEllipse[i].size.width + vEllipse[j].size.width) / T_SIZE_THRE //宽度差在一定范围
				)
			{
				armor.center.x = (vEllipse[i].center.x + vEllipse[j].center.x) / 2; //装甲中心的x坐标 
				armor.center.y = (vEllipse[i].center.y + vEllipse[j].center.y) / 2;
				armor.angle = (vEllipse[i].angle + vEllipse[j].angle) / 2;
				if (180 - dAngle < T_ANGLE_THRE)
					armor.angle += 90;
				nL = (vEllipse[i].size.height + vEllipse[j].size.height) / 2; //装甲的高度
				nW = sqrt((vEllipse[i].center.x - vEllipse[j].center.x) * (vEllipse[i].center.x - vEllipse[j].center.x) + 
					(vEllipse[i].center.y - vEllipse[j].center.y) * (vEllipse[i].center.y - vEllipse[j].center.y)); //装甲的宽度等于两侧LED所在旋转矩形中心坐标的距离
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
				v.push_back(armor); //将找出的装甲的旋转矩形保存到vector
			}
		}
	}
	return v;
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

	double half_x = FACT_X / 2.0;
	double half_y = FACT_Y / 2.0;
	point3d.push_back(Point3f(-half_x, -half_y, 0));
	point3d.push_back(Point3f(half_x, -half_y, 0));
	point3d.push_back(Point3f(half_x, half_y, 0));
	point3d.push_back(Point3f(-half_x, half_y, 0));

	//////////////////////得到屏幕中装甲板的坐标///////
	rect.size.width = rect.size.height * FACT_X / FACT_Y; //屏幕中装甲板大小与实际装甲板的匹配
	rect.points(vertices);//将旋转矩形的各个坐标放入vertices
	Point2f lu, ld, ru, rd;
	sort(vertices, vertices + 4, [](const Point2f & p1, const Point2f & p2) { return p1.x < p2.x; });//根据x的值从小到大排序
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
	trans_camera2ptz1.at<double>(1, 0) = OFF_Y;
	transed_pos = rot_camera2ptz1 * trans - trans_camera2ptz1;               //转换到云台中心

																			 /////////////通过各个方向偏移算出角度并且补偿/////////////////////////
	const double *_xyz = (const double *)transed_pos.data;
	double down_t = 0.0;
	down_t = _xyz[2] / 100.0 / BULLET_SPEED;                 //计算下落时间  速度为m/s，100为单位换算
	diz = _xyz[2];
	cout << "  z轴：" << diz << "  y轴" << _xyz[1] << "  x轴" << _xyz[0] << endl;
	double offset_gravity = -0.5 * 9.8 * down_t * down_t * 100;               //垂直距离单位ms
	double xyz[3] = { _xyz[0], _xyz[1], _xyz[2] };                           //云台与装甲板在运动坐标系中，各个轴向距离；
	double alpha = 0.0, theta = 0.0;
	alpha = asin(OFFSET_Y_BARREL / sqrt(xyz[1] * xyz[1] + xyz[2] * xyz[2]));
	if (xyz[1] < 0) {
		theta = atan(-xyz[1] / xyz[2]);
		angle_y = -(alpha + theta);                                           // camera coordinate
	}
	else if (xyz[1] < OFFSET_Y_BARREL) {
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
