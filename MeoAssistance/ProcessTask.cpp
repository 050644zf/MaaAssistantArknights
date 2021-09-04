#include "ProcessTask.h"

#include <chrono>
#include <random>

#include <opencv2/opencv.hpp>

#include "AsstAux.h"
#include "Configer.h"
#include "WinMacro.h"
#include "Identify.h"

using namespace asst;

ProcessTask::ProcessTask(AsstCallback callback, void* callback_arg)
	: OcrAbstractTask(callback, callback_arg)
{
	m_task_type = TaskType::TaskTypeRecognition | TaskType::TaskTypeClick;
}

bool ProcessTask::run()
{
	if (m_view_ptr == NULL
		|| m_control_ptr == NULL
		|| m_identify_ptr == NULL
		|| m_control_ptr == NULL)
	{
		m_callback(AsstMsg::PtrIsNull, json::value(), m_callback_arg);
		return false;
	}

	json::value task_start_json = json::object{
		{ "task_type",  "ProcessTask" },
		{ "task_chain", m_task_chain},
		{ "tasks", json::array(m_cur_tasks_name)}
	};
	m_callback(AsstMsg::TaskStart, task_start_json, m_callback_arg);

	std::shared_ptr<TaskInfo> task_info_ptr = nullptr;

	Rect rect;
	task_info_ptr = match_image(&rect);
	if (!task_info_ptr) {
		return false;
	}

	json::value callback_json = json::object{
		{ "name", task_info_ptr->name },
		{ "type", static_cast<int>(task_info_ptr->action) },
		{ "exec_times", task_info_ptr->exec_times },
		{ "max_times", task_info_ptr->max_times },
		{ "task_type", "ProcessTask"},
		{ "algorithm", static_cast<int>(task_info_ptr->algorithm)}
	};
	m_callback(AsstMsg::TaskMatched, callback_json, m_callback_arg);

	if (task_info_ptr->exec_times >= task_info_ptr->max_times)
	{
		m_callback(AsstMsg::ReachedLimit, callback_json, m_callback_arg);

		json::value next_json = callback_json;
		next_json["tasks"] = json::array(task_info_ptr->exceeded_next);
		next_json["retry_times"] = m_retry_times;
		next_json["task_chain"] = m_task_chain;
		m_callback(AsstMsg::AppendProcessTask, next_json, m_callback_arg);
		return true;
	}

	// ǰ�ù̶���ʱ
	if (!sleep(task_info_ptr->pre_delay))
	{
		return false;
	}

	bool need_stop = false;
	switch (task_info_ptr->action) {
	case ProcessTaskAction::ClickRect:
		rect = task_info_ptr->specific_area;
		[[fallthrough]];
	case ProcessTaskAction::ClickSelf:
		exec_click_task(rect);
		break;
	case ProcessTaskAction::SwipeToTheLeft:
	case ProcessTaskAction::SwipeToTheRight:
		exec_swipe_task(task_info_ptr->action);
		break;
	case ProcessTaskAction::DoNothing:
		break;
	case ProcessTaskAction::Stop:
		m_callback(AsstMsg::TaskStop, json::object{ {"task_chain", m_task_chain} }, m_callback_arg);
		need_stop = true;
		break;
	case ProcessTaskAction::PrintWindow:
	{
		if (!sleep(Configer::get_instance().m_options.print_window_delay))
		{
			return false;
		}
		static const std::string dirname = GetCurrentDir() + "screenshot\\";
		print_window(dirname);
	}
	break;
	default:
		break;
	}

	++task_info_ptr->exec_times;

	// �������������ִ�д���
	// ���磬���������ҩ�Ľ����ˣ��൱����һ�ε���ɫ��ʼ�ж�û��Ч
	// ����Ҫ����ɫ��ʼ�ж��Ĵ�����һ
	for (const std::string& reduce : task_info_ptr->reduce_other_times) {
		--Configer::get_instance().m_all_tasks_info[reduce]->exec_times;
	}

	if (need_stop) {
		return true;
	}

	callback_json["exec_times"] = task_info_ptr->exec_times;
	m_callback(AsstMsg::TaskCompleted, callback_json, m_callback_arg);

	// ���ù̶���ʱ
	if (!sleep(task_info_ptr->rear_delay)) {
		return false;
	}

	json::value next_json = callback_json;
	next_json["task_chain"] = m_task_chain;
	next_json["retry_times"] = m_retry_times;
	next_json["tasks"] = json::array(task_info_ptr->next);
	m_callback(AsstMsg::AppendProcessTask, next_json, m_callback_arg);

	return true;
}

