#pragma once

#include "promise.hpp"
#include "salticidae/event.h"
#include "salticidae/ref.h"
#include "salticidae/netaddr.h"
#include "salticidae/network.h"
#include "salticidae/stream.h"
#include "salticidae/type.h"
#include "salticidae/util.h"

namespace e2c {

using salticidae::RcObj;
using salticidae::ArcObj;
using salticidae::BoxObj;

using salticidae::uint256_t;
using salticidae::DataStream;
using salticidae::htole;
using salticidae::letoh;
using salticidae::get_hex;
using salticidae::from_hex;
using salticidae::bytearray_t;
using salticidae::get_hex10;
using salticidae::get_hash;

using salticidae::NetAddr;
using salticidae::PeerId;
using salticidae::TimerEvent;
using salticidae::FdEvent;
using salticidae::EventContext;
using promise::promise_t;

class E2CError: public salticidae::SalticidaeError {
    public:
    template<typename... Args>
    E2CError(Args... args): salticidae::SalticidaeError(args...) {}
};

class E2CEntity: public E2CError {
    public:
    template<typename... Args>
    E2CEntity(Args... args): E2CError(args...) {}
};

using salticidae::Serializable;

class Cloneable {
    public:
    virtual ~Cloneable() = default;
    virtual Cloneable *clone() = 0;
};

using ReplicaID = uint16_t;
using opcode_t = uint8_t;
using tls_pkey_bt = BoxObj<salticidae::PKey>;
using tls_x509_bt = BoxObj<salticidae::X509>;

}
