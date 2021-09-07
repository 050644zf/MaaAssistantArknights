#pragma once

#include "OcrAbstractTask.h"

namespace asst {
	class InfrastDormTask : public OcrAbstractTask
	{
	public:
		InfrastDormTask(AsstCallback callback, void* callback_arg);
		virtual ~InfrastDormTask() = default;

		virtual bool run() override;
		bool set_dorm_begin(int index) {
			if (index <= 0 || index >= DormNum) {
				return false;
			}
			m_dorm_begin = index;
			return true;
		}

	protected:
		struct MoodStatus {
			Rect rect;					// �����������ԭͼ�е�λ�ã��ܵĽ�������
			Rect actual_rect;			// �����������ԭͼ�е�λ�ã���ɫ��Ч���֣�
			int actual_length;			// �����������ɫ��Ч���ֵĳ��ȣ����ص�������
			double process = 0.0;		// ���������ʣ��ٷֱ�
			int time_left_hour = 0.0;	// ʣ�๤��ʱ�䣨Сʱ����
		};
		constexpr static int MaxOperNumInDorm = 5;	// ��������Ա��
		constexpr static int DormNum = 4;			// ��������

		// �������ᣬindexΪ���ϵ��µı��
		bool enter_dorm(int index = 0);
		// ������һ������
		bool enter_next_dorm();
		// ���뵱ǰ����ĸ�Աѡ�����
		bool enter_operator_selection();
		// ѡ���Ա�����ر���ѡ���˼�����Ա
		int select_operators();
		// ������ڹ����еĸ�Ա����״̬
		std::vector<MoodStatus> detect_mood_status_at_work(
			const cv::Mat& image, double process_threshold = 1.0) const;

		int m_dorm_begin = 0;	// �ӵڼ������Ὺʼ
	};
}
