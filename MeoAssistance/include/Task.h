#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <unordered_map>

#include "AsstDef.h"
#include "AsstAux.h"

#include <json.h>

namespace cv
{
	class Mat;
}
namespace json
{
	class value;
}

namespace asst {

	class WinMacro;
	class Identify;
	class Configer;
	class OpenRecruitConfiger;

	enum TaskType {
		TaskTypeInvalid = 0,
		TaskTypeWindowZoom = 1,
		TaskTypeRecognition = 2,
		TaskTypeClick = 4
	};

	enum class TaskMsg {
		/* Error Msg */
		PtrIsNull,
		ImageIsEmpty,
		WindowMinimized,
		/* Info Msg */
		TaskStart,
		ImageMatched,
		TaskMatched,
		ReachedLimit,
		ReadyToSleep,
		EndOfSleep,
		AppendMatchTask,
		TaskCompleted,
		PrintWindow,
		TaskStop,
		/* Open Recruit Msg */
		TextDetected,
		RecruitTagsDetected,
		OcrResultError,
		RecruitSpecialTag,
		RecruitResult,
		AppendTask
	};
	static std::ostream& operator<<(std::ostream& os, const TaskMsg& type)
	{
		static const std::unordered_map<TaskMsg, std::string> _type_name = {
			{TaskMsg::PtrIsNull, "PtrIsNull"},
			{TaskMsg::ImageIsEmpty, "ImageIsEmpty"},
			{TaskMsg::WindowMinimized, "WindowMinimized"},
			{TaskMsg::TaskStart, "TaskStart"},
			{TaskMsg::ImageMatched, "ImageMatched"},
			{TaskMsg::TaskMatched, "TaskMatched"},
			{TaskMsg::ReachedLimit, "ReachedLimit"},
			{TaskMsg::ReadyToSleep, "ReadyToSleep"},
			{TaskMsg::EndOfSleep, "EndOfSleep"},
			{TaskMsg::AppendMatchTask, "AppendMatchTask"},
			{TaskMsg::TaskCompleted, "TaskCompleted"},
			{TaskMsg::PrintWindow, "PrintWindow"},
			{TaskMsg::TaskStop, "TaskStop"},
			{TaskMsg::TextDetected, "TextDetected"},
			{TaskMsg::RecruitTagsDetected, "RecruitTagsDetected"},
			{TaskMsg::OcrResultError, "OcrResultError"},
			{TaskMsg::RecruitSpecialTag, "RecruitSpecialTag"},
			{TaskMsg::RecruitResult, "RecruitResult"},
			{TaskMsg::AppendTask, "AppendTask"}
		};
		return os << _type_name.at(type);
	}

	// TaskCallback Task����Ϣ�ص�����
	// ������
	// TaskMsg ��Ϣ����
	// const json::value& ��Ϣ����json��ÿ����Ϣ��ͬ��Todo����Ҫ�����Э���ĵ�ɶ��
	// void* �ⲿ�������Զ��������ÿ�λص������ȥ�����鴫��(void*)thisָ�����
	using TaskCallback = std::function<void(TaskMsg, const json::value&, void*)>;

	class AbstractTask
	{
	public:
		AbstractTask(TaskCallback callback, void* callback_arg);
		~AbstractTask() = default;
		AbstractTask(const AbstractTask&) = default;
		AbstractTask(AbstractTask&&) = default;

		virtual void set_ptr(
			std::shared_ptr<WinMacro> window_ptr,
			std::shared_ptr<WinMacro> view_ptr,
			std::shared_ptr<WinMacro> control_ptr,
			std::shared_ptr<Identify> identify_ptr);
		virtual bool run() = 0;

		virtual void set_exit_flag(bool* exit_flag);
		virtual int get_task_type() { return m_task_type; }
		virtual void set_retry_times(int times) { m_retry_times = times; }
		virtual int get_retry_times() { return m_retry_times; }
	protected:
		virtual cv::Mat get_format_image();
		virtual bool set_control_scale(int cur_width, int cur_height);
		virtual void sleep(unsigned millisecond);
		virtual bool print_window(const std::string& dir);

		std::shared_ptr<WinMacro> m_window_ptr = nullptr;
		std::shared_ptr<WinMacro> m_view_ptr = nullptr;
		std::shared_ptr<WinMacro> m_control_ptr = nullptr;
		std::shared_ptr<Identify> m_identify_ptr = nullptr;

		TaskCallback m_callback;
		void* m_callback_arg = NULL;
		bool* m_exit_flag = NULL;
		int m_task_type = TaskType::TaskTypeInvalid;
		int m_retry_times = INT_MAX;
	};

	class ClickTask : public AbstractTask
	{
	public:
		ClickTask(TaskCallback callback, void* callback_arg)
			: AbstractTask(callback, callback_arg)
		{
			m_task_type = TaskType::TaskTypeClick;
		}
		virtual bool run() override;
		void set_rect(asst::Rect rect) { m_rect = std::move(rect); };
	protected:
		asst::Rect m_rect;
	};

	class MatchTask : public AbstractTask
	{
	public:
		MatchTask(TaskCallback callback, void* callback_arg);

		virtual bool run() override;

		virtual void set_tasks(const std::vector<std::string>& cur_tasks_name) {
			m_cur_tasks_name = cur_tasks_name;
		}

	protected:
		std::optional<std::string> match_image(asst::Rect* matched_rect = NULL);
		void exec_click_task(TaskInfo& task, const asst::Rect& matched_rect);

		std::vector<std::string> m_cur_tasks_name;
	};

	class OcrAbstractTask : public AbstractTask
	{
	public:
		OcrAbstractTask(TaskCallback callback, void* callback_arg);
		virtual bool run() override = 0;

	protected:
		std::vector<TextArea> ocr_detect();

		template<typename FilterArray, typename ReplaceMap>
		std::vector<TextArea> text_filter(const std::vector<TextArea>& src,
			const FilterArray& filter_array, const ReplaceMap& replace_map)
		{
			std::vector<TextArea> dst;
			for (const TextArea& text_area : src) {
				TextArea temp = text_area;
				for (const auto& [old_str, new_str] : replace_map) {
					temp.text = StringReplaceAll(temp.text, old_str, new_str);
				}
				for (const auto& text : filter_array) {
					if (temp.text == text) {
						dst.emplace_back(std::move(temp));
					}
				}
			}
			return dst;
		}
	};

	class OpenRecruitTask : public OcrAbstractTask
	{
	public:
		OpenRecruitTask(TaskCallback callback, void* callback_arg);

		virtual bool run() override;
		virtual void set_param(std::vector<int> required_level, bool set_time = true);

	protected:
		std::vector<int> m_required_level;
		bool m_set_time = false;
	};
}
