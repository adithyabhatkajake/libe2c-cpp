/**
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "libe2c/e2c.h"
#include "libe2c/client.h"
#include "libe2c/liveness.h"

using salticidae::static_pointer_cast;

namespace e2c {

const opcode_t MsgPropose::opcode;
MsgPropose::MsgPropose(const Proposal &proposal) {
    serialized << proposal;
}
void MsgPropose::postponed_parse(E2CCore *hsc) {
    proposal.hsc = hsc;
    serialized >> proposal;
}

// const opcode_t MsgVote::opcode;
// MsgVote::MsgVote(const Vote &vote) { serialized << vote; }
// void MsgVote::postponed_parse(E2CCore *hsc) {
//     vote.hsc = hsc;
//     serialized >> vote;
// }

const opcode_t MsgReqBlock::opcode;
MsgReqBlock::MsgReqBlock(const std::vector<uint256_t> &blk_hashes) {
    serialized << htole((uint32_t)blk_hashes.size());
    for (const auto &h: blk_hashes)
        serialized << h;
}

MsgReqBlock::MsgReqBlock(DataStream &&s) {
    uint32_t size;
    s >> size;
    size = letoh(size);
    blk_hashes.resize(size);
    for (auto &h: blk_hashes) s >> h;
}

const opcode_t MsgRespBlock::opcode;
MsgRespBlock::MsgRespBlock(const std::vector<block_t> &blks) {
    serialized << htole((uint32_t)blks.size());
    for (auto blk: blks) serialized << *blk;
}

void MsgRespBlock::postponed_parse(E2CCore *hsc) {
    uint32_t size;
    serialized >> size;
    size = letoh(size);
    blks.resize(size);
    for (auto &blk: blks)
    {
        Block _blk;
        _blk.unserialize(serialized, hsc);
        blk = hsc->storage->add_blk(std::move(_blk), hsc->get_config());
    }
}

void E2CBase::exec_command(uint256_t cmd_hash, commit_cb_t callback) {
    cmd_pending.enqueue(std::make_pair(cmd_hash, callback));
}

void E2CBase::on_fetch_blk(const block_t &blk) {
    part_fetched++;
    fetched++;
    const uint256_t &blk_hash = blk->get_hash();
    auto it = blk_fetch_waiting.find(blk_hash);
    if (it != blk_fetch_waiting.end())
    {
        it->second.resolve(blk);
        blk_fetch_waiting.erase(it);
    }
}

bool E2CBase::on_deliver_blk(const block_t &blk) {
    const uint256_t &blk_hash = blk->get_hash();
    bool valid;
    /* sanity check: all parents must be delivered */
    for (const auto &p: blk->get_parent_hashes())
        assert(storage->is_blk_delivered(p));
    if ((valid = E2CCore::on_deliver_blk(blk)))
    {
        part_parent_size += blk->get_parent_hashes().size();
        part_delivered++;
        delivered++;
    }

    bool res = true;
    auto it = blk_delivery_waiting.find(blk_hash);
    if (it != blk_delivery_waiting.end())
    {
        auto &pm = it->second;
        if (valid)
        {
            pm.elapsed.stop(false);
            auto sec = pm.elapsed.elapsed_sec;
            part_delivery_time += sec;
            part_delivery_time_min = std::min(part_delivery_time_min, sec);
            part_delivery_time_max = std::max(part_delivery_time_max, sec);

            pm.resolve(blk);
        }
        else
        {
            pm.reject(blk);
            res = false;
        }
        blk_delivery_waiting.erase(it);
    }
    return res;
}

promise_t E2CBase::async_fetch_blk(const uint256_t &blk_hash,
                                        const PeerId *replica,
                                        bool fetch_now) {
    if (storage->is_blk_fetched(blk_hash))
        return promise_t([this, &blk_hash](promise_t pm){
            pm.resolve(storage->find_blk(blk_hash));
        });
    auto it = blk_fetch_waiting.find(blk_hash);
    if (it == blk_fetch_waiting.end())
    {
        it = blk_fetch_waiting.insert(
            std::make_pair(
                blk_hash,
                BlockFetchContext(blk_hash, this))).first;
    }
    if (replica != nullptr)
        it->second.add_replica(*replica, fetch_now);
    return static_cast<promise_t &>(it->second);
}

