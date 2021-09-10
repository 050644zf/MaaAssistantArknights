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

    };
}