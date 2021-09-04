#include "Identify.h"

#include <algorithm>
#include <numeric>
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

	constexpr int min_hessian = 400;
	// SURF��������
	static cv::Ptr<SURF> detector = SURF::create(min_hessian);
	std::vector<KeyPoint> keypoints;
	cv::Mat mat_vector;
	// �ҵ������㲢��������������(����)
	detector->detectAndCompute(mat_gray, Mat(), keypoints, mat_vector);

	return std::make_pair(std::move(keypoints), std::move(mat_vector));
}

std::optional<asst::Rect> asst::Identify::feature_match(
	const std::vector<cv::KeyPoint>& query_keypoints, const cv::Mat& query_mat_vector,
	const std::vector<cv::KeyPoint>& train_keypoints, const cv::Mat& train_mat_vector
#ifdef LOG_TRACE
	, const cv::Mat query_mat, const cv::Mat train_mat
#endif
)
{
	if (query_mat_vector.empty() || train_mat_vector.empty()) {
		DebugTraceError("feature_match | image empty");
		return std::nullopt;
	}
	std::vector<cv::DMatch> matches;
	static FlannBasedMatcher matcher;
	matcher.match(query_mat_vector, train_mat_vector, matches);

#ifdef LOG_TRACE
	//std::cout << matches.size() << " / " << query_keypoints.size() << std::endl;
	cv::Mat allmatch_mat;
	cv::drawMatches(query_mat, query_keypoints, train_mat, train_keypoints, matches, allmatch_mat);
#endif

	// ���ľ���
	auto max_iter = std::max_element(matches.cbegin(), matches.cend(),
		[](const cv::DMatch& lhs, const cv::DMatch& rhs) ->bool {
			return lhs.distance < rhs.distance;
		});	// ������ŷʽ���루knn��
	if (max_iter == matches.cend()) {
		return std::nullopt;;
	}
	float maxdist = max_iter->distance;

	std::vector<cv::DMatch> approach_matches;
	std::vector<cv::KeyPoint> train_approach_keypoints;
	std::vector<cv::KeyPoint> query_approach_keypoints;
	std::vector<cv::Point> train_approach_points;
	std::vector<cv::Point> query_approach_points;
	// ���þ������һ�αƽ�
	constexpr static const double MatchRatio = 0.4;
	int approach_index = 0;
	for (const cv::DMatch dmatch : matches) {
		if (dmatch.distance < maxdist * MatchRatio) {
			// ����˵����Խ�磬�Է���һ���Ǽ��һ��
			if (dmatch.queryIdx >= 0 && dmatch.queryIdx < query_keypoints.size()
				&& dmatch.trainIdx >= 0 && dmatch.trainIdx < train_keypoints.size()) {
				approach_matches.emplace_back(dmatch);
				train_approach_points.emplace_back(train_keypoints.at(dmatch.trainIdx).pt);
				query_approach_points.emplace_back(query_keypoints.at(dmatch.queryIdx).pt);
				train_approach_keypoints.emplace_back(train_keypoints.at(dmatch.trainIdx));
				query_approach_keypoints.emplace_back(query_keypoints.at(dmatch.queryIdx));
			}
		}
	}

#ifdef LOG_TRACE
	cv::Mat approach_mat;
	cv::drawMatches(query_mat, query_keypoints, train_mat, train_keypoints, approach_matches, approach_mat);
#endif

	if (query_approach_points.empty())
	{
		return std::nullopt;
	}

	// ʹ��RANSAC�޳��쳣ֵ
	std::vector<uchar> ransac_status;
	cv::Mat fundametal = cv::findFundamentalMat(query_approach_points, train_approach_points, ransac_status, cv::FM_RANSAC);

	std::vector<cv::DMatch> ransac_matchs;
	std::vector<cv::KeyPoint> train_ransac_keypoints;
	std::vector<cv::KeyPoint> query_ransac_keypoints;
	int index = 0;
	for (size_t i = 0; i != ransac_status.size(); ++i) {
		if (ransac_status.at(i) != 0) {
			train_ransac_keypoints.emplace_back(train_approach_keypoints.at(i));
			query_ransac_keypoints.emplace_back(query_approach_keypoints.at(i));
			cv::DMatch dmatch = approach_matches.at(i);
			ransac_matchs.emplace_back(std::move(dmatch));
			++index;
		}
	}

	// ��һ��������ֵ�˲��������쳣�ĵ㡣����㷨�е����TODO���Կ�����ô��
	size_t point_size = train_ransac_keypoints.size();
	if (point_size == 0) {
		return std::nullopt;
	}
	cv::Point sum_point = std::accumulate(
		train_ransac_keypoints.cbegin(), train_ransac_keypoints.cend(), cv::Point(),
		[](cv::Point sum, const cv::KeyPoint& rhs) -> cv::Point {
			return cv::Point(sum.x + rhs.pt.x, sum.y + rhs.pt.y);
		});
	cv::Point avg_point(sum_point.x / point_size, sum_point.y / point_size);
	std::vector<cv::DMatch> good_matchs;
	std::vector<cv::Point> good_points;
	// TODO�������ֵ��Ҫ���ݷֱ��ʽ������ţ��������д�������ļ���
	constexpr static int DistanceThreshold = 100;
	for (size_t i = 0; i != train_ransac_keypoints.size(); ++i) {
		// û��Ҫ����룬x y����һ�¾����ˣ�ʡ��CPUʱ��
		//int distance = std::sqrt(std::pow(avg_point.x - cur_x, 2) + std::pow(avg_point.y - cur_y, 2));
		cv::Point2f& pt = train_ransac_keypoints.at(i).pt;
		int x_distance = std::abs(avg_point.x - pt.x);
		int y_distance = std::abs(avg_point.y - pt.y);
		if (x_distance < DistanceThreshold && y_distance < DistanceThreshold) {
			good_matchs.emplace_back(ransac_matchs.at(i));
			good_points.emplace_back(pt);
		}
	}

#ifdef LOG_TRACE

	cv::Mat ransac_mat;
	cv::drawMatches(query_mat, query_keypoints, train_mat, train_keypoints, ransac_matchs, ransac_mat);

	cv::Mat good_mat;
	cv::drawMatches(query_mat, query_keypoints, train_mat, train_keypoints, good_matchs, good_mat);

#endif

	constexpr static const double MatchSizeRatioThreshold = 0.1;
	if (good_points.size() >= query_keypoints.size() * MatchSizeRatioThreshold) {
		Rect dst;
		int left = 0, right = 0, top = 0, bottom = 0;
		for (const cv::Point& pt : good_points) {
			if (pt.x < left || left == 0) {
				left = pt.x;
			}
			if (pt.x > right || right == 0) {
				right = pt.x;
			}
			if (pt.y < top || top == 0) {
				top = pt.y;
			}
			if (pt.y > bottom || bottom == 0) {
				bottom = pt.y;
			}
		}
		dst = { left, top, right - left, bottom - top };
		return dst;
	}
	return std::nullopt;
}

