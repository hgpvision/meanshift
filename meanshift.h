#pragma once
#include "meanshift.h"
#include "opencv2/opencv.hpp"
using cv::Mat;
using cv::Rect;
using std::cout;
using std::endl;
#define PI 3.1415926

class MeanShift
{
private:
	Rect _target_region;	//鼠标选中的矩形目标区域，随着目标移动，算法循环进行，会不断更新（注意：只是矩形区域，是Rect类型，不是Mat）
	Mat _rec_binValue;		//_target_region中的每个坐标属于哪个直方图区间
	Mat _target_model;		//目标模型，即根据式（2）（3）计算得到的q，只计算一次，不同于_target_region还会更新，其值计算完之后一直不变，更不同于在主函数中声明的candidate_model，会随着区域移动不断计算
	float _bin_width;		//直方图每个区间的宽度
	int _frame_rows;		//所采集照片的行数（这两个参数用来检查行泪是否超过所采集照片的大小，防止访问溢出）
	int _frame_cols;		//采集照片的列数
	//关于溢出问题，这里再补充几句：Ini_target_frame函数中，不可能会溢出，因为矩形区域是通过鼠标选取的，且在鼠标响应函数1中，加入了
	//rec &= Rect(0, 0, (*image).cols, (*image).rows);语句，可以反之所选区域超过界限，因此，可能溢出的就是在程序运行过程中，矩形区域
	//逐渐往照片外界移动，同时又访问照片中的像素值，这个时候，就会有溢出问题发生，因此，可能发生溢出的地方只有那些输入参数含有frame且
	//有对其进行访问的地方，为防止溢出，这些地方应该加上检查机制。综述，这里只有在Calpdfmodel函数中可能发生溢出，里面先把输入图片
	//撕开，然后有对其访问。
	struct config
	{
		int _num_bins;		//直方图的区间个数
		int _piexl_range;	//像素值的范围（最大值）
		int _Maxiter;		//最大迭代次数
	}cfg;
public:
	bool _flag_overflow;	//溢出标志位，设为public，外部可以访问

	MeanShift();
	void Ini_target_frame(Mat& frame, Rect& rec);							//初始化鼠标所选区域，并调用Calpdfmodel()函数计算目标模型，只在主函数中调用一次
	float Epanechnikov_kernel(Mat& kernel);									//根据式（12）计算Epanechnikov核，kernel的大小与所选矩形区域一样
	Mat Calpdfmodel(const Mat& frame, const Rect& rec);						//根据式（2）（3）计算模型，会调用Epanechnikov_kernel()函数
	Mat Calweight(const Rect & rec, Mat target_model, Mat candidate_model);	//根据式（10）计算权重
	Rect MeanShift::track(Mat frame);										//跟踪，依次调用 Calpdfmodel()和Calweight()函数
};