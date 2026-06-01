#include "libretro_host_internal.hpp"

#include <memory>
#include <utility>

namespace sekaiemu::spike {

LibretroHost::LibretroHost(HostOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {}

LibretroHost::~LibretroHost() = default;

bool LibretroHost::Initialize() { return impl_->Initialize(); }

int LibretroHost::Run() { return impl_->Run(); }

std::string LibretroHost::LastError() const { return impl_->LastError(); }

}  // namespace sekaiemu::spike
