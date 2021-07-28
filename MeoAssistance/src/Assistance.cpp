#include "Assistance.h"

#include "WinMacro.h"
#include "Configer.h"
#include "Identify.h"
#include "Logger.hpp"
#include "AsstAux.h"

#include <opencv2/opencv.hpp>

#include <time.h>
#include <filesystem>

using namespace asst;

Assistance::Assistance()
{
	DebugTraceFunction;

	m_configer.load(GetResourceDir() + "config.json");
	m_recruit_configer.load(GetResourceDir() + "operInfo.json");

	m_pIder = std::make_shared<Identify>();
	for (auto&& [name, info] : m_configer.m_tasks)
	{
		m_pIder->add_image(name, GetResourceDir() + "template\\" + info.template_filename);
	}
	m_pIder->set_use_cache(m_configer.m_options.identify_cache);

	m_pIder->set_ocr_param(m_configer.m_options.ocr_gpu_index, m_configer.m_options.ocr_thread_number);
	m_pIder->ocr_init_models(GetResourceDir() + "OcrLiteNcnn\\models\\");

	m_working_thread = std::thread(working_proc, this);
}

Assistance::~Assistance()
{
	DebugTraceFunction;

	//if (m_pWindow != nullptr) {
	//	m_pWindow->showWindow();
	//}

	m_thread_exit = true;
	m_thread_running = false;
	m_condvar.notify_all();

	if (m_working_thread.joinable()) {
		m_working_thread.join();
	}
}

std::optional<std::string> Assistance::catch_emulator(const std::string& emulator_name)
{
	DebugTraceFunction;

	stop();

	auto create_handles = [&](const EmulatorInfo& info) -> bool {
		m_pWindow = std::make_shared<WinMacro>(info, HandleType::Window);
		m_pView = std::make_shared<WinMacro>(info, HandleType::View);
		m_pCtrl = std::make_shared<WinMacro>(info, HandleType::Control);
		return m_pWindow->captured() && m_pView->captured() && m_pCtrl->captured();
	};

	bool ret = false;
	std::string cor_name = emulator_name;

	std::unique_lock<std::mutex> lock(m_mutex);

	// �Զ�ƥ��ģ�����������
	if (emulator_name.empty()) {
		for (auto&& [name, info] : m_configer.m_handles)
		{
			ret = create_handles(info);
			if (ret) {
				cor_name = name;
				break;
			}
		}
	}
	else {	// ָ����ģ����
		ret = create_handles(m_configer.m_handles[emulator_name]);
	}
	if (ret && m_pWindow->showWindow() && m_pWindow->resizeWindow()) {
		m_inited = true;
		return cor_name;
	}
	else {
		m_inited = false;
		return std::nullopt;
	}
}

void Assistance::start(const std::string& task)
{
	DebugTraceFunction;
	DebugTrace("Start |", task);


	if (m_thread_running || !m_inited) {
		return;
	}

	std::unique_lock<std::mutex> lock(m_mutex);
	m_configer.clear_exec_times();

	m_pIder->clear_cache();
	m_next_tasks.clear();
	m_next_tasks.emplace_back(task);
	m_thread_running = true;
	m_condvar.notify_one();
}

void Assistance::stop(bool block)
{
	DebugTraceFunction;
	DebugTrace("Stop |", block ? "block" : "non block");

	std::unique_lock<std::mutex> lock;
	if (block) { // �ⲿ����
		lock = std::unique_lock<std::mutex>(m_mutex);
		m_configer.clear_exec_times();
	}
	m_thread_running = false;
	m_next_tasks.clear();
	m_pIder->clear_cache();
}

bool Assistance::set_param(const std::string& type, const std::string& param, const std::string& value)
{
	DebugTraceFunction;
	DebugTrace("SetParam |", type, param, value);

	std::unique_lock<std::mutex> lock(m_mutex);
	return m_configer.set_param(type, param, value);
}

std::optional<std::string> Assistance::get_param(const std::string& type, const std::string& param)
{
	// DebugTraceFunction;
	if (type == "status") {
		if (param == "running") {
			return std::to_string(m_thread_running);
		}
		else {
			return std::nullopt;
		}
	}

	std::unique_lock<std::mutex> lock(m_mutex);
	return m_configer.get_param(type, param);
}

