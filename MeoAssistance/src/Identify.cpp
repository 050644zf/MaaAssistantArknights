#include "Identify.h"

#include <filesystem>
#include <opencv2/opencv.hpp>
#include <opencv2/xfeatures2d.hpp>
#include <opencv2/imgproc/types_c.h>
#include "Logger.hpp"
#include "AsstAux.h"

using namespace asst;
using namespace cv;
using namespace cv::xfeatures2d;

bool Identify::add_image(const std::string& name, const std::string& path)
{
	Mat mat = imread(path);
	if (mat.empty()) {
		return false;
	}
	m_mat_map.emplace(name, mat);
	return true;
}

bool asst::Identify::add_text_image(const std::string& text, const std::string& path)
{
	Mat mat = imread(path);
	if (mat.empty()) {
		return false;
	}

	m_feature_map.emplace(text, surf_detect(mat));

	return true;
}

void Identify::set_use_cache(bool b) noexcept
{
	if (b) {
		m_use_cache = true;
	}
	else {
		m_cache_map.clear();
		m_use_cache = false;
	}
}

Mat Identify::image_2_hist(const cv::Mat& src)
{
	Mat src_hsv;
	cvtColor(src, src_hsv, COLOR_BGR2HSV);

	int histSize[] = { 50, 60 };
	float h_ranges[] = { 0, 180 };
	float s_ranges[] = { 0, 256 };
	const float* ranges[] = { h_ranges, s_ranges };
	int channels[] = { 0, 1 };

	MatND src_hist;

	calcHist(&src_hsv, 1, channels, Mat(), src_hist, 2, histSize, ranges);
	normalize(src_hist, src_hist, 0, 1, NORM_MINMAX);

	return src_hist;
}

double Identify::image_hist_comp(const cv::Mat& src, const cv::MatND& hist)
{
	// keep the interface return value unchanged
	return 1 - compareHist(image_2_hist(src), hist, CV_COMP_BHATTACHARYYA);
}

asst::Rect asst::Identify::cvrect_2_rect(const cv::Rect& cvRect)
{
	return asst::Rect(cvRect.x, cvRect.y, cvRect.width, cvRect.height);
}

std::pair<std::vector<cv::KeyPoint>, cv::Mat> asst::Identify::surf_detect(const cv::Mat& mat)
{
	// �Ҷ�ͼת��
	cv::Mat mat_gray;
	cv::cvtColor(mat, mat_gray, cv::COLOR_RGB2GRAY);

	constexpr int min_hessian = 800;
	// SURF��������
	cv::Ptr<SURF> detector = SURF::create(min_hessian);
	std::vector<KeyPoint> keypoints;
	cv::Mat mat_vector;
	// �ҵ������㲢��������������(����)
	detector->detectAndCompute(mat_gray, Mat(), keypoints, mat_vector);

	return std::make_pair(std::move(keypoints), std::move(mat_vector));
}

std::vector<TextArea> asst::Identify::ocr_detect(const cv::Mat& mat)
{
	OcrResult ocr_results = m_ocr_lite.detect(mat,
		50, 0,
		0.6f, 0.3f,
		2.0f, false, false);

	std::vector<TextArea> result;
	for (TextBlock& text_block : ocr_results.textBlocks) {
		if (text_block.boxPoint.size() != 4) {
			continue;
		}
		// the rect like ��
		// 0 - 1
		// 3 - 2
		int x = text_block.boxPoint.at(0).x;
		int y = text_block.boxPoint.at(0).y;
		int width = text_block.boxPoint.at(1).x - x;
		int height = text_block.boxPoint.at(3).y - y;
		result.emplace_back(std::move(text_block.text), x, y, width, height);
	}
	return result;
}

std::pair<double, cv::Point> Identify::match_template(const cv::Mat& image, const cv::Mat& templ)
{
	Mat image_hsv;
	Mat templ_hsv;
	cvtColor(image, image_hsv, COLOR_BGR2HSV);
	cvtColor(templ, templ_hsv, COLOR_BGR2HSV);

	Mat matched;
	matchTemplate(image_hsv, templ_hsv, matched, cv::TM_CCOEFF_NORMED);

	double minVal = 0, maxVal = 0;
	cv::Point minLoc, maxLoc;
	minMaxLoc(matched, &minVal, &maxVal, &minLoc, &maxLoc);
	return { maxVal, maxLoc };
}

