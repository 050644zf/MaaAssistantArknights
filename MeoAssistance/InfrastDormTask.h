#pragma once

#include "InfrastAbstractTask.h"

namespace asst {
	class InfrastDormTask : public InfrastAbstractTask
	{
	public:
		using InfrastAbstractTask::InfrastAbstractTask;
		virtual ~InfrastDormTask() = default;

		virtual bool run() override;
		bool set_dorm_begin(int index) {
			if (index < 0 || index >= DormNum) {
				return false;
			}
			m_dorm_begin = index;
			return true;
		}
		bool set_select_with_swipe(bool flag) {
			m_select_with_swipe = flag;
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

		virtual bool click_confirm_button() override;
		// ������һ������
		bool enter_next_dorm();
		// ���뵱ǰ����ĸ�Աѡ�����
		bool enter_operator_selection();
		// ѡ���Ա�����ر���ѡ���˼�����Ա
		int select_operators(const cv::Mat& image);
		// ѡ���Ա�����η�װ��������ͼ����������ȷ����ť�ȣ����ر���ѡ���˼�����Ա
		int select_operators(bool need_to_the_left = false);
		// һ�߻���һ��ѡ���Ա�����ر���ѡ���˼�����Ա
		int select_operators_with_swipe(bool need_to_the_left = false);
		// ������ڹ����еĸ�Ա����״̬
		std::vector<MoodStatus> detect_mood_status_at_work(
			const cv::Mat& image, double process_threshold = 1.0) const;

		int m_dorm_begin = 0;				// �ӵڼ������Ὺʼ
		int m_select_with_swipe = false;	// ѡ���Ա�Ƿ���Ҫ��������������ֻѡ���һҳ�ĸ�Ա
	};
}