bool asst::Assistance::print_window(const std::string& filename, bool block)
{
	DebugTraceFunction;
	DebugTrace("print_window |", block ? "block" : "non block");

	std::unique_lock<std::mutex> lock;
	if (block) { // �ⲿ����
		lock = std::unique_lock<std::mutex>(m_mutex);
	}

	auto&& image = get_format_image();
	// ����Ľ�ͼ�����ٲü���һȦ����Ȼ�������ʶ�𲻳���
	int offset = m_configer.m_options.print_window_crop_offset;
	cv::Rect rect(offset, offset, image.cols - offset * 2, image.rows - offset * 2);
	bool ret = cv::imwrite(filename.c_str(), image(rect));

	if (ret) {
		DebugTraceInfo("PrintWindow to", filename);
	}
	else {
		DebugTraceError("PrintWindow error", filename);
	}
	return ret;
}

bool asst::Assistance::find_text_and_click(const std::string& text, bool block)
{
	DebugTraceFunction;
	DebugTrace("find_text_and_click |", Utf8ToGbk(text), block ? "block" : "non block");

	std::unique_lock<std::mutex> lock;
	if (block) { // �ⲿ����
		lock = std::unique_lock<std::mutex>(m_mutex);
	}
	const auto& image = get_format_image();
	auto&& result = m_pIder->find_text(image, text);

	if (!result) {
		DebugTrace("Cannot found", Utf8ToGbk(text));
		return false;
	}

	set_control_scale(image.cols, image.rows);
	return m_pCtrl->click(result.value());
}

std::vector<std::string> asst::Assistance::find_tags()
{
	DebugTraceFunction;

	const auto& image = get_format_image();
	auto&& ider_result = m_pIder->find_text(image, m_recruit_configer.m_all_tags);

	std::vector<std::string> tags;
	std::string tags_str;
	for (auto&& t_a : ider_result) {
		tags.emplace_back(t_a.text);
		tags_str += t_a.text + " ,";
	}
	if (tags_str.back() == ',') {
		tags_str.pop_back();
	}
	DebugTraceInfo("All Tags", Utf8ToGbk(tags_str));

	// ����tag������ǿ�󶨣����û�и���tag����ʹ����tagƥ������Ҳ�����ܳ�����
	static const std::string SeniorOper = GbkToUtf8("�߼������Ա");
	bool has_senior = std::find(tags.cbegin(), tags.cend(), SeniorOper) != tags.cend();

	// Tagsȫ���
	std::vector<std::vector<std::string>> combs;
	int len = tags.size();
	int count = std::pow(2, len);

	for (int i = 0; i < count; ++i) {
		std::vector<std::string> temp;
		for (int j = 0, mask = 1; j < len; ++j) {
			if ((i & mask) != 0) {	// What the fuck???
				temp.emplace_back(tags.at(j));
			}
			mask = mask * 2;
		}
		// ��Ϸ�����ѡ��3��tag
		if (!temp.empty() && temp.size() <= 3) {
			combs.emplace_back(std::move(temp));
		}
	}

	std::map<std::vector<std::string>, OperCombs> result_map;
	for (auto&& comb : combs) {
		for (auto&& cur_oper : m_recruit_configer.m_opers) {
			int matched_count = 0;
			// �����ÿһ��tag���Ƿ��ڸ�Աtags��
			for (auto&& tag : comb) {
				if (cur_oper.tags.find(tag) != cur_oper.tags.cend()) {
					++matched_count;
				}
				else {
					break;
				}
			}
			if (matched_count == comb.size()) {
				// ����tag������ǿ�󶨣����û�и���tag����ʹ����tagƥ������Ҳ�����ܳ�����
				if (!has_senior && cur_oper.level == 6) {
					continue;
				}
				if (result_map.find(comb) == result_map.cend()) {
					result_map.emplace(comb, OperCombs());
				}
				auto&& oper_combs = result_map[comb];
				oper_combs.opers.emplace_back(cur_oper);

				// һ��С����������͵ȼ�
				if (cur_oper.level != 1 && (
					oper_combs.min_level == 0 || oper_combs.min_level > cur_oper.level)) {
					oper_combs.min_level = cur_oper.level;
				}

				if (oper_combs.max_level == 0 || oper_combs.max_level < cur_oper.level) {
					oper_combs.max_level = cur_oper.level;
				}
			}
		}
	}

	// mapû����ֵ����ת��vector����
	std::vector<std::pair<std::vector<std::string>, OperCombs>> result_vector;
	for (auto&& pair : result_map) {
		result_vector.emplace_back(std::move(pair));
	}
	std::sort(result_vector.begin(), result_vector.end(), [](const auto& lhs, const auto& rhs) ->bool {
		// 1����С�ȼ�С�ģ������
		// 2����С�ȼ���ͬ�����ȼ�С�ģ��ź���
		// 3��1 2����ͬ����Ա����Խ��ģ��ź���
		if (lhs.second.min_level != rhs.second.min_level) {
			return lhs.second.min_level > rhs.second.min_level;
		}
		else if (lhs.second.max_level != rhs.second.max_level) {
			return lhs.second.max_level > rhs.second.max_level;
		}
		else {
			return lhs.second.opers.size() < rhs.second.opers.size();
		}
		});

#ifdef LOG_TRACE
	for (auto&& [combs, oper_combs] : result_vector) {
		std::string tag_str;
		for (auto&& tag : combs) {
			tag_str += tag + " ,";
		}
		if (tags_str.back() == ',') {
			tag_str.pop_back();
		}

		std::string opers_str;
		for (auto&& oper : oper_combs.opers) {
			opers_str += std::to_string(oper.level) + "-" + oper.name + " ,";
		}
		if (opers_str.back() == ',') {
			opers_str.pop_back();
		}
		DebugTraceInfo("Tags:", Utf8ToGbk(tag_str), "May be recruited: ", Utf8ToGbk(opers_str));
	}
#endif

	return tags;
}

