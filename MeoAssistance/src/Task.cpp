#include "Task.h"
#include "AsstDef.h"
#include "WinMacro.h"
#include "Identify.h"
#include "Configer.h"
#include "OpenRecruitConfiger.h"
#include <json.h>
#include "AsstAux.h"

#include <chrono>
#include <thread>
#include <utility>
#include <cmath>
#include <mutex>
#include <filesystem>

using namespace asst;

AbstractTask::AbstractTask(AsstCallback callback, void* callback_arg)
	:m_callback(callback),
	m_callback_arg(callback_arg)
{
	;
}

void AbstractTask::set_ptr(
	std::shared_ptr<WinMacro> window_ptr,
	std::shared_ptr<WinMacro> view_ptr,
	std::shared_ptr<WinMacro> control_ptr,
	std::shared_ptr<Identify> identify_ptr)
{
	m_window_ptr = window_ptr;
	m_view_ptr = view_ptr;
	m_control_ptr = control_ptr;
	m_identify_ptr = identify_ptr;
}

void AbstractTask::set_exit_flag(bool* exit_flag)
{
	m_exit_flag = exit_flag;
}

cv::Mat AbstractTask::get_format_image()
{
	const cv::Mat& raw_image = m_view_ptr->getImage(m_view_ptr->getWindowRect());
	if (raw_image.empty()) {
		m_callback(AsstMsg::ImageIsEmpty, json::value(), m_callback_arg);
		return raw_image;
	}
	if (raw_image.rows < 100) {
		m_callback(AsstMsg::WindowMinimized, json::value(), m_callback_arg);
		return raw_image;
	}

	// ��ģ�����߿��һȦ�ü���
	const EmulatorInfo& window_info = m_view_ptr->getEmulatorInfo();
	int x_offset = window_info.x_offset;
	int y_offset = window_info.y_offset;
	int width = raw_image.cols - x_offset - window_info.right_offset;
	int height = raw_image.rows - y_offset - window_info.bottom_offset;

	cv::Mat cropped(raw_image, cv::Rect(x_offset, y_offset, width, height));

	// ����ͼ��ߴ磬�������Ƶ�����
	set_control_scale(cropped.cols, cropped.rows);

	//// �����ߴ磬����Դ�н�ͼ�ı�׼�ߴ�һ��
	//cv::Mat dst;
	//cv::resize(cropped, dst, cv::Size(m_configer.DefaultWindowWidth, m_configer.DefaultWindowHeight));

	return cropped;
}

bool AbstractTask::set_control_scale(int cur_width, int cur_height)
{
	double scale_width = static_cast<double>(cur_width) / Configer::DefaultWindowWidth;
	double scale_height = static_cast<double>(cur_height) / Configer::DefaultWindowHeight;
	double scale = std::max(scale_width, scale_height);
	m_control_ptr->setControlScale(scale);
	return true;
}

void AbstractTask::sleep(unsigned millisecond)
{
	if (millisecond == 0) {
		return;
	}
	auto start = std::chrono::system_clock::now();
	unsigned duration = 0;

	json::value callback_json;
	callback_json["time"] = millisecond;
	m_callback(AsstMsg::ReadyToSleep, callback_json, m_callback_arg);

	while ((m_exit_flag == NULL || *m_exit_flag == false)
		&& duration < millisecond) {
		duration = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now() - start).count();
		std::this_thread::yield();
	}
	m_callback(AsstMsg::EndOfSleep, callback_json, m_callback_arg);
}

