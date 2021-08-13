#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <optional>
#include <unordered_map>
#include <queue>

#include "AsstDef.h"
#include "Task.h"

namespace cv {
	class Mat;
}

namespace asst {
	class WinMacro;
	class Identify;

	class Assistance
	{
	public:
		Assistance(AsstCallback callback = nullptr, void* callback_arg = nullptr);
		~Assistance();

		std::optional<std::string> catch_emulator(const std::string& emulator_name = std::string());

		// ��ʼˢ����
		void start_sanity();
		// ��ʼ���ʺ��ѻ���
		void start_visit();

		// ��ʼ������ļ����
		void start_open_recruit(const std::vector<int>& required_level, bool set_time = true);
		// ��ʼ������������
		void start_infrast();

		// ��ʼƥ�����񣬵�����
		void start_match_task(const std::string& task, int retry_times = ProcessTaskRetryTimesDefault, bool block = true);
		// ��ʼOCR���ԣ�������
		void start_ocr_test_task(std::vector<std::string> text_vec, bool need_click = false);

		void stop(bool block = true);

		bool set_param(const std::string& type, const std::string& param, const std::string& value);

		// ����������������
		bool swipe(const Point& p1, const Point& p2);

		static constexpr int ProcessTaskRetryTimesDefault = 20;
		static constexpr int OpenRecruitTaskRetyrTimesDefault = 5;

	private:
		static void working_proc(Assistance* p_this);
		static void msg_proc(Assistance* p_this);
		static void task_callback(AsstMsg msg, const json::value& detail, void* custom_arg);

		void append_match_task(const std::string& task_chain, const std::vector<std::string>& tasks, int retry_times = INT_MAX);
		void append_task(const json::value& detail);
		void append_callback(AsstMsg msg, json::value detail);
		void clear_exec_times();

		std::shared_ptr<WinMacro> m_window_ptr = nullptr;
		std::shared_ptr<WinMacro> m_view_ptr = nullptr;
		std::shared_ptr<WinMacro> m_control_ptr = nullptr;
		std::shared_ptr<Identify> m_identify_ptr = nullptr;
		bool m_inited = false;

		bool m_thread_exit = false;
		std::queue<std::shared_ptr<AbstractTask>> m_tasks_queue;
		AsstCallback m_callback = nullptr;
		void* m_callback_arg = nullptr;

		bool m_thread_idle = true;
		std::thread m_working_thread;
		std::mutex m_mutex;
		std::condition_variable m_condvar;

		std::thread m_msg_thread;
		std::queue<std::pair<AsstMsg, json::value>> m_msg_queue;
		std::mutex m_msg_mutex;
		std::condition_variable m_msg_condvar;
	};
}