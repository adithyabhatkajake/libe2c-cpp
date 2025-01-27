#pragma once

#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "salticidae/util.h"
#include "salticidae/network.h"
#include "salticidae/msg.h"
#include "libe2c/util.h"
#include "libe2c/consensus.h"

namespace e2c {

using salticidae::PeerNetwork;
using salticidae::ElapsedTime;
using salticidae::_1;
using salticidae::_2;

const double ent_waiting_timeout = 10;
const double double_inf = 1e10;

// TODO Seperate message types from core
// TODO Put all op codes in one place

/** Network message format for E2C. */
struct MsgPropose {
    static const opcode_t opcode = 0x0;
    DataStream serialized;
    Proposal proposal;
    MsgPropose(const Proposal &);
    /** Only move the data to serialized, do not parse immediately. */
    MsgPropose(DataStream &&s): serialized(std::move(s)) {}
    /** Parse the serialized data to blks now, with `hsc->storage`. */
    void postponed_parse(E2CCore *hsc);
};

// struct MsgVote {
//     static const opcode_t opcode = 0x1;
//     DataStream serialized;
//     Vote vote;
//     MsgVote(const Vote &);
//     MsgVote(DataStream &&s): serialized(std::move(s)) {}
//     void postponed_parse(E2CCore *hsc);
// };

struct MsgReqBlock {
    static const opcode_t opcode = 0x2;
    DataStream serialized;
    std::vector<uint256_t> blk_hashes;
    MsgReqBlock() = default;
    MsgReqBlock(const std::vector<uint256_t> &blk_hashes);
    MsgReqBlock(DataStream &&s);
};


struct MsgRespBlock {
    static const opcode_t opcode = 0x3;
    DataStream serialized;
    std::vector<block_t> blks;
    MsgRespBlock(const std::vector<block_t> &blks);
    MsgRespBlock(DataStream &&s): serialized(std::move(s)) {}
    void postponed_parse(E2CCore *hsc);
};

// struct MsgEquivBlame {
//     static const opcode_t opcode = 0x4;
//     DataStream serialized;
//     EquivBlame bl;
//     MsgEquivBlame(const EquivBlame &);
//     /** Only move the data to serialized, do not parse immediately. */
//     MsgEquivBlame(DataStream &&s): serialized(std::move(s)) {}
//     /** Parse the serialized data to blks now, with `hsc->storage`. */
//     void postponed_parse(E2CCore *hsc);
// };

// struct MsgNoProgressBlame {
//     static const opcode_t opcode = 0x5;
//     DataStream serialized;
//     NoProgressBlame bl;


//     MsgNoProgressBlame(const NoProgressBlame &);
//     /** Only move the data to serialized, do not parse immediately. */
//     MsgNoProgressBlame(DataStream &&s): serialized(std::move(s)) {}
//     /** Parse the serialized data to blks now, with `hsc->storage`. */
//     void postponed_parse(E2CCore *hsc);
// };

// struct MsgExtBlame {
//     static const opcode_t opcode = 0x6;
//     DataStream serialized;
//     ExtBlame bl;
//     MsgExtBlame(const ExtBlame &);
//     /** Only move the data to serialized, do not parse immediately. */
//     MsgExtBlame(DataStream &&s): serialized(std::move(s)) {}
//     /** Parse the serialized data to blks now, with `hsc->storage`. */
//     void postponed_parse(E2CCore *hsc);
// };

// struct MsgQuitView {
//     static const opcode_t opcode = 0x7;
//     DataStream serialized;
//     QuitView qv ;
//     MsgQuitView(const QuitView &);
//     /** Only move the data to serialized, do not parse immediately. */
//     MsgQuitView(DataStream &&s): serialized(std::move(s)) {}
//     /** Parse the serialized data to blks now, with `hsc->storage`. */
//     void postponed_parse(E2CCore *hsc);
// };

// struct MsgReqVote {
//     static const opcode_t opcode = 0x8;
//     DataStream serialized;
//     ReqVote rv ;
//     MsgReqVote(const ReqVote &);
//     /** Only move the data to serialized, do not parse immediately. */
//     MsgReqVote(DataStream &&s): serialized(std::move(s)) {}
//     /** Parse the serialized data to blks now, with `hsc->storage`. */
//     void postponed_parse(E2CCore *hsc);
// };

using promise::promise_t;

class E2CBase;
using pacemaker_bt = BoxObj<class PaceMaker>;

template<EntityType ent_type>
class FetchContext: public promise_t {
    TimerEvent timeout;
    E2CBase *hs;
    MsgReqBlock fetch_msg;
    const uint256_t ent_hash;
    std::unordered_set<PeerId> replicas;
    inline void timeout_cb(TimerEvent &);
    public:
    FetchContext(const FetchContext &) = delete;
    FetchContext &operator=(const FetchContext &) = delete;
    FetchContext(FetchContext &&other);

