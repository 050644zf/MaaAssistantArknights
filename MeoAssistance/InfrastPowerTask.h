#pragma once
#include "InfrastAbstractTask.h"

namespace asst {
    // ����վ��������
    // ����ʼǰ��Ҫ���ڻ���������
    class InfrastPowerTask : public InfrastAbstractTask
    {
    public:
        using InfrastAbstractTask::InfrastAbstractTask;
        virtual ~InfrastPowerTask() = default;

        virtual bool run() override;
    protected:
        constexpr static int PowerNum = 3;			// ����վ����

        // ���뵱ǰ����վ�ĸ�Աѡ�����
        bool enter_operator_selection();
        // ѡ���Ա�����ر���ѡ���˼�����Ա��DuckType������վ�̶����� 1
        int select_operators(bool need_to_the_left = false);

    };
}