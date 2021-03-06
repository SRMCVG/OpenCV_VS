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
#include "Predictor.h"
#include "Serialport.h"

using namespace cv;
using namespace std;

#define u16 unsigned int
#define u8 unsigned char

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

//常量定义
#define T_ANGLE_THRE 2 //旋转矩形的角度差阈值
#define T_SIZE_THRE 5
#define THREAD_NUM 8 //线程数
#define GRAY_VALUE 50 //蒙版值
#define CONTOUR_POINT_NUM 20 //轮廓的像素点阈值

#define FACT_X 12                   //装甲板的实际宽
#define FACT_Y 6					//装甲板的实际高度
#define BULLET_SPEED 10				//子弹初速度
#define OFFSET_Y_BARREL 0			//云台与炮管的偏移
#define EXPOSURE_TIME 8				//曝光时间
#define OFF_Y 0						//摄像头和云台中心的偏差

//solvepnp 所需内参
double CamInterParaArray[9] = { 498.5673259258161,0,323.2000525530471,0,498.3701474019903,225.0731190707511,0,0,1};
double CamDistCoeffD[5] = { -0.07067215495428462, 0.3336418667923584, 0.004455346447950378, -0.01120722754745704, -0.5235104596836849 };

Mat CAM_INTER_PARA = Mat(3, 3, CV_64FC1, CamInterParaArray);//相机内参矩阵
Mat CAM_DIST_COEFF = Mat(5, 1, CV_64FC1, CamDistCoeffD);//相机的畸变参数 Mat_<double>(5, 1)
vector<Point3f> WORLD_SPACE;//储存实际的装甲板各个坐标


Mat ROT_CAM2PTZ;//摄像头坐标到云台坐标的转换
Mat TRANS_CAM2PTZ;

void getDiffImage(Mat &src, Mat &dst); //二值化 HSV
vector<RotatedRect> armorDetect(vector<RotatedRect> vEllipse); //检测装甲
inline void drawBox(RotatedRect box, Mat img); //标记装甲5
void getAngle(RotatedRect &rect,Point2f & offset, double &diz, double &angle_x, double& angle_y);

