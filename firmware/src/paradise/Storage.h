#pragma once
#include "Model.h"
#include <cstddef>
namespace paradise {
struct Snapshot {uint32_t version=1,size=sizeof(State),sequence=0;State state;uint32_t hash=0;};
inline uint32_t checksum(const Snapshot& s){const auto* bytes=reinterpret_cast<const uint8_t*>(&s);uint32_t hash=2166136261u;for(size_t i=0;i<offsetof(Snapshot,hash);++i)hash=(hash^bytes[i])*16777619u;return hash;}
inline bool valid(const Snapshot& s){return s.version==1&&s.size==sizeof(State)&&s.hash==checksum(s)&&valid(s.state);}
inline const Snapshot* newest(const Snapshot& a,const Snapshot& b){bool x=valid(a),y=valid(b);if(!x&&!y)return nullptr;if(!y)return &a;if(!x)return &b;return int32_t(a.sequence-b.sequence)>=0?&a:&b;}
}
