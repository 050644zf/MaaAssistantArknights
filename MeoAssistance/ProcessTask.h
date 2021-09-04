#pragma once

#include "OcrAbstractTask.h"

namespace asst {
	// �������񣬰��������ļ�������õ���������
	class ProcessTask : public OcrAbstractTask
	{
	public:
		ProcessTask(AsstCallback callback, void* callback_arg);
		virtual ~ProcessTask() = default;

		virtual bool run() override;

		virtual void set_tasks(const std::vector<std::string>& cur_tasks_name) {
			m_cur_tasks_name = cur_tasks_name;
		}

	protected:
		std::shared_ptr<TaskInfo> match_image(asst::Rect* matched_rect = NULL);
		bool delay_random();
		void exec_click_task(const asst::Rect& matched_rect);
		void exec_swipe_task(ProcessTaskAction action);

		std::vector<std::string> m_cur_tasks_name;
	};
}