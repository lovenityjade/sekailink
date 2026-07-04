#pragma once

namespace sekailink_server {

struct RoomProjectionBatch;

class RoomProjectionStore {
 public:
  virtual ~RoomProjectionStore() = default;
  virtual void append_batch(const RoomProjectionBatch& batch) = 0;
};

}  // namespace sekailink_server
