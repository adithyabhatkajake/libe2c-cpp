#pragma once

#include <cassert>
#include <set>
#include <unordered_map>

#include "libe2c/promise.hpp"
#include "libe2c/type.h"
#include "libe2c/entity.h"
#include "libe2c/crypto.h"

namespace e2c {

struct Proposal;
// struct Blame ;     // TODO
// struct QuitView ;  // TODO
// struct ReqVote ;   // TODO
// struct Vote ;      // TODO
struct Finality;

/** Abstraction for E2C protocol state machine (without network implementation). */
class E2CCore {
    block_t b0;                                  /** the genesis block */
    double delta;
    /* === state variables === */
    /** block containing the QC for the highest block having one */
    block_t b_mark;                            /**< locked block */
    block_t b_comm;                            /**< last executed block */
    // On finishing 2\delta, use this to commit this block and all its ancestors
    inline void commit_timer_cb (uint32_t ht);
    /* === auxilliary variables === */
    privkey_bt priv_key;            /**< private key for signing votes */
    std::set<block_t> tails;   /**< set of tail blocks */
    ReplicaConfig config;                   /**< replica configuration */
    /* === async event queues === */
    promise_t propose_waiting;
    promise_t receive_proposal_waiting;
    /* == feature switches == */

    block_t get_delivered_blk(const uint256_t &blk_hash);
    void sanity_check_delivered(const block_t &blk);
    void update(const block_t &nblk);
    void on_propose_(const Proposal &prop);
    void on_receive_proposal_(const Proposal &prop);
    // void on_blame_(const Blame &bl );
    // void on_receive_blame(const Blame &bl);
    // void on_quit_view_(const QuitView &qv);
    // void on_receive_quit_view(const QuitView &qv);
    // void on_request_vote_(const ReqVote &rv);
    // void on_receive_request_vote(const ReqVote &rv);
    // void on_vote_(const Vote &vote);
    // void on_receive_vote(const Vote &vote);

    protected:
    ReplicaID id;                  /**< identity of the replica itself */

    public:
    EventContext ec;
    EventContext commit_ec;

    //  Set Commit timer and handler for every block
    std::unordered_map<uint32_t, TimerEvent> commit_queue;
    // Map between height and block
    std::unordered_map<uint32_t, block_t> ht_blk_map;
    BoxObj<EntityStorage> storage;

    E2CCore(ReplicaID id, privkey_bt &&priv_key);
    virtual ~E2CCore() {}

    /* Inputs of the state machine triggered by external events, should called
     * by the class user, with proper invariants. */

    /** Call to initialize the protocol, should be called once before all other
     * functions. Also initialize EventContext */
    void on_init(uint32_t nfaulty);
    void set_delta(double _d) { delta = _d; }
    double get_delta() { return delta; }
    std::vector<block_t> get_parents();

    /** Call to inform the state machine that a block is ready to be handled.
     * A block is only delivered if itself is fetched, the block for the
     * contained qc is fetched and all parents are delivered. The user should
     * always ensure this invariant. The invalid blocks will be dropped by this
     * function.
     * @return true if valid */
    bool on_deliver_blk(const block_t &blk);

    /** Call upon the delivery of a proposal message.
     * The block mentioned in the message should be already delivered. */
    void on_receive_proposal(const Proposal &prop);

    /** Call to submit new commands to be decided (executed). "Parents" must
     * contain at least one block, and the first block is the actual parent,
     * while the others are uncles/aunts */
    block_t on_propose(const std::vector<uint256_t> &cmds,
                    const std::vector<block_t> &parents,
                    bytearray_t &&extra = bytearray_t());

    /* Functions required to construct concrete instances for abstract classes.
     * */

    /* Outputs of the state machine triggering external events.  The virtual
     * functions should be implemented by the user to specify the behavior upon
     * the events. */
    protected:
    /** Called by E2CCore upon the decision being made for cmd. */
    virtual void do_decide(Finality &&fin) = 0;
    virtual void do_consensus(const block_t &blk) = 0;
    /** Called by E2CCore upon broadcasting a new proposal.
     * The user should send the proposal message to all replicas except for
     * itself. */
    virtual void do_broadcast_proposal(const Proposal &prop) = 0;
    // virtual void do_blame(const Blame &bl) = 0 ;
    // virtual void do_quit_view(const QuitView &qv) = 0;
    // virtual void do_req_vote(const ReqVote &rv) = 0;
    // virtual void do_vote(const Vote &vote) = 0;

