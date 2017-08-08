#include "meanshift.h"

//构造函数
MeanShift::MeanShift()
{ 
	cfg._Maxiter = 20;								//最大迭代次数为10
	cfg._num_bins = 16;								//采用16*16*16直方图（彩色图片，三种颜色，每种颜色分成16个区间）
	cfg._piexl_range = 256;							//像素值共有256个阶
	_bin_width = cfg._piexl_range / cfg._num_bins;	//每个区间的宽度
	_flag_overflow = false;
}

//区域初始化及目标模型计算，两个输入参数可以不必设置为引用参数
void MeanShift::Ini_target_frame(Mat & frame, Rect & rec)
{
	_frame_cols = frame.cols;	//读入照片的规格（尺寸）
	_frame_rows = frame.rows;		
	_target_region = rec;		//鼠标所选的矩形目标区域（rec的参数相对与读入图像而言）
	_rec_binValue = Mat::zeros(rec.height, rec.width, CV_32F);
	_target_model = Calpdfmodel(frame,_target_region);
}

//计算Epanechnikov核，返回核的所有元素的和，注意这里的kernel必须为引用参数，该函数被Calpdfmodel函数调用，
//调用该函数之前，要先定义kernel，必须指明kernel的维数，维数与所选的矩形区域大小一致
float MeanShift::Epanechnikov_kernel(Mat & kernel)
{
	float cd = PI;							//根据[M.P.Wand,M.C.Jones]Kernel Smoothing P105计算cd，里面涉及gamma函数，详见数学札记
	int d = 2;								//这里只有x,y两个坐标，2维，z=(x,y)
	int rows = kernel.rows;
	int cols = kernel.cols;
	float x, y, normz, sum_kernel = 0.0;
	for (int i = 0;i < rows;i++)
	{
		for (int j = 0;j < cols;j++)
		{
			x = static_cast<float>(j - cols / 2);
			y = static_cast<float>(i - rows / 2);
			//normz = (x*x + y*y) / (rows*rows / 4 + cols*cols / 4);	//为与式（13）统一，这里采用下面的方法
			normz = (x*x) / (rows*rows / 4) + (y*y) / (cols*cols / 4);	//z=(x,y)，这里的normz其实是norm(z)的平方
			kernel.at<float>(i, j) = normz <= 1 ? 0.5 / cd*(d + 2)*(1.0 - normz) : 0;
			
			sum_kernel += kernel.at<float>(i, j);
		}
	}
	return sum_kernel;
}

//根据式（2）（3）计算模型（目标及候选模型都是调用该函数），两个参数也可以不设置为引用参数
Mat MeanShift::Calpdfmodel(const Mat & frame, const Rect & rec)
{
	Mat kernel(rec.height, rec.width, CV_32F, cv::Scalar(1e-10));	//核初始化，注意初始赋值为1e-10，核在式（3）中有作分母，虽然此处不可能出错，但要养成这个习惯
	float C = 1 / Epanechnikov_kernel(kernel);						//根据式（3）就算

	int bins = cfg._num_bins*cfg._num_bins*cfg._num_bins;			//直方图的维数，这里采用绛维，将三维颜色转为长整型一维颜色值，这样，直方图也变成一维
	//Mat pdf_model = Mat::zeros(1, bins, CV_32F);					
	Mat pdf_model(1, bins, CV_32F, cv::Scalar(1e-10));				//直方图初始化，主要是大小初始化
	int bin_value = 0;												//矩形区域内每个元素对应的区间索引
	//std::cout << "Part of the frame are:" << std::endl << frame(rec) << std::endl << std::endl;
	std::vector<cv::Mat> bgr_plane;									//颜色图撕分成三个平面图
	cv::split(frame, bgr_plane);
	//std::cout << "Part of the bgr_plane0 are:" << std::endl << bgr_plane[0](rec) << std::endl << std::endl;
	//std::cout << "Part of the bgr_plane1 are:" << std::endl << bgr_plane[1](rec) << std::endl << std::endl;
	//std::cout << "Part of the bgr_plane2 are:" << std::endl << bgr_plane[2](rec) << std::endl << std::endl;
	int row_index = rec.y;
	int col_index;
	int rows = rec.height;
	int cols = rec.width;

	Mat mixImg = Mat::zeros(rows,cols,CV_32F);						//三维颜色图转为对应的一维长整型颜色图（这里的维数是指几种颜色）
	int tempb, tempg, tempr;

	for (int i = 0;i < rows;i++)
	{
		col_index = rec.x;

		if (col_index<0 || col_index>_frame_cols - 1 || row_index<0 || row_index>_frame_rows - 1)
		{
			_flag_overflow = true;
			cout << "Warning: OVERFLOW!! The calculated location of the target is out of the picture!" << endl;
			break;
		}

		uchar *pb = bgr_plane[0].ptr<uchar>(row_index);
		uchar *pg = bgr_plane[1].ptr<uchar>(row_index);
		uchar *pr = bgr_plane[2].ptr<uchar>(row_index);
		int *pmixImg = mixImg.ptr<int>(i);

		//std::cout << "The row index is: " << row_index << std::endl;
		for (int j = 0;j < cols;j++)
		{
			int b = pb[col_index];
			//std::cout << "0(b) is: " << b << std::endl;

			tempb = pb[col_index] / 16;
			tempg = pg[col_index] / 16;
			tempr = pr[col_index] / 16;

			pmixImg[j] = tempb * 256 + tempg * 16 + tempr;		//使用公式blue*256+green*16+red，但是需要先把各种颜色划分成16个区间
			//pmixImg[j] = tempb / 16 + tempg + tempr * 16;		//使用公式blue*256+green*16+red，但是需要先把各种颜色划分成16个区间

			//bin_value = pmixImg[j] / _bin_width;				//根据长整型颜色值计算所属的直方图区间，bin_value是float类型
			bin_value = pmixImg[j];
			_rec_binValue.at<float>(i, j) = bin_value;					//_rec_binValue储存所有元素对应的直方图区间索引，在Calweight函数中将继续使用

			pdf_model.at<float>(0, bin_value) += C*kernel.at<float>(i, j);		//对应的直方图区间更新值，这里bin_value将自动转为int

			col_index++;
		}
		std::cout << std::endl << std::endl;
		row_index++;
	}
	
	return pdf_model;
}