std::vector<TextArea> asst::Identify::ocr_detect(const cv::Mat& mat)
{
	OcrResult ocr_results = m_ocr_lite.detect(mat,
		50, 0,
		0.2f, 0.3f,
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

asst::Identify::FindImageResult asst::Identify::find_image(
	const cv::Mat& image, const std::string& templ_name, double add_cache_thres)
{
	if (m_mat_map.find(templ_name) == m_mat_map.cend()) {
		return { AlgorithmType::JustReturn, 0, Rect() };
	}

	// �л��棬��ֱ��ͼ�Ƚϣ�CPUռ�û�ͺܶ࣬��Ҫ��֤ÿ�ΰ�ťͼƬ��λ�ò���
	if (m_use_cache && m_cache_map.find(templ_name) != m_cache_map.cend()) {
		const auto& [raw_rect, hist] = m_cache_map.at(templ_name);
		double value = image_hist_comp(image(raw_rect), hist);
		Rect dst_rect = cvrect_2_rect(raw_rect).center_zoom(0.8);
		return { AlgorithmType::CompareHist, value, dst_rect };
	}
	else {	// û�����ģ��ƥ��
		const cv::Mat& templ_mat = m_mat_map.at(templ_name);
		const auto& [value, point] = match_template(image, templ_mat);
		cv::Rect raw_rect(point.x, point.y, templ_mat.cols, templ_mat.rows);
		if (m_use_cache && value >= add_cache_thres) {
			m_cache_map.emplace(templ_name, std::make_pair(raw_rect, image_2_hist(image(raw_rect))));
		}
		Rect dst_rect = cvrect_2_rect(raw_rect).center_zoom(0.8);
		return { AlgorithmType::MatchTemplate, value, dst_rect };
	}
}

std::vector<asst::Identify::FindImageResult> asst::Identify::find_all_images(
	const cv::Mat& image, const std::string& templ_name, double threshold) const
{
	if (m_mat_map.find(templ_name) == m_mat_map.cend()) {
		return std::vector<FindImageResult>();
	}
	const cv::Mat& templ_mat = m_mat_map.at(templ_name);

	Mat image_hsv;
	Mat templ_hsv;
	cvtColor(image, image_hsv, COLOR_BGR2HSV);
	cvtColor(templ_mat, templ_hsv, COLOR_BGR2HSV);

	Mat matched;
	matchTemplate(image_hsv, templ_hsv, matched, cv::TM_CCOEFF_NORMED);

	std::vector<FindImageResult> results;
	for (int i = 0; i != matched.rows; ++i) {
		for (int j = 0; j != matched.cols; ++j) {
			auto value = matched.at<float>(i, j);
			if (value >= threshold) {
				Rect rect = Rect(j, i, templ_mat.cols, templ_mat.rows).center_zoom(0.8);

				bool need_push = true;
				// ��������������̫����ֻȡ����÷ָߵ��Ǹ�
				// һ�����ڵĶ��Ǹո�push��ȥ�ģ����ﵹ���һ��
				for (auto iter = results.rbegin(); iter != results.rend(); ++ iter) {
					if (std::abs(j - iter->rect.x) < 5
						|| std::abs(i - iter->rect.y) < 5) {
						if (iter->score < value) {
							iter->rect = rect;
							iter->score = value;
							need_push = false;
						}	// else �����ͷ�����
						break;
					}
				}
				if (need_push) {
					results.emplace_back(AlgorithmType::MatchTemplate, value, std::move(rect));
				}
			}
		}
	}
	std::sort(results.begin(), results.end(), [](const auto& lhs, const auto& rhs) -> bool {
		return lhs.score > rhs.score;
		});
	return results;
}

std::optional<TextArea> asst::Identify::feature_match(const cv::Mat& mat, const std::string& key)
{
	//DebugTraceFunction;

	if (m_feature_map.find(key) == m_feature_map.cend()) {
		return std::nullopt;
	}
	auto&& [query_keypoints, query_mat_vector] = m_feature_map[key];
	auto&& [train_keypoints, train_mat_vector] = surf_detect(mat);

#ifdef LOG_TRACE
	cv::Mat query_mat = cv::imread(GetResourceDir() + "operators\\" + Utf8ToGbk(key) + ".png");
	auto&& ret = feature_match(query_keypoints, query_mat_vector, train_keypoints, train_mat_vector,
		query_mat, mat);
#else
	auto&& ret = feature_match(query_keypoints, query_mat_vector, train_keypoints, train_mat_vector);
#endif
	if (ret) {
		TextArea dst;
		dst.text = key;
		dst.rect = std::move(ret.value());
		return dst;
	}
	else {
		return std::nullopt;
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