bool AbstractTask::print_window(const std::string& dir)
{
	const cv::Mat& image = get_format_image();
	if (image.empty()) {
		return false;
	}
	// ����Ľ�ͼ�����ٲü���һȦ����Ȼ�������ʶ�𲻳���
	int offset = Configer::get_instance().m_options.print_window_crop_offset;
	cv::Rect rect(offset, offset, image.cols - offset * 2, image.rows - offset * 2);

	std::filesystem::create_directory(dir);
	const std::string time_str = StringReplaceAll(StringReplaceAll(GetFormatTimeString(), " ", "_"), ":", "-");
	const std::string filename = dir + time_str + ".png";

	bool ret = cv::imwrite(filename.c_str(), image(rect));

	json::value callback_json;
	callback_json["filename"] = filename;
	callback_json["ret"] = ret;
	callback_json["offset"] = offset;
	m_callback(AsstMsg::PrintWindow, callback_json, m_callback_arg);

	return ret;
}

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
		{ "type", static_cast<int>(task_info_ptr->type) },
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
	sleep(task_info_ptr->pre_delay);

	bool need_stop = false;
	switch (task_info_ptr->type) {
	case ProcessTaskType::ClickRect:
		rect = task_info_ptr->specific_area;
		[[fallthrough]];
	case ProcessTaskType::ClickSelf:
		exec_click_task(rect);
		break;
	case ProcessTaskType::DoNothing:
		break;
	case ProcessTaskType::Stop:
		m_callback(AsstMsg::TaskStop, json::object{ {"task_chain", m_task_chain} }, m_callback_arg);
		need_stop = true;
		break;
	case ProcessTaskType::PrintWindow:
	{
		sleep(Configer::get_instance().m_options.print_window_delay);
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
	sleep(task_info_ptr->rear_delay);

	json::value next_json = callback_json;
	next_json["task_chain"] = m_task_chain;
	next_json["retry_times"] = m_retry_times;
	next_json["tasks"] = json::array(task_info_ptr->next);
	m_callback(AsstMsg::AppendProcessTask, next_json, m_callback_arg);

	return true;
}

std::shared_ptr<TaskInfo> ProcessTask::match_image(Rect* matched_rect)
{
	//// �����ǰ����һ������������������ֵ��0��˵����justreturn�ģ����Ͳ�ץͼ��ʶ���ˣ�ֱ�ӷ��ؾ���
	//if (m_cur_tasks_name.size() == 1)
	//{
	//	// ���ߵ����������ģ��϶���ProcessTaskInfo��ֱ��ת
	//	auto task_info_ptr = std::dynamic_pointer_cast<MatchTaskInfo>(
	//		Configer::get_instance().m_all_tasks_info[m_cur_tasks_name[0]]);
	//	if (task_info_ptr->templ_threshold == 0) {
	//		json::value callback_json;
	//		callback_json["threshold"] = 0.0;
	//		callback_json["algorithm"] = "JustReturn";
	//		callback_json["rect"] = json::array({ 0, 0, 0, 0 });
	//		callback_json["name"] = m_cur_tasks_name[0];
	//		callback_json["algorithm_id"] = static_cast<std::underlying_type<ProcessTaskType>::type>(
	//			AlgorithmType::JustReturn);
	//		callback_json["value"] = 0;

	//		m_callback(AsstMsg::ImageFindResult, callback_json, m_callback_arg);
	//		m_callback(AsstMsg::ImageMatched, callback_json, m_callback_arg);
	//		return task_info_ptr;
	//	}
	//}

	const cv::Mat& cur_image = get_format_image();
	if (cur_image.empty() || cur_image.rows < 100) {
		return nullptr;
	}

	// ���ƥ�䵱ǰ���ܵ�����
	for (const std::string& task_name : m_cur_tasks_name) {
		std::shared_ptr<TaskInfo> task_info_ptr = Configer::get_instance().m_all_tasks_info[task_name];
		if (task_info_ptr == nullptr) {
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

			auto&& [algorithm, value, temp_rect] = m_identify_ptr->find_image(cur_image, task_name, templ_threshold);
			rect = std::move(temp_rect);
			callback_json["value"] = value;

			if (algorithm == AlgorithmType::MatchTemplate) {
				callback_json["threshold"] = templ_threshold;
				callback_json["algorithm"] = "MatchTemplate";
				if (value >= templ_threshold) {
					matched = true;
				}
			}
			else if (algorithm == AlgorithmType::CompareHist) {
				callback_json["threshold"] = hist_threshold;
				callback_json["algorithm"] = "CompareHist";
				if (value >= hist_threshold) {
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

void ProcessTask::exec_click_task(const Rect& matched_rect)
{
	// �����ʱ����
	if (Configer::get_instance().m_options.control_delay_upper != 0) {
		static std::default_random_engine rand_engine(
			std::chrono::system_clock::now().time_since_epoch().count());
		static std::uniform_int_distribution<unsigned> rand_uni(
			Configer::get_instance().m_options.control_delay_lower,
			Configer::get_instance().m_options.control_delay_upper);

		unsigned rand_delay = rand_uni(rand_engine);
		sleep(rand_delay);
	}

	m_control_ptr->click(matched_rect);
}

OcrAbstractTask::OcrAbstractTask(AsstCallback callback, void* callback_arg)
	: AbstractTask(callback, callback_arg)
{
	;
}

std::vector<TextArea> OcrAbstractTask::ocr_detect()
{
	const cv::Mat& image = get_format_image();

	return ocr_detect(image);
}

std::vector<TextArea> OcrAbstractTask::ocr_detect(const cv::Mat& image)
{
	std::vector<TextArea> dst = m_identify_ptr->ocr_detect(image);

	std::vector<json::value> all_text_json_vector;
	for (const TextArea& text_area : dst) {
		all_text_json_vector.emplace_back(Utf8ToGbk(text_area.text));
	}
	json::value all_text_json;
	all_text_json["text"] = json::array(all_text_json_vector);
	all_text_json["task_chain"] = m_task_chain;
	m_callback(AsstMsg::TextDetected, all_text_json, m_callback_arg);

	return dst;
}

OpenRecruitTask::OpenRecruitTask(AsstCallback callback, void* callback_arg)
	: OcrAbstractTask(callback, callback_arg)
{
	m_task_type = TaskType::TaskTypeRecognition & TaskType::TaskTypeClick;
}

bool OpenRecruitTask::run()
{
	if (m_view_ptr == NULL
		|| m_identify_ptr == NULL)
	{
		m_callback(AsstMsg::PtrIsNull, json::value(), m_callback_arg);
		return false;
	}

	json::value task_start_json = json::object{
		{ "task_type",  "OpenRecruitTask" },
		{ "task_chain", m_task_chain},
	};
	m_callback(AsstMsg::TaskStart, task_start_json, m_callback_arg);

	/* Find all text */
	std::vector<TextArea> all_text_area = ocr_detect();

	/* Filter out all tags from all text */
	std::vector<TextArea> all_tags = text_match(
		all_text_area, OpenRecruitConfiger::get_instance().m_all_tags, Configer::get_instance().m_recruit_ocr_replace);

	std::unordered_set<std::string> all_tags_name;
	std::vector<json::value> all_tags_json_vector;
	for (const TextArea& text_area : all_tags) {
		all_tags_name.emplace(text_area.text);
		all_tags_json_vector.emplace_back(Utf8ToGbk(text_area.text));
	}
	json::value all_tags_json;
	all_tags_json["tags"] = json::array(all_tags_json_vector);
	m_callback(AsstMsg::RecruitTagsDetected, all_tags_json, m_callback_arg);

	/* ����tags��������������������ʶ��©�ˣ� */
	if (all_tags.size() != OpenRecruitConfiger::CorrectNumberOfTags) {
		all_tags_json["type"] = "OpenRecruit";
		m_callback(AsstMsg::OcrResultError, all_tags_json, m_callback_arg);
		return false;
	}

	/* ������ļʱ��9Сʱ�������������*/
	if (m_set_time) {
		json::value settime_json;
		settime_json["task"] = "RecruitTime";
		// ֻ��tag�������˲����ߵ��������һ���ǶԵģ������Ҳ���ʱ�����þ�˵��ʱ���Ѿ��ֶ��޸Ĺ��ˣ�����������
		settime_json["retry_times"] = 0;
		settime_json["task_chain"] = m_task_chain;
		m_callback(AsstMsg::AppendProcessTask, settime_json, m_callback_arg);
	}

	/* ���һ�Ǹ�Ա�Ķ���ص���Ϣ */
	static const std::string SupportMachine_GBK = "֧Ԯ��е";
	static const std::string SupportMachine = GbkToUtf8(SupportMachine_GBK);
	if (std::find(all_tags_name.cbegin(), all_tags_name.cend(), SupportMachine) != all_tags_name.cend()) {
		json::value special_tag_json;
		special_tag_json["tag"] = SupportMachine_GBK;
		m_callback(AsstMsg::RecruitSpecialTag, special_tag_json, m_callback_arg);
	}

	// ʶ�𵽵�5��Tags��ȫ�������
	std::vector<std::vector<std::string>> all_combs;
	int len = all_tags.size();
	int count = std::pow(2, len);
	for (int i = 0; i < count; ++i) {
		std::vector<std::string> temp;
		for (int j = 0, mask = 1; j < len; ++j) {
			if ((i & mask) != 0) {	// What the fuck???
				temp.emplace_back(all_tags.at(j).text);
			}
			mask = mask * 2;
		}
		// ��Ϸ�����ѡ��3��tag
		if (!temp.empty() && temp.size() <= 3) {
			all_combs.emplace_back(std::move(temp));
		}
	}

	// key: tags comb, value: ��Ա���
	// ���� key: { "�ѻ�"��"Ⱥ��" }��value: OperCombs.opers{ "����", "��ѩ", "�ձ�" }
	std::map<std::vector<std::string>, OperCombs> result_map;
	for (const std::vector<std::string>& comb : all_combs) {
		for (const OperInfo& cur_oper : OpenRecruitConfiger::get_instance().m_all_opers) {
			int matched_count = 0;
			for (const std::string& tag : comb) {
				if (cur_oper.tags.find(tag) != cur_oper.tags.cend()) {
					++matched_count;
				}
				else {
					break;
				}
			}

			// ����tags comb�е�ÿһ��tag�������Ա���У�����ø�Ա�������tags comb
			if (matched_count != comb.size()) {
				continue;
			}

			if (cur_oper.level == 6) {
				// ����tag������ǿ�󶨣����û�и���tag����ʹ����tagƥ������Ҳ�����ܳ�����
				static const std::string SeniorOper = GbkToUtf8("�߼������Ա");
				if (std::find(comb.cbegin(), comb.cend(), SeniorOper) == comb.cend()) {
					continue;
				}
			}

			OperCombs& oper_combs = result_map[comb];
			oper_combs.opers.emplace_back(cur_oper);

			if (cur_oper.level == 1 || cur_oper.level == 2) {
				if (oper_combs.min_level == 0) oper_combs.min_level = cur_oper.level;
				if (oper_combs.max_level == 0) oper_combs.max_level = cur_oper.level;
				// һ�ǡ����Ǹ�Ա��������͵ȼ�����Ϊ����9Сʱ֮�󲻿��ܳ�1��2��
				continue;
			}
			if (oper_combs.min_level == 0 || oper_combs.min_level > cur_oper.level) {
				oper_combs.min_level = cur_oper.level;
			}
			if (oper_combs.max_level == 0 || oper_combs.max_level < cur_oper.level) {
				oper_combs.max_level = cur_oper.level;
			}
			oper_combs.avg_level += cur_oper.level;
		}
		if (result_map.find(comb) != result_map.cend()) {
			OperCombs& oper_combs = result_map[comb];
			oper_combs.avg_level /= oper_combs.opers.size();
		}
	}

	// mapû����ֵ����ת��vector������
	std::vector<std::pair<std::vector<std::string>, OperCombs>> result_vector;
	for (auto&& pair : result_map) {
		result_vector.emplace_back(std::move(pair));
	}
	std::sort(result_vector.begin(), result_vector.end(), [](const auto& lhs, const auto& rhs)
		->bool {
			// ��С�ȼ���ģ���ǰ��
			if (lhs.second.min_level != rhs.second.min_level) {
				return lhs.second.min_level > rhs.second.min_level;
			}
			// ���ȼ���ģ���ǰ��
			else if (lhs.second.max_level != rhs.second.max_level) {
				return lhs.second.max_level > rhs.second.max_level;
			}
			// ƽ���ȼ��ߵģ���ǰ��
			else if (std::fabs(lhs.second.avg_level - rhs.second.avg_level) > DoubleDiff) {
				return lhs.second.avg_level > rhs.second.avg_level;
			}
			// Tag�����ٵģ���ǰ��
			else {
				return lhs.first.size() < rhs.first.size();
			}
		});

	/* ����ʶ���� */
	std::vector<json::value> result_json_vector;
	for (const auto& [tags_comb, oper_comb] : result_vector) {
		json::value comb_json;

		std::vector<json::value> tags_json_vector;
		for (const std::string& tag : tags_comb) {
			tags_json_vector.emplace_back(Utf8ToGbk(tag));
		}
		comb_json["tags"] = json::array(std::move(tags_json_vector));

		std::vector<json::value> opers_json_vector;
		for (const OperInfo& oper_info : oper_comb.opers) {
			json::value oper_json;
			oper_json["name"] = Utf8ToGbk(oper_info.name);
			oper_json["level"] = oper_info.level;
			opers_json_vector.emplace_back(std::move(oper_json));
		}
		comb_json["opers"] = json::array(std::move(opers_json_vector));
		comb_json["tag_level"] = oper_comb.min_level;
		result_json_vector.emplace_back(std::move(comb_json));
	}
	json::value results_json;
	results_json["result"] = json::array(std::move(result_json_vector));
	m_callback(AsstMsg::RecruitResult, results_json, m_callback_arg);

	/* ������Ž��tags����ӵ������ */
	if (!m_required_level.empty() && !result_vector.empty()) {
		if (std::find(m_required_level.cbegin(), m_required_level.cend(), result_vector[0].second.min_level)
			== m_required_level.cend()) {
			return true;
		}
		const std::vector<std::string>& final_tags_name = result_vector[0].first;

		json::value task_json;
		task_json["type"] = "ClickTask";
		for (const TextArea& text_area : all_tags) {
			if (std::find(final_tags_name.cbegin(), final_tags_name.cend(), text_area.text) != final_tags_name.cend()) {
				task_json["rect"] = json::array({ text_area.rect.x, text_area.rect.y, text_area.rect.width, text_area.rect.height });
				task_json["retry_times"] = m_retry_times;
				task_json["task_chain"] = m_task_chain;
				m_callback(AsstMsg::AppendTask, task_json, m_callback_arg);
			}
		}
	}

	return true;
}

void OpenRecruitTask::set_param(std::vector<int> required_level, bool set_time)
{
	m_required_level = std::move(required_level);
	m_set_time = set_time;
}

ClickTask::ClickTask(AsstCallback callback, void* callback_arg)
	: AbstractTask(callback, callback_arg)
{
	m_task_type = TaskType::TaskTypeClick;
}

bool ClickTask::run()
{
	if (m_control_ptr == NULL)
	{
		m_callback(AsstMsg::PtrIsNull, json::value(), m_callback_arg);
		return false;
	}
	json::value task_start_json = json::object{
		{ "task_type",  "ClickTask" },
		{ "task_chain", m_task_chain},
	};
	m_callback(AsstMsg::TaskStart, task_start_json, m_callback_arg);

	m_control_ptr->click(m_rect);
	return true;
}

TestOcrTask::TestOcrTask(AsstCallback callback, void* callback_arg)
	: OcrAbstractTask(callback, callback_arg)
{
	m_task_type = TaskType::TaskTypeRecognition & TaskType::TaskTypeClick;
}

bool TestOcrTask::run()
{
	if (m_view_ptr == NULL
		|| m_identify_ptr == NULL)
	{
		m_callback(AsstMsg::PtrIsNull, json::value(), m_callback_arg);
		return false;
	}

	json::value task_start_json = json::object{
		{ "task_type",  "TestOcrTask" },
		{ "task_chain", m_task_chain},
	};
	m_callback(AsstMsg::TaskStart, task_start_json, m_callback_arg);

	/* Find all text */
	std::vector<TextArea> all_text_area = ocr_detect();

	std::vector<json::value> all_text_json_vector;
	for (const TextArea& text_area : all_text_area) {
		all_text_json_vector.emplace_back(Utf8ToGbk(text_area.text));
	}
	json::value all_text_json;
	all_text_json["text"] = json::array(all_text_json_vector);
	m_callback(AsstMsg::TextDetected, all_text_json, m_callback_arg);

	/* Filter out all text from all text */
	std::vector<TextArea> all_text = text_search(
		all_text_area, m_text_vec, Configer::get_instance().m_infrast_ocr_replace);
	std::vector<json::value> all_tags_json_vector;
	for (const TextArea& text_area : all_text) {
		all_tags_json_vector.emplace_back(Utf8ToGbk(text_area.text));
	}
	json::value all_tags_json;
	all_tags_json["tags"] = json::array(all_tags_json_vector);
	m_callback(AsstMsg::RecruitTagsDetected, all_tags_json, m_callback_arg);

	// ���ʶ�𵽵����֣�ֱ�ӻص��ӳ�ȥ�����������
	if (m_need_click) {
		for (const TextArea& text_area : all_text) {
			json::value task_json;
			task_json["type"] = "ClickTask";
			task_json["rect"] = json::array({ text_area.rect.x, text_area.rect.y, text_area.rect.width, text_area.rect.height });
			task_json["retry_times"] = m_retry_times;
			task_json["task_chain"] = m_task_chain;
			m_callback(AsstMsg::AppendTask, task_json, m_callback_arg);
		}
	}

	return true;
}