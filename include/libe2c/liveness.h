#pragma once

#include "salticidae/util.h"
#include "libe2c/e2c.h"

namespace e2c {

using salticidae::_1;
using salticidae::_2;

/** Abstraction for liveness gadget (oracle). */
class PaceMaker {
    protected:
    E2CCore *hsc;
    public:
    virtual ~PaceMaker() = default;
    /** Initialize the PaceMaker. A derived class should also call the
     * default implementation to set `hsc`. */
    virtual void init(E2CCore *_hsc) { hsc = _hsc; }
    /** Get a promise resolved when the pace maker thinks it is a *good* time
     * to issue new commands. When promise is resolved, the replica should
     * propose the command. */
    virtual promise_t beat() = 0;
    /** Get the current proposer. */
    virtual ReplicaID get_proposer() = 0;
    /** Select the parent blocks for a new block.
     * @return Parent blocks. The block at index 0 is the direct parent, while
     * the others are uncles/aunts. The returned vector should be non-empty. */
    // virtual std::vector<block_t> get_parents() = 0;
    /** Get a promise resolved when the pace maker thinks it is a *good* time
     * to vote for a block. The promise is resolved with the next proposer's ID
     * */
    virtual promise_t beat_resp(ReplicaID last_proposer) = 0;
    /** Impeach the current proposer. */
    virtual void impeach() {}
    virtual void on_consensus(const block_t &) {}
};

using pacemaker_bt = BoxObj<PaceMaker>;

    /*
     * PaceMaker Implementation for E2C. We require the implementation that does the following:
     * 1. If p blocks are not proposed in (2p+1)\Delta then change to the next leader
     * */

class E2CSyncPaceMaker : public virtual PaceMaker {
    ReplicaID proposer ; // The current proposer for the current view
    EventContext ec ;
    /* Constructor that takes the starting proposer for the pacemaker */
    public:
    E2CSyncPaceMaker(ReplicaID start):
        proposer(start) {}

    ReplicaID get_proposer() override {
        return proposer ;
    }

//     // void reg_blame() {
//     //     hsc->async_wait_blame().then([this](const Blame &bl) {
//     //         // TODO what to do after receiving a BLAME
//     //         // TODO implement Blame class
//     //         // TODO implement async_wait_blame in E2CCore
//     //         reg_blame() ;
//     //     });
//     // }

    void init ( E2CCore *_hsc) override {
        hsc = _hsc ;
    }

    // Send correct BLAMEs
    void impeach() override {
        proposer = (proposer + 1) % hsc->get_config().nreplicas;
    }

    promise_t beat() override {
        return promise_t([prop = get_proposer()](promise_t &pm) {pm.resolve(prop);});
    }

    promise_t beat_resp ( ReplicaID last_proposer ) override {
        return promise_t([prop = get_proposer()](promise_t &pm) {pm.resolve(prop);});
    }

    void on_consensus ( const block_t& blk) override {
        logger.info("Achieved consensus for block: %s" , std::string(*blk));
        return ;
    }

//     // Need to implement beat, get_parents, beat_resp, impeach, on_consensus,
//     // get_pending_size
};

} // end of namespace e2c
