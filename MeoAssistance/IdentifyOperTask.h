#pragma once

#include "InfrastAbstractTask.h"

namespace asst {
	// ʶ���Ա���񣬻Ὣʶ�𵽵���Ϣд���ļ���
	class IdentifyOperTask : public InfrastAbstractTask
	{
	public:
		IdentifyOperTask(AsstCallback callback, void* callback_arg);
		virtual ~IdentifyOperTask() = default;

		virtual bool run() override;

	protected:
		// һ�߻���һ��ʶ��
		std::optional<std::unordered_map<std::string, OperInfrastInfo>> swipe_and_detect();

		// ����Ա��Ӣ���ȼ�
		int detect_elite(const cv::Mat& image, const asst::Rect name_rect);
	};
}