promise_t E2CBase::async_deliver_blk(const uint256_t &blk_hash,
                                        const PeerId &replica) {
    if (storage->is_blk_delivered(blk_hash))
        return promise_t([this, &blk_hash](promise_t pm) {
            pm.resolve(storage->find_blk(blk_hash));
        });
    auto it = blk_delivery_waiting.find(blk_hash);
    if (it != blk_delivery_waiting.end())
        return static_cast<promise_t &>(it->second);
    BlockDeliveryContext pm{[](promise_t){}};
    it = blk_delivery_waiting.insert(std::make_pair(blk_hash, pm)).first;
    /* otherwise the on_deliver_batch will resolve */
    async_fetch_blk(blk_hash, &replica).then([this, replica](block_t blk) {
        /* qc_ref should be fetched */
        std::vector<promise_t> pms;
        if (blk == get_genesis())
            pms.push_back(promise_t([](promise_t &pm){ pm.resolve(true); }));
        else
            pms.push_back(blk->verify(this, vpool));
        /* the parents should be delivered */
        for (const auto &phash: blk->get_parent_hashes())
            pms.push_back(async_deliver_blk(phash, replica));
        promise::all(pms).then([this, blk](const promise::values_t values) {
            auto ret = promise::any_cast<bool>(values[0]) && this->on_deliver_blk(blk);
            if (!ret) {}
        });
    });
    return static_cast<promise_t &>(pm);
}

void E2CBase::propose_handler(MsgPropose &&msg, const Net::conn_t &conn) {
    const PeerId &peer = conn->get_peer_id();
    if (peer.is_null()) return;
    msg.postponed_parse(this);
    auto &prop = msg.proposal;
    block_t blk = prop.blk;
    if(blk->get_height() == 0) {
        return;
    }
    // Ensure the correct proposer is proposing
    if(blk->get_proposer() != get_pace_maker()->get_proposer()) {
        logger.warning("Received a block from rid: %u, expected from rid: %u" ,
                       blk->get_proposer(), get_pace_maker()->get_proposer());
        logger.warning("Incoming Block: %s", std::string(*blk).c_str());
        return ;
    }
    if (!blk) return;
    promise::all(std::vector<promise_t>{
        async_deliver_blk(blk->get_hash(), peer)
    }).then([this, prop = std::move(prop)]() {
        on_receive_proposal(prop);
    });
}

    void E2CBase::do_consensus(const block_t &blk) {
        pmaker->on_consensus(blk);
    }

void E2CBase::req_blk_handler(MsgReqBlock &&msg, const Net::conn_t &conn) {
    const PeerId replica = conn->get_peer_id();
    if (replica.is_null()) return;
    auto &blk_hashes = msg.blk_hashes;
    std::vector<promise_t> pms;
    for (const auto &h: blk_hashes)
        pms.push_back(async_fetch_blk(h, nullptr));
    promise::all(pms).then([replica, this](const promise::values_t values) {
        std::vector<block_t> blks;
        for (auto &v: values)
        {
            auto blk = promise::any_cast<block_t>(v);
            blks.push_back(blk);
        }
        pn.send_msg(MsgRespBlock(blks), replica);
    });
}

void E2CBase::resp_blk_handler(MsgRespBlock &&msg, const Net::conn_t &) {
    msg.postponed_parse(this);
    for (const auto &blk: msg.blks)
        if (blk) on_fetch_blk(blk);
}

bool E2CBase::conn_handler(const salticidae::ConnPool::conn_t &conn, bool connected) {
    if (connected)
    {
        auto cert = conn->get_peer_cert();
        return (!cert) || valid_tls_certs.count(salticidae::get_hash(cert->get_der()));
    }
    return true;
}

void E2CBase::print_stat() const {
    logger.info("===== begin stats =====");
    logger.info("-------- queues -------");
    logger.info("blk_fetch_waiting: %lu", blk_fetch_waiting.size());
    logger.info("blk_delivery_waiting: %lu", blk_delivery_waiting.size());
    logger.info("decision_waiting: %lu", decision_waiting.size());
    logger.info("-------- misc ---------");
    logger.info("fetched: %lu", fetched);
    logger.info("delivered: %lu", delivered);
    logger.info("cmd_cache: %lu", storage->get_cmd_cache_size());
    logger.info("blk_cache: %lu", storage->get_blk_cache_size());
    logger.info("------ misc (10s) -----");
    logger.info("fetched: %lu", part_fetched);
    logger.info("delivered: %lu", part_delivered);
    logger.info("decided: %lu", part_decided);
    logger.info("gened: %lu", part_gened);
    logger.info("avg. parent_size: %.3f",
            part_delivered ? part_parent_size / double(part_delivered) : 0);
    logger.info("delivery time: %.3f avg, %.3f min, %.3f max",
            part_delivered ? part_delivery_time / double(part_delivered) : 0,
            part_delivery_time_min == double_inf ? 0 : part_delivery_time_min,
            part_delivery_time_max);

    part_parent_size = 0;
    part_fetched = 0;
    part_delivered = 0;
    part_decided = 0;
    part_gened = 0;
    part_delivery_time = 0;
    part_delivery_time_min = double_inf;
    part_delivery_time_max = 0;
    logger.info("--- replica msg. (10s) ---");
    size_t _nsent = 0;
    size_t _nrecv = 0;
    for (const auto &replica: peers)
    {
        auto conn = pn.get_peer_conn(replica);
        if (conn == nullptr) continue;
        size_t ns = conn->get_nsent();
        size_t nr = conn->get_nrecv();
        size_t nsb = conn->get_nsentb();
        size_t nrb = conn->get_nrecvb();
        conn->clear_msgstat();
        logger.info("%s: %u(%u), %u(%u), %u",
            get_hex10(replica).c_str(), ns, nsb, nr, nrb, part_fetched_replica[replica]);
        _nsent += ns;
        _nrecv += nr;
        part_fetched_replica[replica] = 0;
    }
    nsent += _nsent;
    nrecv += _nrecv;
    logger.info("sent: %lu", _nsent);
    logger.info("recv: %lu", _nrecv);
    logger.info("--- replica msg. total ---");
    logger.info("sent: %lu", nsent);
    logger.info("recv: %lu", nrecv);
    logger.info("====== end stats ======");
}