//根据式（10）计算权重，这里的参数都可以设置为非引用参数，这个函数没有输入frame参数，是因为已经有_rec_binValue，不需要frame了，
//我们所需要的仅仅是坐标，这个信息rec可以提供，因此不需要frame了
Mat MeanShift::Calweight(const Rect & rec, Mat target_model, Mat candidate_model)
{
	Mat weight(rec.height, rec.width, CV_32F, cv::Scalar(1e-10));		//取值矩阵初始化，与矩形区域大小一致
	int bin_value;
	float* ptarget = target_model.ptr<float>(0);
	float* pcandidate = candidate_model.ptr<float>(0);

	//矩形区域的所有元素遍历，求得各个元素所咱的权重
	for (int i = 0;i < rec.height;i++)
	{
		float* pweight = weight.ptr<float>(i);

		for (int j = 0;j < rec.width;j++)
		{
			bin_value = _rec_binValue.at<float>(i, j);
			pweight[j] = sqrt(ptarget[bin_value] / pcandidate[bin_value]);
		}
	}
	return weight;
}

//执行跟踪
Rect MeanShift::track(Mat next_frame)
{
	Rect next_rec;

	for (int iter = 0;iter < cfg._Maxiter;iter++)
	{
		next_rec = _target_region;
		float delta_x = 0.0;
		float delta_y = 0.0;
		float normlized_x;
		float normlized_y;
		float flag;

		Mat candidate_model = Calpdfmodel(next_frame, _target_region);

		if (_flag_overflow)
			break;

		Mat weight = Calweight(_target_region, _target_model, candidate_model);
		float center_row = static_cast<float>((weight.rows - 1) / 2.0);
		float center_col = static_cast<float>((weight.cols - 1) / 2.0);
		float sum_weight = 0.0;

		for (int i = 0;i < weight.rows;i++)
		{
			normlized_y = static_cast<float>(i - center_row) / center_row;
			for (int j = 0;j < weight.cols;j++)
			{
				normlized_x = static_cast<float>(j - center_col) / center_col;
				flag = normlized_x*normlized_x + normlized_y*normlized_y>1 ? 0.0 : 1.0;
				delta_x += static_cast<float>(normlized_x*weight.at<float>(i, j)*flag);
				delta_y += static_cast<float>(normlized_y*weight.at<float>(i, j)*flag);

				sum_weight += static_cast<float>(weight.at<float>(i, j)*flag);
			}
		}

		next_rec.x += static_cast<float>(delta_x / sum_weight*center_col);	//之前因为进行了归一化，注意这里的实际偏移量要乘回center_row
		next_rec.y += static_cast<float>(delta_y / sum_weight*center_row);

		if (abs(next_rec.x - _target_region.x) < 1 && abs(next_rec.y - _target_region.y) < 1)
			break;
		else
		{
			_target_region.x = next_rec.x;
			_target_region.y = next_rec.y;
		}
	}
	return next_rec;
}
