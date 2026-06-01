#include "sekailink_server/room_server_node.hpp"

#include "sekailink_server/room_projection_restore.hpp"
#include "sekailink_server/room_restore.hpp"
#include "sekailink_server/room_retention.hpp"

#include <chrono>
#include <stdexcept>

namespace sekailink_server {

RoomServerNode::RoomServerNode(RoomServerNodeConfig config)
    : config_(std::move(config)) {}

RoomServerNode::~RoomServerNode() {
  stop();
}

bool RoomServerNode::start() {
  if (running_) {
    return false;
  }

  if (!config_.audit_root.empty()) {
    audit_store_.emplace(config_.audit_root);
  }
  if (!config_.projection_root.empty()) {
    projection_store_ = make_projection_store(config_.projection_backend, config_.projection_root);
  }

  bootstrap_restore();

  tcp_service_ = std::make_unique<RoomServerTcpService>(
      registry_,
      audit_store_ ? &*audit_store_ : nullptr,
      projection_store_.get(),
      &config_.auth_policy);
  if (!tcp_service_->start_background(config_.tcp_port)) {
    tcp_service_.reset();
    if (projection_store_) {
      projection_store_.reset();
    }
    if (audit_store_) {
      audit_store_.reset();
    }
    return false;
  }

  try {
    http_server_ = std::make_unique<RoomServerHttpTcpServer>(registry_, config_.http_port, &config_.auth_policy);
  } catch (...) {
    tcp_service_->stop();
    tcp_service_.reset();
    if (projection_store_) {
      projection_store_.reset();
    }
    if (audit_store_) {
      audit_store_.reset();
    }
    throw;
  }

  stop_requested_ = false;
  running_ = true;
  http_thread_ = std::thread([this]() {
    while (!stop_requested_) {
      try {
        http_server_->serve_one();
      } catch (...) {
        if (stop_requested_) {
          break;
        }
      }
    }
  });

  if (config_.purge_expired_periodically) {
    purge_thread_ = std::thread([this]() {
      purge_loop();
    });
  }

  return true;
}

void RoomServerNode::stop() {
  if (!running_) {
    return;
  }
  stop_requested_ = true;

  if (tcp_service_) {
    tcp_service_->stop();
  }
  if (http_server_) {
    http_server_->stop();
  }

  if (http_thread_.joinable()) {
    http_thread_.join();
  }
  if (purge_thread_.joinable()) {
    purge_thread_.join();
  }

  http_server_.reset();
  tcp_service_.reset();
  projection_store_.reset();
  audit_store_.reset();
  running_ = false;
}

bool RoomServerNode::running() const {
  return running_;
}

std::uint16_t RoomServerNode::tcp_port() const {
  return tcp_service_ ? tcp_service_->port() : 0;
}

std::uint16_t RoomServerNode::http_port() const {
  return http_server_ ? http_server_->port() : 0;
}

const RoomRegistry& RoomServerNode::registry() const {
  return registry_;
}

void RoomServerNode::bootstrap_restore() {
  if (config_.restore_from_audit && audit_store_) {
    restore_all_rooms_from_audit(registry_, *audit_store_);
  }
  if (config_.restore_from_projection && projection_store_) {
    if (auto* sqlite_store = dynamic_cast<RoomProjectionSqliteStore*>(projection_store_.get()); sqlite_store != nullptr) {
      restore_rooms_from_projection_store(registry_, *sqlite_store);
    } else if (auto* mysql_store = dynamic_cast<RoomProjectionMysqlStore*>(projection_store_.get()); mysql_store != nullptr) {
      restore_rooms_from_projection_store(registry_, *mysql_store);
    }
  }
}

void RoomServerNode::purge_loop() {
  while (!stop_requested_) {
    purge_expired_rooms(registry_, audit_store_ ? &*audit_store_ : nullptr, utc_timestamp_now());
    std::this_thread::sleep_for(std::chrono::milliseconds(config_.purge_interval_ms));
  }
}

}  // namespace sekailink_server
