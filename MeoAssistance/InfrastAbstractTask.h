#pragma once

#include "OcrAbstractTask.h"
#include "AsstDef.h"

namespace asst {
    class InfrastAbstractTask : public OcrAbstractTask
    {
    public:
        using OcrAbstractTask::OcrAbstractTask;
        virtual ~InfrastAbstractTask() = default;
        virtual bool run() = 0;
    protected:
        virtual bool swipe_to_the_left();
        virtual bool click_clear_button();
        virtual bool click_confirm_button();
        virtual bool swipe(bool reverse = false);

        // ����Ա��
        virtual std::vector<TextArea> detect_operators_name(const cv::Mat& image,
            std::unordered_map<std::string, std::string>& feature_cond,
            std::unordered_set<std::string>& feature_whatever);

        constexpr static int SwipeExtraDelayDefault = 1000;
        constexpr static int SwipeDurationDefault = 2000;

        double m_cropped_height_ratio = 0.052;	// ͼƬ�ü�����Ա���ĳ�����ͼƬ �ĸ߶ȱ��������ԭͼ��
        double m_cropped_upper_y_ratio = 0.441;	// ͼƬ�ü�����Ա���ĳ�����ͼƬ���ϰ벿�ָ�Ա���Ĳü�����y������������ԭͼ��
        double m_cropped_lower_y_ratio = 0.831;	// ͼƬ�ü�����Ա���ĳ�����ͼƬ���°벿�ָ�Ա���Ĳü�����y������������ԭͼ��
        Rect m_swipe_begin;									// �߻�����ʶ�𣬵��λ�����ʼ�㣨Rect������㣩
        Rect m_swipe_end;									// �߻�����ʶ�𣬵��λ��������㣨Rect������㣩
        int m_swipe_duration = SwipeDurationDefault;		// ��������ʱ�䣬ʱ��Խ������Խ��
        int m_swipe_extra_delay = SwipeExtraDelayDefault;	// ����֮�����ĵȴ�ʱ��
        int m_swipe_times = 0;								// �����˼��Σ����򻬶����ӣ����򻬶�����
        bool m_keep_swipe = false;							// keep_swipe�����Ƿ�����ı�־λ
    };
}
