#pragma once

#include "OcrAbstractTask.h"

namespace asst {
	// ʶ���Ա���񣬻Ὣʶ�𵽵���Ϣд���ļ���
	class IdentifyOperTask : public OcrAbstractTask
	{
	public:
		IdentifyOperTask(AsstCallback callback, void* callback_arg);
		virtual ~IdentifyOperTask() = default;

		virtual bool run() override;

	protected:
		constexpr static int SwipeDuration = 2000;
		constexpr static int SwipeExtraDelay = 1000;

		std::optional<std::unordered_set<OperInfrastInfo>> swipe_and_detect();

		std::vector<TextArea> detect_opers(const cv::Mat& image,
			std::unordered_map<std::string, std::string>& feature_cond,
			std::unordered_set<std::string>& feature_whatever);
		int detect_elite(const cv::Mat& image, const asst::Rect name_rect);

		bool swipe(bool reverse = false);
		bool keep_swipe(bool reverse = false);

		double m_cropped_height_ratio = 0;	// ͼƬ�ü�����Ա���ĳ�����ͼƬ �ĸ߶ȱ��������ԭͼ��
		double m_cropped_upper_y_ratio = 0;	// ͼƬ�ü�����Ա���ĳ�����ͼƬ���ϰ벿�ָ�Ա���Ĳü�����y������������ԭͼ��
		double m_cropped_lower_y_ratio = 0;	// ͼƬ�ü�����Ա���ĳ�����ͼƬ���°벿�ָ�Ա���Ĳü�����y������������ԭͼ��
		Rect m_swipe_begin;			// �߻�����ʶ�𣬵��λ�����ʼ�㣨Rect������㣩
		Rect m_swipe_end;			// �߻�����ʶ�𣬵��λ��������㣨Rect������㣩
		bool m_keep_swipe = false;	// keep_swipe�����Ƿ�����ı�־λ
	};
}
