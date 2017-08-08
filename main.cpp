/*
 *本程序为Meanshift算法跟踪程序
 *opencv3.1+vs14（或opencv2411_vc12_x86）
 *参考：Kernel-Based Object Tracking, Dorin Comaniciu
*/
#include "meanshift.h"
using cv::Mat;
using cv::Rect;
using cv::Point;
using std::cout;
using std::cin;
using std::endl;

Mat frame;					//读入帧
Rect rec;					//鼠标所选出的矩形目标区域
bool selected = false;		//鼠标响应函数标志位，鼠标左键弹起则置为true，表示选择完毕
bool draw_rec = false;		//鼠标响应函数标志位，按下鼠标左键则开始置为true，表示主函数可以开始绘制矩形区域
Point origin;				//鼠标按下时的点的位置（相对图片左上角）

void on_mouse1(int event, int x, int y, int, void*);	//两个鼠标响应函数，二者功能一样
void on_mouse2(int event, int x, int y, int, void*);
int main()
{
	//输入决定是从视频还是从摄像头获取图片
	char src;
	cout << "The way get the frame(v:video/c:camera): ";
	cin >> src;

	cv::VideoCapture frame_cap;
	switch (src)
	{
	case 'v':
		//frame_cap = cv::VideoCapture("Homework_video.avi");
		//frame_cap = cv::VideoCapture("crowds.wmv");
		frame_cap = cv::VideoCapture("car.wmv");
		//frame_cap = cv::VideoCapture("dragonbaby.wmv");
		//frame_cap = cv::VideoCapture("tracking_video.avi");
		break;
	case 'c':
		//frame_cap = cv::VideoCapture(0);
		frame_cap.open(0);
		break;
	default:
		//cout << "Enter a error way!" << endl;
		frame_cap = cv::VideoCapture("Homework_video.avi");
		break;
	}
	
	if (!frame_cap.isOpened())
	{
		cout << "Failed in get the source of images!" << endl;
		return -1;
	}

	frame_cap >> frame;														//从视频流读入帧
	Mat temp_img = frame.clone();

	cv::namedWindow("Happy-Meanshift", 1);
	cv::setMouseCallback("Happy-Meanshift", on_mouse1, (void*) &temp_img);	//设置回调函数
	//cv::setMouseCallback("Meanshift", on_mouse2, (void*)&temp_img);
	while (selected==false)
	{	
		Mat temp_img2 = temp_img.clone();
		//if (draw_rec && rec.width > 0 && rec.height > 0)
		if (draw_rec)
		{
			cv::rectangle(temp_img2, rec, cv::Scalar(0, 255, 0), 2);	//主函数根据子线程（回调函数标志位selected和draw_rec）决定是否绘制矩形区域
		}		
		cv::imshow("Happy-Meanshift", temp_img2);
		//if (cv::waitKey(15) == 27)	
		//	break;
		cv::waitKey(15);		//主函数根据子线程标志位即时绘制矩形区域（鼠标左键按下后，随鼠标移动而不断绘制矩形区域，直至左键弹起）
	}

	//所选矩形区域最好设置为奇数行列，便于就算中心点
	if (rec.height % 2 == 0)
		rec.height++;
	if (rec.width % 2 == 0)
		rec.width++;

	MeanShift ms;
	ms.Ini_target_frame(frame,rec);		//根据第一帧图像选出目标矩形区域，计算目标区域模型（只计算一次，后面的都是跟这个目标模型做比较）

	Rect next_rec;
	while (1)
	{
		if (!frame_cap.read(frame))
			break;
		next_rec=ms.track(frame);

		if (ms._flag_overflow) return -1;

		cv::rectangle(frame, next_rec, cv::Scalar(0, 255, 0), 2);
		cv::imshow("Happy-Meanshift", frame);
		cv::waitKey(25);
	}
}


//鼠标响应函数1
void on_mouse1(int event, int x, int y, int flag, void* param)
{
	Mat *image = (Mat*) param;

	switch (event)
	{
	case CV_EVENT_LBUTTONDOWN:
		origin.x = x;
		origin.y = y;
		cout << origin.x << endl;
		rec = Rect(origin.x, origin.y, 0, 0);	//这条语句是必须的，因为下面一旦开启draw_rec，在主程序中就会绘出所选区域，因此要对矩形区域初始化，不然横从左上角（默认原点）绘制矩形
		draw_rec = true;
		break;
	case CV_EVENT_MOUSEMOVE:
		if (draw_rec)
		{
			rec.x = MIN(origin.x, x);
			rec.y = MIN(origin.y, y);
			rec.width = std::abs(x-origin.x);
			rec.height = std::abs(y - origin.y);

			rec &= Rect(0, 0, (*image).cols, (*image).rows);		//两个矩形区域求与，得到两个矩形区域的交集，这样可保证所选区域在图片内
		}
		break;
	case CV_EVENT_LBUTTONUP:
		//rec.x = MIN(origin.x, x);
		//rec.y = MIN(origin.y, y);		
		//cv::rectangle(*image, rec, cv::Scalar(0, 255, 0), 2);
		selected = true;
		draw_rec = false;		
		break;
	default:
		break;
	}
}

//鼠标响应函数2（摘自20160222MeanShift_GitHub_Based_On_Paper项目，与on_mouse1作对比）
void on_mouse2(int event, int x, int y, int flag, void* param)
{
	cv::Mat *image = (cv::Mat*) param;
	switch (event) {
	case CV_EVENT_MOUSEMOVE:
		if (draw_rec) {
			rec.width = x - rec.x;
			rec.height = y - rec.y;
		}
		break;

	case CV_EVENT_LBUTTONDOWN:
		draw_rec = true;
		rec = cv::Rect(x, y, 0, 0);
		break;

	case CV_EVENT_LBUTTONUP:
		draw_rec = false;
		//其实对于使用Rect提取ROI时，Rect的宽和高可以是负的，因此没有必要进行正负验证
		//此外，这里的正负验证语句根本用不到，因为这个子线程执行完之后会使得draw_rec=false,selected=true，这样主线程都已经停止while循环了，
		//按我目前的理解，子线程必须全部执行完才会回到主程序，因此在有CV_EVENT_LBUTTONUP事件时，必须执行到break才会恢复主程序
		//if (rec.width < 0) {
		//	rec.x += rec.width;
		//	rec.width *= -1;
		//}
		//if (rec.height < 0) {
		//	rec.y += rec.height;
		//	rec.height *= -1;
		//}
		//cv::rectangle(*image,rec,cv::Scalar(0),2);
		selected = true;
		break;
	}
}