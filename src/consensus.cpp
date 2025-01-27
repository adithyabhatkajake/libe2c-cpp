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

#include <cassert>
#include <stack>

#include "libe2c/util.h"
#include "libe2c/consensus.h"

namespace e2c {

/* The core logic of HotStuff, is fairly simple :). */
/*** begin HotStuff protocol logic ***/
E2CCore::E2CCore(ReplicaID id, privkey_bt &&priv_key):
        b0(new Block(true, 1)),
        b_mark(b0),
        b_comm(b0),
        priv_key(std::move(priv_key)),
        tails{b0},
        id(id),
        storage(new EntityStorage()) {
    b0->proposer = 1;
    b0->hash = salticidae::get_hash(0) ;
    //  b0->cmds.clear();
    // b0->extra = nullptr;
    // b0->signature = nullptr;
    // b0->hash = salticidae::get_hash(b0);
    // // Parent hash is the same as current hash for b0
    // b0->parent_hashes[0] = b0->hash;
    storage->add_blk(b0);
}

std::vector<block_t> E2CCore::get_parents() {
    // Fetch parents for a new block
    // b_mark is the highest known block
    std::vector<block_t> parents;
    parents.reserve(b_mark->get_height()+1);
    for ( auto ht = b_mark->get_height() ; ht > 0; ht-- ) {
        parents.push_back(ht_blk_map[ht]);
    }
    // Push Genesis block
    parents.push_back(b0);
    return parents;
}


void E2CCore::sanity_check_delivered(const block_t &blk) {
    if (!blk->delivered)
        throw std::runtime_error("block not delivered");
}

block_t E2CCore::get_delivered_blk(const uint256_t &blk_hash) {
    block_t blk = storage->find_blk(blk_hash);
    if (blk == nullptr || !blk->delivered)
        throw std::runtime_error("block not delivered");
    return blk;
}

bool E2CCore::on_deliver_blk(const block_t &blk) {
    if (blk->delivered)
    {
        return false;
    }
    blk->parents.clear();
    for (const auto &hash: blk->parent_hashes)
        blk->parents.push_back(get_delivered_blk(hash));
    blk->height = blk->parents[0]->height + 1;

    for (auto pblk: blk->parents) tails.erase(pblk);
    tails.insert(blk);

    blk->delivered = true;
    return true;
}

void E2CCore::update(const block_t &nblk) {
    /* Received a new block: nblk
     * We first check if we have already received the parent
     * Then we add it to the commit_queue if it is not already present */
    bool status ;
    uint32_t ht = nblk->get_height();
    logger.info("Processing block at height: %u", ht);
    logger.info("Processing block as per map: %u", ht_blk_map.count(ht));
    if ( (status = (ht_blk_map.count(ht) == 1)) && ht_blk_map[ht]->get_hash() != nblk->get_hash()) {
        logger.warning("The leader has equivocated." );
        logger.warning("First Block");
        logger.warning("[%s]", std::string(*ht_blk_map[ht]).c_str());
        logger.warning("Parents:");
        for(auto &phash: ht_blk_map[ht]->parent_hashes) {
            logger.warning("%s", get_hex10(phash).c_str());
        }
        logger.warning("Commands");
        for(auto &phash: ht_blk_map[ht]->cmds) {
            logger.warning("%s", get_hex10(phash).c_str());
        }
        logger.warning("Extra");
        for(auto e: ht_blk_map[ht]->extra) {
            logger.warning("%u", e);
        }
        logger.warning("Second Block");
        logger.warning("[%s]", std::string(*nblk).c_str());
        logger.warning("Parents:");
        for(auto &phash: nblk->parent_hashes) {
            logger.warning("%s", get_hex10(phash).c_str());
        }
        logger.warning("Commands");
        for(auto &phash: nblk->cmds) {
            logger.warning("%s", get_hex10(phash).c_str());
        }
        logger.warning("Extra");
        for(auto e: nblk->extra) {
            logger.warning("%u", e);
        }
        uint256_t hash = salticidae::get_hash(*ht_blk_map[ht]);
        logger.warning("recomputed hash of old block: %s", get_hex10(hash).c_str());
        hash = salticidae::get_hash(*nblk);
        logger.warning("recomputed hash of new block: %s", get_hex10(hash).c_str());
        return;
    }
    if ( status ) {
        // Already got the block, return
        return;
    }
    /* Mark this block if this is the highest block for this height */
    if ( b_mark->get_height() < ht ) {
        b_mark = nblk ;
    }
    ht_blk_map [ht] = nblk;
    logger.info("Creating commit timer for block at height [%u] for time %.3f" , ht , get_delta());
    nblk->commit_timer = TimerEvent(nblk->commit_ec, [this,ht](TimerEvent &){
            commit_timer_cb(ht);
        });
    nblk->commit_timer.add(2*this->get_delta());
    // Start timer thread for this block
    // TODO Hopefully this will not overflow
    nblk->commit_thread = std::thread([nblk,this]() { nblk->commit_ec.dispatch(); });
}

block_t E2CCore::on_propose(const std::vector<uint256_t> &cmds,
                            const std::vector<block_t> &parents,
                            bytearray_t &&extra) {
    logger.info("Calling propose from Node %u" , get_id());
    if (parents.empty())
        throw std::runtime_error("empty parents");
    for (const auto &_: parents) tails.erase(_);
    /* create the new block */
    block_t bnew = storage->add_blk(
        new Block(parents, cmds,
            std::move(extra),
                  parents[0]->height + 1, get_id())
    );
    const uint256_t bnew_hash = salticidae::get_hash(*bnew);
    // logger.info("Computed Hash: %s, Block hash: %s",
    //             get_hex10(bnew_hash).c_str(), get_hex10(bnew->hash).c_str());
    bnew->hash = bnew_hash;
    bnew->signature = std::move(create_part_cert(*priv_key, bnew_hash));
    on_deliver_blk(bnew);
    update(bnew);
    Proposal prop(bnew, this);
    logger.info("Node %u proposing block %s", get_id(), std::string(*bnew).c_str());
    /* broadcast to other replicas */
    logger.info("Broadcasting proposal %s",std::string(prop).c_str());
    do_broadcast_proposal(prop);
    /* self-vote */
    on_propose_(prop);
    return bnew;
}

void E2CCore::on_receive_proposal(const Proposal &prop) {
    logger.info("Received a proposal from %u", prop.blk->get_proposer());
    block_t bnew = prop.blk;
    sanity_check_delivered(bnew);
    /* Forward proposal only if receiving for the first time */
    if ( ht_blk_map.count(bnew->get_height()) == 1 &&
         ht_blk_map[bnew->get_height()]->get_hash() == bnew->get_hash() ) {
        logger.info("Already handled this proposal. Discarding");
        logger.info("Existing block at height %u:", bnew->get_height());
        logger.info("Old Block: %s", std::string(*ht_blk_map[bnew->get_height()]).c_str());
        logger.info("Incoming Block: %s", std::string(*bnew).c_str());
        return;
    }
    update(bnew);
    //  Forward new proposal
    do_broadcast_proposal(prop) ;
    /* Resolve any callbacks */
    on_receive_proposal_(prop);
}

/*** end E2C protocol logic ***/
void E2CCore::on_init(uint32_t nfaulty) {
    config.nmajority = config.nreplicas - nfaulty;
    ht_blk_map[0] = b0;
}

/* 2\delta has passed. It is safe to commit now */
void E2CCore::commit_timer_cb(uint32_t ht) {
    uint32_t idx = ht;
    logger.info("Commit timer for height %u ended", ht);
    /* Commit this block and all parents */
    for (uint32_t i = ht ; ; i--) {
        auto blk = ht_blk_map[idx];
        logger.info("Committing Block %s", std::string(*blk).c_str());
        if ( blk->decision == 1 ) {
            return ;
        }
        // Clean up Heap
        // if ( blk->commit_ec != nullptr )
        //     delete blk->commit_ec ;
        // if ( blk->commit_timer != nullptr )
        blk->commit_timer.del() ;
        blk->decision = 1;
        do_consensus(blk);
        /* Execute all statements */
        for (size_t i = 0; i < blk->cmds.size(); i++) {
            do_decide(Finality(id, 1, i, blk->height,
                               blk->cmds[i], blk->get_hash()));
        }
    }
}

void E2CCore::add_replica(ReplicaID rid, const PeerId &peer_id,
                                pubkey_bt &&pub_key) {
    config.add_replica(rid,
            ReplicaInfo(rid, peer_id, std::move(pub_key)));
}

promise_t E2CCore::async_wait_proposal() {
    return propose_waiting.then([](const Proposal &prop) {
        return prop;
    });
}

promise_t E2CCore::async_wait_receive_proposal() {
    return receive_proposal_waiting.then([](const Proposal &prop) {
        return prop;
    });
}

void E2CCore::on_propose_(const Proposal &prop) {
    auto t = std::move(propose_waiting);
    propose_waiting = promise_t();
    t.resolve(prop);
}

void E2CCore::on_receive_proposal_(const Proposal &prop) {
    auto t = std::move(receive_proposal_waiting);
    receive_proposal_waiting = promise_t();
    t.resolve(prop);
}

E2CCore::operator std::string () const {
    DataStream s;
    s << "<E2C "
      << "b_mark=" << get_hex10(b_mark->get_hash()) << " "
      << "b_comm=" << get_hex10(b_comm->get_hash()) << " "
      << "tails=" << std::to_string(tails.size()) << ">";
    return s;
}

}
