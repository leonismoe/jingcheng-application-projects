// driver_tester.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <stdext.h>
//#include <jcapp.h>

LOCAL_LOGGER_ENABLE(_T("hello_opencv"), LOGGER_LEVEL_DEBUGINFO);


#include <iostream>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <math.h>

using namespace cv;

#define SCALE 3
#define KERNEL_SIZE 3
#define MAX_TH	500

#define CANNY_LOW_TH	90

Mat g_src, d_dst;
Mat canny_out;
Mat correct;

int hough_th = 78;
int hough_len = 156;
double g_theta;

void RotateImage(const Mat & src, Mat & dst, double arc)
{
	Point center = Point( src.cols / 2, src.rows / 2);
	Mat rot_mat(2, 3, CV_32FC1);
	rot_mat = getRotationMatrix2D(center, arc * 180 / CV_PI, 1);
	dst = Mat::zeros(src.rows, src.cols, src.type() );
	warpAffine(src, dst, rot_mat, dst.size());
}

// call back for track bar
void OnTrackBarTh(int th, void *)
{
	//Mat tmp;
	//printf("on track bar: th = %d\n", th);
	//Canny(src, tmp, th, th * 3, KERNEL_SIZE);
	//imshow("image", tmp);

	//Mat dst;
	d_dst = Scalar::all(0);
//	g_src.copyTo(d_dst, tmp);
	g_src.copyTo(d_dst);

	double avg_theta = 0;
	// 霍夫曼线检测
	vector<Vec4i> lines;
	HoughLinesP(canny_out, lines, 1, CV_PI / 180, hough_th, hough_len, 10);
	size_t ii = 0;
	for (ii = 0; ii < lines.size(); ++ii)
	{
		Vec4i ll = lines[ii];
		line(d_dst, Point(ll[0], ll[1]), Point(ll[2], ll[3]), Scalar(0, 0, 128+ii*2), 3, CV_AA);
		// 计算线段的倾角
		int dx = ll[2] - ll[0];
		int dy = ll[3] - ll[1];
		double theta = atan2( (double)dy, (double)dx);
		printf("line: %d, theta= %f,", ii, theta * 180 / CV_PI);
		if (theta > (CV_PI / 4) )
		{
			avg_theta += -((CV_PI / 2) - theta);
			printf(" => %f\n", -((CV_PI / 2) - theta) * 180 / CV_PI);
		}
		else if (theta < (CV_PI / -4))
		{
			avg_theta += -( (CV_PI / -2 ) - theta);
			printf(" => %f\n", -((CV_PI / -2) - theta) * 180 / CV_PI);
		}
		else
		{
			avg_theta += theta;
			printf("\n");
		}
//		avg_theta += theta;
	}
	avg_theta /= ii;
	printf("average theta = %f\n", avg_theta * 180 / CV_PI);
	imshow("image", d_dst);

	// 旋转图像
	RotateImage(g_src, correct, avg_theta); 
	imshow("corrected", correct);
	g_theta = avg_theta;
}


int ___tmain(int argc, _TCHAR* argv[])
{
	// 读取原始图像
	/*Mat */g_src=imread(("testdata\\test.jpg"));
	int w = g_src.cols, h = g_src.rows;
	int dw = SCALE- (w % SCALE), dh = SCALE-(h%SCALE);

	// 缩小
	Mat tmp;
	resize(g_src, tmp, Size(w/SCALE, h/SCALE));
	g_src = tmp;

	// 灰度
	Mat src_gray;
	cvtColor(g_src, src_gray, CV_BGR2GRAY);
//	src = tmp;

	// 降噪
	blur(src_gray, tmp, Size(3,3));
	src_gray = tmp;

	// 边缘检测 缺省low threshold = 90
	Canny(src_gray, canny_out, CANNY_LOW_TH, CANNY_LOW_TH * 3, KERNEL_SIZE);
	//threshold( src_gray, canny_out, 128, 255, THRESH_BINARY );
	tmp = canny_out;
	// 开运算
	Mat element = getStructuringElement(MORPH_ELLIPSE, Size(3, 3)); 
	morphologyEx(tmp, canny_out, MORPH_CLOSE, element);
	// 轮廓
	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;
	findContours(canny_out, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));
	printf("found %d contours\n", contours.size());
	// 画轮廓
	Mat drawing = Mat::zeros(canny_out.size(), CV_8UC3);
	for( int ii = 0; ii< contours.size(); ii++ )
	{
//		vector<vector<Point> > contours_poly(contours.size());
//		approxPolyDP( Mat(contours[ii]), contours_poly[ii], 3, true );
		Scalar color = Scalar(0, 255, 0);
		drawContours( drawing, contours, ii, color, 1, 8, vector<Vec4i>(), 0, Point() );
//		rectangle( drawing, boundRect[i].tl(), boundRect[i].br(), color, 2, 8, 0 );
//      circle( drawing, center[i], (int)radius[i], color, 2, 8, 0 );
	}

//	for (int ii = 0; ii < contours.size(); ++ii)
//	{
		//boundRect[i] = boundingRect( Mat(contours_poly[i]) );

//	}


	//Mat tmp;
	//copyMakeBorder(src, tmp, 0, dh, 0, dw, BORDER_CONSTANT, Scalar(255, 255, 255));

//	Mat dst;
	//Size new_size(src.cols /SCAL, src.rows / SCAL);
	//pyrDown(src, dst, new_size);

	// 显示
	namedWindow("image");
	namedWindow("corrected");
	imshow("image", drawing);
	imshow("corrected", canny_out);

	int low_th = 90;
	//createTrackbar("Th", "image", &hough_th, MAX_TH, OnTrackBarTh); 
	//createTrackbar("Len", "image", &hough_len, MAX_TH, OnTrackBarTh); 

	//src.copyTo(dst);
	//imshow("image", dst);
	//OnTrackBarTh(0, NULL);

	waitKey();

	//Mat original_src=imread(("testdata\\test.jpg"));
	//Mat original_dst;

	//RotateImage(original_src, original_dst, g_theta);
	//imwrite("testdata\\test_correct.jpg", original_dst);

	return 0;
	//return jcapp::local_main(argc, argv);
}