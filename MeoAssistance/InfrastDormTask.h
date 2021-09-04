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
	};
}
