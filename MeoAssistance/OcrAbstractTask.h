#pragma once

#include "AbstractTask.h"

#include "AsstDef.h"
#include "AsstAux.h"

namespace asst {
	class OcrAbstractTask : public AbstractTask
	{
	public:
		using AbstractTask::AbstractTask;
		virtual ~OcrAbstractTask() = default;

		virtual bool run() override = 0;

	protected:
		std::vector<TextArea> ocr_detect(bool use_raw_image = false);
		std::vector<TextArea> ocr_detect(const cv::Mat& image);
		bool click_text(const cv::Mat& image, const std::string& text);

		// ����ƥ�䣬Ҫ�����
		template<typename FilterArray, typename ReplaceMap>
		std::vector<TextArea> text_match(const std::vector<TextArea>& src,
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

		// �������������Ӵ�����
		template<typename FilterArray, typename ReplaceMap>
		std::vector<TextArea> text_search(const std::vector<TextArea>& src,
			const FilterArray& filter_array, const ReplaceMap& replace_map)
		{
			std::vector<TextArea> dst;
			for (const TextArea& text_area : src) {
				TextArea temp = text_area;
				for (const auto& [old_str, new_str] : replace_map) {
					temp.text = StringReplaceAll(temp.text, old_str, new_str);
				}
				for (const auto& text : filter_array) {
					if (temp.text.find(text) != std::string::npos) {
						dst.emplace_back(text, std::move(temp.rect));
					}
				}
			}
			return dst;
		}
	};
}