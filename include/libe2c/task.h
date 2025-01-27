#pragma once

#include <thread>
#include <unordered_map>
#include <unistd.h>

#include "salticidae/event.h"
#include "libe2c/util.h"

namespace e2c {

class VeriTask {
    friend class VeriPool;
    bool result;
    public:
    virtual bool verify() = 0;
    virtual ~VeriTask() = default;
};

using salticidae::ThreadCall;
using veritask_ut = BoxObj<VeriTask>;
using mpmc_queue_t = salticidae::MPMCQueueEventDriven<VeriTask *>;
using mpsc_queue_t = salticidae::MPSCQueueEventDriven<VeriTask *>;

class VeriPool {
    mpmc_queue_t in_queue;
    mpsc_queue_t out_queue;

    struct Worker {
        std::thread handle;
        EventContext ec;
        BoxObj<ThreadCall> tcall;
    };

    std::vector<Worker> workers;
    std::unordered_map<VeriTask *, std::pair<veritask_ut, promise_t>> pms;

    public:
    VeriPool(EventContext ec, size_t nworker, size_t burst_size = 128) {
        out_queue.reg_handler(ec, [this, burst_size](mpsc_queue_t &q) {
            size_t cnt = burst_size;
            VeriTask *task;
            while (q.try_dequeue(task))
            {
                auto it = pms.find(task);
                it->second.second.resolve(task->result);
                pms.erase(it);
                if (!--cnt) return true;
            }
            return false;
        });

        workers.resize(nworker);
        for (size_t i = 0; i < nworker; i++)
        {
            in_queue.reg_handler(workers[i].ec, [this, burst_size](mpmc_queue_t &q) {
                size_t cnt = burst_size;
                VeriTask *task;
                while (q.try_dequeue(task))
                {
                    /* HOTSTUFF_LOG_DEBUG("%lx working on %u", */
                    /*                     std::this_thread::get_id(), (uintptr_t)task); */
                    task->result = task->verify();
                    out_queue.enqueue(task);
                    if (!--cnt) return true;
                }
                return false;
            });
        }
        for (auto &w: workers)
        {
            w.tcall = new ThreadCall(w.ec);
            w.handle = std::thread([ec=w.ec]() { ec.dispatch(); });
        }
    }

    ~VeriPool() {
        for (auto &w: workers)
            w.tcall->async_call([ec=w.ec](ThreadCall::Handle &) {
                ec.stop();
            });
        for (auto &w: workers)
            w.handle.join();
    }

    promise_t verify(veritask_ut &&task) {
        auto ptr = task.get();
        auto ret = pms.insert(std::make_pair(ptr,
                std::make_pair(std::move(task), promise_t([](promise_t &){}))));
        assert(ret.second);
        in_queue.enqueue(ptr);
        return ret.first->second.second;
    }
};

}