std::tuple<AlgorithmType, double, asst::Rect> Identify::find_image(const Mat& cur, const std::string& templ, double templ_threshold)
{
	if (m_mat_map.find(templ) == m_mat_map.cend()) {
		return { AlgorithmType::JustReturn, 0, asst::Rect() };
	}

	// �л��棬��ֱ��ͼ�Ƚϣ�CPUռ�û�ͺܶ࣬��Ҫ��֤ÿ�ΰ�ťͼƬ��λ�ò���
	if (m_use_cache && m_cache_map.find(templ) != m_cache_map.cend()) {
		const auto& [rect, hist] = m_cache_map.at(templ);
		double value = image_hist_comp(cur(rect), hist);
		return { AlgorithmType::CompareHist, value, cvrect_2_rect(rect).center_zoom(0.8) };
	}
	else {	// û�����ģ��ƥ��
		const cv::Mat& templ_mat = m_mat_map.at(templ);
		const auto& [value, point] = match_template(cur, templ_mat);
		cv::Rect raw_rect(point.x, point.y, templ_mat.cols, templ_mat.rows);

		if (m_use_cache && value >= templ_threshold) {
			m_cache_map.emplace(templ, std::make_pair(raw_rect, image_2_hist(cur(raw_rect))));
		}

		return { AlgorithmType::MatchTemplate, value, cvrect_2_rect(raw_rect).center_zoom(0.8) };
	}
}

void asst::Identify::feature_matching(const cv::Mat& mat)
{
	auto && [income_keypoints, income_mat_vector] = surf_detect(mat);

	static FlannBasedMatcher matcher;
	
	for (auto&& [key, feature] : m_feature_map) {
		auto&& [keypoints, mat_vector] = feature;
		std::vector<cv::DMatch> matches;
		matcher.match(mat_vector, income_mat_vector, matches);
	}
}

void Identify::clear_cache()
{
	m_cache_map.clear();
}

// gpu_index��ncnn��ܵĲ��������ڻ���onnx�ģ��Ѿ�û����������ˣ�����Ϊ�˱��ֽӿ�һ���ԣ��������������ʵ�ʲ�������
void asst::Identify::set_ocr_param(int gpu_index, int number_thread)
{
	m_ocr_lite.setNumThread(number_thread);
}

bool asst::Identify::ocr_init_models(const std::string& dir)
{
	constexpr static const char* DetName = "dbnet.onnx";
	constexpr static const char* ClsName = "angle_net.onnx";
	constexpr static const char* RecName = "crnn_lite_lstm.onnx";
	constexpr static const char* KeysName = "keys.txt";

	const std::string dst_filename = dir + DetName;
	const std::string cls_filename = dir + ClsName;
	const std::string rec_filename = dir + RecName;
	const std::string keys_filename = dir + KeysName;

	if (std::filesystem::exists(dst_filename)
		&& std::filesystem::exists(cls_filename)
		&& std::filesystem::exists(rec_filename)
		&& std::filesystem::exists(keys_filename))
	{
		m_ocr_lite.initModels(dst_filename, cls_filename, rec_filename, keys_filename);
		return true;
	}
	return false;
}

std::optional<asst::Rect> asst::Identify::find_text(const cv::Mat& mat, const std::string& text)
{
	std::vector<TextArea> results = ocr_detect(mat);
	for (const TextArea& res : results) {
		if (res.text == text) {
			return res.rect;
		}
	}
	return std::nullopt;
}

std::vector<TextArea> asst::Identify::find_text(const cv::Mat& mat, const std::vector<std::string>& texts)
{
	std::vector<TextArea> dst;
	std::vector<TextArea> detect_result = ocr_detect(mat);
	for (TextArea& res : detect_result) {
		for (const std::string& t : texts) {
			if (res.text == t) {
				dst.emplace_back(std::move(res));
			}
		}
	}
	return dst;
}

std::vector<TextArea> asst::Identify::find_text(const cv::Mat& mat, const std::unordered_set<std::string>& texts)
{
	std::vector<TextArea> dst;
	std::vector<TextArea> detect_result = ocr_detect(mat);
	for (TextArea& res : detect_result) {
		DebugTrace("detect", Utf8ToGbk(res.text));
		for (const std::string& t : texts) {
			if (res.text == t) {
				dst.emplace_back(std::move(res));
			}
		}
	}
	return dst;
}

/*
std::pair<double, asst::Rect> Identify::findImageWithFile(const cv::Mat& cur, const std::string& filename)
{
	Mat mat = imread(filename);
	if (mat.empty()) {
		return { 0, asst::Rect() };
	}
	return findImage(cur, mat);
}
*/