std::shared_ptr<TaskInfo> ProcessTask::match_image(Rect* matched_rect)
{
	const cv::Mat& cur_image = get_format_image();
	if (cur_image.empty() || cur_image.rows < 100) {
		return nullptr;
	}

	// ���ƥ�䵱ǰ���ܵ�����
	for (const std::string& task_name : m_cur_tasks_name) {
		std::shared_ptr<TaskInfo> task_info_ptr = Configer::get_instance().m_all_tasks_info[task_name];
		if (task_info_ptr == nullptr) {	// ˵�������ļ���û�������
			m_callback(AsstMsg::PtrIsNull, json::value(), m_callback_arg);
			continue;
		}
		json::value callback_json;
		bool matched = false;
		Rect rect;

		switch (task_info_ptr->algorithm)
		{
		case AlgorithmType::JustReturn:
			callback_json["algorithm"] = "JustReturn";
			matched = true;
			break;
		case AlgorithmType::MatchTemplate:
		{
			std::shared_ptr<MatchTaskInfo> process_task_info_ptr =
				std::dynamic_pointer_cast<MatchTaskInfo>(task_info_ptr);
			double templ_threshold = process_task_info_ptr->templ_threshold;
			double hist_threshold = process_task_info_ptr->hist_threshold;
			double add_cache_thres = process_task_info_ptr->cache ? templ_threshold : Identify::NotAddCache;

			auto&& [algorithm, score, temp_rect] = m_identify_ptr->find_image(cur_image, task_name, add_cache_thres);
			rect = std::move(temp_rect);
			callback_json["value"] = score;

			if (algorithm == AlgorithmType::MatchTemplate) {
				callback_json["threshold"] = templ_threshold;
				callback_json["algorithm"] = "MatchTemplate";
				if (score >= templ_threshold) {
					matched = true;
				}
			}
			else if (algorithm == AlgorithmType::CompareHist) {
				callback_json["threshold"] = hist_threshold;
				callback_json["algorithm"] = "CompareHist";
				if (score >= hist_threshold) {
					matched = true;
				}
			}
			else {
				continue;
			}
		}
		break;
		case AlgorithmType::OcrDetect:
		{
			std::shared_ptr<OcrTaskInfo> ocr_task_info_ptr =
				std::dynamic_pointer_cast<OcrTaskInfo>(task_info_ptr);
			std::vector<TextArea> all_text_area = ocr_detect(cur_image);
			std::vector<TextArea> match_result;
			if (ocr_task_info_ptr->need_match) {
				match_result = text_match(all_text_area,
					ocr_task_info_ptr->text, ocr_task_info_ptr->replace_map);
			}
			else {
				match_result = text_search(all_text_area,
					ocr_task_info_ptr->text, ocr_task_info_ptr->replace_map);
			}
			// TODO������ѳ�������������ô����
			// ��ʱͨ�������ļ�����֤����ֻ�ѳ���һ�����orһ�����Ѳ���
			if (!match_result.empty()) {
				callback_json["text"] = Utf8ToGbk(match_result.at(0).text);
				rect = match_result.at(0).rect;
				matched = true;
			}
		}
		break;
		//CompareHist��MatchTemplate�������㷨����Ӧ��Ϊ���������ò�������
		//case AlgorithmType::CompareHist:
		//	break;
		default:
			// TODO���׸�����Ļص���ȥ
			break;
		}

		callback_json["rect"] = json::array({ rect.x, rect.y, rect.width, rect.height });
		callback_json["name"] = task_name;
		if (matched_rect != NULL) {
			*matched_rect = std::move(rect);
		}
		m_callback(AsstMsg::ImageFindResult, callback_json, m_callback_arg);
		if (matched) {
			m_callback(AsstMsg::ImageMatched, callback_json, m_callback_arg);
			return task_info_ptr;
		}
	}
	return nullptr;
}

// �����ʱ����
bool asst::ProcessTask::delay_random()
{
	if (Configer::get_instance().m_options.control_delay_upper != 0) {
		static std::default_random_engine rand_engine(
			std::chrono::system_clock::now().time_since_epoch().count());
		static std::uniform_int_distribution<unsigned> rand_uni(
			Configer::get_instance().m_options.control_delay_lower,
			Configer::get_instance().m_options.control_delay_upper);

		unsigned rand_delay = rand_uni(rand_engine);

		return sleep(rand_delay);
	}
	return true;
}

void ProcessTask::exec_click_task(const Rect& matched_rect)
{
	if (!delay_random()) {
		return;
	}

	m_control_ptr->click(matched_rect);
}

void asst::ProcessTask::exec_swipe_task(ProcessTaskAction action)
{
	if (!delay_random()) {
		return;
	}
	const static Rect right_rect(Configer::WindowWidthDefault * 0.8, 
		Configer::WindowHeightDefault * 0.4, 
		Configer::WindowWidthDefault * 0.1,
		Configer::WindowHeightDefault * 0.2);

	const static Rect left_rect(Configer::WindowWidthDefault * 0.1,
		Configer::WindowHeightDefault * 0.4,
		Configer::WindowWidthDefault * 0.1,
		Configer::WindowHeightDefault * 0.2);

	switch (action)
	{
	case asst::ProcessTaskAction::SwipeToTheLeft:
		m_control_ptr->swipe(left_rect, right_rect);
		break;
	case asst::ProcessTaskAction::SwipeToTheRight:
		m_control_ptr->swipe(right_rect, left_rect);
		break;
	default:	// �߲������TODO ������
		break;
	}
}