E2CBase::E2CBase(uint32_t blk_size,
                    ReplicaID rid,
                    privkey_bt &&priv_key,
                    NetAddr listen_addr,
                    pacemaker_bt pmaker,
                    EventContext ec,
                    size_t nworker,
                    const Net::Config &netconfig):
        E2CCore(rid, std::move(priv_key)),
        ec(ec),
        listen_addr(listen_addr),
        blk_size(blk_size),
        tcall(ec),
        vpool(ec, nworker),
        pn(ec, netconfig),
        pmaker(std::move(pmaker)),

        fetched(0), delivered(0),
        nsent(0), nrecv(0),
        part_parent_size(0),
        part_fetched(0),
        part_delivered(0),
        part_decided(0),
        part_gened(0),
        part_delivery_time(0),
        part_delivery_time_min(double_inf),
        part_delivery_time_max(0)
{
    /* register the handlers for msg from replicas */
    pn.reg_handler(salticidae::generic_bind(&E2CBase::propose_handler, this, _1, _2));
    pn.reg_handler(salticidae::generic_bind(&E2CBase::req_blk_handler, this, _1, _2));
    pn.reg_handler(salticidae::generic_bind(&E2CBase::resp_blk_handler, this, _1, _2));
    pn.reg_conn_handler(salticidae::generic_bind(&E2CBase::conn_handler, this, _1, _2));
    pn.start();
    pn.listen(listen_addr);
}

void E2CBase::do_broadcast_proposal(const Proposal &prop) {
    pn.multicast_msg(MsgPropose(prop), peers);
}

void E2CBase::do_decide(Finality &&fin) {
    part_decided++;
    state_machine_execute(fin);
    auto it = decision_waiting.find(fin.cmd_hash);
    if (it != decision_waiting.end())
    {
        it->second(std::move(fin));
        decision_waiting.erase(it);
    }
}

E2CBase::~E2CBase() {}

void E2CBase::start(
        std::vector<std::tuple<NetAddr, pubkey_bt, uint256_t>> &&replicas,
        bool ec_loop) {
    for (size_t i = 0; i < replicas.size(); i++)
    {
        auto &addr = std::get<0>(replicas[i]);
        auto cert_hash = std::move(std::get<2>(replicas[i]));
        valid_tls_certs.insert(cert_hash);
        salticidae::PeerId peer{cert_hash};
        E2CCore::add_replica(i, peer, std::move(std::get<1>(replicas[i])));
        if (addr != listen_addr)
        {
            peers.push_back(peer);
            pn.add_peer(peer);
            pn.set_peer_addr(peer, addr);
            pn.conn_peer(peer);
        }
    }

    uint32_t nfaulty = (peers.size()-1) / 2;
    if (nfaulty == 0)
    { /* TODO: Logging */}
    on_init(nfaulty);
    pmaker->init(this);
    if (ec_loop)
        ec.dispatch();

    cmd_pending.reg_handler(ec, [this](cmd_queue_t &q) {
        std::pair<uint256_t, commit_cb_t> e;
        while (q.try_dequeue(e))
        {
            ReplicaID proposer = pmaker->get_proposer();
            const auto &cmd_hash = e.first;
            auto it = decision_waiting.find(cmd_hash);
            if (it == decision_waiting.end())
                it = decision_waiting.insert(std::make_pair(cmd_hash, e.second)).first;
            else
                e.second(Finality(id, 0, 0, 0, cmd_hash, uint256_t()));
            if (proposer != get_id()) continue;
            cmd_pending_buffer.push(cmd_hash);
            if (cmd_pending_buffer.size() >= blk_size)
            {
                std::vector<uint256_t> cmds;
                for (uint32_t i = 0; i < blk_size; i++)
                {
                    cmds.push_back(cmd_pending_buffer.front());
                    cmd_pending_buffer.pop();
                }
                logger.info("Leader is trying to propose here.");
                pmaker->beat().then([this, cmds = std::move(cmds)](ReplicaID proposer) {
                    if (proposer == get_id()) {
                        logger.info("Calling propose");
                        on_propose(cmds, get_parents());
                        logger.info("Finished proposing");
                    }
                });
                return true;
            }
        }
        return false;
    });
}

}
