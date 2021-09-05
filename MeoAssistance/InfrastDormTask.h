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

		// �������ᣬindexΪ���ϵ��µı��
		bool enter_dorm(int index = 0);
		// ������һ������
		bool enter_next_dorm();

		// ���뵱ǰ����ĸ�Աѡ�����
		bool enter_operator_selection();

		// ��ղ�ѡ��ԭ�еġ���Ϣ�С���Ա���൱�ڰѡ������С��ĸ�Աȥ����
		bool clear_and_select_the_resting();
	};
}
