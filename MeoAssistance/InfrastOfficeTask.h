#pragma once
#include "InfrastAbstractTask.h"

namespace asst {
    class InfrastOfficeTask : public InfrastAbstractTask
    {
    public:
        using InfrastAbstractTask::InfrastAbstractTask;
        virtual ~InfrastOfficeTask() = default;

        virtual bool run() override;
    protected:
        // �����Աѡ�����
        bool enter_operator_selection();
        // ѡ���Ա�����ر���ѡ���˼�����Ա��DuckType���칫�ҹ̶����� 1
        int select_operators(bool need_to_the_left = false);
    };

}