    FetchContext(const uint256_t &ent_hash, E2CBase *hs);
    ~FetchContext() {}

    inline void send(const PeerId &replica);
    inline void reset_timeout();
    inline void add_replica(const PeerId &replica, bool fetch_now = true);
};

class BlockDeliveryContext: public promise_t {
    public:
    ElapsedTime elapsed;
    BlockDeliveryContext &operator=(const BlockDeliveryContext &) = delete;
    BlockDeliveryContext(const BlockDeliveryContext &other):
        promise_t(static_cast<const promise_t &>(other)),
        elapsed(other.elapsed) {}
    BlockDeliveryContext(BlockDeliveryContext &&other):
        promise_t(static_cast<const promise_t &>(other)),
        elapsed(std::move(other.elapsed)) {}
    template<typename Func>
    BlockDeliveryContext(Func callback): promise_t(callback) {
        elapsed.start();
    }
};

/** E2C protocol (with network implementation). */
class E2CBase: public E2CCore {
    using BlockFetchContext = FetchContext<ENT_TYPE_BLK>;
    using CmdFetchContext = FetchContext<ENT_TYPE_CMD>;

    friend BlockFetchContext;
    friend CmdFetchContext;

    public:
    using Net = PeerNetwork<opcode_t>;
    using commit_cb_t = std::function<void(const Finality &)>;
    EventContext ec;

    protected:
    /** the binding address in replica network */
    NetAddr listen_addr;
    /** the block size */
    size_t blk_size;
    /** libevent handle */
    salticidae::ThreadCall tcall;
    VeriPool vpool;
    std::vector<PeerId> peers;

    private:
    /** whether libevent handle is owned by itself */
    bool ec_loop;
    /** network stack */
    Net pn;
    std::unordered_set<uint256_t> valid_tls_certs;
    pacemaker_bt pmaker;
    /* queues for async tasks */
    std::unordered_map<const uint256_t, BlockFetchContext> blk_fetch_waiting;
    std::unordered_map<const uint256_t, BlockDeliveryContext> blk_delivery_waiting;
    std::unordered_map<const uint256_t, commit_cb_t> decision_waiting;
    using cmd_queue_t = salticidae::MPSCQueueEventDriven<std::pair<uint256_t, commit_cb_t>>;
    cmd_queue_t cmd_pending;
    std::queue<uint256_t> cmd_pending_buffer;

    /* statistics */
    uint64_t fetched;
    uint64_t delivered;
    mutable uint64_t nsent;
    mutable uint64_t nrecv;

    mutable uint32_t part_parent_size;
    mutable uint32_t part_fetched;
    mutable uint32_t part_delivered;
    mutable uint32_t part_decided;
    mutable uint32_t part_gened;
    mutable double part_delivery_time;
    mutable double part_delivery_time_min;
    mutable double part_delivery_time_max;
    mutable std::unordered_map<const PeerId, uint32_t> part_fetched_replica;

    void on_fetch_cmd(const command_t &cmd);
    void on_fetch_blk(const block_t &blk);
    bool on_deliver_blk(const block_t &blk);

    // Set Commit timer and handler for every block
    std::queue<std::pair<uint256_t,TimerEvent>> commit_queue;
    // On finishing 2\delta, use this to commit this block and all its ancestors
    inline void commit_timer_cb (uint256_t blk_hash);

    /** deliver consensus message: <propose> */
    inline void propose_handler(MsgPropose &&, const Net::conn_t &);
    /** fetches full block data */
    inline void req_blk_handler(MsgReqBlock &&, const Net::conn_t &);
    /** receives a block */
    inline void resp_blk_handler(MsgRespBlock &&, const Net::conn_t &);

    inline bool conn_handler(const salticidae::ConnPool::conn_t &, bool);

    // Sendall <PROPOSE>
    void do_broadcast_proposal(const Proposal &) override;
    // We call the action of committing, DECIDING
    void do_decide(Finality &&) override;
    void do_consensus(const block_t &blk) override;

    protected:

    /** Called to replicate the execution of a command, the application should
     * implement this to make transition for the application state. */
    virtual void state_machine_execute(const Finality &) = 0;

    public:
    E2CBase(uint32_t blk_size,
            ReplicaID rid,
            privkey_bt &&priv_key,
            NetAddr listen_addr,
            pacemaker_bt pmaker,
            EventContext ec,
            size_t nworker,
            const Net::Config &netconfig);

    ~E2CBase();

    /* the API for E2CBase */

    /* Submit the command to be decided. */
    void exec_command(uint256_t cmd_hash, commit_cb_t callback);
    void start(std::vector<std::tuple<NetAddr, pubkey_bt, uint256_t>> &&replicas,
                bool ec_loop = false);

    size_t size() const { return peers.size(); }
    const auto &get_decision_waiting() const { return decision_waiting; }
    ThreadCall &get_tcall() { return tcall; }
    PaceMaker *get_pace_maker() { return pmaker.get(); }
    void print_stat() const;
    virtual void do_elected() {}

