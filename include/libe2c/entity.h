#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstddef>
#include <ios>

#include "salticidae/netaddr.h"
#include "salticidae/ref.h"
#include "libe2c/type.h"
#include "libe2c/util.h"
#include "libe2c/crypto.h"

/*
 * NOTE: Defines all the entities/messages used in the protocol
 */

namespace e2c {

enum EntityType {
    ENT_TYPE_CMD = 0x0,
    ENT_TYPE_BLK = 0x1
};

struct ReplicaInfo {
    ReplicaID id;
    salticidae::PeerId peer_id;
    pubkey_bt pubkey;

    ReplicaInfo(ReplicaID id,
                const salticidae::PeerId &peer_id,
                pubkey_bt &&pubkey):
        id(id), peer_id(peer_id), pubkey(std::move(pubkey)) {}

    ReplicaInfo(const ReplicaInfo &other):
        id(other.id), peer_id(other.peer_id),
        pubkey(other.pubkey->clone()) {}

    ReplicaInfo(ReplicaInfo &&other):
        id(other.id), peer_id(other.peer_id),
        pubkey(std::move(other.pubkey)) {}
};

class ReplicaConfig {
    std::unordered_map<ReplicaID, ReplicaInfo> replica_map;

    public:
    size_t nreplicas;
    size_t nmajority;

    ReplicaConfig(): nreplicas(0), nmajority(0) {}

    void add_replica(ReplicaID rid, const ReplicaInfo &info) {
        replica_map.insert(std::make_pair(rid, info));
        nreplicas++;
    }

    const ReplicaInfo &get_info(ReplicaID rid) const {
        auto it = replica_map.find(rid);
        if (it == replica_map.end())
            throw E2CError("rid %s not found",
                    get_hex(rid).c_str());
        return it->second;
    }

    const PubKey &get_pubkey(ReplicaID rid) const {
        return *(get_info(rid).pubkey);
    }

    const salticidae::PeerId &get_peer_id(ReplicaID rid) const {
        return get_info(rid).peer_id;
    }
};

class Block;
class E2CCore;

using block_t = salticidae::ArcObj<Block>;

class Command: public Serializable {
    friend E2CCore;
    public:
    virtual ~Command() = default;
    virtual const uint256_t &get_hash() const = 0;
    virtual bool verify() const = 0;
    virtual operator std::string () const {
        DataStream s;
        s << "<cmd id=" << get_hex10(get_hash()) << ">";
        return s;
    }
};

using command_t = ArcObj<Command>;

template<typename Hashable>
inline static std::vector<uint256_t>
get_hashes(const std::vector<Hashable> &plist) {
    std::vector<uint256_t> hashes;
    for (const auto &p: plist)
        hashes.push_back(p->get_hash());
    return hashes;
}

class Block {
    friend E2CCore;
    std::vector<uint256_t> parent_hashes;
    std::vector<uint256_t> cmds;
    bytearray_t extra;
    ReplicaID proposer ;
    uint32_t height;
    part_cert_bt signature ;
    // The signature is filled in MsgPropose

    /* the following fields can be derived from above */
    std::vector<block_t> parents;
    uint256_t hash;
    bool delivered;
    int8_t decision;
    EventContext commit_ec;
    TimerEvent commit_timer;
    std::thread commit_thread;

    public:
    Block():
        height(0),
        delivered(false), decision(0) {}

    Block(bool delivered, int8_t decision):
        height(0),
        hash(salticidae::get_hash(*this)),
        delivered(delivered), decision(decision) {}

    Block(const std::vector<block_t> &parents,
        const std::vector<uint256_t> &cmds,
        bytearray_t &&extra,
        uint32_t height,
        ReplicaID proposer,
        int8_t decision = 0):
            parent_hashes(get_hashes(parents)),
            cmds(cmds),
            extra(std::move(extra)),
            proposer(proposer),
            height(height),
            parents(parents),
            delivered(0),
            decision(decision) { hash = salticidae::get_hash(*this); }

    void set_signature (part_cert_bt cert) { signature = std::move(cert); }
    part_cert_bt& get_signature () {return signature; }

    /* Fetch the block's proposer */
    ReplicaID get_proposer() { return proposer; }

    void serialize(DataStream &s) const;

    void unserialize(DataStream &s, E2CCore *hsc);

    const std::vector<uint256_t> &get_cmds() const {
        return cmds;
    }

    const std::vector<block_t> &get_parents() const {
        return parents;
    }

    const std::vector<uint256_t> &get_parent_hashes() const {
        return parent_hashes;
    }

    const uint256_t &get_hash() const { return hash; }

    bool verify(const E2CCore *hsc) const;

    promise_t verify(const E2CCore *hsc, VeriPool &vpool) const;

    int8_t get_decision() const { return decision; }

    bool is_delivered() const { return delivered; }

    uint32_t get_height() const { return height; }

    const bytearray_t &get_extra() const { return extra; }

    operator std::string () const {
        DataStream s;
        s << "<block "
          << "id="  << get_hex10(hash) << " "
          << "height=" << std::to_string(height) << " "
          << "parent=" << get_hex10(parent_hashes[0]) << " "
          << "proposer=" << std::to_string(proposer) << " "
          << ">";
        return s;
    }
};

struct BlockHeightCmp {
    bool operator()(const block_t &a, const block_t &b) const {
        return a->get_height() < b->get_height();
    }
};

class EntityStorage {
    std::unordered_map<const uint256_t, block_t> blk_cache;
    std::unordered_map<const uint256_t, command_t> cmd_cache;
    public:
    bool is_blk_delivered(const uint256_t &blk_hash) {
        auto it = blk_cache.find(blk_hash);
        if (it == blk_cache.end()) return false;
        return it->second->is_delivered();
    }

    bool is_blk_fetched(const uint256_t &blk_hash) {
        return blk_cache.count(blk_hash);
    }

    block_t add_blk(Block &&_blk, const ReplicaConfig &/*config*/) {
        block_t blk = new Block(std::move(_blk));
        return blk_cache.insert(std::make_pair(blk->get_hash(), blk)).first->second;
    }

    const block_t &add_blk(const block_t &blk) {
        return blk_cache.insert(std::make_pair(blk->get_hash(), blk)).first->second;
    }

    block_t find_blk(const uint256_t &blk_hash) {
        auto it = blk_cache.find(blk_hash);
        return it == blk_cache.end() ? nullptr : it->second;
    }

    bool is_cmd_fetched(const uint256_t &cmd_hash) {
        return cmd_cache.count(cmd_hash);
    }

    const command_t &add_cmd(const command_t &cmd) {
        return cmd_cache.insert(std::make_pair(cmd->get_hash(), cmd)).first->second;
    }

    command_t find_cmd(const uint256_t &cmd_hash) {
        auto it = cmd_cache.find(cmd_hash);
        return it == cmd_cache.end() ? nullptr: it->second;
    }

    size_t get_cmd_cache_size() {
        return cmd_cache.size();
    }
    size_t get_blk_cache_size() {
        return blk_cache.size();
    }

    bool try_release_cmd(const command_t &cmd) {
        if (cmd.get_cnt() == 2) /* only referred by cmd and the storage */
        {
            const auto &cmd_hash = cmd->get_hash();
            cmd_cache.erase(cmd_hash);
            return true;
        }
        return false;
    }

    bool try_release_blk(const block_t &blk) {
        if (blk.get_cnt() == 2) /* only referred by blk and the storage */
        {
            const auto &blk_hash = blk->get_hash();
            blk_cache.erase(blk_hash);
            return true;
        }
        return false;
    }
};

}
