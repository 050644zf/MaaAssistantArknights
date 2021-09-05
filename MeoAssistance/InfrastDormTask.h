#pragma once

#include "OcrAbstractTask.h"

namespace asst {
	class InfrastDormTask : public OcrAbstractTask
	{
	public:
		InfrastDormTask(AsstCallback callback, void* callback_arg);
		virtual ~InfrastDormTask() = default;

		virtual bool run() override;

	protected:
		struct MoodStatus {
			Rect rect;					// �����������ԭͼ�е�λ�ã��ܵĽ�������
			Rect actual_rect;			// �����������ԭͼ�е�λ�ã���ɫ��Ч���֣�
			double process = 0.0;		// ���������ʣ��ٷֱ�
			int time_left_hour = 0.0;	// ʣ�๤��ʱ�䣨Сʱ����
		};
		constexpr static int MaxOperNumInDorm = 5;	// ��������Ա��

		// �������ᣬindexΪ���ϵ��µı��
		bool enter_dorm(int index = 0);

		// ������һ������
		bool enter_next_dorm();

		// ���뵱ǰ����ĸ�Աѡ�����
		bool enter_operator_selection();

		// ��ղ�ѡ��ԭ�еġ���Ϣ�С���Ա���൱�ڰѡ������С��ĸ�Աȥ�����͡�ע������ɢ���ĸ�Ա
		bool clear_and_select_the_resting();

		// ������ڹ����еĸ�Ա����״̬
		std::vector<MoodStatus> detect_mood_status_at_work(const cv::Mat& image);
	};
}