    /* Helper functions */
    /** Returns a promise resolved (with command_t cmd) when Command is fetched. */
    promise_t async_fetch_cmd(const uint256_t &cmd_hash, const PeerId *replica, bool fetch_now = true);
    /** Returns a promise resolved (with block_t blk) when Block is fetched. */
    promise_t async_fetch_blk(const uint256_t &blk_hash, const PeerId *replica, bool fetch_now = true);
    /** Returns a promise resolved (with block_t blk) when Block is delivered (i.e. prefix is fetched). */
    promise_t async_deliver_blk(const uint256_t &blk_hash,  const PeerId &replica);
};

/** E2C protocol (templated by cryptographic implementation). */
template<typename PrivKeyType = PrivKeyDummy,
        typename PubKeyType = PubKeyDummy,
        typename PartCertType = PartCertDummy,
        typename QuorumCertType = QuorumCertDummy>
class E2C: public E2CBase {
    using E2CBase::E2CBase;
    protected:

    part_cert_bt create_part_cert(const PrivKey &priv_key, const uint256_t &blk_hash) override {
        return new PartCertType(
                    static_cast<const PrivKeyType &>(priv_key),
                    blk_hash);
    }

    part_cert_bt parse_part_cert(DataStream &s) override {
        PartCert *pc = new PartCertType();
        s >> *pc;
        return pc;
    }

    quorum_cert_bt create_quorum_cert(const uint256_t &blk_hash) override {
        return new QuorumCertType(get_config(), blk_hash);
    }

    quorum_cert_bt parse_quorum_cert(DataStream &s) override {
        QuorumCert *qc = new QuorumCertType();
        s >> *qc;
        return qc;
    }

    public:
    E2C(uint32_t blk_size,
            ReplicaID rid,
            const bytearray_t &raw_privkey,
            NetAddr listen_addr,
            pacemaker_bt pmaker,
            EventContext ec = EventContext(),
            size_t nworker = 4,
            const Net::Config &netconfig = Net::Config()):
        E2CBase(blk_size,
                    rid,
                    new PrivKeyType(raw_privkey),
                    listen_addr,
                    std::move(pmaker),
                    ec,
                    nworker,
                    netconfig) {}

    void start(const std::vector<std::tuple<NetAddr, bytearray_t, bytearray_t>> &replicas, bool ec_loop = false) {
        std::vector<std::tuple<NetAddr, pubkey_bt, uint256_t>> reps;
        for (auto &r: replicas)
            reps.push_back(
                std::make_tuple(
                    std::get<0>(r),
                    new PubKeyType(std::get<1>(r)),
                    uint256_t(std::get<2>(r))
                ));
        E2CBase::start(std::move(reps), ec_loop);
    }
};

using E2CNoSig = E2C<>;
using E2CSecp256k1 = E2C<PrivKeySecp256k1, PubKeySecp256k1,
                                    PartCertSecp256k1, QuorumCertSecp256k1>;

template<EntityType ent_type>
FetchContext<ent_type>::FetchContext(FetchContext && other):
        promise_t(static_cast<const promise_t &>(other)),
        hs(other.hs),
        fetch_msg(std::move(other.fetch_msg)),
        ent_hash(other.ent_hash),
        replicas(std::move(other.replicas)) {
    other.timeout.del();
    timeout = TimerEvent(hs->ec,
            std::bind(&FetchContext::timeout_cb, this, _1));
    reset_timeout();
}

template<>
inline void FetchContext<ENT_TYPE_CMD>::timeout_cb(TimerEvent &) {
    for (const auto &replica: replicas)
        send(replica);
    reset_timeout();
}

template<>
inline void FetchContext<ENT_TYPE_BLK>::timeout_cb(TimerEvent &) {
    for (const auto &replica: replicas)
        send(replica);
    reset_timeout();
}

template<EntityType ent_type>
FetchContext<ent_type>::FetchContext(
                                const uint256_t &ent_hash, E2CBase *hs):
            promise_t([](promise_t){}),
            hs(hs), ent_hash(ent_hash) {
    fetch_msg = std::vector<uint256_t>{ent_hash};

    timeout = TimerEvent(hs->ec,
            std::bind(&FetchContext::timeout_cb, this, _1));
    reset_timeout();
}

template<EntityType ent_type>
void FetchContext<ent_type>::send(const PeerId &replica) {
    hs->part_fetched_replica[replica]++;
    hs->pn.send_msg(fetch_msg, replica);
}

template<EntityType ent_type>
void FetchContext<ent_type>::reset_timeout() {
    timeout.add(salticidae::gen_rand_timeout(ent_waiting_timeout));
}

template<EntityType ent_type>
void FetchContext<ent_type>::add_replica(const PeerId &replica, bool fetch_now) {
    if (replicas.empty() && fetch_now)
        send(replica);
    replicas.insert(replica);
}

}
