bool TrackerHeadlessRuntime::PublishSnapshotFastIfChanged(std::string* error) {
    LoadRoomMetadata();
    const auto json = BuildSnapshotJson();
    if (json == last_snapshot_json_) {
        return SaveAutosaveState(error);
    }
    state_.revision += 1;
    const auto revised_json = BuildSnapshotJson();
    if (!AtomicWriteTextFile(config_.snapshot_path, revised_json, error)) {
        return false;
    }
    last_snapshot_json_ = revised_json;
    static_snapshot_payload_published_ = true;
    return SaveAutosaveState(error);
}

bool TrackerHeadlessRuntime::PublishSnapshotIfChanged(std::string* error) {
    LoadRoomMetadata();
    if (!EvaluatePackLogic(error)) {
        return false;
    }
    const auto json = BuildSnapshotJson();
    if (json == last_snapshot_json_) {
        return SaveAutosaveState(error);
    }
    state_.revision += 1;
    const auto revised_json = BuildSnapshotJson();
    if (!AtomicWriteTextFile(config_.snapshot_path, revised_json, error)) {
        return false;
    }
    last_snapshot_json_ = revised_json;
    static_snapshot_payload_published_ = true;
    return SaveAutosaveState(error);
}

void TrackerForwardingEventSink::emit(const Event& event) {
    inner_.emit(event);
    tracker_.ApplyEvent(event);
}

void TrackerForwardingEventSink::trace(const TraceRecord& record) {
    inner_.trace(record);
}