    /* The user plugs in the detailed instances for those
     * polymorphic data types. */
    public:
    /** Create a partial certificate that proves the vote for a block. */
    virtual part_cert_bt create_part_cert(const PrivKey &priv_key, const uint256_t &blk_hash) = 0;
    /** Create a partial certificate from its seralized form. */
    virtual part_cert_bt parse_part_cert(DataStream &s) = 0;
    /** Create a quorum certificate that proves 2f+1 votes for a block. */
    virtual quorum_cert_bt create_quorum_cert(const uint256_t &blk_hash) = 0;
    /** Create a quorum certificate from its serialized form. */
    virtual quorum_cert_bt parse_quorum_cert(DataStream &s) = 0;
    /** Create a command object from its serialized form. */
    //virtual command_t parse_cmd(DataStream &s) = 0;

    public:
    /** Add a replica to the current configuration. This should only be called
     * before running E2CCore protocol. */
    void add_replica(ReplicaID rid, const PeerId &peer_id, pubkey_bt &&pub_key);
    /** Try to prune blocks lower than last committed height - staleness. */
    void prune(uint32_t staleness);

    /* PaceMaker can use these functions to monitor the core protocol state
     * transition */
    /** Get a promise resolved when a new block is proposed. */
    promise_t async_wait_proposal();
    /** Get a promise resolved when a new proposal is received. */
    promise_t async_wait_receive_proposal();
    /** Get a promise resolved when a new blame is detected. */
    // promise_t async_wait_blame();
    /** Get a promise resolved when a new blame is received. */
    // promise_t async_wait_receive_blame();
    /** Get a promise resolved when we quit the view */
    // promise_t async_wait_quit_view();
    /** Get a promise resolved when we receive a quit view message. */
    // promise_t async_wait_receive_quit_view();
    /** Get a promise resolved when we request a vote */
    // promise_t async_wait_request_vote();
    /** Get a promise resolved when we receive a request for a vote. */
    // promise_t async_wait_receive_request_vote();
    /** Get a promise resolved when we vote */
    // promise_t async_wait_vote();
    /** Get a promise resolved when we receive a vote. */
    // promise_t async_wait_receive_vote();
    /** Get a promise resolved when we perform a consensus */
    promise_t async_wait_perform_consensus();

    /* Other useful functions */
    const block_t &get_genesis() const { return b0; }
    const ReplicaConfig &get_config() const { return config; }
    ReplicaID get_id() const { return id; }
    const std::set<block_t> get_tails() const { return tails; }
    operator std::string () const;
};

/** Abstraction for proposal messages. */
struct Proposal: public Serializable {
    /** block being proposed */
    block_t blk;
    /** handle of the core object to allow polymorphism. The user should use
     * a pointer to the object of the class derived from E2CCore */
    E2CCore *hsc;

    Proposal(): blk(nullptr), hsc(nullptr) {}
    Proposal(const block_t &blk,
            E2CCore *hsc):
        blk(blk), hsc(hsc) {}

    void serialize(DataStream &s) const override {
          s << *blk
            << *(blk->get_signature()) ;
    }

    void unserialize(DataStream &s) override {
        assert(hsc != nullptr);
        Block _blk;
        _blk.unserialize(s, hsc);
        _blk.set_signature(hsc->parse_part_cert(s));
        blk = hsc->storage->add_blk(std::move(_blk), hsc->get_config());
    }