void Assistance::working_proc(Assistance* pThis)
{
	DebugTraceFunction;

	while (!pThis->m_thread_exit) {
		std::unique_lock<std::mutex> lock(pThis->m_mutex);
		if (pThis->m_thread_running) {
			auto&& cur_image = pThis->get_format_image();
			pThis->set_control_scale(cur_image.cols, cur_image.rows);

			if (cur_image.empty()) {
				DebugTraceError("Unable to capture window image!!!");
				pThis->stop(false);
				continue;
			}
			if (cur_image.rows < 100) {
				DebugTraceInfo("Window Could not be minimized!!!");
				pThis->m_pWindow->showWindow();
				pThis->m_condvar.wait_for(lock,
					std::chrono::milliseconds(pThis->m_configer.m_options.identify_delay),
					[&]() -> bool { return !pThis->m_thread_running; });
				continue;
			}

			std::string matched_task;
			Rect matched_rect;
			// ���ƥ�䵱ǰ���ܵ�ͼ��
			for (auto&& task_name : pThis->m_next_tasks) {

				double threshold = pThis->m_configer.m_tasks[task_name].threshold;
				double cache_threshold = pThis->m_configer.m_tasks[task_name].cache_threshold;

				auto&& [algorithm, value, rect] = pThis->m_pIder->find_image(cur_image, task_name, threshold);
				DebugTrace(task_name, "Type:", algorithm, "Value:", value);
				if (algorithm == AlgorithmType::JustReturn ||
					(algorithm == AlgorithmType::MatchTemplate && value >= threshold)
					|| (algorithm == AlgorithmType::CompareHist && value >= cache_threshold)) {
					matched_task = task_name;
					matched_rect = rect;
					break;
				}
			}

			// ִ������
			if (!matched_task.empty()) {
				auto&& task = pThis->m_configer.m_tasks[matched_task];
				DebugTraceInfo("***Matched***", matched_task, "Type:", task.type);
				// ǰ�ù̶���ʱ
				if (task.pre_delay > 0) {
					DebugTrace("PreDelay", task.pre_delay);
					// std::this_thread::sleep_for(std::chrono::milliseconds(task.pre_delay));
					bool cv_ret = pThis->m_condvar.wait_for(lock, std::chrono::milliseconds(task.pre_delay),
						[&]() -> bool { return !pThis->m_thread_running; });
					if (cv_ret) { continue; }
				}

				if (task.max_times != INT_MAX) {
					DebugTrace("CurTimes:", task.exec_times, "MaxTimes:", task.max_times);
				}
				if (task.exec_times < task.max_times) {
					// �����ʱ����
					if ((task.type & TaskType::BasicClick)
						&& pThis->m_configer.m_options.control_delay_upper != 0) {
						static std::default_random_engine rand_engine(std::chrono::system_clock::now().time_since_epoch().count());
						static std::uniform_int_distribution<unsigned> rand_uni(pThis->m_configer.m_options.control_delay_lower, pThis->m_configer.m_options.control_delay_upper);
						int delay = rand_uni(rand_engine);
						DebugTraceInfo("Random Delay", delay, "ms");
						bool cv_ret = pThis->m_condvar.wait_for(lock, std::chrono::milliseconds(delay),
							[&]() -> bool { return !pThis->m_thread_running; });
						if (cv_ret) { continue; }
					}
					if (!pThis->m_thread_running) {
						continue;
					}
					switch (task.type) {
					case TaskType::ClickRect:
						matched_rect = task.specific_area;
						[[fallthrough]];
					case TaskType::ClickSelf:
						pThis->m_pCtrl->click(matched_rect);
						break;
					case TaskType::ClickRand:
						pThis->m_pCtrl->click(pThis->m_pCtrl->getWindowRect());
						break;
					case TaskType::DoNothing:
						break;
					case TaskType::Stop:
						DebugTrace("TaskType is Stop");
						pThis->stop(false);
						continue;
						break;
					case TaskType::PrintWindow:
						if (pThis->m_configer.m_options.print_window) {
							// ÿ�ε�������棬������Ʒ����һ���Գ����ģ��и�������������Ҫ��һ���ٽ�ͼ
							int print_delay = pThis->m_configer.m_options.print_window_delay;
							DebugTraceInfo("Ready to print window, delay", print_delay);
							pThis->m_condvar.wait_for(lock,
								std::chrono::milliseconds(print_delay),
								[&]() -> bool { return !pThis->m_thread_running; });

							std::string dirname = GetCurrentDir() + "screenshot\\";
							std::filesystem::create_directory(dirname);
							auto time_str = StringReplaceAll(StringReplaceAll(GetFormatTimeString(), " ", "_"), ":", "-");
							std::string filename = dirname + time_str + ".png";
							pThis->print_window(filename, false);
						}
						break;
					default:
						DebugTraceError("Unknown option type:", task.type);
						break;
					}
					++task.exec_times;

					// �������������ִ�д���
					// ���磬���������ҩ�Ľ����ˣ��൱����һ�ε���ɫ��ʼ�ж�û��Ч
					// ����Ҫ����ɫ��ʼ�ж��Ĵ�����һ
					for (auto&& reduce : task.reduce_other_times) {
						--pThis->m_configer.m_tasks[reduce].exec_times;
						DebugTrace("Reduce exec times", reduce, pThis->m_configer.m_tasks[reduce].exec_times);
					}
					// ���ù̶���ʱ
					if (task.rear_delay > 0) {
						DebugTrace("RearDelay", task.rear_delay);
						// std::this_thread::sleep_for(std::chrono::milliseconds(task.rear_delay));
						auto cv_ret = pThis->m_condvar.wait_for(lock, std::chrono::milliseconds(task.rear_delay),
							[&]() -> bool { return !pThis->m_thread_running; });
						if (cv_ret) { continue; }
					}
					pThis->m_next_tasks = pThis->m_configer.m_tasks[matched_task].next;
				}
				else {
					DebugTraceInfo("Reached limit");
					pThis->m_next_tasks = pThis->m_configer.m_tasks[matched_task].exceeded_next;
				}

				// ����Ϊ�˴�ӡ��־�����о������Ż���
				std::string nexts_str;
				for (auto&& name : pThis->m_next_tasks) {
					nexts_str += name + " ,";
				}
				if (nexts_str.back() == ',') {
					nexts_str.pop_back();
				}
				DebugTrace("Next:", nexts_str);
			}

			pThis->m_condvar.wait_for(lock,
				std::chrono::milliseconds(pThis->m_configer.m_options.identify_delay),
				[&]() -> bool { return !pThis->m_thread_running; });
		}
		else {
			pThis->m_condvar.wait(lock);
		}
	}
}