int main()
{
	vector<vector<Point> > contour;//轮廓线条
	vector<RotatedRect> vEllipse; //定以旋转矩形的向量，用于存储发现的目标区域
	vector<RotatedRect> vRlt; //定义储存装甲板的变量

	double diz;     //z轴距离
	double  angle_x;//算出的角度x
	double  angle_y;//算出的角度y
	Point2f  offset;//二维点的偏移

	float half_x = FACT_X / 2.0;
	float half_y = FACT_Y / 2.0;
	WORLD_SPACE.push_back(Point3f(-half_x, -half_y, 0));//初始化世界坐标,即装甲的实际坐标
	WORLD_SPACE.push_back(Point3f(half_x, -half_y, 0));
	WORLD_SPACE.push_back(Point3f(half_x, half_y, 0));
	WORLD_SPACE.push_back(Point3f(-half_x, half_y, 0));

	ROT_CAM2PTZ = Mat::eye(3, 3, CV_64FC1);
	TRANS_CAM2PTZ = Mat::zeros(3, 1, CV_64FC1);
	TRANS_CAM2PTZ.at<double>(1, 0) = OFF_Y;

	Predictor prex; //x角度预测
	Predictor prey; //y角度预测
	double pre_diz = 0;

	VideoCapture capture("E://OpenCV//res//v1.mp4");//取视频
	//VideoCapture capture(0);
	Mat frame;
	capture >> frame;

	Size imgSize = frame.size();
	Point centerPoint(imgSize.width / 2, imgSize.height / 2);

	Mat binary=Mat(imgSize,CV_8UC1);
	Mat img = Mat(imgSize, CV_8UC1);
	Mat element = getStructuringElement(MORPH_RECT, Size(2, 2));

	u16 imCount = 0;

	while (1)
	{
		double t = (double)cvGetTickCount();
		vEllipse.clear();
		capture >> frame;
		frame -= -GRAY_VALUE;
		
		getDiffImage(frame, binary);//二值化
		dilate(binary, img, element, Point(-1, -1), 3);//闭预算
		erode(img, img, element, Point(-1, -1), 1);
		circle(frame, centerPoint, 5, Scalar(255, 0, 255),-1);//画个中心
		
		findContours(img, contour, RETR_CCOMP, CHAIN_APPROX_SIMPLE);//在二值图像中寻找轮廓
		
		for (int i = 0; i<contour.size(); i++)
			if (contour[i].size()> CONTOUR_POINT_NUM)  //判断当前轮廓是否大于阈值
				vEllipse.push_back(fitEllipse(contour[i])); ////拟合椭圆并将发现的目标保存

		if (vEllipse.size() >= 2) //旋转矩形数量应大于2
		{
			vRlt = armorDetect(vEllipse);//获得装甲的旋转矩形
			if (!vRlt.empty())
			{
				drawBox(vRlt[0], frame);
				getAngle(vRlt[0], offset,diz, angle_x, angle_y);
				cout << "x角度: " << angle_x << "  y角度： " << angle_y << endl;
				
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
			}
		}
		imshow("读取视频", img);
		imshow("对比", frame);

		if (waitKey(1) >= 0)break;
		t = ((double)cvGetTickCount() - t) / (cvGetTickFrequency() * 1000);
		cout << "处理时间: " << t << "ms" << endl;
		cout << "帧率:" << 1000 / t << endl << endl;

		imCount++;
		cout << "imCount:" << imCount << endl;
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

void getAngle(RotatedRect &rect,Point2f & offset,double& diz, double &angle_x, double& angle_y)
{
	vector<Point2f> target2d;        //屏幕中的装甲板坐标
	Point2f vertices[4];             //接受屏幕中装甲板坐标
	Mat  trans;                      //solvepnp之后的平移矩阵
	Mat r;                           //solvepnp之后的旋转向量
	Mat rot;                         //罗德里格斯变化之后的平移矩阵
	Mat transed_pos;                 //从摄像头变换到云台中心之后的平移矩阵

	//得到屏幕中装甲板的坐标
	rect.size.width = rect.size.height * FACT_X / FACT_Y; //屏幕中装甲板比例与实际装甲板的匹配
	rect.points(vertices);//将旋转矩形的各个坐标放入vertices
	
	sort(vertices, vertices + 4, [](const Point2f & p1, const Point2f & p2) { return p1.x < p2.x; });//根据x的值从小到大排序
	
	Point2f lu, ld, ru, rd;
	if (vertices[0].y < vertices[1].y) {
		lu = vertices[0];//最左边下面的顶点
		ld = vertices[1];//左边上面的顶点
	}
	else {
		lu = vertices[1];
		ld = vertices[0];
	}
	if (vertices[2].y < vertices[3].y) {
		ru = vertices[2];//右下
		rd = vertices[3];//右上
	}
	else {
		ru = vertices[3];
		rd = vertices[2];
	}
	target2d.clear();
	target2d.push_back(lu + offset);//将坐标放入target2d
	target2d.push_back(ru + offset);
	target2d.push_back(rd + offset);
	target2d.push_back(ld + offset);

	//png问题求解,参数:特征点的世界坐标(float),特征点在图像中的像素坐标,相机内参,相机畸变系数,输出的旋转向量,输出的平移矩阵
	solvePnP(WORLD_SPACE, target2d, CAM_INTER_PARA, CAM_DIST_COEFF, r, trans); //solvepnp得到摄像头的平移矩阵

	transed_pos = ROT_CAM2PTZ * trans - TRANS_CAM2PTZ; //转换到云台中心
																
													   /////////////通过各个方向偏移算出角度并且补偿/////////////////////////
	const double *_xyz = (const double *)transed_pos.data;
	double down_t = 0.0;
	down_t = _xyz[2] / 100.0 / BULLET_SPEED;                 //计算下落时间  速度为m/s，100为单位换算
	diz = _xyz[2];
	//cout << "  z轴：" << diz << "  y轴" << _xyz[1] << "  x轴" << _xyz[0] << endl;
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

inline void drawBox(RotatedRect box, Mat img)
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