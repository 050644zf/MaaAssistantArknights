#ifndef __OCR_LITE_H__
#define __OCR_LITE_H__

#include "opencv2/core.hpp"
#include "OcrStruct.h"
#include "DbNet.h"
#include "AngleNet.h"
#include "CrnnNet.h"

namespace ocr {
	class OCRLITE_EXPORT OcrLite {
	public:
		OcrLite();

		~OcrLite();

		void setNumThread(int numOfThread);

		void initLogger(bool isConsole, bool isPartImg, bool isResultImg);

		void enableResultTxt(const char* path, const char* imgName);

		void setGpuIndex(int gpuIndex);

		bool initModels(const std::string& detPath, const std::string& clsPath,
			const std::string& recPath, const std::string& keysPath);

		void Logger(const char* format, ...);

		/*
		* padding��ͼ��Ԥ������ͼƬ������Ӱױߣ���������ʶ���ʣ����ֿ�û����ȷ��ס��������ʱ�����Ӵ�ֵ��
		* maxSideLen ����ͼƬ��ߵĳ��ȣ���ֵΪ0�������ţ�����1024�����ͼƬ���ߴ���1024���ͼ��������С��1024�ٽ���ͼ��ָ���㣬���ͼƬ����С��1024�����ţ����ͼƬ����С��32�������ŵ�32��
		* boxScoreThresh�����ֿ����Ŷ����ޣ����ֿ�û����ȷ��ס��������ʱ����С��ֵ��
		* boxThresh�����������顣
		* unClipRatio���������ֿ��С���ʣ�Խ��ʱ�������ֿ�Խ�󡣴�����ͼƬ�Ĵ�С��أ�Խ���ͼƬ��ֵӦ��Խ��
		* doAngle������(1)/����(0) ���ַ����⣬ֻ��ͼƬ���õ������(��ת90~270�ȵ�ͼƬ)������Ҫ�������ַ����⡣
		* mostAngle������(1)/����(0) �Ƕ�ͶƱ(����ͼƬ�����������ַ�����ʶ��)�����������ַ�����ʱ������Ҳ�������á�
		*/
		OcrResult detect(const char* path, const char* imgName,
			int padding, int maxSideLen,
			float boxScoreThresh, float boxThresh, float unClipRatio, bool doAngle, bool mostAngle);

		/*
		* padding��ͼ��Ԥ������ͼƬ������Ӱױߣ���������ʶ���ʣ����ֿ�û����ȷ��ס��������ʱ�����Ӵ�ֵ��
		* maxSideLen ����ͼƬ��ߵĳ��ȣ���ֵΪ0�������ţ�����1024�����ͼƬ���ߴ���1024���ͼ��������С��1024�ٽ���ͼ��ָ���㣬���ͼƬ����С��1024�����ţ����ͼƬ����С��32�������ŵ�32��
		* boxScoreThresh�����ֿ����Ŷ����ޣ����ֿ�û����ȷ��ס��������ʱ����С��ֵ��
		* boxThresh�����������顣
		* unClipRatio���������ֿ��С���ʣ�Խ��ʱ�������ֿ�Խ�󡣴�����ͼƬ�Ĵ�С��أ�Խ���ͼƬ��ֵӦ��Խ��
		* doAngle������(1)/����(0) ���ַ����⣬ֻ��ͼƬ���õ������(��ת90~270�ȵ�ͼƬ)������Ҫ�������ַ����⡣
		* mostAngle������(1)/����(0) �Ƕ�ͶƱ(����ͼƬ�����������ַ�����ʶ��)�����������ַ�����ʱ������Ҳ�������á�
		*/
		OcrResult detect(const cv::Mat& mat,
			int padding, int maxSideLen,
			float boxScoreThresh, float boxThresh, float unClipRatio, bool doAngle, bool mostAngle);
	private:
		bool isOutputConsole = false;
		bool isOutputPartImg = false;
		bool isOutputResultTxt = false;
		bool isOutputResultImg = false;
		FILE* resultTxt;
		DbNet dbNet;
		AngleNet angleNet;
		CrnnNet crnnNet;

		std::vector<cv::Mat> getPartImages(cv::Mat& src, std::vector<TextBox>& textBoxes,
			const char* path, const char* imgName);

		OcrResult detect(const char* path, const char* imgName,
			cv::Mat& src, cv::Rect& originRect, ScaleParam& scale,
			float boxScoreThresh = 0.6f, float boxThresh = 0.3f,
			float unClipRatio = 2.0f, bool doAngle = true, bool mostAngle = true);
	};
}

#endif //__OCR_LITE_H__
