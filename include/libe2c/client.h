#pragma once

#include "salticidae/msg.h"
#include "libe2c/type.h"
#include "libe2c/entity.h"
#include "libe2c/consensus.h"

namespace e2c {

struct MsgReqCmd {
    static const opcode_t opcode = 0x4;
    DataStream serialized;
    command_t cmd;
    MsgReqCmd(const Command &cmd) { serialized << cmd; }
    MsgReqCmd(DataStream &&s): serialized(std::move(s)) {}
};

struct MsgRespCmd {
    static const opcode_t opcode = 0x5;
    DataStream serialized;
// Because E2C does not work as a MACRO !! Dang it! :/
#if EEC_CMD_RESPSIZE > 0
    uint8_t payload[EEC_CMD_RESPSIZE];
#endif
    Finality fin;
    MsgRespCmd(const Finality &fin) {
        serialized << fin;
#if EEC_CMD_RESPSIZE > 0
        serialized.put_data(payload, payload + sizeof(payload));
#endif
    }
    MsgRespCmd(DataStream &&s) {
        s >> fin;
    }
};

class CommandDummy: public Command {
    uint32_t cid;
    uint32_t n;
    uint256_t hash;
#if EEC_CMD_REQSIZE > 0
    uint8_t payload[EEC_CMD_REQSIZE];
#endif

    public:
    CommandDummy() {}
    ~CommandDummy() override {}

    CommandDummy(uint32_t cid, uint32_t n):
        cid(cid), n(n), hash(salticidae::get_hash(*this)) {}

    void serialize(DataStream &s) const override {
        s << cid << n;
#if EEC_CMD_REQSIZE > 0
        s.put_data(payload, payload + sizeof(payload));
#endif
    }

    void unserialize(DataStream &s) override {
        s >> cid >> n;
#if EEC_CMD_REQSIZE > 0
        auto base = s.get_data_inplace(EEC_CMD_REQSIZE);
        memmove(payload, base, sizeof(payload));
#endif
        hash = salticidae::get_hash(*this);
    }

    const uint256_t &get_hash() const override {
        return hash;
    }

    bool verify() const override {
        return true;
    }
};

}