    operator std::string () const {
        DataStream s;
        s << "<proposal "
          << "rid=" << std::to_string(blk->get_proposer()) << " "
          << "blk=" << get_hex10(blk->get_hash()) << ">"
          << std::string(*blk) ;
        return s;
    }
};

// // BLAME type in {Equivocation, Extension, No-PROGRESS}
// struct EquivBlame: public Serializable {
//     /** Height at which we have a blame */
//     block_t blk1, blk2 ;
//     /** handle of the core object to allow polymorphism. The user should use
//      * a pointer to the object of the class derived from E2CCore */
//     E2CCore *hsc;

//     EquivBlame(): blk1(nullptr), blk2(nullptr), hsc(nullptr) {}
//     EquivBlame(const block_t &blk1,
//                const block_t &blk2,
//             E2CCore *hsc):
//         blk1(blk1), blk2(blk2), hsc(hsc) {}

//     void serialize(DataStream &s) const override {
//         s << *blk1
//              << *blk2 ;
//     }

//     void unserialize(DataStream &s) override {
//         assert(hsc != nullptr);
//         s >> proposer;
//         Block _blk1, _blk2;
//         _blk1.unserialize(s, hsc);
//         _blk2.unserialize(s, hsc);
//         blk1 = hsc->storage->add_blk(std::move(_blk1), hsc->get_config());
//         blk2 = hsc->storage->add_blk(std::move(_blk2), hsc->get_config());
//     }

//     operator std::string () const {
//         DataStream s;
//         s << "<Equiv Blame "
//           << "blk1=" << get_hex10(blk->get_hash()) << ">"
//           << "blk2=" << get_hex10(blk->get_hash()) << ">";
//         return s;
//     }
// };

// struct NoProgress: public Serializable {
//     /** Blamer */
//     uint32_t view_num ; // View Number
//     ReplicaID blamer ;  // The node that is blaming
//     part_cert_bt cert ; // My vote for this blame

//     /** handle of the core object to allow polymorphism. The user should use
//      * a pointer to the object of the class derived from E2CCore */
//     E2CCore *hsc;

//     NoProgress(): view_num(0), blamer(nullptr), hsc(nullptr) , cert(nullptr) {}
//     NoProgress(uint32_t _vnum ,
//             ReplicaID bl,
//             part_cert_bt &&cert,
//             E2CCore *hsc):
//         view_num(_vnum),
//         blamer(bl),
//         cert(std::move(cert)),
//         hsc(hsc) {}

//     void serialize(DataStream &s) const override {
//         s << view_num
//             << blamer
//              << *cert ;
//     }

//     void unserialize(DataStream &s) override {
//         assert(hsc != nullptr);
//         s >> view_num >> blamer ;
//         cert = hsc->parse_part_cert(s) ;
//     }

//     operator std::string () const {
//         DataStream s;
//         s << "<No Progress Blame "
//           << "View Number" << view_num
//           << "by" << blamer ;
//         return s;
//     }
// };


struct Finality: public Serializable {
    ReplicaID rid;
    int8_t decision;
    uint32_t cmd_idx;
    uint32_t cmd_height;
    uint256_t cmd_hash;
    uint256_t blk_hash;

    public:
    Finality() = default;
    Finality(ReplicaID rid,
            int8_t decision,
            uint32_t cmd_idx,
            uint32_t cmd_height,
            uint256_t cmd_hash,
            uint256_t blk_hash):
        rid(rid), decision(decision),
        cmd_idx(cmd_idx), cmd_height(cmd_height),
        cmd_hash(cmd_hash), blk_hash(blk_hash) {}

    void serialize(DataStream &s) const override {
        s << rid << decision
          << cmd_idx << cmd_height
          << cmd_hash;
        if (decision == 1) s << blk_hash;
    }

    void unserialize(DataStream &s) override {
        s >> rid >> decision
          >> cmd_idx >> cmd_height
          >> cmd_hash;
        if (decision == 1) s >> blk_hash;
    }

    operator std::string () const {
        DataStream s;
        s << "<fin "
          << "decision=" << std::to_string(decision) << " "
          << "cmd_idx=" << std::to_string(cmd_idx) << " "
          << "cmd_height=" << std::to_string(cmd_height) << " "
          << "cmd=" << get_hex10(cmd_hash) << " "
          << "blk=" << get_hex10(blk_hash) << ">";
        return s;
    }
};

}