cv::Mat asst::Assistance::get_format_image()
{
	auto&& raw_image = m_pView->getImage(m_pView->getWindowRect());
	if (raw_image.empty() || raw_image.rows < 100) {
		DebugTraceError("Window image error");
		return raw_image;
	}
	// ��ģ�����߿��һȦ�ü���
	auto&& window_info = m_pView->getEmulatorInfo();
	int x_offset = window_info.x_offset;
	int y_offset = window_info.y_offset;
	int width = raw_image.cols - x_offset - window_info.right_offset;
	int height = raw_image.rows - y_offset - window_info.bottom_offset;

	cv::Mat cropped(raw_image, cv::Rect(x_offset, y_offset, width, height));

	//// �����ߴ磬����Դ�н�ͼ�ı�׼�ߴ�һ��
	//cv::Mat dst;
	//cv::resize(cropped, dst, cv::Size(m_configer.DefaultWindowWidth, m_configer.DefaultWindowHeight));

	return cropped;
}

void asst::Assistance::set_control_scale(int cur_width, int cur_height)
{
	double scale_width = static_cast<double>(cur_width) / m_configer.DefaultWindowWidth;
	double scale_height = static_cast<double>(cur_height) / m_configer.DefaultWindowHeight;
	// ��Щģ�����п������Ĳ�ߣ������ӿ�ȡ�
	// config.json�����õ��ǲ��չ�����offset
	// ����û��Ѳ���������ˣ����в�ߵ���ͷ�����ü���һЩ������ƫС
	// ���԰������泤���������Ǹ��㣬����Ǳ�û���
	double scale = std::max(scale_width, scale_height);
	m_pCtrl->setControlScale(scale);
}
