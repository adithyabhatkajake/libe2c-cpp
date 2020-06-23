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

#include "libe2c/entity.h"
#include "libe2c/e2c.h"

namespace e2c {

void Block::serialize(DataStream &s) const {
    s << htole((uint32_t)proposer) ;
    s << htole((uint32_t)height) ;
    s << htole((uint32_t)parent_hashes.size());
    for (const auto &hash: parent_hashes)
        s << hash;
    s << htole((uint32_t)cmds.size());
    for (auto cmd: cmds)
        s << cmd;
    s << htole((uint32_t)extra.size()) << extra;
}

void Block::unserialize(DataStream &s, E2CCore *hsc) {
    uint32_t n;
    s >> n;
    n = letoh(n);
    proposer = n;
    s >> n;
    n = letoh(n);
    height = n;
    s >> n;
    n = letoh(n);
    parent_hashes.resize(n);
    for (auto &hash: parent_hashes)
        s >> hash;
    s >> n;
    n = letoh(n);
    cmds.resize(n);
    for (auto &cmd: cmds)
        s >> cmd;
    s >> n;
    n = letoh(n);
    if (n == 0)
        extra.clear();
    else
    {
        auto base = s.get_data_inplace(n);
        extra = bytearray_t(base, base + n);
    }
    hash = salticidae::get_hash(*this);
}

bool Block::verify(const E2CCore *hsc) const {
    return signature->verify ( hsc->get_config().get_pubkey(proposer) ) ;
}

promise_t Block::verify(const E2CCore *hsc, VeriPool &vpool) const {
    bool status = signature->verify ( hsc->get_config().get_pubkey(proposer) ) ;
    return promise_t([status](promise_t &pm) {
        pm.resolve(status);
    });
}

}
